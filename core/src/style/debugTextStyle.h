#pragma once

#include "textStyle.h"

namespace Tangram {

class DebugTextStyle : public TextStyle {

public:
    DebugTextStyle(std::string name, bool sdf = false) : TextStyle(name, sdf) {}

    std::unique_ptr<StyleBuilder> createBuilder() const override;

};

}
