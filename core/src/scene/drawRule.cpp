#include "drawRule.h"

#include "tile/tileBuilder.h"
#include "scene/scene.h"
#include "scene/sceneLayer.h"
#include "scene/stops.h"
#include "scene/styleContext.h"
#include "style/style.h"
#include "platform.h"
#include "drawRuleWarnings.h"
#include "util/hash.h"

#include <algorithm>

namespace Tangram {

DrawRuleData::DrawRuleData(std::string name, int id,
                           std::vector<StyleParam> parameters)
    : parameters(std::move(parameters)),
      name(std::move(name)),
      id(id) {}

std::string DrawRuleData::toString() const {
    std::string str = "{\n";
    for (auto& p : parameters) {
         str += " { "
             + std::to_string(static_cast<int>(p.key))
             + ", "
             + p.toString()
             + " }\n";
    }
    str += "}\n";

    return str;
}

DrawRule::DrawRule(const DrawRuleData& ruleData, const SceneLayer& layer) :
    name(&ruleData.name),
    id(ruleData.id) {

    const char* layerName = layer.name().c_str();
    const auto layerDepth = layer.depth();

    for (const auto& param : ruleData.parameters) {
        auto key = static_cast<uint8_t>(param.key);
        active[key] = true;
        params[key] = { &param, layerName, layerDepth };
    }
}

void DrawRule::merge(const DrawRuleData& ruleData, const SceneLayer& layer) {

    evalConflict(*this, ruleData, layer);

    const auto depthNew = layer.depth();
    const char* layerNew = layer.name().c_str();

    for (const auto& paramNew : ruleData.parameters) {

        auto key = static_cast<uint8_t>(paramNew.key);
        auto& param = params[key];

        if (!active[key] || depthNew > param.depth ||
            (depthNew == param.depth && strcmp(layerNew, param.name) > 0)) {
            param = { &paramNew, layerNew, depthNew };
            active[key] = true;
        }
    }
}

bool DrawRule::isJSFunction(StyleParamKey key) const {
    auto& param = findParameter(key);
    if (!param) {
        return false;
    }
    return param.function >= 0;
}

bool DrawRule::contains(StyleParamKey key) const {
    return findParameter(key) != false;
}

const StyleParam& DrawRule::findParameter(StyleParamKey key) const {
    static const StyleParam NONE;

    uint8_t k = static_cast<uint8_t>(key);
    if (!active[k]) { return NONE; }
    return *params[k].param;
}

const std::string& DrawRule::getStyleName() const {

    const auto& style = findParameter(StyleParamKey::style);

    if (style) {
        return style.value.get<std::string>();
    } else {
        return *name;
    }
}

const char* DrawRule::getLayerName(StyleParamKey key) const {
    return params[static_cast<uint8_t>(key)].name;
}

size_t DrawRule::getParamSetHash() const {
    size_t seed = 0;
    for (size_t i = 0; i < StyleParamKeySize; i++) {
        if (active[i]) { hash_combine(seed, params[i].name); }
    }
    return seed;
}

void DrawRule::logGetError(StyleParamKey expectedKey, const StyleParam& param) const {
    LOGE("wrong type '%d'for StyleParam '%d'", param.value.which(), expectedKey);
}

bool DrawRuleMergeSet::match(const Feature& feature, const SceneLayer& layer, StyleContext& ctx) {

    ctx.setFeature(feature);
    m_matchedRules.clear();
    m_queuedLayers.clear();

    // If uber layer is marked not visible return immediately
    if (!layer.visible()) {
        return false;
    }

    // If the first filter doesn't match, return immediately
    if (!layer.filter().eval(feature, ctx)) { return false; }

    m_queuedLayers.push_back(&layer);

    // Iterate depth-first over the layer hierarchy
    while (!m_queuedLayers.empty()) {

        // Pop a layer off the top of the stack
        const auto& layer = *m_queuedLayers.back();
        m_queuedLayers.pop_back();

        // Merge rules from layer into accumulated set
        mergeRules(layer);

        // Push each of the layer's matching sublayers onto the stack
        for (const auto& sublayer : layer.sublayers()) {
            // Skip matching this sublayer if marked not visible
            if (!sublayer.visible()) {
                continue;
            }

            if (sublayer.filter().eval(feature, ctx)) {
                m_queuedLayers.push_back(&sublayer);
            }
        }
    }

    return true;
}

void DrawRuleMergeSet::apply(const Feature& feature, const SceneLayer& layer,
                             StyleContext& ctx, TileBuilder& builder) {

    // If no rules matched the feature, return immediately
    if (!match(feature, layer, ctx)) { return; }

    // For each matched rule, find the style to be used and
    // build the feature with the rule's parameters
    for (auto& rule : m_matchedRules) {

        StyleBuilder* style = builder.getStyleBuilder(rule.getStyleName());
        if (!style) {
            LOGE("Invalid style %s", rule.getStyleName().c_str());
            continue;
        }

        bool visible;
        if (rule.get(StyleParamKey::visible, visible) && !visible) {
            continue;
        }

        bool valid = true;
        for (size_t i = 0; i < StyleParamKeySize; ++i) {

            if (!rule.active[i]) {
                rule.params[i].param = nullptr;
                continue;
            }

            auto*& param = rule.params[i].param;

            // Evaluate JS functions and Stops
            if (param->function >= 0) {

                // Copy param into 'evaluated' and point param to the evaluated StyleParam.
                m_evaluated[i] = *param;
                param = &m_evaluated[i];

                if (!ctx.evalStyle(param->function, param->key, m_evaluated[i].value)) {
                    if (StyleParam::isRequired(param->key)) {
                        valid = false;
                        break;
                    } else {
                        rule.active[i] = false;
                    }
                }
            } else if (param->stops) {
                m_evaluated[i] = *param;
                param = &m_evaluated[i];

                Stops::eval(*param->stops, param->key, ctx.getGlobalZoom(),
                            m_evaluated[i].value);
            }
        }

        if (valid) {

            // build outline explicitly with outline style
            const auto& outlineStyleName = rule.findParameter(StyleParamKey::outline_style);
            if (outlineStyleName) {
                auto& styleName = outlineStyleName.value.get<std::string>();
                auto* outlineStyle = builder.getStyleBuilder(styleName);
                if (!outlineStyle) {
                    LOGE("Invalid style %s", styleName.c_str());
                } else {
                    rule.isOutlineOnly = true;
                    outlineStyle->addFeature(feature, rule);
                    rule.isOutlineOnly = false;
                }
            }

            // build feature with style
            style->addFeature(feature, rule);
        }
    }
}

void DrawRuleMergeSet::mergeRules(const SceneLayer& layer) {

    size_t pos, end = m_matchedRules.size();

    for (const auto& rule : layer.rules()) {
        for (pos = 0; pos < end; pos++) {
            if (m_matchedRules[pos].id == rule.id) { break; }
        }

        if (pos == end) {
            m_matchedRules.emplace_back(rule, layer);
        } else {
            m_matchedRules[pos].merge(rule, layer);
        }
    }
}

}
