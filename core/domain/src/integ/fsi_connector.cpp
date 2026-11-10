#include "shunya/integ/fsi_connector.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::integ {

namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

FsiConnector::FsiConnector(FloatingCylinderSpec spec, double initial_submerged_m)
    : spec_(spec) {
    state_.submerged_depth_m = initial_submerged_m;
}

double FsiConnector::equilibrium_submerged_depth_m() const noexcept {
    if (spec_.fluid_density_kg_m3 <= 0.0) {
        return spec_.height_m;
    }
    return spec_.height_m * (spec_.solid_density_kg_m3 / spec_.fluid_density_kg_m3);
}

double FsiConnector::analytical_oscillation_freq_hz() const noexcept {
    const double eq = equilibrium_submerged_depth_m();
    if (eq <= 0.0 || spec_.gravity_m_s2 <= 0.0) {
        return 0.0;
    }
    // omega_n = sqrt(rho_f * g / (rho_s * H)) = sqrt(g / h_eq)
    return std::sqrt(spec_.gravity_m_s2 / eq) / (2.0 * kPi);
}

core::base::Result<FloatingBodyState> FsiConnector::step_hydrometer(double dt_s) {
    if (dt_s <= 0.0 || spec_.radius_m <= 0.0 || spec_.height_m <= 0.0 ||
        spec_.solid_density_kg_m3 <= 0.0) {
        return core::base::Error{
            431, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid FSI hydrometer geometry or time step", ""
        };
    }

    const double area = kPi * spec_.radius_m * spec_.radius_m;
    const double mass = spec_.solid_density_kg_m3 * area * spec_.height_m;
    const double weight = mass * spec_.gravity_m_s2;

    // Symplectic Euler (velocity-Verlet half-kick / drift) preserves oscillatory energy accurately
    const double sub_clamped = std::clamp(state_.submerged_depth_m, 0.0, spec_.height_m);
    const double buoy_0 = spec_.fluid_density_kg_m3 * (area * sub_clamped) * spec_.gravity_m_s2;
    const double net_y_0 = buoy_0 - weight - spec_.linear_damping_n_s_m * state_.velocity_y_m_s;
    const double accel_0 = net_y_0 / mass;

    // submerged_depth increases when velocity_y < 0 (moving downward)
    state_.velocity_y_m_s += accel_0 * dt_s;
    state_.submerged_depth_m -= state_.velocity_y_m_s * dt_s;

    const double sub_new = std::clamp(state_.submerged_depth_m, 0.0, spec_.height_m);
    const double buoy_1 = spec_.fluid_density_kg_m3 * (area * sub_new) * spec_.gravity_m_s2;
    state_.buoyancy_force_n = buoy_1;
    state_.net_force_n = buoy_1 - weight - spec_.linear_damping_n_s_m * state_.velocity_y_m_s;
    state_.time_s += dt_s;
    return state_;
}

core::base::Result<std::vector<FloatingBodyState>> FsiConnector::simulate_hydrometer(
    double total_time_s,
    double dt_s) {
    if (total_time_s <= 0.0 || dt_s <= 0.0) {
        return core::base::Error{
            432, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Simulation duration and dt_s must be positive", ""
        };
    }
    const size_t steps = static_cast<size_t>(std::ceil(total_time_s / dt_s));
    std::vector<FloatingBodyState> out;
    out.reserve(steps);
    for (size_t i = 0; i < steps; ++i) {
        auto s = step_hydrometer(dt_s);
        if (!s.is_ok()) return s.error();
        out.push_back(s.value());
    }
    return out;
}

std::pair<double, double> FsiConnector::sample_mac_box_force_2d(
    const phy::MacGridFluidSolver3D& solver,
    size_t i0, size_t j0, size_t i1, size_t j1) noexcept {
    double fx = 0.0;
    double fy = 0.0;
    const auto& cfg = solver.config();
    if (i0 == 0 || j0 == 0 || i1 + 1 >= cfg.nx || j1 + 1 >= cfg.ny || i0 > i1 || j0 > j1) {
        return {0.0, 0.0};
    }
    for (size_t j = j0; j <= j1; ++j) {
        fx += solver.pressure(i0 - 1, j, 0) * cfg.dx_m;
        fx -= solver.pressure(i1 + 1, j, 0) * cfg.dx_m;
    }
    for (size_t i = i0; i <= i1; ++i) {
        fy += solver.pressure(i, j0 - 1, 0) * cfg.dx_m;
        fy -= solver.pressure(i, j1 + 1, 0) * cfg.dx_m;
    }
    return {fx, fy};
}

} // namespace shunya::integ
