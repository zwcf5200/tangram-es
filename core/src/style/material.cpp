#include "material.h"

#include "platform.h"
#include "gl/shaderProgram.h"
#include "gl/texture.h"
#include "gl/renderState.h"

namespace Tangram {

Material::Material() {
}

void Material::setEmission(glm::vec4 emission){
    m_emission = emission;
    m_emission_texture.tex.reset();
    setEmissionEnabled(true);
}

void Material::setEmission(MaterialTexture emissionTexture){
    m_emission_texture = emissionTexture;
    m_emission = glm::vec4(m_emission_texture.amount, 1.f);
    setEmissionEnabled((bool)m_emission_texture.tex);
}

void Material::setAmbient(glm::vec4 ambient){
    m_ambient = ambient;
    m_ambient_texture.tex.reset();
    setAmbientEnabled(true);
}

void Material::setAmbient(MaterialTexture ambientTexture){
    m_ambient_texture = ambientTexture;
    m_ambient = glm::vec4(m_ambient_texture.amount, 1.f);
    setAmbientEnabled((bool)m_ambient_texture.tex);
}

void Material::setDiffuse(glm::vec4 diffuse){
    m_diffuse = diffuse;
    m_diffuse_texture.tex.reset();
    setDiffuseEnabled(true);
}

void Material::setDiffuse(MaterialTexture diffuseTexture){
    m_diffuse_texture = diffuseTexture;
    m_diffuse = glm::vec4(m_diffuse_texture.amount, 1.f);
    setDiffuseEnabled((bool)m_diffuse_texture.tex);
}

void Material::setSpecular(glm::vec4 specular){
    m_specular = specular;
    m_specular_texture.tex.reset();
    setSpecularEnabled(true);
}

void Material::setSpecular(MaterialTexture specularTexture){
    m_specular_texture = specularTexture;
    m_specular = glm::vec4(m_specular_texture.amount, 1.f);
    setSpecularEnabled((bool)m_specular_texture.tex);
}

void Material::setShininess(float shiny) {
    m_shininess = shiny;
    setSpecularEnabled(true);
}

void Material::setEmissionEnabled(bool enable) { m_bEmission = enable; }
void Material::setAmbientEnabled(bool enable) { m_bAmbient = enable; }
void Material::setDiffuseEnabled(bool enable) { m_bDiffuse = enable; }
void Material::setSpecularEnabled(bool enable) { m_bSpecular = enable; }

void Material::setNormal(MaterialTexture normalTexture){
    m_normal_texture = normalTexture;
    if (m_normal_texture.mapping == MappingType::spheremap){
        m_normal_texture.mapping = MappingType::planar;
    }
}

std::string mappingTypeToString(MappingType type) {
    switch(type) {
        case MappingType::uv:        return "UV";
        case MappingType::planar:    return "PLANAR";
        case MappingType::triplanar: return "TRIPLANAR";
        case MappingType::spheremap: return "SPHEREMAP";
        default:                     return "";
    }
}

std::string Material::getDefinesBlock(){
    std::string defines = "";

    bool mappings[4] = { false };

    if (m_bEmission) {
        defines += "#define TANGRAM_MATERIAL_EMISSION\n";
        if (m_emission_texture.tex) {
            defines += "#define TANGRAM_MATERIAL_EMISSION_TEXTURE\n";
            defines += "#define TANGRAM_MATERIAL_EMISSION_TEXTURE_" +
                mappingTypeToString(m_emission_texture.mapping) + "\n";
            mappings[(int)m_emission_texture.mapping] = true;
        }
    }

    if (m_bAmbient) {
        defines += "#define TANGRAM_MATERIAL_AMBIENT\n";
        if (m_ambient_texture.tex) {
            defines += "#define TANGRAM_MATERIAL_AMBIENT_TEXTURE\n";
            defines += "#define TANGRAM_MATERIAL_AMBIENT_TEXTURE_" +
                mappingTypeToString(m_ambient_texture.mapping) + "\n";
            mappings[(int)m_ambient_texture.mapping] = true;
        }
    }

    if (m_bDiffuse) {
        defines += "#define TANGRAM_MATERIAL_DIFFUSE\n";
        if (m_diffuse_texture.tex) {
            defines += "#define TANGRAM_MATERIAL_DIFFUSE_TEXTURE\n";
            defines += "#define TANGRAM_MATERIAL_DIFFUSE_TEXTURE_" +
                mappingTypeToString(m_diffuse_texture.mapping) + "\n";
            mappings[(int)m_diffuse_texture.mapping] = true;
        }
    }

    if (m_bSpecular) {
        defines += "#define TANGRAM_MATERIAL_SPECULAR\n";
        if (m_specular_texture.tex) {
            defines += "#define TANGRAM_MATERIAL_SPECULAR_TEXTURE\n";
            defines += "#define TANGRAM_MATERIAL_SPECULAR_TEXTURE_" +
                mappingTypeToString(m_specular_texture.mapping) + "\n";
            mappings[(int)m_specular_texture.mapping] = true;
        }
    }

    if (m_normal_texture.tex){
        defines += "#define TANGRAM_MATERIAL_NORMAL_TEXTURE\n";
        defines += "#define TANGRAM_MATERIAL_NORMAL_TEXTURE_" +
            mappingTypeToString(m_normal_texture.mapping) + "\n";
        mappings[(int)m_specular_texture.mapping] = true;
    }

    for (int i = 0; i < 4; i++) {
        if (mappings[i]) {
            defines += "#define TANGRAM_MATERIAL_TEXTURE_" + mappingTypeToString((MappingType)i) + "\n";
        }
    }

    return defines;
}

std::string Material::getClassBlock() {
    return stringFromFile("shaders/material.glsl", PathType::internal) + "\n";
}

std::unique_ptr<MaterialUniforms> Material::injectOnProgram(ShaderProgram& shader ) {
    shader.addSourceBlock("defines", getDefinesBlock(), false);
    shader.addSourceBlock("material", getClassBlock(), false);
    shader.addSourceBlock("setup", "material = u_material;", false);

    if (m_bEmission || m_bAmbient || m_bDiffuse || m_bSpecular || m_normal_texture.tex) {
        return std::make_unique<MaterialUniforms>(shader);
    }
    return nullptr;
}

void Material::setupProgram(MaterialUniforms& uniforms) {

    auto& u = uniforms;

    if (m_bEmission) {
        u.shader.setUniformf(u.emission, m_emission);

        if (m_emission_texture.tex) {
            m_emission_texture.tex->update(RenderState::nextAvailableTextureUnit());
            m_emission_texture.tex->bind(RenderState::currentTextureUnit());
            u.shader.setUniformi(u.emissionTexture, RenderState::currentTextureUnit());
            u.shader.setUniformf(u.emissionScale, m_emission_texture.scale);
        }
    }

    if (m_bAmbient) {
        u.shader.setUniformf(u.ambient, m_ambient);

        if (m_ambient_texture.tex) {
            m_ambient_texture.tex->update(RenderState::nextAvailableTextureUnit());
            m_ambient_texture.tex->bind(RenderState::currentTextureUnit());
            u.shader.setUniformi(u.ambientTexture, RenderState::currentTextureUnit());
            u.shader.setUniformf(u.ambientScale, m_ambient_texture.scale);
        }
    }

    if (m_bDiffuse) {
        u.shader.setUniformf(u.diffuse, m_diffuse);

        if (m_diffuse_texture.tex) {
            m_diffuse_texture.tex->update(RenderState::nextAvailableTextureUnit());
            m_diffuse_texture.tex->bind(RenderState::currentTextureUnit());
            u.shader.setUniformi(u.diffuseTexture, RenderState::currentTextureUnit());
            u.shader.setUniformf(u.diffuseScale, m_diffuse_texture.scale);
        }
    }

    if (m_bSpecular) {
        u.shader.setUniformf(u.specular, m_specular);
        u.shader.setUniformf(u.shininess, m_shininess);

        if (m_specular_texture.tex) {
            m_specular_texture.tex->update(RenderState::nextAvailableTextureUnit());
            m_specular_texture.tex->bind(RenderState::currentTextureUnit());
            u.shader.setUniformi(u.specularTexture, RenderState::currentTextureUnit());
            u.shader.setUniformf(u.specularScale, m_specular_texture.scale);
        }
    }

    if (m_normal_texture.tex) {
        m_normal_texture.tex->update(RenderState::nextAvailableTextureUnit());
        m_normal_texture.tex->bind(RenderState::currentTextureUnit());
        u.shader.setUniformi(u.normalTexture, RenderState::currentTextureUnit());
        u.shader.setUniformf(u.normalScale, m_normal_texture.scale);
        u.shader.setUniformf(u.normalAmount, m_normal_texture.amount);
    }
}

}
