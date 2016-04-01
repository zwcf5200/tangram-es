#pragma once

#include "util/color.h"
#include "scene/styleParam.h"
#include "variant.hpp"

#include <vector>

namespace YAML {
    class Node;
}

namespace Tangram {

class MapProjection;

using StopValue = variant<none_type, float, Color, glm::vec2>;

struct Stops {

    struct Frame {
        float key = 0;
        StopValue value;
        Frame(float k, float v) : key(k), value(v) {}
        Frame(float k, Color c) : key(k), value(c) {}
        Frame(float k, glm::vec2 v) : key(k), value(v) {}
    };

    std::vector<Frame> frames;
    static Stops Colors(const YAML::Node& node);
    static Stops Widths(const YAML::Node& node, const MapProjection& projection, const std::vector<Unit>& units);
    static Stops FontSize(const YAML::Node& node);
    static Stops Offsets(const YAML::Node& node, const std::vector<Unit>& units);
    static Stops Numbers(const YAML::Node& node);

    Stops(const std::vector<Frame>& frames) : frames(frames) {}
    Stops(const Stops& rhs) = default;
    Stops() {}

    auto evalFloat(float key) const -> float;
    auto evalWidth(float key) const -> float;
    auto evalColor(float key) const -> uint32_t;
    auto evalVec2(float key) const -> glm::vec2;
    auto nearestHigherFrame(float key) const -> std::vector<Frame>::const_iterator;

    static void eval(const Stops& stops, StyleParamKey key, float zoom, StyleParam::Value& result);
};

}
