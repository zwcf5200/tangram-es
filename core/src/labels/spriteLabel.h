#pragma once

#include "labels/label.h"
#include "labels/labelProperty.h"
#include "labels/labelSet.h"

namespace Tangram {

class SpriteLabels;
class PointStyle;

struct SpriteVertex {
    glm::i16vec2 pos;
    glm::u16vec2 uv;
    uint32_t color;
    glm::i16vec2 extrude;
    struct State {
        glm::i16vec2 screenPos;
        uint8_t alpha;
        uint8_t scale;
        int16_t rotation;
    } state;

    static const float position_scale;
    static const float rotation_scale;
    static const float alpha_scale;
    static const float texture_scale;
    static const float extrusion_scale;
};

class SpriteLabel : public Label {
public:

    SpriteLabel(Label::Transform transform, glm::vec2 size, Label::Options options,
                float extrudeScale, LabelProperty::Anchor anchor,
                SpriteLabels& labels, size_t labelsPos);

    void updateBBoxes(float zoomFract) override;
    void align(glm::vec2& screenPosition, const glm::vec2& ap1, const glm::vec2& ap2) override;

    void pushTransform() override;

private:
    // Back-pointer to owning container and position
    const SpriteLabels& m_labels;
    const size_t m_labelsPos;

    float m_extrudeScale;
    glm::vec2 m_anchor;
};

struct SpriteQuad {
    struct {
        glm::i16vec2 pos;
        glm::u16vec2 uv;
        glm::i16vec2 extrude;
    } quad[4];
    // TODO color and stroke must not be stored per quad
    uint32_t color;
};

class SpriteLabels : public LabelSet {
public:
    SpriteLabels(const PointStyle& style) : m_style(style) {}

    void setQuads(std::vector<SpriteQuad>& quads) {
        this->quads.insert(this->quads.end(), quads.begin(), quads.end());
    }

    // TODO: hide within class if needed
    const PointStyle& m_style;
    std::vector<SpriteQuad> quads;
};

}
