#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/domain/rendering.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace shunya::cad3d {

struct CascadeSplitInfo {
    double near_depth{0.1};
    double far_depth{100.0};
    core::base::math::Matrix4x4<double> light_view_proj;
    double ortho_width{20.0};
    Point3D center{0.0, 0.0, 0.0};
};

struct PointLight3D {
    Point3D position{0.0, 0.0, 0.0};
    double radius{10.0};
    Vec3d color{1.0, 1.0, 1.0};
    double intensity{1.0};
};

struct CsmConfig {
    uint32_t cascade_count{4};
    uint32_t shadow_map_size{256};
    double split_lambda{0.75}; // 0 = uniform, 1 = logarithmic PSSM
    double max_shadow_distance{150.0};
    double min_bias{0.002};
    double slope_bias_scale{0.01};
};

} // namespace shunya::cad3d
