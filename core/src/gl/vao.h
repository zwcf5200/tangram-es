#pragma once

#include "gl.h"
#include <vector>
#include <string>

namespace Tangram {

class ShaderProgram;
class VertexLayout;

class Vao {

public:
    Vao();
    ~Vao();

    void init(ShaderProgram& program, const std::vector<std::pair<uint32_t, uint32_t>>& vertexOffsets,
              VertexLayout& layout, GLuint vertexBuffer, GLuint indexBuffer);

    void bind(unsigned int index);
    void unbind();

private:
    GLuint* m_glVAOs;
    GLuint m_glnVAOs;

};

}

