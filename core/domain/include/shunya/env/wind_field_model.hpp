#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/env/turbulence_generator.hpp"

#include <cstdint>

namespace shunya::env {

struct WindFieldConfig {
    double reference_speed_m_s{10.0};
    double reference_height_m{10.0};
    double roughness_length_z0_m{0.03}; // open flat terrain
    double azimuth_rad{0.0};            // 0 = +X, pi/2 = +Z (assuming Y-up)
    bool enable_turbulence{false};
    double turbulence_intensity{0.10};
    double integral_length_scale_m{50.0};
    uint64_t seed{42};
};

/**
 * @brief 3D wind vector field combining logarithmic surface shear v(z) = (u* / kappa) * ln(z / z0) with Dryden turbulence.
 */
class WindFieldModel {
public:
    static constexpr double kVonKarmanKappa = 0.41;

    explicit WindFieldModel(WindFieldConfig config = {});

    [[nodiscard]] double friction_velocity_u_star() const noexcept;
    [[nodiscard]] double mean_speed_at_height(double height_m) const noexcept;
    [[nodiscard]] core::base::math::Vector3<double> velocity_at(
        double height_m,
        double time_s = 0.0,
        const core::base::math::Vector3<double>& pos_m = {0.0, 0.0, 0.0}) const noexcept;

    [[nodiscard]] const DrydenTurbulenceGenerator& turbulence() const noexcept { return turb_; }

private:
    WindFieldConfig config_;
    DrydenTurbulenceGenerator turb_;
};

} // namespace shunya::env
