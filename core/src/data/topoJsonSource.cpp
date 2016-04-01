#include "topoJsonSource.h"

#include "tileData.h"
#include "tile/tile.h"
#include "tile/tileTask.h"
#include "util/mapProjection.h"
#include "util/topoJson.h"
#include "platform.h"


namespace Tangram {

TopoJsonSource::TopoJsonSource(const std::string& name, const std::string& urlTemplate, int32_t maxZoom) :
    DataSource(name, urlTemplate, maxZoom) {
}

std::shared_ptr<TileData> TopoJsonSource::parse(const TileTask& task,
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

    // Transform JSON data into a TileData using TopoJson functions
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

    // Parse topology and transform
    auto topology = TopoJson::getTopology(document, projFn);

    // Parse each TopoJson object as a data layer
    auto objectsIt = document.FindMember("objects");
    if (objectsIt == document.MemberEnd()) { return tileData; }
    auto& objects = objectsIt->value;
    for (auto layer = objects.MemberBegin(); layer != objects.MemberEnd(); ++layer) {
        tileData->layers.push_back(TopoJson::getLayer(layer, topology, m_id));
    }

    // Discard JSON object and return TileData
    return tileData;

}

}
