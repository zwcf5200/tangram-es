#pragma once

#include "util/variant.h"
#include "glm/vec2.hpp"
#include <string>
#include <vector>

namespace Tangram {

struct Stops;

enum class StyleParamKey : uint8_t {
    align,
    anchor,
    cap,
    centroid,
    collide,
    color,
    extrude,
    font_family,
    font_fill,
    font_size,
    font_stroke_color,
    font_stroke_width,
    font_style,
    font_weight,
    interactive,
    join,
    miter_limit,
    none,
    offset,
    order,
    outline_cap,
    outline_color,
    outline_join,
    outline_miter_limit,
    outline_order,
    outline_width,
    outline_style,
    priority,
    repeat_distance,
    repeat_group,
    size,
    sprite,
    sprite_default,
    style,
    text_source,
    text_wrap,
    tile_edges,
    transform,
    transition_hide_time,
    transition_selected_time,
    transition_show_time,
    visible,
    width,
    NUM_ELEMENTS
};

constexpr size_t StyleParamKeySize = static_cast<size_t>(StyleParamKey::NUM_ELEMENTS);

enum class Unit { pixel, milliseconds, meter, seconds };

struct StyleParam {

    struct ValueUnitPair {
        ValueUnitPair() = default;
        ValueUnitPair(float value, Unit unit)
            : value(value), unit(unit) {}

        float value;
        Unit unit = Unit::meter;

        bool isMeter() const { return unit == Unit::meter; }
        bool isPixel() const { return unit == Unit::pixel; }
        bool isSeconds() const { return unit == Unit::seconds; }
        bool isMilliseconds() const { return unit == Unit::milliseconds; }

    };
    struct Width : ValueUnitPair {

        Width() = default;
        Width(float value) :
            ValueUnitPair(value, Unit::meter) {}
        Width(float value, Unit unit)
            : ValueUnitPair(value, unit) {}

        Width(ValueUnitPair& other) :
            ValueUnitPair(other) {}

        bool operator==(const Width& other) const {
            return value == other.value && unit == other.unit;
        }
        bool operator!=(const Width& other) const {
            return value != other.value || unit != other.unit;
        }
    };

    using Value = variant<none_type, bool, float, uint32_t, std::string, glm::vec2, Width>;

    StyleParam() :
        key(StyleParamKey::none),
        value(none_type{}) {};

    StyleParam(const std::string& key, const std::string& value);

    StyleParam(StyleParamKey key, std::string value) :
        key(key),
        value(std::move(value)) {}

    StyleParam(StyleParamKey key, Stops* stops) :
        key(key),
        value(none_type{}),
        stops(stops) {
    }

    StyleParamKey key;
    Value value;
    Stops* stops = nullptr;
    int32_t function = -1;

    bool operator<(const StyleParam& rhs) const { return key < rhs.key; }
    bool valid() const { return !value.is<none_type>() || stops != nullptr || function >= 0; }
    operator bool() const { return valid(); }

    std::string toString() const;

    /* parse a font size (in em, pt, %) and give the appropriate size in pixel */
    static bool parseFontSize(const std::string& size, float& pxSize);

    static uint32_t parseColor(const std::string& color);

    static bool parseTime(const std::string& value, float& time);

    static bool parseVec2(const std::string& value, const std::vector<Unit> allowedUnits, glm::vec2& vec2);

    static int parseValueUnitPair(const std::string& value, size_t start,
                                  StyleParam::ValueUnitPair& result);

    static Value parseString(StyleParamKey key, const std::string& value);

    static bool isColor(StyleParamKey key);
    static bool isWidth(StyleParamKey key);
    static bool isOffsets(StyleParamKey key);
    static bool isFontSize(StyleParamKey key);
    static bool isRequired(StyleParamKey key);

    static bool unitsForStyleParam(StyleParamKey key, std::vector<Unit>& unit);

    static StyleParamKey getKey(const std::string& key);

    static const std::string& keyName(StyleParamKey key);

    template<typename T>
    struct visitor {
        using result_type = bool;
        T& out;
        bool operator()(const T& v) const {
            out = v;
            return true;
        }
        template<typename O>
        bool operator()(const O v) const {
            return false;
        }
    };
    template<typename T>
    struct visitor_ptr {
        using result_type = const T*;
        const T* operator()(const T& v) const {
            return &v;
        }
        template<typename O>
        const T* operator()(const O v) const {
            return nullptr;
        }
    };
};

}
