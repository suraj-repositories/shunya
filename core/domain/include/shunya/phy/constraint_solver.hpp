#pragma once

#include "shunya/phy/joint_models.hpp"
#include "shunya/phy/rigid_body_kinematics.hpp"

#include <vector>

namespace shunya::phy {

struct SolverOptions {
    int pga_iterations{40};
    int substeps{4};
    double baumgarte_beta{0.25};
    double allowed_penetration{1e-4};
};

/**
 * @brief Projected Gauss-Seidel (Sequential Impulse) constraint solver for articulated chains and Coulomb contacts.
 */
class ConstraintSolver {
public:
    static void step_world(
        std::vector<RotationalBodyState>& bodies,
        const std::vector<ArticulatedJoint>& joints,
        const std::vector<ContactConstraintSpec>& contacts,
        const core::domain::mechanics::Vector3d& gravity_mps2,
        double dt_s,
        const SolverOptions& opts = {});

    [[nodiscard]] static double max_joint_position_error(
        const std::vector<RotationalBodyState>& bodies,
        const std::vector<ArticulatedJoint>& joints) noexcept;
};

} // namespace shunya::phy
