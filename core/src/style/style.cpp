#include "style.h"

#include "material.h"
#include "gl/renderState.h"
#include "gl/shaderProgram.h"
#include "gl/mesh.h"
#include "scene/light.h"
#include "scene/styleParam.h"
#include "scene/drawRule.h"
#include "scene/scene.h"
#include "scene/spriteAtlas.h"
#include "tile/tile.h"
#include "view/view.h"
#include "tangram.h"

namespace Tangram {

Style::Style(std::string name, Blending blendMode, GLenum drawMode) :
    m_name(name),
    m_shaderProgram(std::make_unique<ShaderProgram>()),
    m_blend(blendMode),
    m_drawMode(drawMode) {
    m_material.material = std::make_shared<Material>();
}

Style::~Style() {}

Style::LightHandle::LightHandle(Light* light, std::unique_ptr<LightUniforms> uniforms)
    : light(light), uniforms(std::move(uniforms)){}

const std::vector<std::string>& Style::builtInStyleNames() {
    static std::vector<std::string> builtInStyleNames{ "points", "lines", "polygons", "text", "debug", "debugtext" };
    return builtInStyleNames;
}

void Style::build(const std::vector<std::unique_ptr<Light>>& lights) {

    constructVertexLayout();
    constructShaderProgram();

    switch (m_lightingType) {
        case LightingType::vertex:
            m_shaderProgram->addSourceBlock("defines", "#define TANGRAM_LIGHTING_VERTEX\n", false);
            break;
        case LightingType::fragment:
            m_shaderProgram->addSourceBlock("defines", "#define TANGRAM_LIGHTING_FRAGMENT\n", false);
            break;
        default:
            break;
    }

    if (m_material.material) {
        m_material.uniforms = m_material.material->injectOnProgram(*m_shaderProgram);
    }

    if (m_lightingType != LightingType::none) {
        for (auto& light : lights) {
            auto uniforms = light->injectOnProgram(*m_shaderProgram);
            if (uniforms) {
                m_lights.emplace_back(light.get(), std::move(uniforms));
            }
        }
    }
}

void Style::setMaterial(const std::shared_ptr<Material>& material) {
    m_material.material = material;
    m_material.uniforms.reset();
}

void Style::setLightingType(LightingType type) {
    m_lightingType = type;
}

void Style::setupShaderUniforms(Scene& scene) {
    for (auto& uniformPair : m_styleUniforms) {
        const auto& name = uniformPair.first;
        auto& value = uniformPair.second;

        if (value.is<std::string>()) {
            std::shared_ptr<Texture> texture = nullptr;

            if (!scene.texture(value.get<std::string>(), texture) || !texture) {
                // TODO: LOG, would be nice to do have a notify-once-log not to overwhelm logging
                continue;
            }

            texture->update(RenderState::nextAvailableTextureUnit());
            texture->bind(RenderState::currentTextureUnit());

            m_shaderProgram->setUniformi(name, RenderState::currentTextureUnit());
        } else {

            if (value.is<bool>()) {
                m_shaderProgram->setUniformi(name, value.get<bool>());
            } else if(value.is<float>()) {
                m_shaderProgram->setUniformf(name, value.get<float>());
            } else if(value.is<glm::vec2>()) {
                m_shaderProgram->setUniformf(name, value.get<glm::vec2>());
            } else if(value.is<glm::vec3>()) {
                m_shaderProgram->setUniformf(name, value.get<glm::vec3>());
            } else if(value.is<glm::vec4>()) {
                m_shaderProgram->setUniformf(name, value.get<glm::vec4>());
            } else if (value.is<UniformArray>()) {
                m_shaderProgram->setUniformf(name, value.get<UniformArray>());
            } else if (value.is<UniformTextureArray>()) {
                UniformTextureArray& textureUniformArray = value.get<UniformTextureArray>();
                textureUniformArray.slots.clear();

                for (const auto& textureName : textureUniformArray.names) {
                    std::shared_ptr<Texture> texture = nullptr;

                    if (!scene.texture(textureName, texture) || !texture) {
                        // TODO: LOG, would be nice to do have a notify-once-log not to overwhelm logging
                        continue;
                    }

                    texture->update(RenderState::nextAvailableTextureUnit());
                    texture->bind(RenderState::currentTextureUnit());

                    textureUniformArray.slots.push_back(RenderState::currentTextureUnit());
                }

                m_shaderProgram->setUniformi(name, textureUniformArray);
            } else {
                // TODO: Throw away uniform on loading!
                // none_type
            }
        }
    }
}

void Style::onBeginDrawFrame(const View& view, Scene& scene) {

    // Reset the currently used texture unit to 0
    RenderState::resetTextureUnit();

    // Set time uniforms style's shader programs
    m_shaderProgram->setUniformf(m_uTime, Tangram::frameTime());

    m_shaderProgram->setUniformf(m_uDevicePixelRatio, m_pixelScale);

    if (m_material.uniforms) {
        m_material.material->setupProgram(*m_material.uniforms);
    }

    // Set up lights
    for (const auto& light : m_lights) {
        light.light->setupProgram(view, *light.uniforms);
    }

    // Set Map Position
    m_shaderProgram->setUniformf(m_uResolution, view.getWidth(), view.getHeight());

    const auto& mapPos = view.getPosition();
    m_shaderProgram->setUniformf(m_uMapPosition, mapPos.x, mapPos.y, view.getZoom());
    m_shaderProgram->setUniformMatrix3f(m_uNormalMatrix, view.getNormalMatrix());
    m_shaderProgram->setUniformMatrix3f(m_uInverseNormalMatrix, view.getInverseNormalMatrix());
    m_shaderProgram->setUniformf(m_uMetersPerPixel, 1.0 / view.pixelsPerMeter());
    m_shaderProgram->setUniformMatrix4f(m_uView, view.getViewMatrix());
    m_shaderProgram->setUniformMatrix4f(m_uProj, view.getProjectionMatrix());

    setupShaderUniforms(scene);

    // Configure render state
    switch (m_blend) {
        case Blending::none:
            RenderState::blending(GL_FALSE);
            RenderState::blendingFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            RenderState::depthTest(GL_TRUE);
            RenderState::depthWrite(GL_TRUE);
            break;
        case Blending::add:
            RenderState::blending(GL_TRUE);
            RenderState::blendingFunc(GL_ONE, GL_ONE);
            RenderState::depthTest(GL_FALSE);
            RenderState::depthWrite(GL_TRUE);
            break;
        case Blending::multiply:
            RenderState::blending(GL_TRUE);
            RenderState::blendingFunc(GL_ZERO, GL_SRC_COLOR);
            RenderState::depthTest(GL_FALSE);
            RenderState::depthWrite(GL_TRUE);
            break;
        case Blending::overlay:
            RenderState::blending(GL_TRUE);
            RenderState::blendingFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            RenderState::depthTest(GL_FALSE);
            RenderState::depthWrite(GL_FALSE);
            break;
        case Blending::inlay:
            // TODO: inlay does not behave correctly for labels because they don't have a z position
            RenderState::blending(GL_TRUE);
            RenderState::blendingFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            RenderState::depthTest(GL_TRUE);
            RenderState::depthWrite(GL_FALSE);
            break;
        default:
            break;
    }
}

void Style::draw(const Tile& tile) {

    auto& styleMesh = tile.getMesh(*this);

    if (styleMesh) {
        m_shaderProgram->setUniformMatrix4f(m_uModel, tile.getModelMatrix());
        m_shaderProgram->setUniformf(m_uProxyDepth, tile.isProxy() ? 1.f : 0.f);
        m_shaderProgram->setUniformf(m_uTileOrigin,
                                     tile.getOrigin().x,
                                     tile.getOrigin().y,
                                     tile.getID().s,
                                     tile.getID().z);

        styleMesh->draw(*m_shaderProgram);
    }
}

bool StyleBuilder::checkRule(const DrawRule& rule) const {

    uint32_t checkColor;
    uint32_t checkOrder;

    if (!rule.get(StyleParamKey::color, checkColor)) {
        if (!m_hasColorShaderBlock) {
            return false;
        }
    }

    if (!rule.get(StyleParamKey::order, checkOrder)) {
        return false;
    }

    return true;
}

void StyleBuilder::addFeature(const Feature& feat, const DrawRule& rule) {

    if (!checkRule(rule)) { return; }

    switch (feat.geometryType) {
        case GeometryType::points:
            for (auto& point : feat.points) {
                addPoint(point, feat.props, rule);
            }
            break;
        case GeometryType::lines:
            for (auto& line : feat.lines) {
                addLine(line, feat.props, rule);
            }
            break;
        case GeometryType::polygons:
            for (auto& polygon : feat.polygons) {
                addPolygon(polygon, feat.props, rule);
            }
            break;
        default:
            break;
    }

}

StyleBuilder::StyleBuilder(const Style& style) {
    const auto& blocks = style.getShaderProgram()->getSourceBlocks();
    if (blocks.find("color") != blocks.end() ||
        blocks.find("filter") != blocks.end()) {
        m_hasColorShaderBlock = true;
    }
}

void StyleBuilder::addPoint(const Point& point, const Properties& props, const DrawRule& rule) {
    // No-op by default
}

void StyleBuilder::addLine(const Line& line, const Properties& props, const DrawRule& rule) {
    // No-op by default
}

void StyleBuilder::addPolygon(const Polygon& polygon, const Properties& props, const DrawRule& rule) {
    // No-op by default
}

}
