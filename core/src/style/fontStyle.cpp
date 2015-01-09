#include "fontStyle.h"
#define GLFONTSTASH_IMPLEMENTATION
#include "fontstash/glfontstash.h"

FontStyle::FontStyle(const std::string& _fontFile, std::string _name, float _fontSize, GLenum _drawMode)
: Style(_name, _drawMode), m_fontSize(_fontSize) {

    constructVertexLayout();
    constructShaderProgram();
    initFontContext(_fontFile);
}

FontStyle::~FontStyle() {
    glfonsDelete(m_fontContext->m_fsContext);
    for (auto pair : m_tileTexTransforms) {
        glDeleteTextures(1, &pair.second);
    }
    glDeleteTextures(1, &m_atlas);
}

void FontStyle::constructVertexLayout() {
    m_vertexLayout = std::shared_ptr<VertexLayout>(new VertexLayout({
        {"a_position", 2, GL_FLOAT, false, 0},
        {"a_texCoord", 2, GL_FLOAT, false, 0},
        {"a_fsid", 1, GL_FLOAT, false, 0},
    }));
}

void FontStyle::constructShaderProgram() {
    std::string vertShaderSrcStr = stringFromResource("text.vs");
    std::string fragShaderSrcStr = stringFromResource("text.fs");

    m_shaderProgram = std::make_shared<ShaderProgram>();

    if (!m_shaderProgram->buildFromSourceStrings(fragShaderSrcStr, vertShaderSrcStr)) {
        logMsg("[FontStyle] Error building text shader program\n");
    }
}

void FontStyle::buildPoint(Point& _point, std::string& _layer, Properties& _props, VboMesh& _mesh) {

}

void FontStyle::buildLine(Line& _line, std::string& _layer, Properties& _props, VboMesh& _mesh) {
    std::vector<float> vertData;
    int nVerts = 0;

    fonsSetSize(m_fontContext->m_fsContext, m_fontSize * m_pixelScale);
    fonsSetFont(m_fontContext->m_fsContext, m_font);

    if (_layer.compare("roads") == 0) {
        for (auto prop : _props.stringProps) {
            if (prop.first.compare("name") == 0 && _line.size() == 2) { // don't treat polylines

                glm::dvec2 p1 = glm::dvec2(_line[0]);
                glm::dvec2 p2 = glm::dvec2(_line[1]);
                glm::dvec2 p1p2 = p1 - p2;

                if(glm::length(p1p2) < 0.5) { // some random label discard for now
                    return;
                }

                p1p2 = glm::normalize(p1p2);
                float r = (float) atan2(p1p2.x, p1p2.y) + M_PI_2;

                double offset = 1;
                if (r > M_PI_2 || r < -M_PI_2) {
                    r += M_PI;
                    offset = -1;
                }

                fsuint textId;
                glfonsGenText(m_fontContext->m_fsContext, 1, &textId);

                // place the text in the middle and give more strength to p1 since text has a width
                glm::dvec2 position = (p1 + p2) / 2.0 + p1p2 * glm::length(p1p2) * 0.2 * offset;

                // FUTURE: label logic
                // 1. project label on screen and compute its bbox, to do so we need the view and label model matrix
                // 2. ask any kind of label manager to perform label discards

                std::unique_ptr<Label> label(new Label {
                    m_fontContext,
                    textId,
                    prop.second.c_str(),
                    position,   // world position
                    1.0,        // alpha
                    r           // rotation
                });

                if (m_processedTile->addLabel(*this, std::move(label))) { // could potentially refuse to add label

                    logMsg("[FontStyle] Rasterize label: %s, angle: %f\n", prop.second.c_str(), r * (180/M_PI));

                    glfonsRasterize(m_fontContext->m_fsContext, textId, prop.second.c_str(), FONS_EFFECT_NONE);
                }
            }
        }
    }

    fonsClearState(m_fontContext->m_fsContext);

    if (glfonsVertices(m_fontContext->m_fsContext, &vertData, &nVerts)) {
        _mesh.addVertices((GLbyte*)vertData.data(), nVerts);
    }
}

void FontStyle::buildPolygon(Polygon& _polygon, std::string& _layer, Properties& _props, VboMesh& _mesh) {

}

void FontStyle::prepareDataProcessing(MapTile& _tile) {
    m_fontContext->m_contextMutex->lock();

    m_processedTile = &_tile;

    fsuint buffer;
    
    glfonsBufferCreate(m_fontContext->m_fsContext, 32, &buffer);
    _tile.setTextBuffer(*this, buffer);
    glfonsBindBuffer(m_fontContext->m_fsContext, buffer);
}

void FontStyle::finishDataProcessing(MapTile& _tile) {
    glfonsBindBuffer(m_fontContext->m_fsContext, 0);

    m_processedTile = nullptr;

    m_fontContext->m_contextMutex->unlock();
}

GLuint FontStyle::textureTransformName(const TileID _tileId) const {

    auto it = m_tileTexTransforms.find(_tileId);

    if (it != m_tileTexTransforms.end()) {
        return it->second;
    }

    return 0; // non-valid texture name
}

void FontStyle::setupForTile(const MapTile& _tile) {

    GLuint textureName = textureTransformName(_tile.getID());

    if(textureName != 0) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, textureName);

        m_shaderProgram->setUniformi("u_transforms", 1); // transform texture
    }
}

