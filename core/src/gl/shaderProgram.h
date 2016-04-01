#pragma once

#include "gl.h"
#include "uniform.h"
#include "util/fastmap.h"

#include "glm/glm.hpp"

#include <string>
#include <vector>
#include <map>

namespace Tangram {

/*
 * ShaderProgram - utility class representing an OpenGL shader program
 */
class ShaderProgram {

public:

    ShaderProgram();
    virtual ~ShaderProgram();

    /* Set the vertex and fragment shader GLSL source to the given strings */
    void setSourceStrings(const std::string& fragSrc, const std::string& vertSrc);

    /*  Add a block of GLSL to be injected at "#pragma tangram: [tagName]" in the shader sources */
    void addSourceBlock(const std::string& tagName, const std::string& glslSource, bool allowDuplicate = true);

    /*
     * Applies all source blocks to the source strings for this shader and attempts to compile
     * and then link the resulting vertex and fragment shaders; if compiling or linking fails
     * it prints the compiler log, returns false, and keeps the program's previous state; if
     * successful it returns true.
     */
    bool build();

    /* Getters */
    GLuint getGlProgram() const { return m_glProgram; };
    GLuint getGlFragmentShader() const { return m_glFragmentShader; };
    GLuint getGlVertexShader() const { return m_glVertexShader; };

    /*
     * Fetches the location of a shader attribute, caching the result
     */
    GLint getAttribLocation(const std::string& attribName);

    /*
     * Fetches the location of a shader uniform, caching the result
     */
    GLint getUniformLocation(const UniformLocation& uniformName);

    /*
     * Returns true if this object represents a valid OpenGL shader program
     */
    bool isValid() const { return m_glProgram != 0; };

    /*
     * Binds the program in openGL if it is not already bound; If the shader sources
     * have been modified since the last time build() was called, also calls build().
     * Returns true if shader can be used (i.e. is valid)
     */
    bool use();

    /*
     * Ensures the program is bound and then sets the named uniform to the given value(s)
     */
    void setUniformi(const UniformLocation& loc, int value);
    void setUniformi(const UniformLocation& loc, int value0, int value1);
    void setUniformi(const UniformLocation& loc, int value0, int value1, int value2);
    void setUniformi(const UniformLocation& loc, int value0, int value1, int value2, int value3);

    void setUniformf(const UniformLocation& loc, float value);
    void setUniformf(const UniformLocation& loc, float value0, float value1);
    void setUniformf(const UniformLocation& loc, float value0, float value1, float value2);
    void setUniformf(const UniformLocation& loc, float value0, float value1, float value2, float value3);

    void setUniformf(const UniformLocation& loc, const glm::vec2& value);
    void setUniformf(const UniformLocation& loc, const glm::vec3& value);
    void setUniformf(const UniformLocation& loc, const glm::vec4& value);

    void setUniformf(const UniformLocation& loc, const UniformArray& value);
    void setUniformi(const UniformLocation& loc, const UniformTextureArray& value);

    /*
     * Ensures the program is bound and then sets the named uniform to the values
     * beginning at the pointer value; 4 values are used for a 2x2 matrix, 9 values for a 3x3, etc.
     */
    void setUniformMatrix2f(const UniformLocation& loc, const glm::mat2& value, bool transpose = false);
    void setUniformMatrix3f(const UniformLocation& loc, const glm::mat3& value, bool transpose = false);
    void setUniformMatrix4f(const UniformLocation& loc, const glm::mat4& value, bool transpose = false);

    /* Invalidates all managed ShaderPrograms
     *
     * This should be called in the event of a GL context loss; former GL shader object
     * handles are invalidated and immediately recreated.
     */
    static void invalidateAllPrograms();

    static std::string getExtensionDeclaration(const std::string& extension);

    auto getSourceBlocks() const { return  m_sourceBlocks; }

private:

    struct ShaderLocation {
        GLint loc;
        ShaderLocation() : loc(-2) {}
        // This struct exists to resolve an ambiguity in shader locations:
        // In the unordered_maps that store shader uniform and attrib locations,
        // Un-mapped 'keys' are initialized by constructing the 'value' type.
        // For numerical types this constructs a value of 0. But 0 is a valid
        // location, so it is ambiguous whether the value is unmapped or simply 0.
        // Therefore, we use a dummy structure which does nothing but initialize
        // to a value that is not a valid uniform or attribute location.
    };

    static int s_validGeneration; // Incremented when GL context is invalidated

    // Get a uniform value from the cache, and returns false when it's a cache miss
    template <class T>
    inline bool getFromCache(GLint location, T value) {
        auto& v = m_uniformCache[location];
        if (v.is<T>()) {
            T& cached = v.get<T>();
            if (cached == value) {
                return true;
            }
        }
        v = value;
        return false;
    }

    int m_generation;
    GLuint m_glProgram;
    GLuint m_glFragmentShader;
    GLuint m_glVertexShader;

    fastmap<std::string, ShaderLocation> m_attribMap;
    fastmap<GLint, UniformValue> m_uniformCache;

    std::string m_fragmentShaderSource;
    std::string m_vertexShaderSource;

    std::map<std::string, std::vector<std::string>> m_sourceBlocks;

    bool m_needsBuild;
    bool m_invalidShaderSource;

    void checkValidity();
    GLuint makeLinkedShaderProgram(GLint fragShader, GLint vertShader);
    GLuint makeCompiledShader(const std::string& src, GLenum type);

    std::string applySourceBlocks(const std::string& source, bool fragShader);

};

}
