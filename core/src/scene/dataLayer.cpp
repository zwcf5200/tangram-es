#include "dataLayer.h"

namespace Tangram {

DataLayer::DataLayer(SceneLayer layer, const std::string& source, const std::vector<std::string>& collections) :
    SceneLayer(std::move(layer)),
    m_source(source),
    m_collections(collections) {}

}
