#pragma once
#include <string>
#include <cstdint>

namespace Tangram {

struct Color {

    union {
        struct {
            uint8_t r, g, b, a;
        };
        uint32_t abgr = 0;
    };

    Color() = default;
    Color(uint32_t abgr) : abgr(abgr) {}
    Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a) {}

    static Color mix(Color x, Color y, float a) {
        return Color(
            x.r * (1 - a) + y.r * a,
            x.g * (1 - a) + y.g * a,
            x.b * (1 - a) + y.b * a,
            x.a * (1 - a) + y.a * a
        );
    }

    static Color parse(const std::string& css_string);
    static Color parse(const std::string& css_string, bool& is_valid);

};

}
