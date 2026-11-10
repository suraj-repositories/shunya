#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/phy/mac_grid_solver_3d.hpp"

#include <cstddef>

namespace shunya::env {

struct WindTunnelConfig {
    size_t nx{48};
    size_t ny{24};
    double dx_m{0.05};
    double inlet_velocity_m_s{5.0};
    double fluid_density_kg_m3{1.225};
    double kinematic_viscosity_m2_s{0.015};
};

struct WindTunnelForceReport {
    double drag_force_n{0.0};
    double lift_force_n{0.0};
    double drag_coeff_cd{0.0};
    double lift_coeff_cl{0.0};
    double max_divergence{0.0};
};

/**
 * @brief Virtual aerodynamic wind tunnel fixture with laminar inlet, open-pressure outlet, and NACA 0012 obstacle placement.
 */
class WindTunnelFixture {
public:
    explicit WindTunnelFixture(WindTunnelConfig cfg = {});

    void reset();
    void place_naca0012_airfoil(
        double center_x_m,
        double center_y_m,
        double chord_m,
        double angle_of_attack_deg);

    void place_bluff_box(
        double min_x_m, double min_y_m,
        double max_x_m, double max_y_m);

    [[nodiscard]] core::base::Result<WindTunnelForceReport> advance_and_measure(
        int steps,
        double dt_s);

    /**
     * @brief NASA / Abbott-von Doenhoff NACA 0012 thin-airfoil polar reference C_L(alpha) & C_D(alpha).
     */
    [[nodiscard]] static double naca0012_reference_cl(double angle_of_attack_deg) noexcept;
    [[nodiscard]] static double naca0012_reference_cd(double angle_of_attack_deg) noexcept;

    [[nodiscard]] const phy::MacGridFluidSolver3D& solver() const noexcept { return solver_; }

private:
    void apply_tunnel_bcs();
    [[nodiscard]] WindTunnelForceReport integrate_solid_forces() const noexcept;

    WindTunnelConfig cfg_;
    phy::MacGridFluidSolver3D solver_;
    std::vector<bool> solid_mask_;
    double reference_area_m2_{0.5};
};

} // namespace shunya::env
