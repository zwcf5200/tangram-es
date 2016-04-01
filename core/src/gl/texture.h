#pragma once

#include "gl.h"

#include <vector>
#include <memory>
#include <string>

namespace Tangram {

struct TextureFiltering {
    GLenum min;
    GLenum mag;
};

struct TextureWrapping {
    GLenum wraps;
    GLenum wrapt;
};

struct TextureOptions {
    GLenum internalFormat;
    GLenum format;
    TextureFiltering filtering;
    TextureWrapping wrapping;
};

#define DEFAULT_TEXTURE_OPTION \
    {GL_ALPHA, GL_ALPHA, \
    {GL_LINEAR, GL_LINEAR}, \
    {GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE}

class Texture {

public:

    Texture(unsigned int width, unsigned int height,
            TextureOptions options = DEFAULT_TEXTURE_OPTION},
            bool generateMipmaps = false);

    Texture(const unsigned char* data, size_t dataSize,
            TextureOptions options = DEFAULT_TEXTURE_OPTION},
            bool generateMipmaps = false);

    Texture(const std::string& file,
            TextureOptions options = DEFAULT_TEXTURE_OPTION},
            bool generateMipmaps = false);

    Texture(Texture&& other);
    Texture& operator=(Texture&& other);

    virtual ~Texture();

    /* Perform texture updates, should be called at least once and after adding data or resizing */
    virtual void update(GLuint textureSlot);

    virtual void update(GLuint textureSlot, const GLuint* data);

    /* Resize the texture */
    void resize(const unsigned int width, const unsigned int height);

    /* Width and Height texture getters */
    unsigned int getWidth() const { return m_width; }
    unsigned int getHeight() const { return m_height; }

    void bind(GLuint unit);

    void setDirty(size_t yOffset, size_t height);

    GLuint getGlHandle() { return m_glHandle; }

    /* Sets texture data
     *
     * Has less priority than set sub data
     */
    void setData(const GLuint* data, unsigned int dataSize);

    /* Update a region of the texture */
    void setSubData(const GLuint* subData, uint16_t xoff, uint16_t yoff,
                    uint16_t width, uint16_t height, uint16_t stride);

    bool isValid();

    typedef std::pair<GLuint, GLuint> TextureSlot;

    static void invalidateAllTextures();

    static bool isRepeatWrapping(TextureWrapping wrapping);

    void loadPNG(const unsigned char* blob, unsigned int size);

protected:

    void generate(GLuint textureUnit);
    void checkValidity();

    TextureOptions m_options;
    std::vector<GLuint> m_data;
    GLuint m_glHandle;

    struct DirtyRange {
        size_t min;
        size_t max;
    };
    std::vector<DirtyRange> m_dirtyRanges;

    bool m_shouldResize;

    unsigned int m_width;
    unsigned int m_height;

    GLenum m_target;

    int m_generation;

private:

    size_t bytesPerPixel();

    bool m_generateMipmaps;
};

}
