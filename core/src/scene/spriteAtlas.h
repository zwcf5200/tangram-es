#pragma once

#include <map>
#include <memory>
#include "gl/texture.h"
#include "glm/glm.hpp"

namespace Tangram {

struct SpriteNode {
    glm::vec2 m_uvBL;
    glm::vec2 m_uvTR;
    glm::vec2 m_size;
};

class SpriteAtlas {

public:
    SpriteAtlas(std::shared_ptr<Texture> texture, const std::string& file);

    /* Creates a sprite node in the atlas located at origin in the texture by a size in pixels size */
    void addSpriteNode(const std::string& name, glm::vec2 origin, glm::vec2 size);
    bool getSpriteNode(const std::string& name, SpriteNode& node) const;

    /* Bind the atlas in the driver */
    void bind(GLuint slot);

private:
    std::map<std::string, SpriteNode> m_spritesNodes;
    std::string m_file;
    std::shared_ptr<Texture> m_texture;
};

}
