#include "shaderProgram.h"

#include "platform.h"
#include "scene/light.h"
#include "gl/renderState.h"
#include "glm/gtc/type_ptr.hpp"

#include <sstream>
#include <regex>
#include <set>

namespace Tangram {


ShaderProgram::ShaderProgram() {

    m_glProgram = 0;
    m_glFragmentShader = 0;
    m_glVertexShader = 0;
    m_needsBuild = true;
    m_generation = -1;
    m_invalidShaderSource = false;
}

ShaderProgram::~ShaderProgram() {

    if (m_glProgram != 0) {
        glDeleteProgram(m_glProgram);
    }

    if (m_glFragmentShader != 0) {
        glDeleteShader(m_glFragmentShader);
    }

    if (m_glVertexShader != 0) {
        glDeleteShader(m_glVertexShader);
    }

    // Deleting a shader program being used ends up setting up the current shader program to 0
    // after the driver finishes using it, force this setup by setting the current program
    if (RenderState::shaderProgram.compare(m_glProgram)) {
        RenderState::shaderProgram.init(0, false);
    }

    m_attribMap.clear();

}

void ShaderProgram::setSourceStrings(const std::string& fragSrc, const std::string& vertSrc){
    m_fragmentShaderSource = std::string(fragSrc);
    m_vertexShaderSource = std::string(vertSrc);
    m_needsBuild = true;
}

void ShaderProgram::addSourceBlock(const std::string& tagName, const std::string& glslSource, bool allowDuplicate){

    if (!allowDuplicate) {
        for (auto& source : m_sourceBlocks[tagName]) {
            if (glslSource == source) {
                return;
            }
        }
    }

    m_sourceBlocks[tagName].push_back(glslSource);
    m_needsBuild = true;

    //  TODO:
    //          - add Global Blocks
}

GLint ShaderProgram::getAttribLocation(const std::string& attribName) {

    // Get uniform location at this key, or create one valued at -2 if absent
    GLint& location = m_attribMap[attribName].loc;

    // -2 means this is a new entry
    if (location == -2) {
        // Get the actual location from OpenGL
        location = glGetAttribLocation(m_glProgram, attribName.c_str());
    }

    return location;

}

GLint ShaderProgram::getUniformLocation(const UniformLocation& uniform) {

    if (m_generation == uniform.generation) {
        return uniform.location;
    }

    uniform.generation = m_generation;
    uniform.location = glGetUniformLocation(m_glProgram, uniform.name.c_str());

    return uniform.location;
}

bool ShaderProgram::use() {

    checkValidity();

    if (m_needsBuild) {
        build();
    }

    if (m_glProgram != 0) {
        RenderState::shaderProgram(m_glProgram);
        return true;
    }
    return false;
}

bool ShaderProgram::build() {

    m_needsBuild = false;
    m_generation = RenderState::generation();

    if (m_invalidShaderSource) { return false; }

    // Inject source blocks

    Light::assembleLights(m_sourceBlocks);

    auto vertSrc = applySourceBlocks(m_vertexShaderSource, false);
    auto fragSrc = applySourceBlocks(m_fragmentShaderSource, true);

    // Try to compile vertex and fragment shaders, releasing resources and quiting on failure

    GLint vertexShader = makeCompiledShader(vertSrc, GL_VERTEX_SHADER);

    if (vertexShader == 0) {
        return false;
    }

    GLint fragmentShader = makeCompiledShader(fragSrc, GL_FRAGMENT_SHADER);

    if (fragmentShader == 0) {
        glDeleteShader(vertexShader);
        return false;
    }

    // Try to link shaders into a program, releasing resources and quiting on failure

    GLint program = makeLinkedShaderProgram(fragmentShader, vertexShader);

    if (program == 0) {
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        return false;
    }

    // Delete handles for old shaders and program; values of 0 are silently ignored

    glDeleteShader(m_glFragmentShader);
    glDeleteShader(m_glVertexShader);
    glDeleteProgram(m_glProgram);

    m_glFragmentShader = fragmentShader;
    m_glVertexShader = vertexShader;
    m_glProgram = program;

    // Clear any cached shader locations

    m_attribMap.clear();

    return true;
}

GLuint ShaderProgram::makeLinkedShaderProgram(GLint fragShader, GLint vertShader) {

    GLuint program = glCreateProgram();
    glAttachShader(program, fragShader);
    glAttachShader(program, vertShader);
    glLinkProgram(program);

    GLint isLinked;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);

