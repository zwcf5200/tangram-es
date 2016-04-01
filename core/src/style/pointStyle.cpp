#include "pointStyle.h"

#include "platform.h"
#include "material.h"
#include "gl/shaderProgram.h"
#include "gl/texture.h"
#include "gl/dynamicQuadMesh.h"
#include "gl/vertexLayout.h"
#include "gl/renderState.h"
#include "labels/spriteLabel.h"
#include "scene/drawRule.h"
#include "scene/spriteAtlas.h"
#include "labels/labelSet.h"
#include "tile/tile.h"
#include "util/builders.h"
#include "view/view.h"
#include "data/propertyItem.h" // Include wherever Properties is used!
#include "scene/stops.h"

namespace Tangram {

PointStyle::PointStyle(std::string name, Blending blendMode, GLenum drawMode)
    : Style(name, blendMode, drawMode) {}

PointStyle::~PointStyle() {}

void PointStyle::constructVertexLayout() {

    m_vertexLayout = std::shared_ptr<VertexLayout>(new VertexLayout({
        {"a_position", 2, GL_SHORT, false, 0},
        {"a_uv", 2, GL_UNSIGNED_SHORT, true, 0},
        {"a_color", 4, GL_UNSIGNED_BYTE, true, 0},
        {"a_extrude", 2, GL_SHORT, false, 0},
        {"a_screen_position", 2, GL_SHORT, false, 0},
        {"a_alpha", 1, GL_UNSIGNED_BYTE, true, 0},
        {"a_scale", 1, GL_UNSIGNED_BYTE, false, 0},
        {"a_rotation", 1, GL_SHORT, false, 0},
    }));
}

void PointStyle::constructShaderProgram() {

    std::string fragShaderSrcStr = stringFromFile("shaders/point.fs", PathType::internal);
    std::string vertShaderSrcStr = stringFromFile("shaders/point.vs", PathType::internal);

    m_shaderProgram->setSourceStrings(fragShaderSrcStr, vertShaderSrcStr);

    std::string defines;

    if (!m_spriteAtlas && !m_texture) {
        defines += "#define TANGRAM_POINT\n";
    }

    m_shaderProgram->addSourceBlock("defines", defines);

    m_mesh = std::make_unique<DynamicQuadMesh<SpriteVertex>>(m_vertexLayout, m_drawMode);
}

void PointStyle::onBeginUpdate() {
    m_mesh->clear();
}

void PointStyle::onBeginFrame() {
    // Upload meshes for next frame
    m_mesh->upload();
}

void PointStyle::onBeginDrawFrame(const View& view, Scene& scene) {
    Style::onBeginDrawFrame(view, scene);

    auto texUnit = RenderState::nextAvailableTextureUnit();

    if (m_spriteAtlas) {
        m_spriteAtlas->bind(texUnit);
    } else if (m_texture) {
        m_texture->update(texUnit);
        m_texture->bind(texUnit);
    }

    m_shaderProgram->setUniformi(m_uTex, texUnit);
    m_shaderProgram->setUniformMatrix4f(m_uOrtho, view.getOrthoViewportMatrix());

    m_mesh->draw(*m_shaderProgram);
}

struct PointStyleBuilder : public StyleBuilder {

    const PointStyle& m_style;

    std::vector<std::unique_ptr<Label>> m_labels;
    std::vector<SpriteQuad> m_quads;

    std::unique_ptr<SpriteLabels> m_spriteLabels;
    float m_zoom;

    void setup(const Tile& tile) override {

        m_zoom = tile.getID().z;
        m_spriteLabels = std::make_unique<SpriteLabels>(m_style);
    }

    bool checkRule(const DrawRule& rule) const override;

    void addPolygon(const Polygon& polygon, const Properties& props, const DrawRule& rule) override;
    void addLine(const Line& line, const Properties& props, const DrawRule& rule) override;
    void addPoint(const Point& line, const Properties& props, const DrawRule& rule) override;

    std::unique_ptr<StyledMesh> build() override {
        if (m_labels.empty()) { return nullptr; }

        m_spriteLabels->setLabels(m_labels);
        m_spriteLabels->setQuads(m_quads);
        m_labels.clear();
        m_quads.clear();
        return std::move(m_spriteLabels);
    };

    const Style& style() const override { return m_style; }

    PointStyleBuilder(const PointStyle& style) : StyleBuilder(style), m_style(style) {}

    bool getUVQuad(PointStyle::Parameters& params, glm::vec4& quad) const;

    PointStyle::Parameters applyRule(const DrawRule& rule, const Properties& props) const;

