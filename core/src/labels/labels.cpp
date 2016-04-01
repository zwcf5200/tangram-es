#include "labels.h"

#include "tangram.h"
#include "platform.h"
#include "gl/shaderProgram.h"
#include "gl/primitives.h"
#include "view/view.h"
#include "style/style.h"
#include "style/pointStyle.h"
#include "style/textStyle.h"
#include "tile/tile.h"
#include "tile/tileCache.h"
#include "labels/labelSet.h"
#include "labels/textLabel.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/rotate_vector.hpp"
#include "glm/gtx/norm.hpp"

namespace Tangram {

Labels::Labels()
    : m_needUpdate(false),
      m_lastZoom(0.0f)
{}

Labels::~Labels() {}

// int Labels::LODDiscardFunc(float maxZoom, float zoom) {
//     return (int) MIN(floor(((log(-zoom + (maxZoom + 2)) / log(maxZoom + 2) * (maxZoom )) * 0.5)), MAX_LOD);
// }

void Labels::updateLabels(const std::vector<std::unique_ptr<Style>>& styles,
                          const std::vector<std::shared_ptr<Tile>>& tiles,
                          float dt, float dz, const View& view)
{
    glm::vec2 screenSize = glm::vec2(view.getWidth(), view.getHeight());

    // int lodDiscard = LODDiscardFunc(View::s_maxZoom, view.getZoom());

    for (const auto& tile : tiles) {

        // discard based on level of detail
        // if ((zoom - tile->getID().z) > lodDiscard) {
        //     continue;
        // }

        bool proxyTile = tile->isProxy();

        glm::mat4 mvp = view.getViewProjectionMatrix() * tile->getModelMatrix();

        for (const auto& style : styles) {
            const auto& mesh = tile->getMesh(*style);
            if (!mesh) { continue; }

            auto labelMesh = dynamic_cast<const LabelSet*>(mesh.get());
            if (!labelMesh) { continue; }

            for (auto& label : labelMesh->getLabels()) {
                if (!label->update(mvp, screenSize, dz)) {
                    // skip dead labels
                    continue;
                }
                if (label->canOcclude()) {
                    label->setProxy(proxyTile);
                    m_labels.push_back(label.get());

                } else {
                    m_needUpdate |= label->evalState(screenSize, dt);
                    label->pushTransform();
                }
            }
        }
    }
}

void Labels::skipTransitions(const std::vector<const Style*>& styles, Tile& tile, Tile& proxy) const {

    for (const auto& style : styles) {

        auto* mesh0 = dynamic_cast<const LabelSet*>(tile.getMesh(*style).get());
        if (!mesh0) { continue; }

        auto* mesh1 = dynamic_cast<const LabelSet*>(proxy.getMesh(*style).get());
        if (!mesh1) { continue; }

        for (auto& l0 : mesh0->getLabels()) {
            if (!l0->canOcclude()) { continue; }
            if (l0->state() != Label::State::wait_occ) { continue; }

            for (auto& l1 : mesh1->getLabels()) {
                if (!l1->visibleState()) { continue; }
                if (!l1->canOcclude()) { continue;}

                // Using repeat group to also handle labels with dynamic style properties
                if (l0->options().repeatGroup != l1->options().repeatGroup) { continue; }
                // if (l0->hash() != l1->hash()) { continue; }

                float d2 = glm::distance2(l0->transform().state.screenPos,
                                          l1->transform().state.screenPos);

                // The new label lies within the circle defined by the bbox of l0
                if (sqrt(d2) < std::max(l0->dimension().x, l0->dimension().y)) {
                    l0->skipTransitions();
                }
            }
        }
    }
}

std::shared_ptr<Tile> findProxy(int32_t sourceID, const TileID& proxyID,
                                const std::vector<std::shared_ptr<Tile>>& tiles,
                                std::unique_ptr<TileCache>& cache) {

    auto proxy = cache->contains(sourceID, proxyID);
    if (proxy) { return proxy; }

    for (auto& tile : tiles) {
        if (tile->getID() == proxyID && tile->sourceID() == sourceID) {
            return tile;
        }
    }
    return nullptr;
}

void Labels::skipTransitions(const std::vector<std::unique_ptr<Style>>& styles,
                             const std::vector<std::shared_ptr<Tile>>& tiles,
                             std::unique_ptr<TileCache>& cache, float currentZoom) const {

    std::vector<const Style*> stylePtrs;

    for (const auto& style : styles) {
        if (dynamic_cast<const TextStyle*>(style.get()) ||
            dynamic_cast<const PointStyle*>(style.get())) {
            stylePtrs.push_back(style.get());
        }
    }

    for (const auto& tile : tiles) {
        TileID tileID = tile->getID();
        std::shared_ptr<Tile> proxy;

        if (m_lastZoom < currentZoom) {
            // zooming in, add the one cached parent tile
            proxy = findProxy(tile->sourceID(), tileID.getParent(), tiles, cache);
            if (proxy) { skipTransitions(stylePtrs, *tile, *proxy); }
        } else {
            // zooming out, add the 4 cached children tiles
            proxy = findProxy(tile->sourceID(), tileID.getChild(0), tiles, cache);
            if (proxy) { skipTransitions(stylePtrs, *tile, *proxy); }

            proxy = findProxy(tile->sourceID(), tileID.getChild(1), tiles, cache);
            if (proxy) { skipTransitions(stylePtrs, *tile, *proxy); }

            proxy = findProxy(tile->sourceID(), tileID.getChild(2), tiles, cache);
            if (proxy) { skipTransitions(stylePtrs, *tile, *proxy); }

            proxy = findProxy(tile->sourceID(), tileID.getChild(3), tiles, cache);
            if (proxy) { skipTransitions(stylePtrs, *tile, *proxy); }
        }
    }
}

void Labels::checkRepeatGroups(std::vector<Label*>& visibleSet) const {

    struct GroupElement {
        Label* label;

        bool operator==(const GroupElement& ge) {
            return ge.label->center() == label->center();
        };
    };

    std::map<size_t, std::vector<GroupElement>> repeatGroups;

    for (Label* textLabel : visibleSet) {
        auto& options = textLabel->options();
        GroupElement element { textLabel };

        auto& group = repeatGroups[options.repeatGroup];
        if (group.empty()) {
            group.push_back(element);
            continue;
        }

        if (std::find(group.begin(), group.end(), element) != group.end()) {
            //Two tiles contain the same label - have the same screen position.
            continue;
        }

        float threshold2 = pow(options.repeatDistance, 2);

        bool add = true;
        for (GroupElement& ge : group) {

            float d2 = distance2(ge.label->center(), textLabel->center());
            if (d2 < threshold2) {
                if (textLabel->visibleState() && !ge.label->visibleState()) {
                    // If textLabel is already visible, the added GroupElement is not
                    // replace GroupElement with textLabel and set the other occluded.
                    ge.label->occlude();
                    ge.label = textLabel;
                } else {
                    textLabel->occlude();
                }
                add = false;
                break;
            }
        }

        if (add) {
            // No other label of this group within repeatDistance
            group.push_back(element);
        }
    }
}

void Labels::update(const View& view, float dt,
                    const std::vector<std::unique_ptr<Style>>& styles,
                    const std::vector<std::shared_ptr<Tile>>& tiles,
                    std::unique_ptr<TileCache>& cache)
{
    // Could clear this at end of function unless debug draw is active
    m_labels.clear();
    m_aabbs.clear();

    float currentZoom = view.getZoom();
    float dz = currentZoom - std::floor(currentZoom);

    /// Collect and update labels from visible tiles

    updateLabels(styles, tiles, dt, dz, view);

    /// Mark labels to skip transitions

    if (int(m_lastZoom) != int(view.getZoom())) {
        skipTransitions(styles, tiles, cache, currentZoom);
    }

    /// Manage occlusions

    // Update collision context size
    m_isect2d.resize({view.getWidth() / 256, view.getHeight() / 256},
                     {view.getWidth(), view.getHeight()});

    // Broad phase collision detection
    for (auto* label : m_labels) {
        m_aabbs.push_back(label->aabb());
        m_aabbs.back().m_userData = (void*)label;
    }

    m_isect2d.intersect(m_aabbs);

    // Narrow Phase, resolve conflicts
    for (auto& pair : m_isect2d.pairs) {
        const auto& aabb1 = m_aabbs[pair.first];
        const auto& aabb2 = m_aabbs[pair.second];

        auto l1 = static_cast<Label*>(aabb1.m_userData);
        auto l2 = static_cast<Label*>(aabb2.m_userData);

        if (l1->isOccluded() || l2->isOccluded()) {
            // One of this pair is already occluded.
            // => conflict solved
            continue;
        }

        if (!intersect(l1->obb(), l2->obb())) { continue; }

        if (l1->isProxy() != l2->isProxy()) {
            // check first is the label belongs to a proxy tile
            if (l1->isProxy()) {
                l1->occlude();
            } else {
                l2->occlude();
            }
        } else if (l1->options().priority != l2->options().priority) {
            // lower numeric priority means higher priority
            if(l1->options().priority > l2->options().priority) {
                l1->occlude();
            } else {
                l2->occlude();
            }
        } else if (l1->occludedLastFrame() != l2->occludedLastFrame()) {
            // keep the one that way active previously
            if (l1->occludedLastFrame()) {
                l1->occlude();
            } else {
                l2->occlude();
            }
        } else if (l1->visibleState() != l2->visibleState()) {
            // keep the visible one, different from occludedLastframe
            // when one lets labels fade out.
            // (A label is also in visibleState() when skip_transition is set)
            if (!l1->visibleState()) {
                l1->occlude();
            } else {
                l2->occlude();
            }
        } else {
            // just so it is consistent between two instances
            if (l1 < l2) {
                l1->occlude();
            } else {
                l2->occlude();
            }
        }
    }

    /// Apply repeat groups

    std::vector<Label*> repeatGroupSet;
    for (auto* label : m_labels) {
        if (label->isOccluded()) {
            continue;
        }
        if (label->options().repeatDistance == 0.f) {
            continue;
        }
        if (!dynamic_cast<TextLabel*>(label)) {
            continue;
        }

        repeatGroupSet.push_back(label);
    }

    // Ensure the labels are always treated in the same order in the visible set
    std::sort(repeatGroupSet.begin(), repeatGroupSet.end(),
              [](auto* a, auto* b) {
        return glm::length2(a->transform().modelPosition1) <
               glm::length2(b->transform().modelPosition1);
    });

    checkRepeatGroups(repeatGroupSet);


    /// Update label meshes

    glm::vec2 screenSize = glm::vec2(view.getWidth(), view.getHeight());

    m_needUpdate = false;
    for (auto* label : m_labels) {
        m_needUpdate |= label->evalState(screenSize, dt);
        label->pushTransform();
    }

    // Request for render if labels are in fading in/out states
    if (m_needUpdate) {
        requestRender();
    }

    m_lastZoom = currentZoom;
}

const std::vector<TouchItem>& Labels::getFeaturesAtPoint(const View& view, float dt,
                                                         const std::vector<std::unique_ptr<Style>>& styles,
                                                         const std::vector<std::shared_ptr<Tile>>& tiles,
                                                         float x, float y, bool visibleOnly) {
    // FIXME dpi dependent threshold
    const float thumbSize = 50;

    m_touchItems.clear();

    glm::vec2 screenSize = glm::vec2(view.getWidth(), view.getHeight());
    glm::vec2 touchPoint(x, y);

    OBB obb(x - thumbSize/2, y - thumbSize/2, 0, thumbSize, thumbSize);

    float z = view.getZoom();
    float dz = z - std::floor(z);

    for (const auto& tile : tiles) {

        glm::mat4 mvp = view.getViewProjectionMatrix() * tile->getModelMatrix();

        for (const auto& style : styles) {
            const auto& mesh = tile->getMesh(*style);
            if (!mesh) { continue; }

            auto labelMesh = dynamic_cast<const LabelSet*>(mesh.get());
            if (!labelMesh) { continue; }

            for (auto& label : labelMesh->getLabels()) {

                auto& options = label->options();
                if (!options.interactive) { continue; }

                if (!visibleOnly) {
                    label->updateScreenTransform(mvp, screenSize, false);
                    label->updateBBoxes(dz);
                } else if (!label->visibleState()) {
                    continue;
                }

                if (isect2d::intersect(label->obb(), obb)) {
                    float distance = glm::length2(label->transform().state.screenPos - touchPoint);
                    auto labelCenter = label->center();
                    m_touchItems.push_back({options.properties, {labelCenter.x, labelCenter.y}, std::sqrt(distance)});
                }
            }
        }
    }

    std::sort(m_touchItems.begin(), m_touchItems.end(),
              [](auto& a, auto& b){ return a.distance < b.distance; });


    return m_touchItems;
}

void Labels::drawDebug(const View& view) {

    if (!Tangram::getDebugFlag(Tangram::DebugFlags::labels)) {
        return;
    }

    for (auto label : m_labels) {
        if (label->canOcclude()) {
            glm::vec2 offset = label->options().offset;
            glm::vec2 sp = label->transform().state.screenPos;
            float angle = label->transform().state.rotation;
            offset = glm::rotate(offset, angle);

            // draw bounding box
            Label::State state = label->state();
            switch (state) {
                case Label::State::sleep:
                    Primitives::setColor(0x00ff00);
                    break;
                case Label::State::visible:
                    Primitives::setColor(0x000000);
                    break;
                case Label::State::wait_occ:
                    Primitives::setColor(0x0000ff);
                    break;
                case Label::State::fading_in:
                case Label::State::fading_out:
                    Primitives::setColor(0xffff00);
                    break;
                default:
                    Primitives::setColor(0xff0000);
            }

            Primitives::drawPoly(&(label->obb().getQuad())[0], 4);

            // draw offset
            Primitives::setColor(0x000000);
            Primitives::drawLine(sp, sp - offset);

            // draw projected anchor point
            Primitives::setColor(0x0000ff);
            Primitives::drawRect(sp - glm::vec2(1.f), sp + glm::vec2(1.f));

            if (label->options().repeatGroup != 0 && label->state() == Label::State::visible) {
                size_t seed = 0;
                hash_combine(seed, label->options().repeatGroup);
                float repeatDistance = label->options().repeatDistance;

                Primitives::setColor(seed);
                Primitives::drawLine(label->center(),
                    glm::vec2(repeatDistance, 0.f) + label->center());

                float off = M_PI / 6.f;
                for (float pad = 0.f; pad < M_PI * 2.f; pad += off) {
                    glm::vec2 p0 = glm::vec2(cos(pad), sin(pad)) * repeatDistance
                        + label->center();
                    glm::vec2 p1 = glm::vec2(cos(pad + off), sin(pad + off)) * repeatDistance
                        + label->center();
                    Primitives::drawLine(p0, p1);
                }
            }
        }
    }

    glm::vec2 split(view.getWidth() / 256, view.getHeight() / 256);
    glm::vec2 res(view.getWidth(), view.getHeight());
    const short xpad = short(ceilf(res.x / split.x));
    const short ypad = short(ceilf(res.y / split.y));

    Primitives::setColor(0x7ef586);
    short x = 0, y = 0;
    for (int j = 0; j < split.y; ++j) {
        for (int i = 0; i < split.x; ++i) {
            AABB cell(x, y, x + xpad, y + ypad);
            Primitives::drawRect({x, y}, {x + xpad, y + ypad});
            x += xpad;
            if (x >= res.x) {
                x = 0;
                y += ypad;
            }
        }
    }
}

}
