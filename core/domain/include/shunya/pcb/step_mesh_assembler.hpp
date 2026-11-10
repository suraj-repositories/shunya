#pragma once

#include "shunya/core/base/math.hpp"

#include <string>

namespace shunya::pcb {

struct PcbComponent3DBody {
    std::string ref_des{"U1"};
    core::base::math::Vector2<double> center_mm;
    double size_x_mm{5.0};
    double size_y_mm{5.0};
    double height_mm{1.5};
    bool on_top{true};
};

} // namespace shunya::pcb
