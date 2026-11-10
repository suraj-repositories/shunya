#pragma once

#include "shunya/core/domain/mechanics.hpp"
#include "shunya/phy/inertia_tensor_calculator.hpp"

namespace shunya::phy {

struct RotationalBodyState {
    core::domain::mechanics::Vector3d position{0.0, 0.0, 0.0};
    core::domain::mechanics::Vector3d linear_velocity{0.0, 0.0, 0.0};
    core::domain::mechanics::Quaterniond orientation{1.0, 0.0, 0.0, 0.0};
    core::domain::mechanics::Vector3d angular_velocity_world{0.0, 0.0, 0.0};

    double mass_kg{1.0};
    core::domain::mechanics::Matrix3x3 local_inertia{core::domain::mechanics::Matrix3x3::identity()};
    core::domain::mechanics::Matrix3x3 inv_local_inertia{core::domain::mechanics::Matrix3x3::identity()};

    void set_local_inertia(const core::domain::mechanics::Matrix3x3& I_body) {
        local_inertia = I_body;
        inv_local_inertia = I_body.inverse();
    }
};

/**
 * @brief Rigid-body Euler rotational dynamics and unit-quaternion kinematics (dq/dt = 0.5 * omega * q).
 */
class RigidBodyKinematics {
public:
    [[nodiscard]] static core::domain::mechanics::Vector3d world_to_body(
        const core::domain::mechanics::Quaterniond& q,
        const core::domain::mechanics::Vector3d& v_world) noexcept;

    [[nodiscard]] static core::domain::mechanics::Vector3d body_to_world(
        const core::domain::mechanics::Quaterniond& q,
        const core::domain::mechanics::Vector3d& v_body) noexcept;

    [[nodiscard]] static core::domain::mechanics::Vector3d angular_momentum_world(
        const RotationalBodyState& state) noexcept;

    [[nodiscard]] static double rotational_kinetic_energy(
        const RotationalBodyState& state) noexcept;

    /**
     * @brief Exact quaternion derivative dq/dt = 0.5 * (0, omega_world) * q, integrated cleanly with renormalization.
     */
    [[nodiscard]] static core::domain::mechanics::Quaterniond quaternion_derivative(
        const core::domain::mechanics::Quaterniond& q,
        const core::domain::mechanics::Vector3d& omega_world) noexcept;

    /**
     * @brief Advances rigid body linear motion and Euler rotational equations via 4th-order Runge-Kutta + quaternion normalization.
     */
    static void step_rk4(
        RotationalBodyState& state,
        double dt_s,
        const core::domain::mechanics::Vector3d& force_world = {},
        const core::domain::mechanics::Vector3d& torque_world = {}) noexcept;

    [[nodiscard]] static core::base::Result<RotationalBodyState> from_closed_mesh(
        const core::domain::TriangleMesh& mesh,
        double density_kg_m3 = 1000.0);
};

} // namespace shunya::phy
