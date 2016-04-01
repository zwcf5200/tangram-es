#include "light.h"

#include "glm/gtx/string_cast.hpp"
#include "platform.h"
#include "gl/shaderProgram.h"

#include <sstream>

namespace Tangram {

std::string Light::s_mainLightingBlock;

Light::Light(const std::string& name, bool dynamic):
    m_name(name),
    m_ambient(0.0f),
    m_diffuse(1.0f),
    m_specular(0.0f),
    m_type(LightType::ambient),
    m_origin(LightOrigin::camera),
    m_dynamic(dynamic) {
}

Light::~Light() {

}

void Light::setInstanceName(const std::string &name) {
    m_name = name;
}

void Light::setAmbientColor(const glm::vec4 ambient) {
    m_ambient = ambient;
}

void Light::setDiffuseColor(const glm::vec4 diffuse) {
    m_diffuse = diffuse;
}

void Light::setSpecularColor(const glm::vec4 specular) {
    m_specular = specular;
}

void Light::setOrigin( LightOrigin origin ) {
    m_dynamic = true;
    m_origin = origin;
}

void Light::injectSourceBlocks(ShaderProgram& shader) {

    // Inject all needed #defines for this light instance
    shader.addSourceBlock("defines", getInstanceDefinesBlock(), false);

    if (m_dynamic) {
        // If the light is dynamic, initialize it using the corresponding uniform at the start of main()
        shader.addSourceBlock("setup", getInstanceName() + " = " + getUniformName() + ";", false);
    }

    shader.addSourceBlock("__lighting", getClassBlock(), false);
    shader.addSourceBlock("__lighting", getInstanceBlock());
    shader.addSourceBlock("__lights_to_compute", getInstanceComputeBlock());
}

void Light::setupProgram(const View& view, LightUniforms& uniforms) {
    uniforms.shader.setUniformf(uniforms.ambient, m_ambient);
    uniforms.shader.setUniformf(uniforms.diffuse, m_diffuse);
    uniforms.shader.setUniformf(uniforms.specular, m_specular);
}

void Light::assembleLights(std::map<std::string, std::vector<std::string>>& sourceBlocks) {

    // Create strings to contain the assembled lighting source code
    std::stringstream lighting;

    // Concatenate all strings at the "__lighting" keys
    // (struct definitions and function definitions)
    for (const auto& string : sourceBlocks["__lighting"]) {
        lighting << '\n';
        lighting << string;
    }

    // After lights definitions are all added, add the main lighting functions
    if (s_mainLightingBlock.empty()) {
        s_mainLightingBlock = stringFromFile("shaders/lights.glsl", PathType::internal);
    }
    std::string lightingBlock = s_mainLightingBlock;

    // The main lighting functions each contain a tag where all light instances should be computed;
    // Insert all of our "lights_to_compute" at this tag

    std::string tag = "#pragma tangram: lights_to_compute";
    std::stringstream lights;
    for (const auto& string : sourceBlocks["__lights_to_compute"]) {
        lights << '\n';
        lights << string;
    }

    size_t pos = lightingBlock.find(tag) + tag.length();
    lightingBlock.insert(pos, lights.str());

    // Place our assembled lighting source code back into the map of "source blocks";
    // The assembled strings will then be injected into a shader at the "vertex_lighting"
    // and "fragment_lighting" tags
    sourceBlocks["lighting"] = { lighting.str() + lightingBlock  };
}

LightType Light::getType() {
    return m_type;
}

std::string Light::getUniformName() {
	return "u_" + m_name;
}

std::string Light::getInstanceName() {
    return m_name;
}

std::string Light::getInstanceBlock() {
    std::string block = "";
    const std::string& typeName = getTypeName();
    if (m_dynamic) {
        //  If is dynamic, define the uniform and copy it to the global instance of the light struct
        block += "uniform " + typeName + " " + getUniformName() + ";\n";
        block += typeName + " " + getInstanceName() + ";\n";
    } else {
        //  If is not dynamic define the global instance of the light struct and fill the variables
        block += typeName + " " + getInstanceName() + getInstanceAssignBlock() +";\n";
    }
    return block;
}

std::string Light::getInstanceAssignBlock() {
    std::string block = "";
    const std::string& typeName = getTypeName();
    if (!m_dynamic) {
        block += " = " + typeName + "(" + glm::to_string(m_ambient);
        block += ", " + glm::to_string(m_diffuse);
        block += ", " + glm::to_string(m_specular);
    }
    return block;
}

std::string Light::getInstanceComputeBlock() {
    return  "calculateLight(" + getInstanceName() + ", eyeToPoint, normal);\n";
}

}
