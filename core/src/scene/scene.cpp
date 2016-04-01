#include "scene.h"

#include "gl/shaderProgram.h"
#include "platform.h"
#include "style/material.h"
#include "style/style.h"
#include "scene/dataLayer.h"
#include "scene/light.h"
#include "scene/spriteAtlas.h"
#include "scene/stops.h"
#include "util/mapProjection.h"
#include "view/view.h"

#include <atomic>
#include <algorithm>

namespace Tangram {

static std::atomic<int32_t> s_serial;

Scene::Scene() : id(s_serial++) {
    m_view = std::make_shared<View>();
    // For now we only have one projection..
    // TODO how to share projection with view?
    m_mapProjection.reset(new MercatorProjection());
}

Scene::~Scene() {}

const Style* Scene::findStyle(const std::string& name) const {

    for (auto& style : m_styles) {
        if (style->getName() == name) { return style.get(); }
    }
    return nullptr;

}

int Scene::addIdForName(const std::string& name) {
    int id = getIdForName(name);

    if (id < 0) {
        m_names.push_back(name);
        return m_names.size() - 1;
    }
    return id;
}

int Scene::getIdForName(const std::string& name) const {
    auto it = std::find(m_names.begin(), m_names.end(), name);
    if (it == m_names.end()) {
        return -1;
    }
    return it - m_names.begin();
}

const Light* Scene::findLight(const std::string &name) const {
    for (auto& light : m_lights) {
        if (light->getInstanceName() == name) { return light.get(); }
    }
    return nullptr;
}

bool Scene::texture(const std::string& textureName, std::shared_ptr<Texture>& texture) const {
    auto texIt = m_textures.find(textureName);

    if (texIt == m_textures.end()) {
        return false;
    }

    texture = texIt->second;

    return true;
}

}
