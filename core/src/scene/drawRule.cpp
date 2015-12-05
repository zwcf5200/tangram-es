#include "drawRule.h"

#include "tile/tile.h"
#include "scene/scene.h"
#include "scene/sceneLayer.h"
#include "scene/stops.h"
#include "scene/styleContext.h"
#include "style/style.h"
#include "platform.h"

#include <algorithm>
#include <mutex>
#include <set>

namespace Tangram {

struct RuleTieHandler {

    RuleTieHandler(int d, const std::string& n) : currentDepth(d), currentLayerName(n.c_str()) {}

    int currentDepth = 0;
    const char* currentLayerName = nullptr;
    static std::set<std::string> log;
    static std::mutex logMutex;

    bool evalTie(DrawRule& rule, const DrawRuleData& data, const StyleParam& param) const {
        auto key = static_cast<uint8_t>(param.key);
        if (currentDepth == rule.depths[key] && !(param.value == rule.params[key]->value)) {
            std::lock_guard<std::mutex> lock(logMutex);
            std::string logString = "Draw parameter '" + StyleParam::keyName(param.key) + "' in rule '" +
                data.name + "' in layer '" + currentLayerName + "' conflicts with layer '" + rule.layers[key] + "'";

            if (log.insert(logString).second) { LOGW("%s", logString.c_str()); }

            return true;
        }
        rule.depths[key] = currentDepth;
        rule.layers[key] = currentLayerName;
        return false;
    };

};

std::set<std::string> RuleTieHandler::log{};
std::mutex RuleTieHandler::logMutex{};

DrawRuleData::DrawRuleData(std::string _name, int _id,
                               const std::vector<StyleParam>& _parameters) :
    parameters(_parameters),
    name(std::move(_name)),
    id(_id) {}

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

DrawRule::DrawRule(const DrawRuleData& _ruleData) :
    name(&_ruleData.name),
    id(_ruleData.id) {}

void DrawRule::merge(const DrawRuleData& _ruleData, const RuleTieHandler* _tieHandler) {
    for (const auto& param : _ruleData.parameters) {
        if (_tieHandler) { _tieHandler->evalTie(*this, _ruleData, param); }
        params[static_cast<uint8_t>(param.key)] = &param;
    }
}

bool DrawRule::isJSFunction(StyleParamKey _key) const {
    auto& param = findParameter(_key);
    if (!param) {
        return false;
    }
    return param.function >= 0;
}

bool DrawRule::contains(StyleParamKey _key) const {
    return findParameter(_key) != false;
}

const StyleParam& DrawRule::findParameter(StyleParamKey _key) const {
    static const StyleParam NONE;

    const auto* p = params[static_cast<uint8_t>(_key)];
    if (p) {
        if (p->function >= 0 || p->stops != nullptr) {
            return evaluated[static_cast<uint8_t>(_key)];
        }
        return *p;
    }

    return NONE;
}

const std::string& DrawRule::getStyleName() const {

    const auto& style = findParameter(StyleParamKey::style);

    if (style) {
        return style.value.get<std::string>();
    } else {
        return *name;
    }
}

void DrawRule::logGetError(StyleParamKey _expectedKey, const StyleParam& _param) const {
    LOGE("wrong type '%d'for StyleParam '%d'", _param.value.which(), _expectedKey);
}

bool DrawRuleMergeSet::match(const Feature& _feature, const SceneLayer& _layer, StyleContext& _ctx) {

    _ctx.setFeature(_feature);
    matchedRules.clear();
    queuedLayers.clear();

    // Match layers
    if (!_layer.filter().eval(_feature, _ctx)) { return false; }

    // Add initial drawrules
    RuleTieHandler tieHandler(1, _layer.name());
    mergeRules(_layer.rules(), &tieHandler);

    const auto& sublayers = _layer.sublayers();
    for (auto it = sublayers.begin(); it != sublayers.end(); ++it) {
        queuedLayers.push_back({ &(*it), 2 });
    }

    while (!queuedLayers.empty()) {
        const auto& layer = *(queuedLayers.front().first);
        int depth = queuedLayers.front().second;
        queuedLayers.pop_front();

        if (!layer.filter().eval(_feature, _ctx)) { continue; }

        const auto& sublayers = layer.sublayers();
        for (auto it = sublayers.begin(); it != sublayers.end(); ++it) {
            queuedLayers.push_back({ &(*it), depth + 1 });
        }
        // override with sublayer drawrules
        tieHandler = { depth, layer.name() };
        mergeRules(layer.rules(), &tieHandler);
    }
    return true;
}

void DrawRuleMergeSet::apply(const Feature& _feature, const Scene& _scene, const SceneLayer& _layer,
                    StyleContext& _ctx, Tile& _tile) {

    if (!match(_feature, _layer, _ctx)) { return; }

    // Apply styles
    for (auto& rule : matchedRules) {

        auto* style = _scene.findStyle(rule.getStyleName());

        if (!style) {
            LOGE("Invalid style %s", rule.getStyleName().c_str());
            continue;
        }

        // Evaluate JS functions and Stops
        bool valid = true;
        for (size_t i = 0; i < StyleParamKeySize; ++i) {
            auto* param = rule.params[i];
            if (!param) { continue; }

            if (param->function >= 0) {
                if (!_ctx.evalStyle(param->function, param->key, rule.evaluated[i].value) &&
                    StyleParam::isRequired(param->key)) {
                    valid = false;
                    break;
                }
            }
            if (param->stops) {
                rule.evaluated[i].stops = param->stops;

                if (StyleParam::isColor(param->key)) {
                    rule.evaluated[i].value = param->stops->evalColor(_ctx.getGlobalZoom());
                } else if (StyleParam::isWidth(param->key)) {
                    // FIXME widht result is isgnored from here..
                    rule.evaluated[i].value = param->stops->evalWidth(_ctx.getGlobalZoom());
                } else {
                    rule.evaluated[i].value = param->stops->evalFloat(_ctx.getGlobalZoom());
                }
            }
            rule.evaluated[i].key = param->key;
        }

        if (valid) {
            style->buildFeature(_tile, _feature, rule);
        }
    }
}

void DrawRuleMergeSet::mergeRules(const std::vector<DrawRuleData>& rules, const RuleTieHandler* _tieHandler) {
    for (auto& rule : rules) {

        auto it = std::find_if(matchedRules.begin(), matchedRules.end(),
                               [&](auto& m) { return rule.id == m.id; });

        if (it == matchedRules.end()) {
            it = matchedRules.insert(it, DrawRule(rule));
        }

        it->merge(rule, _tieHandler);
    }
}

}
