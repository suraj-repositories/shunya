#pragma once

#include "shunya/core/base/math.hpp"

#include <string>
#include <vector>

namespace shunya::pcb {

struct KeepoutZone {
    std::string name;
    std::vector<core::base::math::Vector2<double>> polygon_mm;
    bool block_components{true};
    bool block_traces{true};
    bool block_vias{true};
};

} // namespace shunya::pcb
