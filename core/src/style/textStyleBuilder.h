#pragma once

#include "textStyle.h"

#include "labels/textLabel.h"
#include "labels/labelProperty.h"
#include "text/fontContext.h"

#include <memory>
#include <vector>
#include <string>

namespace Tangram {

class TextStyleBuilder : public StyleBuilder {

public:

    TextStyleBuilder(const TextStyle& style);

    const Style& style() const override { return m_style; }

    void addFeature(const Feature& feature, const DrawRule& rule) override;

    void setup(const Tile& tile) override;

    std::unique_ptr<StyledMesh> build() override;

    TextStyle::Parameters applyRule(const DrawRule& rule, const Properties& props) const;

    bool prepareLabel(TextStyle::Parameters& params, Label::Type type);

    // Add label to the mesh using the prepared label data
    void addLabel(const TextStyle::Parameters& params, Label::Type type,
                  Label::Transform transform);

    std::string applyTextTransform(const TextStyle::Parameters& params, const std::string& string);

    std::string resolveTextSource(const std::string& textSource, const Properties& props) const;

protected:

    const TextStyle& m_style;

    // Result: TextLabel container
    std::unique_ptr<TextLabels> m_textLabels;

    // Buffers to hold data for TextLabels until build()
    std::vector<GlyphQuad> m_quads;
    std::vector<std::unique_ptr<Label>> m_labels;

    // Attributes of the currently prepared Label
    struct {
        float width;
        float height;

        // start position in m_quads
        size_t quadsStart;

        uint32_t fill;
        uint32_t stroke;
        uint8_t fontScale;
    } m_attributes;

    float m_tileSize;
    bool m_sdf;
};

}
