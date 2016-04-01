#pragma once

#include "text/fontContext.h"
#include "labels/textLabel.h"

#include <vector>
#include <bitset>

namespace Tangram {

class TextLabels : public LabelSet {

public:

    TextLabels(const TextStyle& style) : style(style) {}

    ~TextLabels() override;

    void setQuads(std::vector<GlyphQuad>& quads);

    std::vector<GlyphQuad> quads;
    const TextStyle& style;

private:

    std::bitset<FontContext::max_textures> m_atlasRefs;
};

}
