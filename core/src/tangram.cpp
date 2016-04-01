#include "tangram.h"

#include "platform.h"
#include "scene/scene.h"
#include "scene/sceneLoader.h"
#include "scene/skybox.h"
#include "style/material.h"
#include "style/style.h"
#include "labels/labels.h"
#include "tile/tileManager.h"
#include "tile/tile.h"
#include "gl/error.h"
#include "gl/shaderProgram.h"
#include "gl/renderState.h"
#include "gl/primitives.h"
#include "gl/hardware.h"
#include "util/inputHandler.h"
#include "tile/tileCache.h"
#include "view/view.h"
#include "data/clientGeoJsonSource.h"
#include "gl.h"
#include "gl/hardware.h"
#include "util/ease.h"
#include "debug/textDisplay.h"
#include <memory>
#include <array>
#include <cmath>
#include <bitset>
#include <mutex>
#include <queue>


namespace Tangram {

const static size_t MAX_WORKERS = 2;

std::mutex m_tilesMutex;
std::mutex m_tasksMutex;
std::queue<std::function<void()>> m_tasks;
std::unique_ptr<TileManager> m_tileManager;
std::unique_ptr<TileWorker> m_tileWorker;
std::shared_ptr<Scene> m_scene;
std::shared_ptr<View> m_view;
std::unique_ptr<Labels> m_labels;
std::unique_ptr<Skybox> m_skybox;
std::unique_ptr<InputHandler> m_inputHandler;

std::array<Ease, 4> m_eases;
enum class EaseField { position, zoom, rotation, tilt };
void setEase(EaseField f, Ease e) {
    m_eases[static_cast<size_t>(f)] = e;
    requestRender();
}
void clearEase(EaseField f) {
    static Ease none = {};
    m_eases[static_cast<size_t>(f)] = none;
}

static float g_time = 0.0;
static std::bitset<8> g_flags = 0;
float m_pixelsPerPoint = 1.f;
int log_level = 2;

static float lastUpdateTime = 0.0;

void initialize(const char* scenePath) {

    if (m_tileManager) {
        LOG("Notice: Already initialized");
        return;
    }

    LOG("initialize");

    // Create view
    m_view = std::make_shared<View>();

    // Create a scene object
    m_scene = std::make_shared<Scene>();

    // Input handler
    m_inputHandler = std::make_unique<InputHandler>(m_view);

    // Instantiate workers
    m_tileWorker = std::make_unique<TileWorker>(MAX_WORKERS);

    // Create a tileManager
    m_tileManager = std::make_unique<TileManager>(*m_tileWorker);

    // label setup
    m_labels = std::make_unique<Labels>();

    loadScene(scenePath, true);

    glm::dvec2 projPos = m_view->getMapProjection().LonLatToMeters(m_scene->startPosition);
    m_view->setPosition(projPos.x, projPos.y);
    m_view->setZoom(m_scene->startZoom);

    LOG("finish initialize");

}

void loadScene(const char* scenePath, bool setPositionFromScene) {
    LOG("Loading scene file: %s", scenePath);

    auto sceneString = stringFromFile(setResourceRoot(scenePath).c_str(), PathType::resource);

    bool setPositionFromCurrentView = bool(m_scene);

    auto scene = std::make_shared<Scene>();
    if (m_view) {
        scene->view() = std::make_shared<View>(*m_view);
    }
    if (SceneLoader::loadScene(sceneString, *scene)) {
        m_scene = scene;
        if (setPositionFromCurrentView && !setPositionFromScene) {
            m_scene->view()->setPosition(m_view->getPosition());
            m_scene->view()->setZoom(m_view->getZoom());
        }
        m_view = m_scene->view();
        m_inputHandler->setView(m_view);
        m_tileManager->setDataSources(scene->dataSources());
        setPixelScale(m_view->pixelScale());

        m_tileWorker->setScene(scene);
    }
}

void resize(int newWidth, int newHeight) {

    LOGS("resize: %d x %d", newWidth, newHeight);
    LOG("resize: %d x %d", newWidth, newHeight);

    glViewport(0, 0, newWidth, newHeight);

    if (m_view) {
        m_view->setSize(newWidth, newHeight);
    }

    Primitives::setResolution(newWidth, newHeight);

    while (Error::hadGlError("Tangram::resize()")) {}

}

void update(float dt) {

    clock_t start{0};
    if (Tangram::getDebugFlag(Tangram::DebugFlags::tangram_infos)) {
        start = clock();
    }

    g_time += dt;

    for (auto& ease : m_eases) {
        if (!ease.finished()) { ease.update(dt); }
    }

    m_inputHandler->update(dt);

    m_view->update();

    {
        std::lock_guard<std::mutex> lock(m_tasksMutex);

        while (!m_tasks.empty()) {
            m_tasks.front()();
            m_tasks.pop();
        }
    }

    bool animated = m_scene->animated() == Scene::animate::yes;
    for (const auto& style : m_scene->styles()) {
        style->onBeginUpdate();
        if (m_scene->animated() == Scene::animate::none) {
            animated |= style->isAnimated();
        }
    }

    if (animated != isContinuousRendering()) {
        setContinuousRendering(animated);
    }

    {
        std::lock_guard<std::mutex> lock(m_tilesMutex);
        ViewState viewState {
            m_view->getMapProjection(),
            m_view->changedOnLastUpdate(),
            glm::dvec2{m_view->getPosition().x, -m_view->getPosition().y },
            m_view->getZoom()
        };

        m_tileManager->updateTileSets(viewState, m_view->getVisibleTiles());

        bool updateLabels = m_labels->needUpdate();
        auto& tiles = m_tileManager->getVisibleTiles();

        if (m_view->changedOnLastUpdate() || m_tileManager->hasTileSetChanged()) {
            for (const auto& tile : tiles) {
                tile->update(dt, *m_view);
            }
            updateLabels = true;
        }

        if (updateLabels) {
            auto& cache = m_tileManager->getTileCache();
            m_labels->update(*m_view, dt, m_scene->styles(), tiles, cache);
        }
    }

    if (Tangram::getDebugFlag(Tangram::DebugFlags::tangram_infos)) {
        clock_t end = clock();
        lastUpdateTime = (float(end - start) / CLOCKS_PER_SEC) * 1000.f;
    }
}

void render() {
    clock_t start{0};

    if (Tangram::getDebugFlag(Tangram::DebugFlags::tangram_infos)) {
        start = clock();
    }

    // Set up openGL for new frame
    RenderState::depthWrite(GL_TRUE);
    auto& color = m_scene->background();
    RenderState::clearColor(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    for (const auto& style : m_scene->styles()) {
        style->onBeginFrame();
    }

    {
        std::lock_guard<std::mutex> lock(m_tilesMutex);

        // Loop over all styles
        for (const auto& style : m_scene->styles()) {

            style->onBeginDrawFrame(*m_view, *m_scene);

            // Loop over all tiles in m_tileSet
            for (const auto& tile : m_tileManager->getVisibleTiles()) {
                style->draw(*tile);
            }

            style->onEndDrawFrame();
        }
    }

    m_labels->drawDebug(*m_view);

    if (Tangram::getDebugFlag(Tangram::DebugFlags::tangram_infos)) {
        static int cpt = 0;

        clock_t endCpu = clock();
        static float timeCpu[60] = { 0 };
        static float timeUpdate[60] = { 0 };
        static float timeRender[60] = { 0 };
        timeCpu[cpt] = (float(endCpu - start) / CLOCKS_PER_SEC) * 1000.f;

        // Force opengl to finish commands (for accurate frame time)
        glFinish();

        clock_t end = clock();
        timeRender[cpt] = (float(end - start) / CLOCKS_PER_SEC) * 1000.f;

        if (++cpt == 60) { cpt = 0; }

        // Only compute average frame time every 60 frames
        float avgTimeRender = 0.f;
        float avgTimeCpu = 0.f;
        float avgTimeUpdate = 0.f;

        timeUpdate[cpt] = lastUpdateTime;

        for (int i = 0; i < 60; i++) {
            avgTimeRender += timeRender[i];
            avgTimeCpu += timeCpu[i];
            avgTimeUpdate += timeUpdate[i];
        }
        avgTimeRender /= 60;
        avgTimeCpu /= 60;
        avgTimeUpdate /= 60;

        size_t memused = 0;
        size_t dynamicmem = 0;
        for (const auto& tile : m_tileManager->getVisibleTiles()) {
            memused += tile->getMemoryUsage();
        }

        for (const auto& style: m_scene->styles()) {
            dynamicmem += style->dynamicMeshSize();
        }

        std::vector<std::string> debuginfos;

        debuginfos.push_back("visible tiles:"
                + std::to_string(m_tileManager->getVisibleTiles().size()));
        debuginfos.push_back("tile cache size:"
                + std::to_string(m_tileManager->getTileCache()->getMemoryUsage() / 1024) + "kb");
        debuginfos.push_back("buffer size:" + std::to_string(memused / 1024) + "kb");
        debuginfos.push_back("dynamic buffer size:" + std::to_string(dynamicmem / 1024) + "kb");
        debuginfos.push_back("avg frame cpu time:" + to_string_with_precision(avgTimeCpu, 2) + "ms");
        debuginfos.push_back("avg frame render time:" + to_string_with_precision(avgTimeRender, 2) + "ms");
        debuginfos.push_back("avg frame update time:" + to_string_with_precision(avgTimeUpdate, 2) + "ms");
        debuginfos.push_back("zoom:" + std::to_string(m_view->getZoom()));
        debuginfos.push_back("pos:" + std::to_string(m_view->getPosition().x) + "/"
                + std::to_string(m_view->getPosition().y));
        debuginfos.push_back("tilt:" + std::to_string(m_view->getPitch() * 57.3) + "deg");
        debuginfos.push_back("pixel scale:" + std::to_string(m_pixelsPerPoint));

        TextDisplay::Instance().draw(debuginfos);
    }

    while (Error::hadGlError("Tangram::render()")) {}
}

void setPositionNow(double lon, double lat) {

    glm::dvec2 meters = m_view->getMapProjection().LonLatToMeters({ lon, lat});
    m_view->setPosition(meters.x, meters.y);
    m_inputHandler->cancelFling();
    requestRender();

}

void setPosition(double lon, double lat) {

    setPositionNow(lon, lat);
    clearEase(EaseField::position);

}

void setPosition(double lon, double lat, float duration, EaseType e) {

    double lon_start, lat_start;
    getPosition(lon_start, lat_start);
    auto cb = [=](float t) { setPositionNow(ease(lon_start, lon, t, e), ease(lat_start, lat, t, e)); };
    setEase(EaseField::position, { duration, cb });

}

void getPosition(double& lon, double& lat) {

    glm::dvec2 meters(m_view->getPosition().x, m_view->getPosition().y);
    glm::dvec2 degrees = m_view->getMapProjection().MetersToLonLat(meters);
    lon = degrees.x;
    lat = degrees.y;

}

void setZoomNow(float z) {

    m_view->setZoom(z);
    m_inputHandler->cancelFling();
    requestRender();

}

void setZoom(float z) {

    setZoomNow(z);
    clearEase(EaseField::zoom);

}

void setZoom(float z, float duration, EaseType e) {

    float z_start = getZoom();
    auto cb = [=](float t) { setZoomNow(ease(z_start, z, t, e)); };
    setEase(EaseField::zoom, { duration, cb });

}

float getZoom() {

    return m_view->getZoom();

}

void setRotationNow(float radians) {

    m_view->setRoll(radians);
    requestRender();

}

void setRotation(float radians) {

    setRotationNow(radians);
    clearEase(EaseField::rotation);

}

void setRotation(float radians, float duration, EaseType e) {

    float radians_start = getRotation();

    // Ease over the smallest angular distance needed
    float radians_delta = glm::mod(radians - radians_start, (float)TWO_PI);
    if (radians_delta > PI) { radians_delta -= TWO_PI; }
    radians = radians_start + radians_delta;

    auto cb = [=](float t) { setRotationNow(ease(radians_start, radians, t, e)); };
    setEase(EaseField::rotation, { duration, cb });

}

float getRotation() {

    return m_view->getRoll();

}


void setTiltNow(float radians) {

    m_view->setPitch(radians);
    requestRender();

}

void setTilt(float radians) {

    setTiltNow(radians);
    clearEase(EaseField::tilt);

}

void setTilt(float radians, float duration, EaseType e) {

    float tilt_start = getTilt();
    auto cb = [=](float t) { setTiltNow(ease(tilt_start, radians, t, e)); };
    setEase(EaseField::tilt, { duration, cb });

}

float getTilt() {

    return m_view->getPitch();

}

void screenToWorldCoordinates(double& x, double& y) {

    m_view->screenToGroundPlane(x, y);
    glm::dvec2 meters(x + m_view->getPosition().x, y + m_view->getPosition().y);
    glm::dvec2 lonLat = m_view->getMapProjection().MetersToLonLat(meters);
    x = lonLat.x;
    y = lonLat.y;

}

void setPixelScale(float pixelsPerPoint) {

    m_pixelsPerPoint = pixelsPerPoint;

    if (m_view) {
        m_view->setPixelScale(pixelsPerPoint);
    }

    for (auto& style : m_scene->styles()) {
        style->setPixelScale(pixelsPerPoint);
    }
}

void setCameraType(uint8_t cameraType) {

    if (m_view) {
        m_view->setCameraType(static_cast<CameraType>(cameraType));
    }
}

uint8_t getCameraType() {
    return static_cast<uint8_t>(m_view->cameraType());
}

void addDataSource(std::shared_ptr<DataSource> source) {
    if (!m_tileManager) { return; }
    std::lock_guard<std::mutex> lock(m_tilesMutex);

    m_tileManager->addDataSource(source);
}

bool removeDataSource(DataSource& source) {
    if (!m_tileManager) { return false; }
    std::lock_guard<std::mutex> lock(m_tilesMutex);

    return m_tileManager->removeDataSource(source);
}

void clearDataSource(DataSource& source, bool data, bool tiles) {
    if (!m_tileManager) { return; }
    std::lock_guard<std::mutex> lock(m_tilesMutex);

    if (tiles) { m_tileManager->clearTileSet(source.id()); }
    if (data) { source.clearData(); }

    requestRender();
}

void handleTapGesture(float posX, float posY) {

    m_inputHandler->handleTapGesture(posX, posY);

}

void handleDoubleTapGesture(float posX, float posY) {

    m_inputHandler->handleDoubleTapGesture(posX, posY);

}

void handlePanGesture(float startX, float startY, float endX, float endY) {

    m_inputHandler->handlePanGesture(startX, startY, endX, endY);

}

void handleFlingGesture(float posX, float posY, float velocityX, float velocityY) {

    m_inputHandler->handleFlingGesture(posX, posY, velocityX, velocityY);

}

void handlePinchGesture(float posX, float posY, float scale, float velocity) {

    m_inputHandler->handlePinchGesture(posX, posY, scale, velocity);

}

void handleRotateGesture(float posX, float posY, float radians) {

    m_inputHandler->handleRotateGesture(posX, posY, radians);

}

void handleShoveGesture(float distance) {

    m_inputHandler->handleShoveGesture(distance);

}

void setDebugFlag(DebugFlags flag, bool on) {

    g_flags.set(flag, on);
    m_view->setZoom(m_view->getZoom()); // Force the view to refresh

}

bool getDebugFlag(DebugFlags flag) {

    return g_flags.test(flag);

}

void toggleDebugFlag(DebugFlags flag) {

    g_flags.flip(flag);
    m_view->setZoom(m_view->getZoom()); // Force the view to refresh

    // Rebuild tiles for debug modes that needs it
    if (flag == DebugFlags::proxy_colors
     || flag == DebugFlags::tile_bounds
     || flag == DebugFlags::tile_infos) {
        if (m_tileManager) {
            std::lock_guard<std::mutex> lock(m_tilesMutex);
            m_tileManager->clearTileSets();
        }
    }
}

const std::vector<TouchItem>& pickFeaturesAt(float x, float y) {
    return m_labels->getFeaturesAtPoint(*m_view, 0, m_scene->styles(),
                                        m_tileManager->getVisibleTiles(),
                                        x, y);
}



void setupGL() {

    LOG("setup GL");

    if (m_tileManager) {
        m_tileManager->clearTileSets();
    }

    // Reconfigure the render states. Increases context 'generation'.
    // The OpenGL context has been destroyed since the last time resources were
    // created, so we invalidate all data that depends on OpenGL object handles.
    RenderState::configure();

    // Set default primitive render color
    Primitives::setColor(0xffffff);

    // Load GL extensions and capabilities
    Hardware::loadExtensions();
    Hardware::loadCapabilities();

    Hardware::printAvailableExtensions();

    while (Error::hadGlError("Tangram::setupGL()")) {}
}

void runOnMainLoop(std::function<void()> task) {
    std::lock_guard<std::mutex> lock(m_tasksMutex);
    m_tasks.emplace(std::move(task));
}

float frameTime() {
    return g_time;
}

}
