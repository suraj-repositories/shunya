#pragma once

#include "shunya/core/base/math.hpp"

#include <array>
#include <string>

namespace shunya::pcb {

struct ThermalReliefSpoke {
    core::base::math::Vector2<double> start_mm;
    core::base::math::Vector2<double> end_mm;
    double width_mm{0.25};
};

struct ThermalReliefPad {
    std::string net_name;
    core::base::math::Vector2<double> center_mm;
    double pad_radius_mm{0.80};
    double isolation_gap_mm{0.20};
    std::array<ThermalReliefSpoke, 4> spokes{};
};

} // namespace shunya::pcb
