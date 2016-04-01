#pragma once

#include "dataSource.h"

namespace Tangram {

class MVTSource : public DataSource {

protected:

    virtual std::shared_ptr<TileData> parse(const TileTask& task,
                                            const MapProjection& projection) const override;

public:

    MVTSource(const std::string& name, const std::string& urlTemplate, int32_t maxZoom);

};

}
