#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <variant>
#include <cmath>
#include <algorithm>

namespace shunya::core::domain::mechanics {

using Vector3d = base::math::Vector3<double>;
using Quaterniond = base::math::Quaternion<double>;

// =============================================================================
// Matrix3x3: Inertia Tensor & 3D Spatial Rotation Transformations
// =============================================================================

struct Matrix3x3 {
    std::array<double, 9> m = {
        1.0, 0.0, 0.0,
        0.0, 1.0, 0.0,
        0.0, 0.0, 1.0
    };

    constexpr Matrix3x3() noexcept = default;
    constexpr Matrix3x3(double m00, double m01, double m02,
                        double m10, double m11, double m12,
                        double m20, double m21, double m22) noexcept
        : m{m00, m01, m02, m10, m11, m12, m20, m21, m22} {}

    [[nodiscard]] static constexpr Matrix3x3 identity() noexcept {
        return Matrix3x3(1.0, 0.0, 0.0,
                         0.0, 1.0, 0.0,
                         0.0, 0.0, 1.0);
    }

    [[nodiscard]] static constexpr Matrix3x3 zero() noexcept {
        return Matrix3x3(0.0, 0.0, 0.0,
                         0.0, 0.0, 0.0,
                         0.0, 0.0, 0.0);
    }

    [[nodiscard]] static constexpr Matrix3x3 diagonal(double d0, double d1, double d2) noexcept {
        return Matrix3x3( d0, 0.0, 0.0,
                         0.0,  d1, 0.0,
                         0.0, 0.0,  d2);
    }

    [[nodiscard]] double at(std::size_t r, std::size_t c) const noexcept {
        return m[r * 3 + c];
    }

    void set(std::size_t r, std::size_t c, double val) noexcept {
        m[r * 3 + c] = val;
    }

    [[nodiscard]] Matrix3x3 transpose() const noexcept {
        return Matrix3x3(
            m[0], m[3], m[6],
            m[1], m[4], m[7],
            m[2], m[5], m[8]
        );
    }

    [[nodiscard]] double determinant() const noexcept {
        return m[0] * (m[4] * m[8] - m[5] * m[7])
             - m[1] * (m[3] * m[8] - m[5] * m[6])
             + m[2] * (m[3] * m[7] - m[4] * m[6]);
    }

    [[nodiscard]] Matrix3x3 inverse() const noexcept {
        const double det = determinant();
        if (std::abs(det) < 1e-15) {
            return zero();
        }
        const double inv_det = 1.0 / det;
        return Matrix3x3(
            (m[4] * m[8] - m[5] * m[7]) * inv_det,
            (m[2] * m[7] - m[1] * m[8]) * inv_det,
            (m[1] * m[5] - m[2] * m[4]) * inv_det,

            (m[5] * m[6] - m[3] * m[8]) * inv_det,
            (m[0] * m[8] - m[2] * m[6]) * inv_det,
            (m[2] * m[3] - m[0] * m[5]) * inv_det,

            (m[3] * m[7] - m[4] * m[6]) * inv_det,
            (m[1] * m[6] - m[0] * m[7]) * inv_det,
            (m[0] * m[4] - m[1] * m[3]) * inv_det
        );
    }

    [[nodiscard]] Vector3d operator*(const Vector3d& v) const noexcept {
        return {
            m[0] * v.x + m[1] * v.y + m[2] * v.z,
            m[3] * v.x + m[4] * v.y + m[5] * v.z,
            m[6] * v.x + m[7] * v.y + m[8] * v.z
        };
    }

    [[nodiscard]] Matrix3x3 operator*(const Matrix3x3& o) const noexcept {
        Matrix3x3 res = zero();
        for (std::size_t r = 0; r < 3; ++r) {
            for (std::size_t c = 0; c < 3; ++c) {
                double s = 0.0;
                for (std::size_t k = 0; k < 3; ++k) {
                    s += at(r, k) * o.at(k, c);
                }
                res.set(r, c, s);
            }
        }
        return res;
    }

    [[nodiscard]] Matrix3x3 operator*(double s) const noexcept {
        return Matrix3x3(
            m[0] * s, m[1] * s, m[2] * s,
            m[3] * s, m[4] * s, m[5] * s,
            m[6] * s, m[7] * s, m[8] * s
        );
    }

