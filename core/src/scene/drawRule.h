#pragma once

#include "scene/styleParam.h"

#include <vector>
#include <set>
#include <bitset>

namespace Tangram {

struct Feature;
class TileBuilder;
class Scene;
class SceneLayer;
class StyleContext;

/*
 * A draw rule is a named collection of style parameters. When a draw rule is found to match a
 * feature, the feature's geometry is built into drawable buffers using a style determined from the
 * rule with the parameters contained in the rule.
 *
 * Draw rules are represented in two ways: by a DrawRuleData and by a DrawRule.
 *
 * DrawRuleData represents a named set of style parameters *as they are written in the layer*.
 * This is different from the set of style parameters that is applied to a feature after matching
 * and merging; the merged set of style parameters is represented by DrawRule.
 *
 * DrawRule is a temporary object and only contains style parameters that are defined in at least
 * one DrawRuleData, so it can safely reference any needed parameters by pointers to the original.
 * However, style parameters that need to be evaluated per-feature must also have space to store
 * their evaluated values.
 *
 * When matching and merging draw rules, the name of the rule is frequently copied and compared. To
 * make this process faster, each string used as the name of a draw rule is assigned to an integer
 * index within the scene object and then stored as the id of a draw rule.
 */

struct DrawRuleData {

    // https://github.com/tangrams/tangram-docs/blob/gh-pages/pages/draw.md#style-parameters
    std::vector<StyleParam> parameters;

    // draw-rule name (and assigned id)
    // https://github.com/tangrams/tangram-docs/blob/gh-pages/pages/draw.md#draw-rule
    std::string name;
    int id;

    DrawRuleData(std::string name, int id, std::vector<StyleParam> parameters);

    std::string toString() const;

};

struct DrawRule {

    // Map of StypeParamKey => StyleParam pointer
    // of the matched SceneLayer or the evaluated
    // Function/Stops in DrawRuleMergeset.
    struct {
        const StyleParam* param;
        // SceneLayer name and depth
        const char* name;
        size_t depth;

    } params[StyleParamKeySize];

    // A mask to indicate which parameters are set.
    // 'active' MUST be checked before accessing 'params'
    // This is cheaper to zero out 4 byte than
    // 480 (on 32bit arch) or 980 byte for params array.
    std::bitset<StyleParamKeySize> active = { 0 };

    // draw-style name and id
    const std::string* name = nullptr;
    int id;
    bool isOutlineOnly = false;

    DrawRule(const DrawRuleData& ruleData, const SceneLayer& layer);

    void merge(const DrawRuleData& ruleData, const SceneLayer& layer);

    bool isJSFunction(StyleParamKey key) const;

    bool contains(StyleParamKey key) const;

    const std::string& getStyleName() const;

    const char* getLayerName(StyleParamKey key) const;

    size_t getParamSetHash() const;

    const StyleParam& findParameter(StyleParamKey key) const;

    template<typename T>
    bool get(StyleParamKey key, T& value) const {
        if (auto& param = findParameter(key)) {
            return StyleParam::Value::visit(param.value, StyleParam::visitor<T>{ value });
        }
        return false;
    }

    template<typename T>
    const T* get(StyleParamKey key) const {
        if (auto& param = findParameter(key)) {
            return StyleParam::Value::visit(param.value, StyleParam::visitor_ptr<T>{});
        }
        return nullptr;
    }

private:
    void logGetError(StyleParamKey expectedKey, const StyleParam& param) const;

};

class DrawRuleMergeSet {

public:
    /* Determine and apply DrawRules for a @feature and add
     * the result to @tile
     */
    void apply(const Feature& feature, const SceneLayer& sceneLayer,
               StyleContext& ctx, TileBuilder& builder);

    // internal
    bool match(const Feature& feature, const SceneLayer& layer, StyleContext& ctx);

    // internal
    void mergeRules(const SceneLayer& layer);

    auto& matchedRules() { return m_matchedRules; }

private:
    // Reusable containers 'matchedRules' and 'queuedLayers'
    std::vector<DrawRule> m_matchedRules;
    std::vector<const SceneLayer*> m_queuedLayers;

    // Container for dynamically-evaluated parameters
    StyleParam m_evaluated[StyleParamKeySize];

};

}