    if (isLinked == GL_FALSE) {
        GLint infoLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength > 1) {
            std::vector<GLchar> infoLog(infoLength);
            glGetProgramInfoLog(program, infoLength, NULL, &infoLog[0]);
            LOGE("linking program:\n%s", &infoLog[0]);
        }
        glDeleteProgram(program);
        m_invalidShaderSource = true;
        return 0;
    }

    return program;
}

GLuint ShaderProgram::makeCompiledShader(const std::string& src, GLenum type) {

    GLuint shader = glCreateShader(type);
    const GLchar* source = (const GLchar*) src.c_str();
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);

    GLint isCompiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);

    if (isCompiled == GL_FALSE) {
        GLint infoLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLength);
        if (infoLength > 1) {
            std::vector<GLchar> infoLog(infoLength);
            glGetShaderInfoLog(shader, infoLength, NULL, &infoLog[0]);
            LOGE("Compiling shader:\n%s", &infoLog[0]);
            //logMsg("\n%s\n", source);
        }
        glDeleteShader(shader);
        m_invalidShaderSource = true;
        return 0;
    }

    return shader;

}


std::string ShaderProgram::applySourceBlocks(const std::string& source, bool fragShader) {

    static const std::regex pragmaLine("^\\s*#pragma tangram:\\s+(\\w+).*$");

    std::stringstream sourceOut;
    std::set<std::string> pragmas;
    std::smatch sm;

    sourceOut << "#define TANGRAM_EPSILON 0.00001\n";
    sourceOut << "#define TANGRAM_WORLD_POSITION_WRAP 100000.\n";

    if (fragShader) {
        sourceOut << "#define TANGRAM_FRAGMENT_SHADER\n";
    } else {
        float depthDelta = 2.f / (1 << 16);
        sourceOut << "#define TANGRAM_DEPTH_DELTA " << std::to_string(depthDelta) << '\n';
        sourceOut << "#define TANGRAM_VERTEX_SHADER\n";
    }

    auto sourcePos = source.begin();
    size_t lineStart = 0, lineEnd;

    while ((lineEnd = source.find('\n', lineStart)) != std::string::npos) {

        if (lineEnd - lineStart == 0) {
            // skip empty lines
            lineStart += 1;
            continue;
        }

        auto matchPos = source.begin() + lineStart;
        auto matchEnd = source.begin() + lineEnd;
        lineStart = lineEnd + 1;

        if (std::regex_match(matchPos, matchEnd, sm, pragmaLine)) {

            std::string pragmaName = sm[1];

            bool unique;
            std::tie(std::ignore, unique) = pragmas.emplace(std::move(pragmaName));

            // ignore duplicates
            if (!unique) { continue; }

            auto block = m_sourceBlocks.find(sm[1]);
            if (block == m_sourceBlocks.end()) { continue; }

            // write from last source position to end of pragma
            sourceOut << '\n';
            std::copy(sourcePos, matchEnd, std::ostream_iterator<char>(sourceOut));
            sourcePos = matchEnd;

            // insert blocks
            for (auto& source : block->second) {
                sourceOut << '\n';
                sourceOut << source;
            }
        }
    }

    // write from last written source position to end of source
    std::copy(sourcePos, source.end(), std::ostream_iterator<char>(sourceOut));

    // for (auto& block : m_sourceBlocks) {
    //     if (pragmas.find(block.first) == pragmas.end()) {
    //         logMsg("Warning: expected pragma '%s' in shader source\n",
    //                block.first.c_str());
    //     }
    // }

    return sourceOut.str();
}

