#include "pointLight.h"
#include "util/stringsOp.h"

std::string PointLight::s_classBlock;

PointLight::PointLight(const std::string& _name, bool _dynamic):Light(_name,_dynamic),m_position(0.0),m_constantAttenuation(0.0),m_linearAttenuation(0.0),m_quadraticAttenuation(0.0) {
    m_typeName = "PointLight";
    m_type = LightType::POINT;
    m_injType = FRAGMENT;
}

PointLight::~PointLight() {

}

void PointLight::setPosition(const glm::vec3 &_pos) {
    m_position.x = _pos.x;
    m_position.y = _pos.y;
    m_position.z = _pos.z;
    m_position.w = 0.0;
}

void PointLight::setConstantAttenuation(float _constantAtt) {
    m_constantAttenuation = _constantAtt;
}

void PointLight::setLinearAttenuation(float _linearAtt) {
    m_linearAttenuation = _linearAtt;
}

void PointLight::setQuadreaticAttenuation(float _quadraticAtt) {
    m_quadraticAttenuation = _quadraticAtt;
}

void PointLight::setAttenuation(float _constant, float _linear, float _quadratic) {
    m_constantAttenuation = _constant;
    m_linearAttenuation = _linear;
    m_quadraticAttenuation = _quadratic;
}

void PointLight::setupProgram(std::shared_ptr<ShaderProgram> _shader) {
    if (m_dynamic) {
        Light::setupProgram(_shader);
        _shader->setUniformf(getUniformName()+".position", glm::vec4(m_position));

        if (m_constantAttenuation!=0.0) {
            _shader->setUniformf(getUniformName()+".constantAttenuation", m_constantAttenuation);
        }

        if (m_linearAttenuation!=0.0) {
            _shader->setUniformf(getUniformName()+".linearAttenuation", m_linearAttenuation);
        }

        if (m_quadraticAttenuation!=0.0) {
            _shader->setUniformf(getUniformName()+".quadraticAttenuation", m_quadraticAttenuation);
        }
    }
}

std::string PointLight::getClassBlock() {
    if (s_classBlock.empty()) {
        s_classBlock = stringFromResource("point_light.glsl")+"\n";
    }
    return s_classBlock;
}

std::string PointLight::getInstanceDefinesBlock() {
    std::string defines = "\n";

    //  TODO:
    //       - the shader program class have to be inteligent enought 
    //          to do this for us.
    //
    if (m_constantAttenuation!=0.0) {
        defines += "#ifndef TANGRAM_POINTLIGHT_CONSTANT_ATTENUATION\n";
        defines += "#define TANGRAM_POINTLIGHT_CONSTANT_ATTENUATION\n";
        defines += "#endif\n";
    }

    if (m_linearAttenuation!=0.0) {
        defines += "#ifndef TANGRAM_POINTLIGHT_LINEAR_ATTENUATION\n";
        defines += "#define TANGRAM_POINTLIGHT_LINEAR_ATTENUATION\n";
        defines += "#endif\n";
    }

    if (m_quadraticAttenuation!=0.0) {
        defines += "#ifndef TANGRAM_POINTLIGHT_QUADRATIC_ATTENUATION\n";
        defines += "#define TANGRAM_POINTLIGHT_QUADRATIC_ATTENUATION\n";
        defines += "#endif\n";
    }
    return defines;
}

std::string PointLight::getInstanceAssignBlock() {
    std::string block = Light::getInstanceAssignBlock();
    if (!m_dynamic) {
        block += ", " + getString(m_position);
        if (m_constantAttenuation!=0.0) {
            block += ", " + getString(m_constantAttenuation);
        }
        if (m_linearAttenuation!=0.0) {
            block += ", " + getString(m_linearAttenuation);
        }
        if (m_quadraticAttenuation!=0.0) {
            block += ", " + getString(m_quadraticAttenuation);
        }
        block += ")";
    }
    return block;
}
