#pragma once

#include "label.h"
#include "spriteLabel.h"
#include "tile/tileID.h"
#include "data/properties.h"
#include "isect2d.h"
#include "glm_vec.h" // for isect2d.h

#include <memory>
#include <mutex>
#include <vector>
#include <map>
#include <set>

namespace Tangram {

class FontContext;
class Tile;
class View;
class Style;
class TileCache;
struct TouchItem;

class Labels {

public:
    Labels();

    virtual ~Labels();

    void drawDebug(const View& view);

    void update(const View& view, float dt, const std::vector<std::unique_ptr<Style>>& styles,
                const std::vector<std::shared_ptr<Tile>>& tiles, std::unique_ptr<TileCache>& cache);

    const std::vector<TouchItem>& getFeaturesAtPoint(const View& view, float dt,
                                                     const std::vector<std::unique_ptr<Style>>& styles,
                                                     const std::vector<std::shared_ptr<Tile>>& tiles,
                                                     float x, float y, bool visibleOnly = true);

    bool needUpdate() const { return m_needUpdate; }

private:

    using AABB = isect2d::AABB<glm::vec2>;
    using OBB = isect2d::OBB<glm::vec2>;
    using CollisionPairs = std::vector<isect2d::ISect2D<glm::vec2>::Pair>;

    void updateLabels(const std::vector<std::unique_ptr<Style>>& styles,
                      const std::vector<std::shared_ptr<Tile>>& tiles,
                      float dt, float dz, const View& view);

    void skipTransitions(const std::vector<std::unique_ptr<Style>>& styles,
                         const std::vector<std::shared_ptr<Tile>>& tiles,
                         std::unique_ptr<TileCache>& cache, float currentZoom) const;

    void skipTransitions(const std::vector<const Style*>& styles, Tile& tile, Tile& proxy) const;

    void checkRepeatGroups(std::vector<Label*>& visibleSet) const;

    int LODDiscardFunc(float maxZoom, float zoom);

    bool m_needUpdate;

    // temporary data used in update()
    std::vector<Label*> m_labels;
    std::vector<AABB> m_aabbs;

    isect2d::ISect2D<glm::vec2> m_isect2d;

    std::vector<TouchItem> m_touchItems;

    float m_lastZoom;
};

}

