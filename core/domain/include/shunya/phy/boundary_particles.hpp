#pragma once

#include "shunya/core/base/math.hpp"

#include <vector>

namespace shunya::phy {

struct SphBoundaryParticle {
    core::base::math::Vector3<double> position{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> normal{0.0, 1.0, 0.0};
    core::base::math::Vector3<double> wall_velocity{0.0, 0.0, 0.0};
    double volume_m3{1e-6};
};

/**
 * @brief Akinci (2012) rigid wall boundary particle generator eliminating density deficit and wall penetration.
 */
class BoundaryParticleGenerator {
public:
    static void add_box_floor_and_walls(
        std::vector<SphBoundaryParticle>& out_boundary,
        const core::base::math::Vector3<double>& min_pt,
        const core::base::math::Vector3<double>& max_pt,
        double spacing_m,
        bool include_floor = true,
        bool include_x_walls = true,
        bool include_z_walls = true);
};

} // namespace shunya::phy
