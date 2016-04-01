#pragma once

#include <string>

namespace Tangram {

template <class M, class T>
inline bool tryFind(M& map, const std::string& transform, T& out) {
    auto it = map.find(transform);
    if (it != map.end()) {
        out = it->second;
        return true;
    }
    return false;
}

namespace LabelProperty {

enum Anchor {
    center,
    top,
    bottom,
    left,
    right,
    top_left,
    top_right,
    bottom_left,
    bottom_right,
};

bool anchor(const std::string& transform, Anchor& out);

} // LabelProperty

namespace TextLabelProperty {

enum Transform {
    none,
    capitalize,
    uppercase,
    lowercase,
};

enum Align {
    right,
    left,
    center,
};

bool transform(const std::string& transform, Transform& out);
bool align(const std::string& transform, Align& out);

} // TextLabelProperty
} // Tangram
