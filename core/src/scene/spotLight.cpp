#include "spotLight.h"

#include "glm/gtx/string_cast.hpp"
#include "platform.h"
#include "gl/shaderProgram.h"
#include "view/view.h"

namespace Tangram {

std::string SpotLight::s_classBlock;
std::string SpotLight::s_typeName = "SpotLight";

SpotLight::SpotLight(const std::string& name, bool dynamic) :
    PointLight(name, dynamic),
    m_direction(1.0,0.0,0.0),
    m_spotExponent(0.0),
    m_spotCutoff(0.0),
    m_spotCosCutoff(0.0) {

    m_type = LightType::spot;
}

SpotLight::~SpotLight() {}

void SpotLight::setDirection(const glm::vec3 &dir) {
    m_direction = dir;
}

void SpotLight::setCutoffAngle(float cutoffAngle) {
    m_spotCutoff = cutoffAngle;
    m_spotCosCutoff = cos(cutoffAngle * 3.14159 / 180.0);
}

void SpotLight::setCutoffExponent(float exponent) {
    m_spotExponent = exponent;
}

std::unique_ptr<LightUniforms> SpotLight::injectOnProgram(ShaderProgram& shader) {
    injectSourceBlocks(shader);

    if (!m_dynamic) { return nullptr; }

    return std::make_unique<Uniforms>(shader, getUniformName());
}

void SpotLight::setupProgram(const View& view, LightUniforms& uniforms ) {
    PointLight::setupProgram(view, uniforms);

    glm::vec3 direction = m_direction;
    if (m_origin == LightOrigin::world) {
        direction = glm::normalize(view.getNormalMatrix() * direction);
    }

    auto& u = static_cast<Uniforms&>(uniforms);
    u.shader.setUniformf(u.direction, direction);
    u.shader.setUniformf(u.spotCosCutoff, m_spotCosCutoff);
    u.shader.setUniformf(u.spotExponent, m_spotExponent);
}

std::string SpotLight::getClassBlock() {
    if (s_classBlock.empty()) {
        s_classBlock = stringFromFile("shaders/spotLight.glsl", PathType::internal)+"\n";
    }
    return s_classBlock;
}

std::string SpotLight::getInstanceAssignBlock() {
    std::string block = Light::getInstanceAssignBlock();

    if (!m_dynamic) {
        block += ", " + glm::to_string(m_position);
        if (m_attenuation!=0.0) {
            block += ", " + std::to_string(m_attenuation);
        }
        if (m_innerRadius!=0.0) {
            block += ", " + std::to_string(m_innerRadius);
        }
        if (m_outerRadius!=0.0) {
            block += ", " + std::to_string(m_outerRadius);
        }

        block += ", " + glm::to_string(m_direction);
        block += ", " + std::to_string(m_spotCosCutoff);
        block += ", " + std::to_string(m_spotExponent);

        block += ")";
    }
    return block;
}

const std::string& SpotLight::getTypeName() {

    return s_typeName;

}

}
