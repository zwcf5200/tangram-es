#pragma once

#include "light.h"
#include "glm/vec3.hpp"

namespace Tangram {


class DirectionalLight : public Light {
public:

    DirectionalLight(const std::string& name, bool dynamic = false);
    virtual ~DirectionalLight();

    /*	Set the direction of the light */
    virtual void setDirection(const glm::vec3& dir);

    virtual void setupProgram(const View& view, LightUniforms& uniforms) override;

    struct Uniforms : public LightUniforms {

        Uniforms(ShaderProgram& shader, const std::string& name)
            : LightUniforms(shader, name),
              direction(name+".direction") {}

        UniformLocation direction;
    };


    std::unique_ptr<LightUniforms> injectOnProgram(ShaderProgram& shader) override;

protected:

    /*  GLSL block code with structs and need functions for this light type */
    virtual std::string getClassBlock() override;
    virtual std::string getInstanceDefinesBlock() override;
    virtual std::string getInstanceAssignBlock() override;
    virtual const std::string& getTypeName() override;

    static std::string s_classBlock;

    glm::vec3 m_direction;

private:

    static std::string s_typeName;

};

}
