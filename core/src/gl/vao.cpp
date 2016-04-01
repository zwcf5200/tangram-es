#include "vao.h"
#include "renderState.h"
#include "shaderProgram.h"
#include "vertexLayout.h"
#include <unordered_map>

namespace Tangram {

Vao::Vao() {
    m_glVAOs = nullptr;
    m_glnVAOs = 0;
}

Vao::~Vao() {
    if (m_glVAOs) {
        glDeleteVertexArrays(m_glnVAOs, m_glVAOs);
        delete[] m_glVAOs;
    }
}

void Vao::init(ShaderProgram& program, const std::vector<std::pair<uint32_t, uint32_t>>& vertexOffsets,
               VertexLayout& layout, GLuint vertexBuffer, GLuint indexBuffer) {

    m_glnVAOs = vertexOffsets.size();
    m_glVAOs = new GLuint[m_glnVAOs];

    glGenVertexArrays(m_glnVAOs, m_glVAOs);

    fastmap<std::string, GLuint> locations;

    // FIXME (use a bindAttrib instead of getLocation) to make those locations shader independent
    for (auto& attrib : layout.getAttribs()) {
        GLint location = program.getAttribLocation(attrib.name);
        locations[attrib.name] = location;
    }

    int vertexOffset = 0;
    for (size_t i = 0; i < vertexOffsets.size(); ++i) {
        auto vertexIndexOffset = vertexOffsets[i];
        int nVerts = vertexIndexOffset.second;
        glBindVertexArray(m_glVAOs[i]);

        RenderState::vertexBuffer.init(vertexBuffer, true);

        if (indexBuffer != 0) {
            RenderState::indexBuffer.init(indexBuffer, true);
        }

        // Enable vertex layout on the specified locations
        layout.enable(locations, vertexOffset * layout.getStride());

        vertexOffset += nVerts;
    }

}

void Vao::bind(unsigned int index) {
    if (index < m_glnVAOs) {
        glBindVertexArray(m_glVAOs[index]);
    }
}

void Vao::unbind() {
    glBindVertexArray(0);
}

}

