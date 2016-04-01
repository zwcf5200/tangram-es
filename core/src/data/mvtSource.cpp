#include "mvtSource.h"

#include "tileData.h"
#include "tile/tileID.h"
#include "tile/tile.h"
#include "tile/tileTask.h"
#include "util/pbfParser.h"
#include "platform.h"

namespace Tangram {


MVTSource::MVTSource(const std::string& name, const std::string& urlTemplate, int32_t maxZoom) :
    DataSource(name, urlTemplate, maxZoom) {
}

std::shared_ptr<TileData> MVTSource::parse(const TileTask& task, const MapProjection& projection) const {

    auto tileData = std::make_shared<TileData>();

    auto& dltask = static_cast<const DownloadTileTask&>(task);

    protobuf::message item(dltask.rawTileData->data(), dltask.rawTileData->size());
    PbfParser::ParserContext ctx(m_id);

    while(item.next()) {
        if(item.tag == 3) {
            tileData->layers.push_back(PbfParser::getLayer(ctx, item.getMessage()));
        } else {
            item.skip();
        }
    }
    return tileData;
}

}
