#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/virtual_lab.hpp"

namespace shunya::env {

enum class GravityPreset {
    Earth,
    Moon,
    Mars,
    Jupiter,
    ZeroG,
    CustomUniform,
    CentralNewtonian
};

struct GravityConfig {
    GravityPreset preset{GravityPreset::Earth};
    core::domain::env::UpAxis up_axis{core::domain::env::UpAxis::Y_Up};
    double uniform_g_m_s2{9.80665};
    double central_mass_kg{5.97219e24};
    double surface_radius_m{6371000.0};
    core::base::math::Vector3<double> central_origin_m{0.0, -6371000.0, 0.0};
};

/**
 * @brief Configurable planetary uniform and Newtonian central gravity field evaluator.
 */
class GravityFieldModel {
public:
    static constexpr double kGravitationalConstantG = 6.67430e-11;
    static constexpr double kEarthGravity = 9.80665;
    static constexpr double kMoonGravity = 1.62;
    static constexpr double kMarsGravity = 3.72;
    static constexpr double kJupiterGravity = 24.79;

    explicit GravityFieldModel(GravityConfig config = {});

    [[nodiscard]] static GravityConfig preset_config(
        GravityPreset preset,
        core::domain::env::UpAxis up_axis = core::domain::env::UpAxis::Y_Up);

    void set_preset(GravityPreset preset);
    void set_config(GravityConfig config);

    [[nodiscard]] const GravityConfig& config() const noexcept { return config_; }

    [[nodiscard]] core::base::math::Vector3<double> acceleration_at(
        const core::base::math::Vector3<double>& pos_m = {0.0, 0.0, 0.0}) const noexcept;

    [[nodiscard]] double magnitude_at(
        const core::base::math::Vector3<double>& pos_m = {0.0, 0.0, 0.0}) const noexcept;

    [[nodiscard]] double pendulum_period_s(double length_m) const noexcept;

private:
    GravityConfig config_;
};

} // namespace shunya::env
