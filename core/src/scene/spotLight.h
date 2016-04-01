#pragma once

#include "pointLight.h"
#include "glm/vec3.hpp"

namespace Tangram {

class SpotLight : public PointLight {
public:

    SpotLight(const std::string& name, bool dynamic = false);
    virtual ~SpotLight();

    /*  Set the direction of the light */
    virtual void setDirection(const glm::vec3& dir);

    /*  Set the properties of the cutoff light cone */
    virtual void setCutoffAngle(float cutoffConeDegrees);

    virtual void setCutoffExponent(float exponent);

    virtual void setupProgram(const View& view, LightUniforms& uniforms) override;

    struct Uniforms : public PointLight::Uniforms {

        Uniforms(ShaderProgram& shader, const std::string& name)
            : PointLight::Uniforms(shader, name),
            direction(name+".direction"),
            spotCosCutoff(name+".spotCosCutoff"),
            spotExponent(name+".spotExponent") {}

        UniformLocation direction;
        UniformLocation spotCosCutoff;
        UniformLocation spotExponent;
    };

    std::unique_ptr<LightUniforms> injectOnProgram(ShaderProgram& shader) override;

protected:
    /*  GLSL block code with structs and need functions for this light type */
    virtual std::string getClassBlock() override;
    virtual std::string getInstanceAssignBlock() override;
    virtual const std::string& getTypeName() override;

    static std::string s_classBlock;

    glm::vec3 m_direction;

    float m_spotExponent;
    float m_spotCutoff;
    float m_spotCosCutoff;

private:

    static std::string s_typeName;

};

}
