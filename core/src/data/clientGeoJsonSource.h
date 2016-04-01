#pragma once

#include "dataSource.h"
#include "util/types.h"

#include <mutex>

namespace mapbox {
namespace util {
namespace geojsonvt {
class GeoJSONVT;
class TilePoint;
class ProjectedFeature;
}}}

namespace Tangram {

using GeoJSONVT = mapbox::util::geojsonvt::GeoJSONVT;

struct Properties;

class ClientGeoJsonSource : public DataSource {

public:

    ClientGeoJsonSource(const std::string& name, const std::string& url, int32_t maxZoom = 18);
    ~ClientGeoJsonSource();

    // Add geometry from a GeoJSON string
    void addData(const std::string& data);
    void addPoint(const Properties& props, LngLat point);
    void addLine(const Properties& props, const Coordinates& coords);
    void addPoly(const Properties& props, const std::vector<Coordinates>& poly);

    virtual bool loadTileData(std::shared_ptr<TileTask>&& task, TileTaskCb cb) override;
    std::shared_ptr<TileTask> createTask(TileID tileId) override;

    virtual void cancelLoadingTile(const TileID& tile) override {};
    virtual void clearData() override;

protected:

    virtual std::shared_ptr<TileData> parse(const TileTask& task,
                                            const MapProjection& projection) const override;

    std::unique_ptr<GeoJSONVT> m_store;
    mutable std::mutex m_mutexStore;
    std::vector<mapbox::util::geojsonvt::ProjectedFeature> m_features;

};

}
