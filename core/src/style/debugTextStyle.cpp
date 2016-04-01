#include "debugTextStyle.h"

#include "textStyleBuilder.h"
#include "labels/textLabels.h"

#include "tangram.h"
#include "tile/tile.h"

namespace Tangram {

class DebugTextStyleBuilder : public TextStyleBuilder {

public:

    DebugTextStyleBuilder(const TextStyle& style)
        : TextStyleBuilder(style) {}

    void setup(const Tile& tile) override;

    std::unique_ptr<StyledMesh> build() override;

private:
    std::string m_tileID;

};

void DebugTextStyleBuilder::setup(const Tile& tile) {
    if (!Tangram::getDebugFlag(Tangram::DebugFlags::tile_infos)) {
        return;
    }

    m_tileID = tile.getID().toString();

    TextStyleBuilder::setup(tile);
}

std::unique_ptr<StyledMesh> DebugTextStyleBuilder::build() {
    if (!Tangram::getDebugFlag(Tangram::DebugFlags::tile_infos)) {
        return nullptr;
    }

    TextStyle::Parameters params;

    params.text = m_tileID;
    params.fontSize = 30.f;

    params.font = m_style.context()->getFont("sans-serif", "normal", "400", 32 * m_style.pixelScale());

    if (!prepareLabel(params, Label::Type::debug)) {
        return nullptr;
    }

    addLabel(params, Label::Type::debug, { glm::vec2(.5f) });

    m_textLabels->setLabels(m_labels);
    m_textLabels->setQuads(m_quads);

    m_quads.clear();
    m_labels.clear();

    return std::move(m_textLabels);
}

std::unique_ptr<StyleBuilder> DebugTextStyle::createBuilder() const {
    return std::make_unique<DebugTextStyleBuilder>(*this);
}

}