    void addLabel(const Point& point, const glm::vec4& quad,
                  const PointStyle::Parameters& params);

};

bool PointStyleBuilder::checkRule(const DrawRule& rule) const {
    uint32_t checkColor;
    // require a color or texture atlas/texture to be valid
    if (!rule.get(StyleParamKey::color, checkColor) &&
        !m_style.texture() &&
        !m_style.spriteAtlas()) {
        return false;
    }
    return true;
}

auto PointStyleBuilder::applyRule(const DrawRule& rule, const Properties& props) const -> PointStyle::Parameters {

    PointStyle::Parameters p;
    glm::vec2 size;
    std::string anchor;

    rule.get(StyleParamKey::color, p.color);
    rule.get(StyleParamKey::sprite, p.sprite);
    rule.get(StyleParamKey::offset, p.labelOptions.offset);
    rule.get(StyleParamKey::priority, p.labelOptions.priority);
    rule.get(StyleParamKey::sprite_default, p.spriteDefault);
    rule.get(StyleParamKey::centroid, p.centroid);
    rule.get(StyleParamKey::interactive, p.labelOptions.interactive);
    rule.get(StyleParamKey::collide, p.labelOptions.collide);
    rule.get(StyleParamKey::transition_hide_time, p.labelOptions.hideTransition.time);
    rule.get(StyleParamKey::transition_selected_time, p.labelOptions.selectTransition.time);
    rule.get(StyleParamKey::transition_show_time, p.labelOptions.showTransition.time);
    rule.get(StyleParamKey::anchor, anchor);

    auto sizeParam = rule.findParameter(StyleParamKey::size);
    if (sizeParam.stops && sizeParam.value.is<float>()) {
        float lowerSize = sizeParam.value.get<float>();
        float higherSize = sizeParam.stops->evalWidth(m_zoom + 1);
        p.extrudeScale = (higherSize - lowerSize) * 0.5f - 1.f;
        p.size = glm::vec2(lowerSize);
    } else if (rule.get(StyleParamKey::size, size)) {
        if (size.x == 0.f || std::isnan(size.y)) {
            p.size = glm::vec2(size.x);
        } else {
            p.size = size;
        }
    } else {
        p.size = glm::vec2(NAN, NAN);
    }

    LabelProperty::anchor(anchor, p.anchor);

    if (p.labelOptions.interactive) {
        p.labelOptions.properties = std::make_shared<Properties>(props);
    }

    std::hash<PointStyle::Parameters> hash;
    p.labelOptions.paramHash = hash(p);

    return p;
}

void PointStyleBuilder::addLabel(const Point& point, const glm::vec4& quad,
                                 const PointStyle::Parameters& params) {

    m_labels.push_back(std::make_unique<SpriteLabel>(Label::Transform{glm::vec2(point)},
                                                     params.size,
                                                     params.labelOptions,
                                                     params.extrudeScale,
                                                     params.anchor,
                                                     *m_spriteLabels,
                                                     m_quads.size()));

    glm::i16vec2 size = params.size * SpriteVertex::position_scale;

    // Attribute will be normalized - scale to max short;
    glm::vec2 uvTR = glm::vec2{quad.z, quad.w} * SpriteVertex::texture_scale;
    glm::vec2 uvBL = glm::vec2{quad.x, quad.y} * SpriteVertex::texture_scale;

    int16_t extrude = params.extrudeScale * SpriteVertex::extrusion_scale;

    m_quads.push_back({
            {{{0, 0},
             {uvBL.x, uvTR.y},
             {-extrude, extrude}},
            {{size.x, 0},
             {uvTR.x, uvTR.y},
             {extrude, extrude}},
            {{0, -size.y},
             {uvBL.x, uvBL.y},
             {-extrude, -extrude}},
            {{size.x, -size.y},
             {uvTR.x, uvBL.y},
             {extrude, -extrude}}},
            params.color});
}

bool PointStyleBuilder::getUVQuad(PointStyle::Parameters& params, glm::vec4& quad) const {
    quad = glm::vec4(0.0, 0.0, 1.0, 1.0);

    if (m_style.spriteAtlas()) {
        SpriteNode spriteNode;

        if (!m_style.spriteAtlas()->getSpriteNode(params.sprite, spriteNode) &&
            !m_style.spriteAtlas()->getSpriteNode(params.spriteDefault, spriteNode)) {
            return false;
        }

        if (std::isnan(params.size.x)) {
            params.size = spriteNode.m_size;
        }

        quad.x = spriteNode.m_uvBL.x;
        quad.y = spriteNode.m_uvBL.y;
        quad.z = spriteNode.m_uvTR.x;
        quad.w = spriteNode.m_uvTR.y;
    } else {
        // default point size
        if (std::isnan(params.size.x)) {
            params.size = glm::vec2(8.0);
        }
    }

    params.size *= m_style.pixelScale();

    return true;
}

void PointStyleBuilder::addPoint(const Point& point, const Properties& props,
                                 const DrawRule& rule) {

    PointStyle::Parameters p = applyRule(rule, props);
    glm::vec4 uvsQuad;

    if (!getUVQuad(p, uvsQuad)) {
        return;
    }

    addLabel(point, uvsQuad, p);
}

void PointStyleBuilder::addLine(const Line& line, const Properties& props,
                                const DrawRule& rule) {

    PointStyle::Parameters p = applyRule(rule, props);
    glm::vec4 uvsQuad;

    if (!getUVQuad(p, uvsQuad)) {
        return;
    }

    for (size_t i = 0; i < line.size(); ++i) {
        addLabel(line[i], uvsQuad, p);
    }
}

void PointStyleBuilder::addPolygon(const Polygon& polygon, const Properties& props,
                                   const DrawRule& rule) {

    PointStyle::Parameters p = applyRule(rule, props);
    glm::vec4 uvsQuad;

    if (!getUVQuad(p, uvsQuad)) {
        return;
    }

    if (!p.centroid) {
        for (auto line : polygon) {
            for (auto point : line) {
                addLabel(point, uvsQuad, p);
            }
        }
    } else {
        glm::vec2 c = centroid(polygon);

        addLabel(Point{c,0}, uvsQuad, p);
    }
}

std::unique_ptr<StyleBuilder> PointStyle::createBuilder() const {
    return std::make_unique<PointStyleBuilder>(*this);
}

}
