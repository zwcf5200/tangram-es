#include "geoJsonSource.h"
#include "util/mapProjection.h"

#include "tileData.h"
#include "tile/tile.h"
#include "tile/tileID.h"
#include "tile/tileTask.h"
#include "util/geoJson.h"
#include "platform.h"

namespace Tangram {

GeoJsonSource::GeoJsonSource(const std::string& name, const std::string& urlTemplate, int32_t maxZoom) :
    DataSource(name, urlTemplate, maxZoom) {
}

std::shared_ptr<TileData> GeoJsonSource::parse(const TileTask& task,
                                               const MapProjection& projection) const {

    auto& dltask = static_cast<const DownloadTileTask&>(task);

    std::shared_ptr<TileData> tileData = std::make_shared<TileData>();

    // Parse data into a JSON document
    const char* error;
    size_t offset;
    auto document = JsonParseBytes(dltask.rawTileData->data(), dltask.rawTileData->size(), &error, &offset);

    if (error) {
        LOGE("Json parsing failed on tile [%s]: %s (%u)", dltask.tileId().toString().c_str(), error, offset);
        return tileData;
    }

    BoundingBox tileBounds(projection.TileBounds(dltask.tileId()));
    glm::dvec2 tileOrigin = {tileBounds.min.x, tileBounds.max.y*-1.0};
    double tileInverseScale = 1.0 / tileBounds.width();

    const auto projFn = [&](glm::dvec2 lonLat){
        glm::dvec2 tmp = projection.LonLatToMeters(lonLat);
        return Point {
            (tmp.x - tileOrigin.x) * tileInverseScale,
            (tmp.y - tileOrigin.y) * tileInverseScale,
             0
        };
    };

    // Transform JSON data into TileData using GeoJson functions
    if (GeoJson::isFeatureCollection(document)) {
        tileData->layers.push_back(GeoJson::getLayer(document, projFn, m_id));
    } else {
        for (auto layer = document.MemberBegin(); layer != document.MemberEnd(); ++layer) {
            if (GeoJson::isFeatureCollection(layer->value)) {
                tileData->layers.push_back(GeoJson::getLayer(layer->value, projFn, m_id));
                tileData->layers.back().name = layer->name.GetString();
            }
        }
    }


    // Discard original JSON object and return TileData

    return tileData;

}

}
