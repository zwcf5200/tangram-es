#include "styleMixer.h"
#include "style/style.h"
#include "util/topologicalSort.h"
#include "yaml-cpp/yaml.h"

#include <algorithm>
#include <set>

namespace Tangram {

std::vector<std::string> StyleMixer::getStylesToMix(const Node& style) {

    std::vector<std::string> names;

    // 'base' style is the first item to mix.
    if (const Node& base = style["base"]) {
        if (base.IsScalar()) { names.push_back(base.Scalar()); }
    }

    // 'mix' styles are mixed next, in order of declaration.
    if (const Node& mix = style["mix"]) {
        if (mix.IsScalar()) {
            names.push_back(mix.Scalar());
        } else if (mix.IsSequence()) {
            for (const auto& m : mix) {
                if (m.IsScalar()) { names.push_back(m.Scalar()); }
            }
        }
    }

    return names;
}

std::vector<std::string> StyleMixer::getMixingOrder(const Node& styles) {

    // Input must be a map of names to style configuration nodes.
    if (!styles.IsMap()) {
        return {};
    }

    // Dependencies are pairs of strings that form a DAG.
    // If style 'a' mixes style 'b', the dependency would be {'b', 'a'}.
    std::vector<std::pair<std::string, std::string>> dependencies;

    for (const auto& entry : styles) {
        const auto& name = entry.first;
        const auto& config = entry.second;
        for (const auto& mix : getStylesToMix(config)) {
            dependencies.push_back({ mix, name.Scalar() });
        }
    }

    return topologicalSort(dependencies);
}

void StyleMixer::mixStyleNodes(Node styles) {

    // First determine the order of nodes to evaluate.
    auto styleNamesSorted = getMixingOrder(styles);

    for (const auto& name : styleNamesSorted) {

        const auto& style = styles[name];

        if (!style || !style.IsMap()) {
            // Something's wrong here, try the next one!
            continue;
        }

        // For each style to evaluate, get the list of styles that need to be mixed with this one.
        auto stylesToMix = getStylesToMix(style);

        std::vector<Node> mixins;
        for (const auto& styleNameToMix : stylesToMix) {

            // Skip mixing built-in styles.
            const auto& builtIn = Style::builtInStyleNames();
            if (std::find(builtIn.begin(), builtIn.end(), styleNameToMix) != builtIn.end()) {
                continue;
            }

            mixins.push_back(styles[styleNameToMix]);
        }

        applyStyleMixins(style, mixins);
    }
}

void StyleMixer::applyStyleMixins(Node style, const std::vector<Node>& mixins) {

    // Merge boolean flags as a disjunction.
    mergeBooleanFieldAsDisjunction("animated", style, mixins);
    mergeBooleanFieldAsDisjunction("texcoords", style, mixins);

    // Merge scalar fields with newer values taking precedence.
    mergeFieldTakingLast("base", style, mixins);
    mergeFieldTakingLast("lighting", style, mixins);
    mergeFieldTakingLast("texture", style, mixins);
    mergeFieldTakingLast("blend", style, mixins);
    mergeFieldTakingLast("blend_order", style, mixins);

    // Merge map fields with newer values taking precedence.
    mergeMapFieldTakingLast("material", style, mixins);

    // Produce a list of all 'mixins' with shader nodes and merge those separately.
    std::vector<Node> shaderMixins;
    for (const auto& mixin : mixins) {
        if (const auto& shaders = mixin["shaders"]) {
            shaderMixins.push_back(shaders);
        }
    }
    applyShaderMixins(style["shaders"], shaderMixins);
}

void StyleMixer::applyShaderMixins(Node shaders, const std::vector<Node>& mixins) {

    // Merge maps fields with newer values taking precedence.
    mergeMapFieldTakingLast("defines", shaders, mixins);
    mergeMapFieldTakingLast("uniforms", shaders, mixins);

    // Merge "extensions" as a non-repeating set.
    {
        std::set<std::string> set;
        Node output = shaders["extensions_mixed"];
        output = Node(); // Clear this node in case something was already there.
        for (const auto& mixin : mixins) {
            Node extensions = mixin["extensions_mixed"];
            for (const auto& e : extensions) {
                set.insert(e.Scalar());
            }
        }
        Node extensions = shaders["extensions"];
        if (extensions.IsScalar()) {
            set.insert(extensions.Scalar());
        } else if (extensions.IsSequence()) {
            for (const auto& e : extensions) {
                set.insert(e.Scalar());
            }
        }
        for (const auto& extension : set) {
            output.push_back(extension);
        }
    }

    // Merge "blocks" into a list of strings for each key.
    {
        Node output = shaders["blocks_mixed"];
        output = Node(); // Clear this node in case something was already there.
        for (const auto& mixin : mixins) {
            Node blocks = mixin["blocks_mixed"];
            for (const auto& entry : blocks) {
                Node list = output[entry.first.Scalar()];
                for (const auto& block : entry.second) {
                    // If the list already contains an exact reference to the same node,
                    // don't add it again.
                    if (std::find(list.begin(), list.end(), block) == list.end()) {
                        list.push_back(block);
                    }
                }
            }
        }
        for (const auto& entry : shaders["blocks"]) {
            output[entry.first.Scalar()].push_back(entry.second.Scalar());
        }
    }
}

void StyleMixer::mergeBooleanFieldAsDisjunction(const std::string& key, Node target, const std::vector<Node>& sources) {

    auto current = target[key];
    if (current && current.as<bool>(false)) {
        // Target field is already true, we can stop here.
        return;
    }

    for (const auto& source : sources) {
        const auto& value = source[key];
        if (value && value.as<bool>(false)) {
            target[key] = true;
            return;
        }
    }
}

void StyleMixer::mergeFieldTakingLast(const std::string& key, Node target, const std::vector<Node>& sources) {

    if (target[key]) {
        // Target already has a value, we can stop here.
        return;
    }

    for (auto it = sources.rbegin(); it != sources.rend(); ++it) {
        const auto& value = (*it)[key];
        if (value) {
            target[key] = value;
            return;
        }
    }
}

void StyleMixer::mergeMapFieldTakingLast(const std::string& key, Node target, const std::vector<Node>& sources) {

    Node map = target[key];
    if (map && !map.IsMap()) { return; }

    for (auto it = sources.rbegin(); it != sources.rend(); ++it) {
        const auto& source = (*it)[key];
        if (!source.IsMap()) {
            continue;
        }
        for (const auto& entry : source) {
            const auto& subkey = entry.first.Scalar();
            if (!map[subkey]) {
                map[subkey] = entry.second;
            }
        }
    }

}

}
