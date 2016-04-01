#pragma once


#include <string>
#include <memory>
#include <vector>

namespace Tangram {

class MapProjection;
struct TileData;
struct TileID;
class Tile;
class TileManager;
struct RawCache;
class TileTask;
struct TileTaskCb;

class DataSource : public std::enable_shared_from_this<DataSource> {

public:

    /* Tile data sources must have a name and a URL template that defines where to find
     * a tile based on its coordinates. A URL template includes exactly one occurrance
     * each of '{x}', '{y}', and '{z}' which will be replaced by the x index, y index,
     * and zoom level of tiles to produce their URL.
     */
    DataSource(const std::string& name, const std::string& urlTemplate, int32_t maxZoom = 18);

    virtual ~DataSource();

    /* Fetches data for the map tile specified by @tileID
     *
     * LoadTile starts an asynchronous I/O task to retrieve the data for a tile. When
     * the I/O task is complete, the tile data is added to a queue in @tileManager for
     * further processing before it is renderable.
     */
    virtual bool loadTileData(std::shared_ptr<TileTask>&& task, TileTaskCb cb);


    /* Stops any running I/O tasks pertaining to @tile */
    virtual void cancelLoadingTile(const TileID& tile);

    /* Parse a <TileTask> with data into a <TileData>, returning an empty TileData on failure */
    virtual std::shared_ptr<TileData> parse(const TileTask& task, const MapProjection& projection) const = 0;

    /* Clears all data associated with this DataSource */
    virtual void clearData();

    const std::string& name() const { return m_name; }

    virtual bool equals(const DataSource& other) const {
        return m_name == other.m_name &&
               m_urlTemplate == other.m_urlTemplate;
    }

    virtual std::shared_ptr<TileTask> createTask(TileID tile);

    /* @cacheSize: Set size of in-memory cache for tile data in bytes.
     * This cache holds unprocessed tile data for fast recreation of recently used tiles.
     */
    void setCacheSize(size_t cacheSize);

    /* ID of this DataSource instance */
    int32_t id() const { return m_id; }

    /* Generation ID of DataSource state (incremented for each update, e.g. on clearData()) */
    int64_t generation() const { return m_generation; }

    int32_t maxZoom() const { return m_maxZoom; }

protected:

    void onTileLoaded(std::vector<char>&& rawData, std::shared_ptr<TileTask>& task, TileTaskCb cb);

    /* Constructs the URL of a tile using <m_urlTemplate> */
    virtual void constructURL(const TileID& tileCoord, std::string& url) const;

    std::string constructURL(const TileID& tileCoord) const {
        std::string url;
        constructURL(tileCoord, url);
        return url;
    }

    // Name used to identify this source in the style sheet
    std::string m_name;

    // Maximum zoom for which tiles will be requested
    int32_t m_maxZoom;

    // Unique id for DataSource
    int32_t m_id;

    // Generation of dynamic DataSource state (incremented for each update)
    int64_t m_generation = 1;

    // URL template for requesting tiles from a network or filesystem
    std::string m_urlTemplate;

    std::unique_ptr<RawCache> m_cache;
};

}
