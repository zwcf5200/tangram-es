#include "clientGeoJsonSource.h"
#define GEOJSONVT_CUSTOM_TAGS
#include "mapbox/geojsonvt/geojsonvt_types.hpp"
#include "mapbox/geojsonvt/geojsonvt.hpp"
#include "mapbox/geojsonvt/geojsonvt_convert.hpp"
#include "platform.h"
#include "tile/tileTask.h"
#include "util/geom.h"
#include "data/propertyItem.h"
#include "data/tileData.h"
#include "tile/tile.h"
#include "view/view.h"

using namespace mapbox::util;

namespace Tangram {

const double extent = 4096;
const uint32_t indexMaxPoints = 100000;
double tolerance = 1E-8;

std::shared_ptr<TileTask> ClientGeoJsonSource::createTask(TileID tileId) {
    return std::make_shared<TileTask>(tileId, shared_from_this());
}

// Transform a geojsonvt::TilePoint into the corresponding Tangram::Point
Point transformPoint(geojsonvt::TilePoint pt) {
    return { pt.x / extent, 1. - pt.y / extent, 0 };
}

ClientGeoJsonSource::ClientGeoJsonSource(const std::string& name, const std::string& url, int32_t maxZoom)
    : DataSource(name, url, maxZoom) {

    if (!url.empty()) {
        // Load from file
        const auto& string = stringFromFile(url.c_str(), PathType::resource);
        addData(string);
    }
}

ClientGeoJsonSource::~ClientGeoJsonSource() {}

void ClientGeoJsonSource::addData(const std::string& data) {

    auto features = geojsonvt::GeoJSONVT::convertFeatures(data);

    for (auto& f : features) {
        m_features.push_back(std::move(f));
    }

    std::lock_guard<std::mutex> lock(m_mutexStore);
    m_store = std::make_unique<GeoJSONVT>(m_features, m_maxZoom, m_maxZoom, indexMaxPoints, tolerance);
    m_generation++;

}

bool ClientGeoJsonSource::loadTileData(std::shared_ptr<TileTask>&& task, TileTaskCb cb) {

    cb.func(std::move(task));

    return true;
}

void ClientGeoJsonSource::clearData() {

    m_features.clear();

    std::lock_guard<std::mutex> lock(m_mutexStore);
    m_store.reset();
    m_generation++;
}

void ClientGeoJsonSource::addPoint(const Properties& props, LngLat point) {

    auto container = geojsonvt::Convert::project({ geojsonvt::LonLat(point.longitude, point.latitude) }, tolerance);

    geojsonvt::Tags tags;

    auto feature = geojsonvt::Convert::create(geojsonvt::Tags{std::make_shared<Properties>(props)},
                                              geojsonvt::ProjectedFeatureType::Point,
                                              container.members);

    m_features.push_back(std::move(feature));

    std::lock_guard<std::mutex> lock(m_mutexStore);
    m_store = std::make_unique<GeoJSONVT>(m_features, m_maxZoom, m_maxZoom, indexMaxPoints, tolerance);
    m_generation++;
}

void ClientGeoJsonSource::addLine(const Properties& props, const Coordinates& line) {
    auto& gjvtline = reinterpret_cast<const std::vector<geojsonvt::LonLat>&>(line);

    std::vector<geojsonvt::ProjectedGeometry> geometry = { geojsonvt::Convert::project(gjvtline, tolerance) };

    auto feature = geojsonvt::Convert::create(geojsonvt::Tags{std::make_shared<Properties>(props)},
                                              geojsonvt::ProjectedFeatureType::LineString,
                                              geometry);

    m_features.push_back(std::move(feature));

    std::lock_guard<std::mutex> lock(m_mutexStore);
    m_store = std::make_unique<GeoJSONVT>(m_features, m_maxZoom, m_maxZoom, indexMaxPoints, tolerance);
    m_generation++;
}

void ClientGeoJsonSource::addPoly(const Properties& props, const std::vector<Coordinates>& poly) {

    geojsonvt::ProjectedGeometryContainer geometry;
    for (auto& ring : poly) {
        auto& gjvtring = reinterpret_cast<const std::vector<geojsonvt::LonLat>&>(ring);
        geometry.members.push_back(geojsonvt::Convert::project(gjvtring, tolerance));
    }

    auto feature = geojsonvt::Convert::create(geojsonvt::Tags{std::make_shared<Properties>(props)},
                                              geojsonvt::ProjectedFeatureType::Polygon,
                                              geometry);

    m_features.push_back(std::move(feature));

    std::lock_guard<std::mutex> lock(m_mutexStore);
    m_store = std::make_unique<GeoJSONVT>(m_features, m_maxZoom, m_maxZoom, indexMaxPoints, tolerance);
    m_generation++;
}

std::shared_ptr<TileData> ClientGeoJsonSource::parse(const TileTask& task,
                                                     const MapProjection& projection) const {

    auto data = std::make_shared<TileData>();

    geojsonvt::Tile tile;
    {
        std::lock_guard<std::mutex> lock(m_mutexStore);
        if (!m_store) { return nullptr; }
        tile = m_store->getTile(task.tileId().z, task.tileId().x, task.tileId().y);
    }

    Layer layer(""); // empty name will skip filtering by 'collection'

    for (auto& it : tile.features) {

        Feature feat(m_id);

        const auto& geom = it.tileGeometry;
        const auto type = it.type;

        switch (type) {
            case geojsonvt::TileFeatureType::Point: {
                feat.geometryType = GeometryType::points;
                for (const auto& pt : geom) {
                    const auto& point = pt.get<geojsonvt::TilePoint>();
                    feat.points.push_back(transformPoint(point));
                }
                break;
            }
            case geojsonvt::TileFeatureType::LineString: {
                feat.geometryType = GeometryType::lines;
                for (const auto& r : geom) {
                    Line line;
                    for (const auto& pt : r.get<geojsonvt::TileRing>().points) {
                        line.push_back(transformPoint(pt));
                    }
                    feat.lines.emplace_back(std::move(line));
                }
                break;
            }
            case geojsonvt::TileFeatureType::Polygon: {
                feat.geometryType = GeometryType::polygons;
                for (const auto& r : geom) {
                    Line line;
                    for (const auto& pt : r.get<geojsonvt::TileRing>().points) {
                        line.push_back(transformPoint(pt));
                    }
                    // Polygons are in a flat list of rings, with ccw rings indicating
                    // the beginning of a new polygon
                    if (signedArea(line.begin(), line.end()) >= 0 || feat.polygons.empty()) {
                        feat.polygons.emplace_back();
                    }
                    feat.polygons.back().push_back(std::move(line));
                }
                break;
            }
            default: break;
        }

        feat.props = *it.tags.map;
        layer.features.emplace_back(std::move(feat));

    }

    data->layers.emplace_back(std::move(layer));

    return data;

}

}
