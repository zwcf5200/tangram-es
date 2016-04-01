#include "polylineStyle.h"

#include "tangram.h"
#include "platform.h"
#include "material.h"
#include "gl/shaderProgram.h"
#include "gl/mesh.h"
#include "scene/stops.h"
#include "scene/drawRule.h"
#include "tile/tile.h"
#include "util/builders.h"
#include "util/mapProjection.h"
#include "util/extrude.h"

#include "glm/vec3.hpp"
#include "glm/gtc/type_precision.hpp"

constexpr float extrusion_scale = 4096.0f;
constexpr float position_scale = 8192.0f;
constexpr float texture_scale = 65535.0f;
constexpr float order_scale = 2.0f;

namespace Tangram {

struct PolylineVertexNoUVs {
    PolylineVertexNoUVs(glm::vec2 position, glm::vec2 extrude, glm::vec2 uv,
                   glm::i16vec2 width, glm::i16vec2 height, GLuint abgr)
        : pos(glm::i16vec2{ glm::round(position * position_scale)}, height),
          extrude(glm::i16vec2{extrude * extrusion_scale}, width),
          abgr(abgr) {}

    PolylineVertexNoUVs(PolylineVertexNoUVs v, short order, glm::i16vec2 width, GLuint abgr)
        : pos(glm::i16vec4{glm::i16vec3{v.pos}, order}),
          extrude(glm::i16vec4{ v.extrude.x, v.extrude.y, width }),
          abgr(abgr) {}

    glm::i16vec4 pos;
    glm::i16vec4 extrude;
    GLuint abgr;
};

struct PolylineVertex : PolylineVertexNoUVs {
    PolylineVertex(glm::vec2 position, glm::vec2 extrude, glm::vec2 uv,
                   glm::i16vec2 width, glm::i16vec2 height, GLuint abgr)
        : PolylineVertexNoUVs(position, extrude, uv, width, height, abgr),
          texcoord(uv * texture_scale) {}

    PolylineVertex(PolylineVertex v, short order, glm::i16vec2 width, GLuint abgr)
        : PolylineVertexNoUVs(v, order, width, abgr),
          texcoord(v.texcoord) {}

    glm::u16vec2 texcoord;
};

PolylineStyle::PolylineStyle(std::string name, Blending blendMode, GLenum drawMode)
    : Style(name, blendMode, drawMode)
{}

void PolylineStyle::constructVertexLayout() {

    // TODO: Ideally this would be in the same location as the struct that it basically describes
    if (m_texCoordsGeneration) {
        m_vertexLayout = std::shared_ptr<VertexLayout>(new VertexLayout({
            {"a_position", 4, GL_SHORT, false, 0},
            {"a_extrude", 4, GL_SHORT, false, 0},
            {"a_color", 4, GL_UNSIGNED_BYTE, true, 0},
            {"a_texcoord", 2, GL_UNSIGNED_SHORT, true, 0},
        }));
    } else {
        m_vertexLayout = std::shared_ptr<VertexLayout>(new VertexLayout({
            {"a_position", 4, GL_SHORT, false, 0},
            {"a_extrude", 4, GL_SHORT, false, 0},
            {"a_color", 4, GL_UNSIGNED_BYTE, true, 0},
        }));
    }

}

void PolylineStyle::constructShaderProgram() {

    std::string vertShaderSrcStr = stringFromFile("shaders/polyline.vs", PathType::internal);
    std::string fragShaderSrcStr = stringFromFile("shaders/polyline.fs", PathType::internal);

    m_shaderProgram->setSourceStrings(fragShaderSrcStr, vertShaderSrcStr);

    if (m_texCoordsGeneration) {
        m_shaderProgram->addSourceBlock("defines", "#define TANGRAM_USE_TEX_COORDS\n");
    }
}

template <class V>
struct PolylineStyleBuilder : public StyleBuilder {

public:

    struct Parameters {

        struct Attributes {
            // Values prepared for the currently build mesh
            glm::i16vec2 height;
            glm::i16vec2 width;
            uint32_t color;
            float miterLimit = 3.0;
            CapTypes cap = CapTypes::butt;
            JoinTypes join = JoinTypes::miter;

            void set(float w, float dWdZ, float h, float order) {
                height = { glm::round(h * position_scale), order * order_scale};
                width = glm::vec2{w, dWdZ} * extrusion_scale;
            }
        } fill, stroke;

        bool keepTileEdges = false;
        bool outlineOn = false;
        bool lineOn = true;
    };

    void setup(const Tile& tile) override;

    const Style& style() const override { return m_style; }

    void addFeature(const Feature& feat, const DrawRule& rule) override;

    std::unique_ptr<StyledMesh> build() override;

    PolylineStyleBuilder(const PolylineStyle& style)
        : StyleBuilder(style), m_style(style),
          m_meshData(2) {}

    void addMesh(const Line& line, const Parameters& params);

    void buildLine(const Line& line, const typename Parameters::Attributes& att,
                   MeshData<V>& mesh);

    Parameters parseRule(const DrawRule& rule, const Properties& props);

