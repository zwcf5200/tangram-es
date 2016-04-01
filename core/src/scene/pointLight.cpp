#include "pointLight.h"

#include "glm/gtx/string_cast.hpp"
#include "platform.h"
#include "gl/shaderProgram.h"
#include "view/view.h"

namespace Tangram {

std::string PointLight::s_classBlock;
std::string PointLight::s_typeName = "PointLight";

PointLight::PointLight(const std::string& name, bool dynamic) :
    Light(name, dynamic),
    m_position(0.0),
    m_attenuation(0.0),
    m_innerRadius(0.0),
    m_outerRadius(0.0) {

    m_type = LightType::point;

}

PointLight::~PointLight() {}

void PointLight::setPosition(const glm::vec3 &pos) {
    m_position.x = pos.x;
    m_position.y = pos.y;
    m_position.z = pos.z;
    m_position.w = 0.0;
}

void PointLight::setAttenuation(float att){
    m_attenuation = att;
}

void PointLight::setRadius(float outer){
    m_innerRadius = 0.0;
    m_outerRadius = outer;
}

void PointLight::setRadius(float inner, float outer){
    m_innerRadius = inner;
    m_outerRadius = outer;
}

std::unique_ptr<LightUniforms> PointLight::injectOnProgram(ShaderProgram& shader) {
    injectSourceBlocks(shader);

    if (!m_dynamic) { return nullptr; }

    return std::make_unique<Uniforms>(shader, getUniformName());
}

void PointLight::setupProgram(const View& view, LightUniforms& uniforms) {
    Light::setupProgram(view, uniforms);

    glm::vec4 position = m_position;

    if (m_origin == LightOrigin::world) {
        // For world origin, format is: [longitude, latitude, meters (default) or pixels w/px units]

        // Move light's world position into camera space
        glm::dvec2 camSpace = view.getMapProjection().LonLatToMeters(glm::dvec2(m_position.x, m_position.y));
        position.x = camSpace.x - (view.getPosition().x + view.getEye().x);
        position.y = camSpace.y - (view.getPosition().y + view.getEye().y);
        position.z = position.z - view.getEye().z;

    } else if (m_origin == LightOrigin::ground) {
        // Move light position relative to the eye position in world space
        position -= glm::vec4(view.getEye(), 0.0);
    }

    if (m_origin == LightOrigin::world || m_origin == LightOrigin::ground) {
        // Light position is a vector from the camera to the light in world space;
        // we can transform this vector into camera space the same way we would with normals
        position = view.getViewMatrix() * position;
    }

    auto& u = static_cast<Uniforms&>(uniforms);

    u.shader.setUniformf(u.position, position);

    if (m_attenuation != 0.0) {
        u.shader.setUniformf(u.attenuation, m_attenuation);
    }

    if (m_innerRadius != 0.0) {
        u.shader.setUniformf(u.innerRadius, m_innerRadius);
    }

    if (m_outerRadius != 0.0) {
        u.shader.setUniformf(u.outerRadius, m_outerRadius);
    }
}

std::string PointLight::getClassBlock() {
    if (s_classBlock.empty()) {
        s_classBlock = stringFromFile("shaders/pointLight.glsl", PathType::internal)+"\n";
    }
    return s_classBlock;
}

std::string PointLight::getInstanceDefinesBlock() {
    std::string defines = "";

    if (m_attenuation!=0.0) {
        defines += "#define TANGRAM_POINTLIGHT_ATTENUATION_EXPONENT\n";
    }

    if (m_innerRadius!=0.0) {
        defines += "#define TANGRAM_POINTLIGHT_ATTENUATION_INNER_RADIUS\n";
    }

    if (m_outerRadius!=0.0) {
        defines += "#define TANGRAM_POINTLIGHT_ATTENUATION_OUTER_RADIUS\n";
    }
    return defines;
}

std::string PointLight::getInstanceAssignBlock() {
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
        block += ")";
    }
    return block;
}

const std::string& PointLight::getTypeName() {

    return s_typeName;

}

}
