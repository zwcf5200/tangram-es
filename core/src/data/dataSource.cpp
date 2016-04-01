#include "dataSource.h"
#include "util/geoJson.h"
#include "platform.h"
#include "tileData.h"
#include "tile/tileID.h"
#include "tile/tileHash.h"
#include "tile/tile.h"
#include "tile/tileManager.h"
#include "tile/tileTask.h"

#include <atomic>
#include <mutex>
#include <list>
#include <functional>
#include <unordered_map>

namespace Tangram {

struct RawCache {

    // Used to ensure safe access from async loading threads
    std::mutex m_mutex;

    // LRU in-memory cache for raw tile data
    using CacheEntry = std::pair<TileID, std::shared_ptr<std::vector<char>>>;
    using CacheList = std::list<CacheEntry>;
    using CacheMap = std::unordered_map<TileID, typename CacheList::iterator>;

    CacheMap m_cacheMap;
    CacheList m_cacheList;
    int m_usage = 0;
    int m_maxUsage = 0;

    bool get(DownloadTileTask& task) {

        if (m_maxUsage <= 0) { return false; }

        std::lock_guard<std::mutex> lock(m_mutex);
        TileID id(task.tileId().x, task.tileId().y, task.tileId().z);

        auto it = m_cacheMap.find(id);
        if (it != m_cacheMap.end()) {
            // Move cached entry to start of list
            m_cacheList.splice(m_cacheList.begin(), m_cacheList, it->second);
            task.rawTileData = m_cacheList.front().second;

            return true;
        }

        return false;
    }
    void put(const TileID& tileID, std::shared_ptr<std::vector<char>> rawDataRef) {

        if (m_maxUsage <= 0) { return; }

        std::lock_guard<std::mutex> lock(m_mutex);
        TileID id(tileID.x, tileID.y, tileID.z);

        m_cacheList.push_front({id, rawDataRef});
        m_cacheMap[id] = m_cacheList.begin();

        m_usage += rawDataRef->size();

        while (m_usage > m_maxUsage) {
            if (m_cacheList.empty()) {
                LOGE("Error: invalid cache state!");
                m_usage = 0;
                break;
            }

            // LOGE("Limit raw cache tiles:%d, %fMB ", m_cacheList.size(),
            //        double(m_cacheUsage) / (1024*1024));

            auto& entry = m_cacheList.back();
            m_usage -= entry.second->size();

            m_cacheMap.erase(entry.first);
            m_cacheList.pop_back();
        }
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cacheMap.clear();
        m_cacheList.clear();
        m_usage = 0;
    }
};

static std::atomic<int32_t> s_serial;

DataSource::DataSource(const std::string& name, const std::string& urlTemplate, int32_t maxZoom) :
    m_name(name), m_maxZoom(maxZoom), m_urlTemplate(urlTemplate),
    m_cache(std::make_unique<RawCache>()){

    m_id = s_serial++;
}

DataSource::~DataSource() {
    clearData();
}

std::shared_ptr<TileTask> DataSource::createTask(TileID tileId) {
    auto task = std::make_shared<DownloadTileTask>(tileId, shared_from_this());

    m_cache->get(*task);

    return task;
}

void DataSource::setCacheSize(size_t cacheSize) {
    m_cache->m_maxUsage = cacheSize;
}

void DataSource::clearData() {
    m_cache->clear();
    m_generation++;
}

void DataSource::constructURL(const TileID& tileCoord, std::string& url) const {
    url.assign(m_urlTemplate);
    try {
        size_t xpos = url.find("{x}");
        url.replace(xpos, 3, std::to_string(tileCoord.x));
        size_t ypos = url.find("{y}");
        url.replace(ypos, 3, std::to_string(tileCoord.y));
        size_t zpos = url.find("{z}");
        url.replace(zpos, 3, std::to_string(tileCoord.z));
    } catch(...) {
        LOGE("Bad URL template!");
    }
}

void DataSource::onTileLoaded(std::vector<char>&& rawData, std::shared_ptr<TileTask>& task, TileTaskCb cb) {
    TileID tileID = task->tileId();

    if (!rawData.empty()) {

        auto rawDataRef = std::make_shared<std::vector<char>>();
        std::swap(*rawDataRef, rawData);

        auto& dltask = static_cast<DownloadTileTask&>(*task);
        dltask.rawTileData = rawDataRef;

        cb.func(std::move(task));

        m_cache->put(tileID, rawDataRef);
    }
}


bool DataSource::loadTileData(std::shared_ptr<TileTask>&& task, TileTaskCb cb) {

    std::string url(constructURL(task->tileId()));

    // Using bind instead of lambda to be able to 'move' (until c++14)
    return startUrlRequest(url, std::bind(&DataSource::onTileLoaded,
                                          this,
                                          std::placeholders::_1,
                                          std::move(task), cb));

}

void DataSource::cancelLoadingTile(const TileID& tileID) {
    cancelUrlRequest(constructURL(tileID));
}

}
