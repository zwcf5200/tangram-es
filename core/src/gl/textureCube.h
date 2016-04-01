#pragma once

#include "texture.h"
#include "gl.h"

#include <vector>
#include <string>

namespace Tangram {

class TextureCube : public Texture {
    
public:
    TextureCube(std::string file, TextureOptions options =
                {GL_RGBA, GL_RGBA, {GL_LINEAR, GL_LINEAR}, {GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE}});
    
    void update(GLuint textureUnit) override;
    
    void resize(const unsigned int width, const unsigned int height) = delete;
    void setData(const GLuint* data, unsigned int dataSize) = delete;
    void setSubData(const GLuint* subData, unsigned int xoff, unsigned int yoff, unsigned int width, unsigned int height) = delete;
    
private:
    
    struct Face {
        GLenum m_face;
        std::vector<unsigned int> m_data;
        int m_offset;
    };
    
    const GLenum CubeMapFace[6] {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    
    std::vector<Face> m_faces;
    
    void load(const std::string& file);

};

}
