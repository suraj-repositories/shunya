#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/data_exchange.hpp"
#include "shunya/core/domain/mechanics.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace shunya::physim {

using Vector3d = core::domain::mechanics::Vector3d;
using Quaterniond = core::domain::mechanics::Quaterniond;
using Matrix3x3 = core::domain::mechanics::Matrix3x3;
using TriangleMesh = core::domain::TriangleMesh;

// Quaternion arithmetic helpers
inline Quaterniond operator+(const Quaterniond& a, const Quaterniond& b) noexcept {
    return {a.w + b.w, a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Quaterniond operator*(const Quaterniond& q, double s) noexcept {
    return {q.w * s, q.x * s, q.y * s, q.z * s};
}

inline Quaterniond operator*(double s, const Quaterniond& q) noexcept {
    return {q.w * s, q.x * s, q.y * s, q.z * s};
}

// Matrix3x3 arithmetic helpers
inline Matrix3x3 operator+(const Matrix3x3& a, const Matrix3x3& b) noexcept {
    Matrix3x3 res;
    for (size_t i = 0; i < 9; ++i) {
        res.m[i] = a.m[i] + b.m[i];
    }
    return res;
}

// =============================================================================
// Body Type: Dynamic, Kinematic, Static (PH4-09.01.004)
// =============================================================================
enum class BodyType : uint8_t {
    Dynamic,   ///< Fully simulated with mass, inertia, forces, momentum & collisions
    Kinematic, ///< Script/user controlled velocity & position; infinite mass, unaffected by forces
    Static     ///< Immovable rigid boundary/obstacle with infinite mass and zero velocity
};

// =============================================================================
// Rigid Body State: x, q, p, L (PH4-09.01.001)
// =============================================================================
struct RigidBodyState {
    Vector3d position{0.0, 0.0, 0.0};              ///< Position vector x in world coordinates (m)
    Quaterniond orientation{1.0, 0.0, 0.0, 0.0};   ///< Unit orientation quaternion q (w, x, y, z)
    Vector3d linear_momentum{0.0, 0.0, 0.0};       ///< Linear momentum p = m * v (kg * m / s)
    Vector3d angular_momentum{0.0, 0.0, 0.0};      ///< Angular momentum L = I_world * omega (kg * m^2 / s)
    Vector3d kinematic_linear_velocity{0.0, 0.0, 0.0};  ///< Prescribed velocity for kinematic bodies
    Vector3d kinematic_angular_velocity{0.0, 0.0, 0.0}; ///< Prescribed angular velocity for kinematic bodies

    double mass_kg{1.0};                           ///< Total body mass m (kg)
    double inv_mass{1.0};                          ///< Inverse mass 1 / m (1 / kg)
    Matrix3x3 local_inertia{Matrix3x3::identity()};     ///< Inertia tensor I_body relative to COM (kg * m^2)
    Matrix3x3 inv_local_inertia{Matrix3x3::identity()}; ///< Inverse inertia tensor I_body^-1 (1 / (kg * m^2))

    // Accumulators
    Vector3d force_accumulator{0.0, 0.0, 0.0};     ///< Net external force F (N)
    Vector3d torque_accumulator{0.0, 0.0, 0.0};    ///< Net external torque tau (N * m)

    void set_mass(double mass) noexcept;
    void set_local_inertia(const Matrix3x3& I_body) noexcept;

    // Velocity and Angular Velocity derived from Momentum
    [[nodiscard]] Vector3d linear_velocity() const noexcept;
    void set_linear_velocity(const Vector3d& v) noexcept;

    [[nodiscard]] Matrix3x3 world_inertia_tensor() const noexcept;
    [[nodiscard]] Matrix3x3 inv_world_inertia_tensor() const noexcept;

    [[nodiscard]] Vector3d angular_velocity() const noexcept;
    void set_angular_velocity(const Vector3d& omega_world) noexcept;

    [[nodiscard]] double kinetic_energy() const noexcept;
    [[nodiscard]] double linear_kinetic_energy() const noexcept;
    [[nodiscard]] double rotational_kinetic_energy() const noexcept;

    // Coordinate Frame Transformations
    [[nodiscard]] Vector3d local_to_world_point(const Vector3d& p_local) const noexcept;
    [[nodiscard]] Vector3d world_to_local_point(const Vector3d& p_world) const noexcept;
    [[nodiscard]] Vector3d local_to_world_vector(const Vector3d& v_local) const noexcept;
    [[nodiscard]] Vector3d world_to_local_vector(const Vector3d& v_world) const noexcept;
    [[nodiscard]] Vector3d point_velocity(const Vector3d& p_world) const noexcept;

    // Force, Torque and Impulse Applications
    void apply_force_at_com(const Vector3d& force) noexcept;
    void apply_force_at_world_point(const Vector3d& force, const Vector3d& world_point) noexcept;
    void apply_torque(const Vector3d& torque) noexcept;

    void apply_linear_impulse(const Vector3d& impulse) noexcept;
    void apply_angular_impulse(const Vector3d& angular_impulse) noexcept;
    void apply_impulse_at_world_point(const Vector3d& impulse, const Vector3d& world_point) noexcept;

    void clear_accumulators() noexcept;
};

// =============================================================================
// Mesh Inertia Calculation via Mirtich's Divergence Theorem (PH4-09.01.002)
// =============================================================================
struct MeshInertiaProperties {
    double volume_m3{0.0};
    double mass_kg{0.0};
    Vector3d center_of_mass{0.0, 0.0, 0.0};
    Matrix3x3 inertia_origin{Matrix3x3::identity()};
    Matrix3x3 inertia_com{Matrix3x3::identity()};
    Vector3d principal_moments{1.0, 1.0, 1.0};
};

class MeshInertiaCalculator {
public:
    /**
     * @brief Evaluates exact volume, center of mass, and 3x3 inertia tensor using
     *        Brian Mirtich's 1996 divergence theorem reduction over closed triangle boundary.
     */
    [[nodiscard]] static core::base::Result<MeshInertiaProperties> compute_from_mesh(
        const TriangleMesh& mesh,
        double density_kg_m3 = 1000.0);

    // Analytic helper shapes
    [[nodiscard]] static MeshInertiaProperties compute_solid_box(
        double mass_kg, const Vector3d& size_m);
    [[nodiscard]] static MeshInertiaProperties compute_solid_sphere(
        double mass_kg, double radius_m);
    [[nodiscard]] static MeshInertiaProperties compute_solid_cylinder(
        double mass_kg, double radius_m, double height_m);
};

// =============================================================================
// Environmental Aerodynamics, Buoyancy & Gravitational Controls (PH4-09.01.005)
// =============================================================================
struct AerodynamicProperties {
    double fluid_density_kg_m3{1.225}; ///< Air density at sea level (1.225 kg/m^3)
    double drag_coefficient{0.47};     ///< Drag coefficient Cd (sphere ~0.47, cube ~1.05)
    double cross_section_area_m2{0.01};///< Projected frontal area A (m^2)
    double linear_damping{0.0};        ///< Linear drag factor gamma_lin (N * s / m)
    double angular_damping{0.01};      ///< Rotational damping factor gamma_rot (N * m * s / rad)
    double volume_m3{0.0};             ///< Displaced body volume for buoyancy (m^3)
    bool enable_buoyancy{true};        ///< Flag to compute Archimedes buoyant force
    bool enable_drag{true};            ///< Flag to compute aerodynamic quadratic drag
};

class EnvironmentForces {
public:
    [[nodiscard]] static Vector3d compute_gravity(
        const RigidBodyState& state,
        const Vector3d& gravity_vector) noexcept;

    [[nodiscard]] static Vector3d compute_buoyancy(
        const AerodynamicProperties& aero,
        const Vector3d& gravity_vector) noexcept;

    [[nodiscard]] static Vector3d compute_aerodynamic_drag(
        const RigidBodyState& state,
        const AerodynamicProperties& aero) noexcept;

    [[nodiscard]] static Vector3d compute_rotational_drag(
        const RigidBodyState& state,
        const AerodynamicProperties& aero) noexcept;

    static void apply_environmental_forces(
        RigidBodyState& state,
        const Vector3d& gravity_vector,
        const AerodynamicProperties& aero) noexcept;
};

// =============================================================================
// Physim Rigid Body Entity (PH4-09.01.004)
// =============================================================================
class RigidBodyEntity {
public:
    explicit RigidBodyEntity(
        std::string id = "",
        BodyType type = BodyType::Dynamic)
        : id_(std::move(id)), type_(type) {
        if (id_.empty()) {
            id_ = core::base::Uuid::generate_random().to_string();
        }
        if (type_ != BodyType::Dynamic) {
            state_.set_mass(0.0);
            state_.inv_mass = 0.0;
            state_.local_inertia = Matrix3x3::zero();
            state_.inv_local_inertia = Matrix3x3::zero();
        }
    }

    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] BodyType type() const noexcept { return type_; }
    void set_type(BodyType type) noexcept;

    [[nodiscard]] const RigidBodyState& state() const noexcept { return state_; }
    [[nodiscard]] RigidBodyState& state() noexcept { return state_; }
    void set_state(const RigidBodyState& st) noexcept { state_ = st; }

    [[nodiscard]] const AerodynamicProperties& aero() const noexcept { return aero_; }
    [[nodiscard]] AerodynamicProperties& aero() noexcept { return aero_; }
    void set_aero(const AerodynamicProperties& aero) noexcept { aero_ = aero; }

    [[nodiscard]] bool is_dynamic() const noexcept { return type_ == BodyType::Dynamic; }
    [[nodiscard]] bool is_kinematic() const noexcept { return type_ == BodyType::Kinematic; }
    [[nodiscard]] bool is_static() const noexcept { return type_ == BodyType::Static; }

private:
    std::string id_;
    BodyType type_{BodyType::Dynamic};
    RigidBodyState state_;
    AerodynamicProperties aero_;
};

} // namespace shunya::physim