    [[nodiscard]] static Matrix3x3 from_quaternion(const Quaterniond& q) noexcept {
        const double w = q.w, x = q.x, y = q.y, z = q.z;
        return Matrix3x3(
            1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w),       2.0 * (x * z + y * w),
            2.0 * (x * y + z * w),       1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - x * w),
            2.0 * (x * z - y * w),       2.0 * (y * z + x * w),       1.0 - 2.0 * (x * x + y * y)
        );
    }
};

// =============================================================================
// MEC-01: Rigid Body State & Mass Properties
// =============================================================================

enum class BodyType {
    Dynamic,
    Kinematic,
    Static
};

struct MassProperties {
    double mass = 1.0;
    double inv_mass = 1.0;
    Vector3d center_of_mass{0.0, 0.0, 0.0};
    Matrix3x3 local_inertia = Matrix3x3::identity();
    Matrix3x3 inv_local_inertia = Matrix3x3::identity();

    [[nodiscard]] static MassProperties create_solid_sphere(double mass, double radius) noexcept;
    [[nodiscard]] static MassProperties create_solid_box(double mass, const Vector3d& half_extents) noexcept;
    [[nodiscard]] static MassProperties create_solid_cylinder(double mass, double radius, double height) noexcept;
};

struct RigidBodyState {
    Vector3d position{0.0, 0.0, 0.0};
    Quaterniond orientation{1.0, 0.0, 0.0, 0.0}; // w, x, y, z
    Vector3d linear_velocity{0.0, 0.0, 0.0};
    Vector3d angular_velocity{0.0, 0.0, 0.0};
};

// =============================================================================
// MEC-03: Collision Shapes & Bounding Volumes
// =============================================================================

struct AABB {
    Vector3d min_point{-0.5, -0.5, -0.5};
    Vector3d max_point{0.5, 0.5, 0.5};

    [[nodiscard]] bool intersects(const AABB& other) const noexcept;
    [[nodiscard]] bool contains(const Vector3d& pt) const noexcept;
    void expand(const Vector3d& pt) noexcept;
};

struct SphereShape {
    double radius = 0.5;
};

struct BoxShape {
    Vector3d half_extents{0.5, 0.5, 0.5};
};

struct CapsuleShape {
    double radius = 0.25;
    double half_height = 0.5;
};

struct PlaneShape {
    Vector3d normal{0.0, 1.0, 0.0};
    double offset = 0.0; // distance from origin along normal
};

using CollisionShape = std::variant<SphereShape, BoxShape, CapsuleShape, PlaneShape>;

// =============================================================================
// MEC-04: Contact Manifold & Friction
// =============================================================================

struct ContactPoint {
    Vector3d world_position{0.0, 0.0, 0.0};
    Vector3d normal{0.0, 1.0, 0.0}; // points from Body B to Body A
    double penetration_depth = 0.0;
    double normal_impulse = 0.0;
    double tangent_impulse = 0.0;
};

struct ContactManifold {
    std::string body_a_id;
    std::string body_b_id;
    std::vector<ContactPoint> contacts;
    double restitution = 0.2;
    double static_friction = 0.5;
    double dynamic_friction = 0.4;
};

// =============================================================================
// MEC-02: Forces, Impulses & Accumulators
// =============================================================================

struct AppliedForce {
    Vector3d force{0.0, 0.0, 0.0};
    Vector3d world_point{0.0, 0.0, 0.0};
};