void ShaderProgram::checkValidity() {

    if (!RenderState::isValidGeneration(m_generation)) {
        m_glFragmentShader = 0;
        m_glVertexShader = 0;
        m_glProgram = 0;
        m_needsBuild = true;
        m_uniformCache.clear();
    }
}

std::string ShaderProgram::getExtensionDeclaration(const std::string& extension) {
    std::ostringstream oss;
    oss << "#if defined(GL_ES) == 0 || defined(GL_" << extension << ")\n";
    oss << "    #extension GL_" << extension << " : enable\n";
    oss << "    #define TANGRAM_EXTENSION_" << extension << '\n';
    oss << "#endif\n";
    return oss.str();
}

void ShaderProgram::setUniformi(const UniformLocation& loc, int value) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, value);
        if (!cached) { glUniform1i(location, value); }
    }
}

void ShaderProgram::setUniformi(const UniformLocation& loc, int value0, int value1) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, glm::vec2(value0, value1));
        if (!cached) { glUniform2i(location, value0, value1); }
    }
}

void ShaderProgram::setUniformi(const UniformLocation& loc, int value0, int value1, int value2) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, glm::vec3(value0, value1, value2));
        if (!cached) { glUniform3i(location, value0, value1, value2); }
    }
}

void ShaderProgram::setUniformi(const UniformLocation& loc, int value0, int value1, int value2, int value3) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, glm::vec4(value0, value1, value2, value3));
        if (!cached) { glUniform4i(location, value0, value1, value2, value3); }
    }
}

void ShaderProgram::setUniformf(const UniformLocation& loc, float value) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, value);
        if (!cached) { glUniform1f(location, value); }
    }
}

void ShaderProgram::setUniformf(const UniformLocation& loc, float value0, float value1) {
    setUniformf(loc, glm::vec2(value0, value1));
}

void ShaderProgram::setUniformf(const UniformLocation& loc, float value0, float value1, float value2) {
    setUniformf(loc, glm::vec3(value0, value1, value2));
}

void ShaderProgram::setUniformf(const UniformLocation& loc, float value0, float value1, float value2, float value3) {
    setUniformf(loc, glm::vec4(value0, value1, value2, value3));
}

void ShaderProgram::setUniformf(const UniformLocation& loc, const glm::vec2& value) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, value);
        if (!cached) { glUniform2f(location, value.x, value.y); }
    }
}

void ShaderProgram::setUniformf(const UniformLocation& loc, const glm::vec3& value) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, value);
        if (!cached) { glUniform3f(location, value.x, value.y, value.z); }
    }
}

void ShaderProgram::setUniformf(const UniformLocation& loc, const glm::vec4& value) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, value);
        if (!cached) { glUniform4f(location, value.x, value.y, value.z, value.w); }
    }
}

void ShaderProgram::setUniformMatrix2f(const UniformLocation& loc, const glm::mat2& value, bool transpose) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = !transpose && getFromCache(location, value);
        if (!cached) { glUniformMatrix2fv(location, 1, transpose, glm::value_ptr(value)); }
    }
}

void ShaderProgram::setUniformMatrix3f(const UniformLocation& loc, const glm::mat3& value, bool transpose) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = !transpose && getFromCache(location, value);
        if (!cached) { glUniformMatrix3fv(location, 1, transpose, glm::value_ptr(value)); }
    }
}

void ShaderProgram::setUniformMatrix4f(const UniformLocation& loc, const glm::mat4& value, bool transpose) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = !transpose && getFromCache(location, value);
        if (!cached) { glUniformMatrix4fv(location, 1, transpose, glm::value_ptr(value)); }
    }
}

void ShaderProgram::setUniformf(const UniformLocation& loc, const UniformArray& value) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, value);
        if (!cached) { glUniform1fv(location, value.size(), value.data()); }
    }
}

void ShaderProgram::setUniformi(const UniformLocation& loc, const UniformTextureArray& value) {
    use();
    GLint location = getUniformLocation(loc);
    if (location >= 0) {
        bool cached = getFromCache(location, value);
        if (!cached) { glUniform1iv(location, value.slots.size(), value.slots.data()); }
    }
}

}
