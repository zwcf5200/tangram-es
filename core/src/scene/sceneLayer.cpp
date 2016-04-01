#include "sceneLayer.h"

#include <algorithm>

namespace Tangram {

SceneLayer::SceneLayer(std::string name, Filter filter,
                       std::vector<DrawRuleData> rules,
                       std::vector<SceneLayer> sublayers,
                       bool visible) :
    m_filter(std::move(filter)),
    m_name(name),
    m_rules(rules),
    m_sublayers(std::move(sublayers)),
    m_visible(visible) {

    setDepth(1);

}

void SceneLayer::setDepth(size_t d) {

    m_depth = d;

    for (auto& layer : m_sublayers) {
        layer.setDepth(m_depth + 1);
    }

}

}
