#include "shunya/env/gravity_field.hpp"

#include <cmath>
#include <limits>

namespace shunya::env {

GravityFieldModel::GravityFieldModel(GravityConfig config) : config_(config) {}

GravityConfig GravityFieldModel::preset_config(
    GravityPreset preset,
    core::domain::env::UpAxis up_axis) {
    GravityConfig c;
    c.preset = preset;
    c.up_axis = up_axis;

    switch (preset) {
        case GravityPreset::Earth:
            c.uniform_g_m_s2 = kEarthGravity;
            c.central_mass_kg = 5.97219e24;
            c.surface_radius_m = 6371000.0;
            break;
        case GravityPreset::Moon:
            c.uniform_g_m_s2 = kMoonGravity;
            c.central_mass_kg = 7.342e22;
            c.surface_radius_m = 1737400.0;
            break;
        case GravityPreset::Mars:
            c.uniform_g_m_s2 = kMarsGravity;
            c.central_mass_kg = 6.4171e23;
            c.surface_radius_m = 3389500.0;
            break;
        case GravityPreset::Jupiter:
            c.uniform_g_m_s2 = kJupiterGravity;
            c.central_mass_kg = 1.8982e27;
            c.surface_radius_m = 69911000.0;
            break;
        case GravityPreset::ZeroG:
            c.uniform_g_m_s2 = 0.0;
            c.central_mass_kg = 0.0;
            break;
        case GravityPreset::CustomUniform:
        case GravityPreset::CentralNewtonian:
            break;
    }

    if (up_axis == core::domain::env::UpAxis::Y_Up) {
        c.central_origin_m = {0.0, -c.surface_radius_m, 0.0};
    } else {
        c.central_origin_m = {0.0, 0.0, -c.surface_radius_m};
    }
    return c;
}

void GravityFieldModel::set_preset(GravityPreset preset) {
    config_ = preset_config(preset, config_.up_axis);
}

void GravityFieldModel::set_config(GravityConfig config) {
    config_ = config;
}

core::base::math::Vector3<double> GravityFieldModel::acceleration_at(
    const core::base::math::Vector3<double>& pos_m) const noexcept {
    if (config_.preset == GravityPreset::ZeroG) {
        return {0.0, 0.0, 0.0};
    }
    if (config_.preset == GravityPreset::CentralNewtonian) {
        const double dx = pos_m.x - config_.central_origin_m.x;
        const double dy = pos_m.y - config_.central_origin_m.y;
        const double dz = pos_m.z - config_.central_origin_m.z;
        const double r2 = dx * dx + dy * dy + dz * dz;
        const double r = std::sqrt(r2);
        if (r < 1.0) {
            return {0.0, 0.0, 0.0};
        }
        const double factor = -(kGravitationalConstantG * config_.central_mass_kg) / (r2 * r);
        return {factor * dx, factor * dy, factor * dz};
    }

    if (config_.up_axis == core::domain::env::UpAxis::Y_Up) {
        return {0.0, -config_.uniform_g_m_s2, 0.0};
    }
    return {0.0, 0.0, -config_.uniform_g_m_s2};
}

double GravityFieldModel::magnitude_at(
    const core::base::math::Vector3<double>& pos_m) const noexcept {
    const auto a = acceleration_at(pos_m);
    return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
}

double GravityFieldModel::pendulum_period_s(double length_m) const noexcept {
    const double g = magnitude_at();
    if (g <= 1e-12 || length_m <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    constexpr double kTwoPi = 2.0 * 3.14159265358979323846;
    return kTwoPi * std::sqrt(length_m / g);
}

} // namespace shunya::env
