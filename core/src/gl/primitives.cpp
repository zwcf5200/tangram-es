#include "primitives.h"

#include "debug.h"
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "gl/shaderProgram.h"
#include "gl/vertexLayout.h"
#include "gl/renderState.h"
#include "platform.h"

namespace Tangram {

namespace Primitives {

static bool s_initialized;
static std::unique_ptr<ShaderProgram> s_shader;
static std::unique_ptr<VertexLayout> s_layout;
static glm::vec2 s_resolution;
static GLuint s_boundBuffer;

static UniformLocation s_uColor{"u_color"};
static UniformLocation s_uProj{"u_proj"};

void init() {

    // lazy init
    if (!s_initialized) {
        std::string vert, frag;
        s_shader = std::unique_ptr<ShaderProgram>(new ShaderProgram());

        vert = stringFromFile("shaders/debugPrimitive.vs", PathType::internal);
        frag = stringFromFile("shaders/debugPrimitive.fs", PathType::internal);

        s_shader->setSourceStrings(frag, vert);

        s_layout = std::unique_ptr<VertexLayout>(new VertexLayout({
            {"a_position", 2, GL_FLOAT, false, 0},
        }));

        s_initialized = true;
        glLineWidth(1.5f);
    }
}

void saveState() {
    // save the current gl state
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, (GLint*) &s_boundBuffer);
    RenderState::depthTest(GL_FALSE);
    RenderState::vertexBuffer(0);
}

void popState() {
    // undo modification on the gl states
    RenderState::vertexBuffer(s_boundBuffer);
}

void drawLine(const glm::vec2& origin, const glm::vec2& destination) {

    init();

    glm::vec2 verts[2] = {
        glm::vec2(origin.x, origin.y),
        glm::vec2(destination.x, destination.y)
    };

    saveState();

    s_shader->use();

    // enable the layout for the line vertices
    s_layout->enable(*s_shader, 0, &verts);

    glDrawArrays(GL_LINES, 0, 2);
    popState();

}

void drawRect(const glm::vec2& origin, const glm::vec2& destination) {
    drawLine(origin, {destination.x, origin.y});
    drawLine({destination.x, origin.y}, destination);
    drawLine(destination, {origin.x, destination.y});
    drawLine({origin.x,destination.y}, origin);
}

void drawPoly(const glm::vec2* polygon, size_t n) {
    init();

    saveState();

    s_shader->use();

    // enable the layout for the polygon vertices
    s_layout->enable(*s_shader, 0, (void*)polygon);

    glDrawArrays(GL_LINE_LOOP, 0, n);
    popState();
}

void setColor(unsigned int color) {
    init();

    float r = (color >> 16 & 0xff) / 255.0;
    float g = (color >> 8  & 0xff) / 255.0;
    float b = (color       & 0xff) / 255.0;

    s_shader->setUniformf(s_uColor, r, g, b);
}

void setResolution(float width, float height) {
    init();

    glm::mat4 proj = glm::ortho(0.f, width, height, 0.f, -1.f, 1.f);
    s_shader->setUniformMatrix4f(s_uProj, proj);
}

}

}
