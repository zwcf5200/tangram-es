#include "spriteAtlas.h"
#include "platform.h"

namespace Tangram {

SpriteAtlas::SpriteAtlas(std::shared_ptr<Texture> texture, const std::string& file) : m_file(file), m_texture(texture) {}

void SpriteAtlas::addSpriteNode(const std::string& name, glm::vec2 origin, glm::vec2 size) {

    glm::vec2 atlasSize = {m_texture->getWidth(), m_texture->getHeight()};
    glm::vec2 uvBL = origin / atlasSize;
    glm::vec2 uvTR = (origin + size) / atlasSize;

    m_spritesNodes[name] = SpriteNode { uvBL, uvTR, size };
}

bool SpriteAtlas::getSpriteNode(const std::string& name, SpriteNode& node) const {
    auto it = m_spritesNodes.find(name);
    if (it == m_spritesNodes.end()) {
        return false;
    }

    node = it->second;
    return true;
}

void SpriteAtlas::bind(GLuint slot) {
    m_texture->update(slot);
    m_texture->bind(slot);
}

}
