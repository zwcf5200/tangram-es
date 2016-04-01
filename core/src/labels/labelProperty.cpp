#include "labelProperty.h"
#include <map>

namespace Tangram {
namespace LabelProperty {

const std::map<std::string, Anchor> s_AnchorMap = {
    {"center", Anchor::center},
    {"top", Anchor::top},
    {"bottom", Anchor::bottom},
    {"left", Anchor::left},
    {"right", Anchor::right},
    {"top-left", Anchor::top_left},
    {"top-right", Anchor::top_right},
    {"bottom-left", Anchor::bottom_left},
    {"bottom-right", Anchor::bottom_right},
};

bool anchor(const std::string& anchor, Anchor& out) {
    return tryFind(s_AnchorMap, anchor, out);
}

} // LabelProperty

namespace TextLabelProperty {

const std::map<std::string, Transform> s_TransformMap = {
    {"none", Transform::none},
    {"capitalize", Transform::capitalize},
    {"uppercase", Transform::uppercase},
    {"lowercase", Transform::lowercase},
};

const std::map<std::string, Align> s_AlignMap = {
    {"right", Align::right},
    {"left", Align::left},
    {"center", Align::center},
};

bool transform(const std::string& transform, Transform& out) {
    return tryFind(s_TransformMap, transform, out);
}

bool align(const std::string& align, Align& out) {
    return tryFind(s_AlignMap, align, out);
}

} // TextLabelProperty
} // Tangram