void FontStyle::processTileTransformCreation() {

    while (m_pendingTileTexTransforms.size() > 0) {

        auto pair = m_pendingTileTexTransforms.front();

        glm::vec2 size = pair.second;

        auto defaultTransforms = new unsigned int[(int)(size.x * size.y)] {0}; // zero filled

        logMsg("[FontStyle] Create texture transform %d x %d\n", (int)size.x, (int)size.y);

        GLuint texTransform;
        glGenTextures(1, &texTransform);
        glBindTexture(GL_TEXTURE_2D, texTransform);

        // fill texture with 0
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, defaultTransforms);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        delete[] defaultTransforms;

        m_tileTexTransforms[pair.first] = texTransform;

        m_pendingTileTexTransforms.pop();
    }

}

void FontStyle::processTileTransformUpdate() {

    while (m_pendingTexTransformsData.size() > 0) {
        TileID id = m_pendingTexTransformsData.front().m_id;
        TextureData data = m_pendingTexTransformsData.front().m_data;

        glBindTexture(GL_TEXTURE_2D, m_tileTexTransforms[id]);

        logMsg("[FontStyle] Update texture transform %d x %d\n", data.m_width, data.m_height);

        glTexSubImage2D(GL_TEXTURE_2D, 0, data.m_xoff, data.m_yoff, data.m_width, data.m_height,
                        GL_RGBA, GL_UNSIGNED_BYTE, data.m_pixels);

        glBindTexture(GL_TEXTURE_2D, 0);

        m_pendingTexTransformsData.pop();
    }

}

void FontStyle::processAtlasUpdate() {

    glBindTexture(GL_TEXTURE_2D, m_atlas);
    while (m_pendingTexAtlasData.size() > 0) {
        TextureData data = m_pendingTexAtlasData.front().m_data;

        logMsg("[FontStyle] Update atlas texture %d x %d\n", data.m_width, data.m_height);

        glTexSubImage2D(GL_TEXTURE_2D, 0, data.m_xoff, data.m_yoff, data.m_width, data.m_height,
                        GL_ALPHA, GL_UNSIGNED_BYTE, data.m_pixels);

        m_pendingTexAtlasData.pop();
    }
    glBindTexture(GL_TEXTURE_2D, 0);

}

void FontStyle::setup(View& _view) {
    float projectionMatrix[16] = {0};

    // process pending opengl texture updates / creation
    processAtlasUpdate();
    processTileTransformCreation();
    processTileTransformUpdate();

    glfonsProjection(m_fontContext->m_fsContext, projectionMatrix);

    // activate the atlas on the texture slot0, the texture transform is on slot1
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_atlas);

    m_shaderProgram->setUniformi("u_tex", 0); // atlas
    m_shaderProgram->setUniformf("u_tresolution", 32, 64); // resolution of transform texture
    m_shaderProgram->setUniformf("u_resolution", _view.getWidth(), _view.getHeight());
    m_shaderProgram->setUniformf("u_color", 1.0, 1.0, 1.0);
    m_shaderProgram->setUniformMatrix4f("u_proj", projectionMatrix);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

void FontStyle::teardown() {
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void createTexTransforms(void* _userPtr, unsigned int _width, unsigned int _height) {
    FontStyle* fontStyle = static_cast<FontStyle*>(_userPtr);

    glm::vec2 size = glm::vec2(_width, _height);

    fontStyle->m_pendingTileTexTransforms.push({
        fontStyle->m_processedTile->getID(), size
    });
}

void updateTransforms(void* _userPtr, unsigned int _xoff, unsigned int _yoff, unsigned int _width,
                      unsigned int _height, const unsigned int* _pixels, void* _ownerPtr) {

    FontStyle* fontStyle = static_cast<FontStyle*>(_userPtr);
    MapTile* tile = static_cast<MapTile*>(_ownerPtr);

    fontStyle->m_pendingTexTransformsData.push({
        tile->getID(),
        { _pixels, _xoff, _yoff, _width, _height }
    });
}

void updateAtlas(void* _userPtr, unsigned int _xoff, unsigned int _yoff,
                 unsigned int _width, unsigned int _height, const unsigned int* _pixels) {

    FontStyle* fontStyle = static_cast<FontStyle*>(_userPtr);

    fontStyle->m_pendingTexAtlasData.push({
        { _pixels, _xoff, _yoff, _width, _height }
    });
}

void createAtlas(void* _userPtr, unsigned int _width, unsigned int _height) {
    logMsg("[FontStyle] Create atlas");
    FontStyle* fontStyle = static_cast<FontStyle*>(_userPtr);

    glGenTextures(1, &fontStyle->m_atlas);
    glBindTexture(GL_TEXTURE_2D, fontStyle->m_atlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, _width, _height, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void errorCallback(void* _userPtr, fsuint buffer, GLFONSError error) {
    switch (error) {
        case GLFONSError::ID_OVERFLOW:
            logMsg("[FontStyle] FontError : ID_OVERFLOW in text buffer %d\n", buffer);
            break;

        default:
            logMsg("[FontStyle] FontError : undefined error\n");
            break;
    }
}

void FontStyle::initFontContext(const std::string& _fontFile) {

    int atlasWidth = 512;
    int atlasHeight = 512;

    GLFONSparams params;

    params.errorCallback = errorCallback;
    params.createAtlas = createAtlas;
    params.createTexTransforms = createTexTransforms;
    params.updateAtlas = updateAtlas;
    params.updateTransforms = updateTransforms;

    FONScontext* context = glfonsCreate(atlasWidth, atlasHeight, FONS_ZERO_TOPLEFT, params, (void*) this);

    unsigned int dataSize;
    unsigned char* data = bytesFromResource(_fontFile.c_str(), &dataSize);
    m_font = fonsAddFont(context, "droid-serif", data, dataSize);

    if (m_font == FONS_INVALID) {
        logMsg("[FontStyle] Error loading font file %s\n", _fontFile.c_str());
    }

    m_fontContext = std::shared_ptr<FontContext>(new FontContext {
        std_patch::make_unique<std::mutex>(), context
    });

}