class RigidBody {
public:
    explicit RigidBody(std::string id, BodyType type = BodyType::Dynamic);

    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] BodyType type() const noexcept { return type_; }
    void set_type(BodyType type) noexcept;

    [[nodiscard]] const RigidBodyState& state() const noexcept { return state_; }
    void set_state(const RigidBodyState& st) noexcept { state_ = st; }

    [[nodiscard]] const Vector3d& position() const noexcept { return state_.position; }
    void set_position(const Vector3d& pos) noexcept { state_.position = pos; }

    [[nodiscard]] const Quaterniond& orientation() const noexcept { return state_.orientation; }
    void set_orientation(const Quaterniond& q) noexcept { state_.orientation = q.normalized(); }

    [[nodiscard]] const Vector3d& linear_velocity() const noexcept { return state_.linear_velocity; }
    void set_linear_velocity(const Vector3d& v) noexcept { state_.linear_velocity = v; }

    [[nodiscard]] const Vector3d& angular_velocity() const noexcept { return state_.angular_velocity; }
    void set_angular_velocity(const Vector3d& w) noexcept { state_.angular_velocity = w; }

    [[nodiscard]] const MassProperties& mass_properties() const noexcept { return mass_props_; }
    void set_mass_properties(const MassProperties& props) noexcept;

    [[nodiscard]] const CollisionShape& shape() const noexcept { return shape_; }
    void set_shape(CollisionShape shape) noexcept { shape_ = shape; }

    [[nodiscard]] double linear_damping() const noexcept { return linear_damping_; }
    void set_linear_damping(double d) noexcept { linear_damping_ = d; }

    [[nodiscard]] double angular_damping() const noexcept { return angular_damping_; }
    void set_angular_damping(double d) noexcept { angular_damping_ = d; }

    [[nodiscard]] double restitution() const noexcept { return restitution_; }
    void set_restitution(double r) noexcept { restitution_ = r; }

    [[nodiscard]] double friction() const noexcept { return friction_; }
    void set_friction(double f) noexcept { friction_ = f; }

    [[nodiscard]] bool is_sleeping() const noexcept { return is_sleeping_; }
    void set_sleeping(bool s) noexcept { is_sleeping_ = s; }

    [[nodiscard]] Matrix3x3 world_inv_inertia() const noexcept;
    [[nodiscard]] AABB compute_world_aabb() const noexcept;

    void apply_force_center(const Vector3d& force) noexcept;
    void apply_force_at_world_point(const Vector3d& force, const Vector3d& point) noexcept;
    void apply_torque(const Vector3d& torque) noexcept;
    void apply_impulse_center(const Vector3d& impulse) noexcept;
    void apply_impulse_at_world_point(const Vector3d& impulse, const Vector3d& point) noexcept;

    void clear_accumulators() noexcept;

    [[nodiscard]] const Vector3d& force_accumulator() const noexcept { return force_accum_; }
    [[nodiscard]] const Vector3d& torque_accumulator() const noexcept { return torque_accum_; }

    [[nodiscard]] Vector3d local_to_world_point(const Vector3d& local_pt) const noexcept;
    [[nodiscard]] Vector3d world_to_local_point(const Vector3d& world_pt) const noexcept;
    [[nodiscard]] Vector3d point_velocity(const Vector3d& world_point) const noexcept;

private:
    std::string id_;
    BodyType type_ = BodyType::Dynamic;
    RigidBodyState state_;
    MassProperties mass_props_;
    CollisionShape shape_ = BoxShape{};
    double linear_damping_ = 0.01;
    double angular_damping_ = 0.01;
    double restitution_ = 0.2;
    double friction_ = 0.4;
    bool is_sleeping_ = false;
    int sleep_counter_ = 0;

    Vector3d force_accum_{0.0, 0.0, 0.0};
    Vector3d torque_accum_{0.0, 0.0, 0.0};

    friend class PhysicsWorld;
};

// =============================================================================
// MEC-05: Joints & Constraints
// =============================================================================

enum class JointType {
    Fixed,
    BallSocket,
    Hinge,
    Slider,
    SpringDamper
};

struct JointDefinition {
    std::string joint_id;
    JointType type = JointType::Fixed;
    std::string body_a_id;
    std::string body_b_id;
    Vector3d local_anchor_a{0.0, 0.0, 0.0};
    Vector3d local_anchor_b{0.0, 0.0, 0.0};
    Vector3d joint_axis{0.0, 1.0, 0.0}; // for hinge or slider

    // Parameters for spring-damper
    double spring_stiffness = 100.0;
    double spring_damping = 5.0;
    double rest_length = 1.0;

    // Angle/distance limits
    bool enable_limits = false;
    double lower_limit = -base::math::PI;
    double upper_limit = base::math::PI;
};

// =============================================================================
// MEC-07: Energy and Momentum Diagnostics
// =============================================================================

