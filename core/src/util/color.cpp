#include "color.h"
#include "csscolorparser.hpp"

namespace Tangram {

Color Color::parse(const std::string& css_string) {
    bool valid;
    return parse(css_string, valid);
}

Color Color::parse(const std::string& css_string, bool& is_valid) {
    auto parsed = CSSColorParser::parse(css_string, is_valid);
    if (is_valid) {
        return Color(parsed.getInt());
    } else {
        return Color();
    }
}

}
