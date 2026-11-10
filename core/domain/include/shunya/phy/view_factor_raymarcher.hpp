#pragma once

#include "shunya/core/base/math.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace shunya::phy {

struct RadiativePatch3D {
    core::base::math::Vector3<double> center{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> normal{0.0, 0.0, 1.0};
    core::base::math::Vector3<double> tangent_u{1.0, 0.0, 0.0};
    core::base::math::Vector3<double> tangent_v{0.0, 1.0, 0.0};
    double half_u{0.5};
    double half_v{0.5};
    double emissivity{1.0};
    double temperature_k{300.0};

    [[nodiscard]] double area() const noexcept { return 4.0 * half_u * half_v; }
};

/**
 * @brief Cosine-weighted hemisphere raymarcher & quadrature integrator for geometric view factors F_ij.
 */
class ViewFactorRaymarcher {
public:
    /**
     * @brief Computes geometric view factor matrix F_ij between surface patches enforcing enclosure & reciprocity.
     */
    [[nodiscard]] static std::vector<std::vector<double>> compute_view_factor_matrix(
        const std::vector<RadiativePatch3D>& patches,
        uint32_t samples_per_axis_i = 8,
        uint32_t samples_per_axis_j = 8) noexcept;
};

} // namespace shunya::phy