struct MechanicsDiagnostics {
    Vector3d total_linear_momentum{0.0, 0.0, 0.0};
    Vector3d total_angular_momentum{0.0, 0.0, 0.0};
    double kinetic_energy_translational = 0.0;
    double kinetic_energy_rotational = 0.0;
    double potential_energy_gravity = 0.0;
    double total_mechanical_energy = 0.0;
    double energy_drift_ratio = 0.0; // compared to initial
};

// =============================================================================
// MEC-08: Mechanics Visualisation & Telemetry
// =============================================================================

struct TelemetrySample {
    double timestamp = 0.0;
    Vector3d position{0.0, 0.0, 0.0};
    Vector3d linear_velocity{0.0, 0.0, 0.0};
    double kinetic_energy = 0.0;
};

struct ContactVisualizerData {
    Vector3d point{0.0, 0.0, 0.0};
    Vector3d normal{0.0, 1.0, 0.0};
    double impulse = 0.0;
};

// =============================================================================
// MEC-10: Physics World & Simulation Core
// =============================================================================

struct PhysicsWorldConfig {
    Vector3d gravity{0.0, -9.80665, 0.0};
    int solver_iterations = 10;
    double baumgarte_factor = 0.2;
    double penetration_slop = 0.005;
    bool enable_sleeping = true;
    double sleep_linear_threshold = 0.05;
    double sleep_angular_threshold = 0.05;
    int sleep_steps_to_deactivate = 30;
};

class PhysicsWorld {
public:
    explicit PhysicsWorld(PhysicsWorldConfig config = {});

    void add_body(std::shared_ptr<RigidBody> body);
    void remove_body(const std::string& id);
    [[nodiscard]] std::shared_ptr<RigidBody> get_body(const std::string& id) const;
    [[nodiscard]] const std::vector<std::shared_ptr<RigidBody>>& bodies() const noexcept { return bodies_; }

    void add_joint(const JointDefinition& joint);
    void remove_joint(const std::string& joint_id);
    [[nodiscard]] const std::vector<JointDefinition>& joints() const noexcept { return joints_; }

    void step(double dt);

    [[nodiscard]] double current_time() const noexcept { return current_time_; }
    [[nodiscard]] std::uint64_t step_count() const noexcept { return step_count_; }

    [[nodiscard]] MechanicsDiagnostics compute_diagnostics() const;
    [[nodiscard]] const std::vector<ContactManifold>& active_contacts() const noexcept { return active_contacts_; }
    [[nodiscard]] const std::vector<ContactVisualizerData>& contact_visualizers() const noexcept { return contact_visuals_; }

    void reset();

    // Telemetry recorder for bodies
    void record_telemetry();
    [[nodiscard]] const std::vector<TelemetrySample>& body_telemetry(const std::string& body_id) const;

private:
    void integrate_forces(double dt);
    void detect_collisions();
    void solve_constraints(double dt);
    void integrate_velocities(double dt);
    void update_sleeping(double dt);

    PhysicsWorldConfig config_;
    std::vector<std::shared_ptr<RigidBody>> bodies_;
    std::vector<JointDefinition> joints_;
    std::vector<ContactManifold> active_contacts_;
    std::vector<ContactVisualizerData> contact_visuals_;
    double current_time_ = 0.0;
    std::uint64_t step_count_ = 0;
    std::optional<double> initial_energy_;
    std::unordered_map<std::string, std::vector<TelemetrySample>> telemetry_history_;
};

// =============================================================================
// MEC-09: Validation Benchmark Suite Factory
// =============================================================================

class MechanicsBenchmarkFactory {
public:
    /// Analytical free fall under constant gravity
    [[nodiscard]] static PhysicsWorld create_free_fall_benchmark(double initial_height, double mass = 1.0);

    /// Harmonic spring-mass oscillator
    [[nodiscard]] static PhysicsWorld create_spring_oscillator_benchmark(double mass, double stiffness, double initial_displacement);

    /// Single rigid pendulum
    [[nodiscard]] static PhysicsWorld create_pendulum_benchmark(double length, double mass, double initial_angle_rad);

    /// Two-body elastic collision along 1D axis
    [[nodiscard]] static PhysicsWorld create_elastic_collision_benchmark(double m1, double v1, double m2, double v2);
};

} // namespace shunya::core::domain::mechanics
