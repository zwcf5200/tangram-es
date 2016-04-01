#pragma once

#include "scene/styleParam.h"
#include "util/fastmap.h"

#include <string>
#include <functional>
#include <memory>
#include <array>

struct duk_hthread;
typedef struct duk_hthread duk_context;

namespace Tangram {

class Scene;
struct Feature;
struct StyleParam;

enum class StyleParamKey : uint8_t;
enum class FilterGlobal : uint8_t;


class StyleContext {

public:

    using FunctionID = uint32_t;

    StyleContext();
    ~StyleContext();

    /*
     * Set currently processed Feature
     */
    void setFeature(const Feature& feature);

    /*
     * Set global for currently processed Tile
     */
    void setGlobalZoom(int zoom);

    /* Called from Filter::eval */
    float getGlobalZoom() const { return m_globalZoom; }

    const Value& getGlobal(FilterGlobal key) const {
        return m_globals[static_cast<uint8_t>(key)];
    }

    /* Called from Filter::eval */
    bool evalFilter(FunctionID id);

    /* Called from DrawRule::eval */
    bool evalStyle(FunctionID id, StyleParamKey key, StyleParam::Value& val);

    /*
     * Setup filter and style functions from @scene
     */
    void initFunctions(const Scene& scene);

    /*
     * Unset Feature handle
     */
    void clear();

    bool setFunctions(const std::vector<std::string>& functions);

    void setGlobal(const std::string& key, Value value);
    const Value& getGlobal(const std::string& key) const;

private:
    static int jsGetProperty(duk_context *ctx);
    static int jsHasProperty(duk_context *ctx);

    bool parseStyleResult(StyleParamKey key, StyleParam::Value& val) const;

    std::array<Value, 4> m_globals;
    int m_globalGeom = -1;
    int m_globalZoom = -1;

    int32_t m_sceneId = -1;

    const Feature* m_feature = nullptr;

    mutable duk_context *m_ctx;
};

}
