#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/phy/mac_grid_solver_3d.hpp"

#include <vector>

namespace shunya::integ {

struct FloatingCylinderSpec {
    double radius_m{0.025};
    double height_m{0.30};
    double solid_density_kg_m3{600.0}; // Wood in water
    double fluid_density_kg_m3{1000.0};
    double gravity_m_s2{9.80665};
    double linear_damping_n_s_m{0.0};
};

struct FloatingBodyState {
    double time_s{0.0};
    double submerged_depth_m{0.0}; // depth below water surface
    double velocity_y_m_s{0.0};    // upward positive
    double buoyancy_force_n{0.0};
    double net_force_n{0.0};
};

/**
 * @brief Fluid-Structure Interaction (FSI) connector bridging fluid pressure/buoyancy and rigid body motion.
 */
class FsiConnector {
public:
    explicit FsiConnector(FloatingCylinderSpec spec = {}, double initial_submerged_m = 0.22);

    [[nodiscard]] core::base::Result<FloatingBodyState> step_hydrometer(double dt_s);
    [[nodiscard]] core::base::Result<std::vector<FloatingBodyState>> simulate_hydrometer(
        double total_time_s,
        double dt_s);

    [[nodiscard]] double equilibrium_submerged_depth_m() const noexcept;
    [[nodiscard]] double analytical_oscillation_freq_hz() const noexcept;

    /**
     * @brief Integrates hydrodynamic pressure force from MAC grid surrounding fluid cells on an axis-aligned solid box [i0..i1] x [j0..j1].
     */
    [[nodiscard]] static std::pair<double, double> sample_mac_box_force_2d(
        const phy::MacGridFluidSolver3D& solver,
        size_t i0, size_t j0, size_t i1, size_t j1) noexcept;

private:
    FloatingCylinderSpec spec_;
    FloatingBodyState state_;
};

} // namespace shunya::integ
