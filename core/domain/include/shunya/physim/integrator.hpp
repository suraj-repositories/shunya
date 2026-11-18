#pragma once

#include "shunya/physim/rigid_body.hpp"

#include <functional>
#include <vector>

namespace shunya::physim {

enum class IntegratorScheme : uint8_t {
    SymplecticEuler, ///< 1st-order semi-implicit symplectic momentum-position scheme
    RungeKutta4      ///< 4th-order classical Runge-Kutta integrator with quaternion renormalization
};

struct AnalyticalFreeFallResult {
    double position_y{0.0};
    double velocity_y{0.0};
};

struct AnalyticalProjectileResult {
    Vector3d position{0.0, 0.0, 0.0};
    Vector3d velocity{0.0, 0.0, 0.0};
    double max_height{0.0};
    double flight_time{0.0};
    double range{0.0};
};

struct AnalyticalPrecessionResult {
    double precession_frequency_rad_s{0.0}; ///< Omega_p = tau / (I_z * omega_s)
    double precession_period_s{0.0};        ///< T_p = 2 * pi / Omega_p
};

/**
 * @brief Symplectic Euler and Runge-Kutta 4th-order (RK4) kinematic integrators
 *        preserving angular and linear momentum (PH4-09.01.003).
 */
class RigidBodyIntegrator {
public:
    using ForceTorqueCallback = std::function<void(
        const RigidBodyState& state,
        Vector3d& out_force,
        Vector3d& out_torque)>;

    /**
     * @brief Steps a dynamic rigid body using Symplectic Euler.
     */
    static void step_symplectic_euler(
        RigidBodyState& state,
        double dt_s,
        const Vector3d& constant_force = {0.0, 0.0, 0.0},
        const Vector3d& constant_torque = {0.0, 0.0, 0.0}) noexcept;

    /**
     * @brief Steps a dynamic rigid body using 4th-order Runge-Kutta with exact
     *        quaternion differential kinematics and state-dependent force/torque callback.
     */
    static void step_rk4(
        RigidBodyState& state,
        double dt_s,
        const ForceTorqueCallback& force_torque_fn = nullptr) noexcept;

    /**
     * @brief Steps an entire multi-body system using the chosen scheme.
     */
    static void step_system(
        std::vector<RigidBodyEntity>& bodies,
        double dt_s,
        IntegratorScheme scheme,
        const Vector3d& gravity_vector = {0.0, -9.80665, 0.0}) noexcept;

    // =========================================================================
    // Analytical Classical Mechanics Validators (PH4-09.01.006)
    // =========================================================================

    [[nodiscard]] static AnalyticalFreeFallResult analytical_free_fall(
        double y0, double v0, double g, double t_s) noexcept;

    [[nodiscard]] static double analytical_terminal_velocity(
        double mass_kg, double g, double fluid_density, double cd, double area_m2) noexcept;

    [[nodiscard]] static AnalyticalProjectileResult analytical_projectile(
        const Vector3d& r0, const Vector3d& v0, double g, double t_s) noexcept;

    [[nodiscard]] static AnalyticalPrecessionResult analytical_gyroscopic_precession(
        double mass_kg, double g, double pivot_dist_m, double izz_kg_m2, double spin_rate_rad_s) noexcept;
};

} // namespace shunya::physim