    bool evalWidth(const StyleParam& styleParam, float& width, float& slope);

    PolyLineBuilder& polylineBuilder() { return m_builder; }

private:

    const PolylineStyle& m_style;
    PolyLineBuilder m_builder;

    std::vector<MeshData<V>> m_meshData;

    float m_tileUnitsPerMeter;
    float m_tileSizePixels;
    int m_zoom;
};

template <class V>
void PolylineStyleBuilder<V>::setup(const Tile& tile) {

    const auto& id = tile.getID();

    // Use the 'style zoom' to evaluate style parameters.
    m_zoom = id.s;
    m_tileUnitsPerMeter = tile.getInverseScale();
    m_tileSizePixels = tile.getProjection()->TileSize();

    // When a tile is overzoomed, we are actually styling the area of its
    // 'source' tile, which will have a larger effective pixel size at the
    // 'style' zoom level.
    m_tileSizePixels *= std::exp2(id.s - id.z);
}

template <class V>
std::unique_ptr<StyledMesh> PolylineStyleBuilder<V>::build() {
    if (m_meshData[0].vertices.empty() &&
        m_meshData[1].vertices.empty()) {
        return nullptr;
    }

    auto mesh = std::make_unique<Mesh<V>>(m_style.vertexLayout(), m_style.drawMode());

    bool painterMode = (m_style.blendMode() == Blending::overlay ||
                        m_style.blendMode() == Blending::inlay);

    // Swap draw order to draw outline first when not using depth testing
    if (painterMode) { std::swap(m_meshData[0], m_meshData[1]); }

    mesh->compile(m_meshData);

    // Swapping back since fill mesh may have more vertices than outline
    if (painterMode) { std::swap(m_meshData[0], m_meshData[1]); }

    m_meshData[0].clear();
    m_meshData[1].clear();
    return std::move(mesh);
}

template <class V>
auto PolylineStyleBuilder<V>::parseRule(const DrawRule& rule, const Properties& props) -> Parameters {
    Parameters p;

    uint32_t cap = 0, join = 0;

    struct {
        uint32_t order = 0;
        uint32_t color = 0xff00ffff;
        float width = 0.f;
        float slope = 0.f;
    } fill, stroke;

    auto& width = rule.findParameter(StyleParamKey::width);
    if (!evalWidth(width, fill.width, fill.slope)) {
        fill.width = 0;
        return p;
    }
    fill.slope -= fill.width;
    rule.get(StyleParamKey::color, p.fill.color);
    rule.get(StyleParamKey::cap, cap);
    rule.get(StyleParamKey::join, join);
    rule.get(StyleParamKey::order, fill.order);
    rule.get(StyleParamKey::tile_edges, p.keepTileEdges);
    rule.get(StyleParamKey::miter_limit, p.fill.miterLimit);

    p.fill.cap = static_cast<CapTypes>(cap);
    p.fill.join = static_cast<JoinTypes>(join);

    glm::vec2 extrude = glm::vec2(0);
    rule.get(StyleParamKey::extrude, extrude);

    float height = getUpperExtrudeMeters(extrude, props);
    height *= m_tileUnitsPerMeter;

    p.fill.set(fill.width, fill.slope, height, fill.order);
    p.lineOn = !rule.isOutlineOnly;

    stroke.order = fill.order;
    p.stroke.cap = p.fill.cap;
    p.stroke.join = p.fill.join;
    p.stroke.miterLimit = p.fill.miterLimit;

    auto& strokeWidth = rule.findParameter(StyleParamKey::outline_width);
    if (!p.lineOn || !rule.findParameter(StyleParamKey::outline_style)) {
        if (strokeWidth |
            rule.get(StyleParamKey::outline_order, stroke.order) |
            rule.get(StyleParamKey::outline_cap, cap) |
            rule.get(StyleParamKey::outline_join, join) |
            rule.get(StyleParamKey::outline_miter_limit, p.stroke.miterLimit)) {

            p.stroke.cap = static_cast<CapTypes>(cap);
            p.stroke.join = static_cast<JoinTypes>(join);

            if (!rule.get(StyleParamKey::outline_color, p.stroke.color)) { return p; }
            if (!evalWidth(strokeWidth, stroke.width, stroke.slope)) {
                return p;
            }

            // NB: Multiply by 2 for the stroke to get the expected stroke pixel width.
            stroke.width *= 2.0f;
            stroke.slope *= 2.0f;
            stroke.slope -= stroke.width;

            stroke.width += fill.width;
            stroke.slope += fill.slope;

            stroke.order = std::min(stroke.order, fill.order);

            p.stroke.set(stroke.width, stroke.slope,
                    height, stroke.order - 0.5f);

            p.outlineOn = true;
        }
    }

    if (Tangram::getDebugFlag(Tangram::DebugFlags::proxy_colors)) {
        fill.color <<= (m_zoom % 6);
        stroke.color <<= (m_zoom % 6);
    }

    return p;
}

double widthMeterToPixel(int zoom, double tileSize, double width) {
    // pixel per meter at z == 0
    double meterRes = tileSize / (2.0 * MapProjection::HALF_CIRCUMFERENCE);
    // pixel per meter at zoom
    meterRes *= exp2(zoom);

    return width * meterRes;
}

template <class V>
bool PolylineStyleBuilder<V>::evalWidth(const StyleParam& styleParam, float& width, float& slope) {

    // NB: 0.5 because 'width' will be extruded in both directions
    float tileRes = 0.5 / m_tileSizePixels;

    // auto& styleParam = rule.findParameter(key);
    if (styleParam.stops) {

        width = styleParam.value.get<float>();
        width *= tileRes;

        slope = styleParam.stops->evalWidth(m_zoom + 1);
        slope *= tileRes;
        return true;
    }

    if (styleParam.value.is<StyleParam::Width>()) {
        auto& widthParam = styleParam.value.get<StyleParam::Width>();

        width = widthParam.value;

        if (widthParam.isMeter()) {
            width = widthMeterToPixel(m_zoom, m_tileSizePixels, width);
            width *= tileRes;
            slope = width * 2;
        } else {
            width *= tileRes;
            slope = width;
        }
        return true;
    }

    LOGD("Invalid type for Width '%d'", styleParam.value.which());
    return false;
}

template <class V>
void PolylineStyleBuilder<V>::addFeature(const Feature& feat, const DrawRule& rule) {

    if (feat.geometryType == GeometryType::points) { return; }
    if (!checkRule(rule)) { return; }

    Parameters params = parseRule(rule, feat.props);

    if (params.fill.width[0] <= 0.0f && params.fill.width[1] <= 0.0f ) { return; }

    if (feat.geometryType == GeometryType::lines) {
        // Line geometries are never clipped to tiles, so keep all segments
        params.keepTileEdges = true;

        for (auto& line : feat.lines) {
            addMesh(line, params);
        }
    } else {
        for (auto& polygon : feat.polygons) {
            for (const auto& line : polygon) {
                addMesh(line, params);
            }
        }
    }
}

template <class V>
void PolylineStyleBuilder<V>::buildLine(const Line& line, const typename Parameters::Attributes& att,
                        MeshData<V>& mesh) {

    m_builder.addVertex = [&mesh, &att](const glm::vec3& coord,
                                   const glm::vec2& normal,
                                   const glm::vec2& uv) {
        mesh.vertices.push_back({{ coord.x,coord.y }, normal, uv,
                              att.width, att.height, att.color});
    };

    Builders::buildPolyLine(line, m_builder);

    mesh.indices.insert(mesh.indices.end(),
                         m_builder.indices.begin(),
                         m_builder.indices.end());

    mesh.offsets.emplace_back(m_builder.indices.size(),
                               m_builder.numVertices);

    m_builder.clear();
}

template <class V>
void PolylineStyleBuilder<V>::addMesh(const Line& line, const Parameters& params) {

    m_builder.cap = params.fill.cap;
    m_builder.join = params.fill.join;
    m_builder.miterLimit = params.fill.miterLimit;
    m_builder.keepTileEdges = params.keepTileEdges;

    if (params.lineOn) { buildLine(line, params.fill, m_meshData[0]); }

    if (!params.outlineOn) { return; }

    if (!params.lineOn ||
        params.stroke.cap != params.fill.cap ||
        params.stroke.join != params.fill.join ||
        params.stroke.miterLimit != params.fill.miterLimit) {
        // need to re-triangulate with different cap and/or join
        m_builder.cap = params.stroke.cap;
        m_builder.join = params.stroke.join;
        m_builder.miterLimit = params.stroke.miterLimit;

        buildLine(line, params.stroke, m_meshData[1]);

    } else {
        auto& fill = m_meshData[0];
        auto& stroke = m_meshData[1];

        // reuse indices from original line, overriding color and width
        size_t nIndices = fill.offsets.back().first;
        size_t nVertices = fill.offsets.back().second;
        stroke.offsets.emplace_back(nIndices, nVertices);

        auto indicesIt = fill.indices.end() - nIndices;
        stroke.indices.insert(stroke.indices.end(),
                                 indicesIt,
                                 fill.indices.end());

        auto vertexIt = fill.vertices.end() - nVertices;

        glm::vec2 width = params.stroke.width;
        GLuint abgr = params.stroke.color;
        short order = params.stroke.height[1];

        for (; vertexIt != fill.vertices.end(); ++vertexIt) {
            stroke.vertices.emplace_back(*vertexIt, order, width, abgr);
        }
    }
}

std::unique_ptr<StyleBuilder> PolylineStyle::createBuilder() const {
    if (m_texCoordsGeneration) {
        auto builder = std::make_unique<PolylineStyleBuilder<PolylineVertex>>(*this);
        builder->polylineBuilder().useTexCoords = true;
        return std::move(builder);
    } else {
        auto builder = std::make_unique<PolylineStyleBuilder<PolylineVertexNoUVs>>(*this);
        builder->polylineBuilder().useTexCoords = false;
        return std::move(builder);
    }
}

}
