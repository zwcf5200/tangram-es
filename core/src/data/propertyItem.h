#pragma once

#include "util/variant.h"

namespace Tangram {

struct PropertyItem {
    PropertyItem(std::string key, Value value) :
        key(std::move(key)), value(std::move(value)) {}

    std::string key;
    Value value;
    bool operator<(const PropertyItem& rhs) const {
        return key.size() == rhs.key.size()
            ? key < rhs.key
            : key.size() < rhs.key.size();
    }
};

}
