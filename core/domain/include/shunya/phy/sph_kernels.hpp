#pragma once

#include "shunya/core/base/math.hpp"

#include <cmath>

namespace shunya::phy {

/**
 * @brief Compact-support 3D SPH kernels (Cubic Spline M4 & Wendland C2) with gradients.
 */
class SphKernels {
public:
    static constexpr double kPi = 3.14159265358979323846;

    // 3D Wendland C2 kernel: compact support r in [0, 2h]
    [[nodiscard]] static double wendland_c2_3d(double r, double h) noexcept {
        if (r < 0.0 || h <= 0.0) return 0.0;
        const double q = r / h;
        if (q >= 2.0) return 0.0;
        const double sigma = 21.0 / (16.0 * kPi * h * h * h);
        const double t = 1.0 - 0.5 * q;
        const double t2 = t * t;
        return sigma * (t2 * t2) * (2.0 * q + 1.0);
    }

    // Derivative dW/dr (<= 0) for 3D Wendland C2: W(q) = sigma * (1-q/2)^4 * (2q+1)
    // dW/dq = -5 * sigma * q * (1 - q/2)^3
    [[nodiscard]] static double wendland_c2_3d_dw_dr(double r, double h) noexcept {
        if (r <= 1e-15 || h <= 0.0) return 0.0;
        const double q = r / h;
        if (q >= 2.0) return 0.0;
        const double sigma = 21.0 / (16.0 * kPi * h * h * h);
        const double t = 1.0 - 0.5 * q;
        return sigma * (-5.0 * q * t * t * t) / h;
    }

    [[nodiscard]] static core::base::math::Vector3<double> wendland_c2_3d_grad(
        const core::base::math::Vector3<double>& r_vec, double h) noexcept {
        const double r = r_vec.length();
        if (r <= 1e-15 || r >= 2.0 * h) return {0.0, 0.0, 0.0};
        const double dw_dr = wendland_c2_3d_dw_dr(r, h);
        return r_vec * (dw_dr / r);
    }

    // 3D Cubic Spline M4 kernel: support r in [0, 2h]
    [[nodiscard]] static double cubic_spline_3d(double r, double h) noexcept {
        if (r < 0.0 || h <= 0.0) return 0.0;
        const double q = r / h;
        const double sigma = 1.0 / (kPi * h * h * h);
        if (q < 1.0) {
            return sigma * (1.0 - 1.5 * q * q + 0.75 * q * q * q);
        }
        if (q < 2.0) {
            const double t = 2.0 - q;
            return sigma * 0.25 * t * t * t;
        }
        return 0.0;
    }
};

} // namespace shunya::phy
