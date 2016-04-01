#pragma once

#include "light.h"

namespace Tangram {

class PointLight : public Light {
public:

    PointLight(const std::string& name, bool dynamic = false);
    virtual ~PointLight();

    /*  Set the position relative to the camera */
    virtual void setPosition(const glm::vec3& pos);

    /*  Set the constant attenuation */
    virtual void setAttenuation(float att);

    /*  Set the constant outer radius or inner/outer radius*/
    virtual void setRadius(float outer);
    virtual void setRadius(float inner, float outer);

    virtual void setupProgram(const View& view, LightUniforms& uniforms) override;

    struct Uniforms : public LightUniforms {
        Uniforms(ShaderProgram& shader, const std::string& name)
            : LightUniforms(shader, name),
              position(name+".position"),
              attenuation(name+".attenuation"),
              innerRadius(name+".innerRadius"),
              outerRadius(name+".outerRadius") {}

        UniformLocation position;
        UniformLocation attenuation;
        UniformLocation innerRadius;
        UniformLocation outerRadius;
    };

    std::unique_ptr<LightUniforms> injectOnProgram(ShaderProgram& shader) override;

protected:

    /*  GLSL block code with structs and need functions for this light type */
    virtual std::string getClassBlock() override;
    virtual std::string getInstanceDefinesBlock() override;
    virtual std::string getInstanceAssignBlock() override;
    virtual const std::string& getTypeName() override;

    static std::string s_classBlock;

    glm::vec4 m_position;

    float m_attenuation;
    float m_innerRadius;
    float m_outerRadius;

private:

    static std::string s_typeName;

};

}
