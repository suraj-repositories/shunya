#pragma once

#include "shunya/physim/collision_pipeline.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <vector>

namespace shunya::physim {

struct PgsSolverOptions {
    int iterations{30};                 ///< Number of Gauss-Seidel velocity iterations
    double baumgarte_factor{0.2};       ///< Baumgarte position stabilization factor beta (0.1 - 0.3)
    double penetration_slop{0.001};     ///< Allowed penetration slop before position correction kicks in (m)
    double restitution_velocity_threshold{0.2}; ///< Relative normal velocity threshold for bounce (m/s)
    bool enable_warm_starting{true};    ///< Reuse impulses from previous step for fast convergence
};

struct InclineSlidingExperimentResult {
    bool is_sliding{false};
    double acceleration_mps2{0.0};
    double final_velocity_mps{0.0};
    double final_distance_m{0.0};
};

struct BouncingBallExperimentResult {
    double initial_height_m{0.0};
    double first_rebound_height_m{0.0};
    double measured_restitution{0.0};
};

/**
 * @brief Projected Gauss-Seidel (PGS) sequential impulse solver for contact restitution
 *        and static/dynamic Coulomb friction (PH4-09.02.005).
 */
class PgsContactSolver {
public:
    /**
     * @brief Solves contact impulses and friction for an arbitrary collection of bodies and manifolds.
     */
    static void solve_contacts(
        std::vector<RigidBodyEntity>& bodies,
        std::vector<PhysimContactManifold>& manifolds,
        double dt_s,
        const PgsSolverOptions& options = {}) noexcept;

    // =========================================================================
    // Physical Experiment Validators (PH4-09.02.006)
    // =========================================================================

    /**
     * @brief Simulates a ball dropped from h0 onto a rigid surface and measures rebound height & restitution.
     */
    [[nodiscard]] static BouncingBallExperimentResult simulate_bouncing_ball(
        double drop_height_m,
        double restitution_coeff,
        double ball_radius_m = 0.1,
        double ball_mass_kg = 1.0,
        double dt_s = 0.001,
        int max_steps = 2000) noexcept;

    /**
     * @brief Simulates a block resting or sliding on an inclined plane with gravity and Coulomb friction.
     */
    [[nodiscard]] static InclineSlidingExperimentResult simulate_incline_block(
        double incline_angle_deg,
        double static_friction_coeff,
        double dynamic_friction_coeff,
        double block_mass_kg = 2.0,
        double duration_s = 1.0,
        double dt_s = 0.002) noexcept;
};

} // namespace shunya::physim
