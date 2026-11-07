#include "shunya/core/domain/mechanics.hpp"

#include <cmath>
#include <limits>
#include <algorithm>

namespace shunya::core::domain::mechanics {

namespace {

// Cross product for Vector3d
[[nodiscard]] constexpr Vector3d cross(const Vector3d& a, const Vector3d& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// Dot product for Vector3d
[[nodiscard]] constexpr double dot(const Vector3d& a, const Vector3d& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

// Vector norm / length
[[nodiscard]] inline double length(const Vector3d& v) noexcept {
    return std::sqrt(dot(v, v));
}

// Normalize Vector3d
[[nodiscard]] inline Vector3d normalize(const Vector3d& v) noexcept {
    const double len = length(v);
    if (len < 1e-12) {
        return {0.0, 1.0, 0.0};
    }
    return {v.x / len, v.y / len, v.z / len};
}

// Quaternion multiplication
[[nodiscard]] Quaterniond mult_quat(const Quaterniond& q1, const Quaterniond& q2) noexcept {
    return {
        q1.w * q2.w - q1.x * q2.x - q1.y * q2.y - q1.z * q2.z,
        q1.w * q2.x + q1.x * q2.w + q1.y * q2.z - q1.z * q2.y,
        q1.w * q2.y - q1.x * q2.z + q1.y * q2.w + q1.z * q2.x,
        q1.w * q2.z + q1.x * q2.y - q1.y * q2.x + q1.z * q2.w
    };
}

} // namespace

// =============================================================================
// MassProperties
// =============================================================================

MassProperties MassProperties::create_solid_sphere(double mass, double radius) noexcept {
    MassProperties p;
    p.mass = std::max(mass, 1e-6);
    p.inv_mass = 1.0 / p.mass;
    p.center_of_mass = {0.0, 0.0, 0.0};
    const double i_val = 0.4 * p.mass * radius * radius;
    p.local_inertia = Matrix3x3::diagonal(i_val, i_val, i_val);
    p.inv_local_inertia = p.local_inertia.inverse();
    return p;
}

MassProperties MassProperties::create_solid_box(double mass, const Vector3d& half_extents) noexcept {
    MassProperties p;
    p.mass = std::max(mass, 1e-6);
    p.inv_mass = 1.0 / p.mass;
    p.center_of_mass = {0.0, 0.0, 0.0};
    const double a = half_extents.x;
    const double b = half_extents.y;
    const double c = half_extents.z;
    const double factor = p.mass / 3.0; // using half_extents: (1/12)*m*( (2b)^2 + (2c)^2 ) = (1/3)*m*(b^2 + c^2)
    const double ix = factor * (b * b + c * c);
    const double iy = factor * (a * a + c * c);
    const double iz = factor * (a * a + b * b);
    p.local_inertia = Matrix3x3::diagonal(ix, iy, iz);
    p.inv_local_inertia = p.local_inertia.inverse();
    return p;
}

MassProperties MassProperties::create_solid_cylinder(double mass, double radius, double height) noexcept {
    MassProperties p;
    p.mass = std::max(mass, 1e-6);
    p.inv_mass = 1.0 / p.mass;
    p.center_of_mass = {0.0, 0.0, 0.0};
    const double iy = 0.5 * p.mass * radius * radius;
    const double ixz = (1.0 / 12.0) * p.mass * (3.0 * radius * radius + height * height);
    p.local_inertia = Matrix3x3::diagonal(ixz, iy, ixz);
    p.inv_local_inertia = p.local_inertia.inverse();
    return p;
}

// =============================================================================
// AABB
// =============================================================================

bool AABB::intersects(const AABB& other) const noexcept {
    if (max_point.x < other.min_point.x || min_point.x > other.max_point.x) return false;
    if (max_point.y < other.min_point.y || min_point.y > other.max_point.y) return false;
    if (max_point.z < other.min_point.z || min_point.z > other.max_point.z) return false;
    return true;
}

bool AABB::contains(const Vector3d& pt) const noexcept {
    return pt.x >= min_point.x && pt.x <= max_point.x &&
           pt.y >= min_point.y && pt.y <= max_point.y &&
           pt.z >= min_point.z && pt.z <= max_point.z;
}

void AABB::expand(const Vector3d& pt) noexcept {
    min_point.x = std::min(min_point.x, pt.x);
    min_point.y = std::min(min_point.y, pt.y);
    min_point.z = std::min(min_point.z, pt.z);
    max_point.x = std::max(max_point.x, pt.x);
    max_point.y = std::max(max_point.y, pt.y);
    max_point.z = std::max(max_point.z, pt.z);
}

// =============================================================================
// RigidBody
// =============================================================================

RigidBody::RigidBody(std::string id, BodyType type)
    : id_(std::move(id)), type_(type) {
    mass_props_ = MassProperties::create_solid_box(1.0, Vector3d{0.5, 0.5, 0.5});
    if (type_ == BodyType::Static) {
        mass_props_.mass = 0.0;
        mass_props_.inv_mass = 0.0;
        mass_props_.inv_local_inertia = Matrix3x3::zero();
    }
}

void RigidBody::set_type(BodyType type) noexcept {
    type_ = type;
    if (type_ == BodyType::Static) {
        mass_props_.inv_mass = 0.0;
        mass_props_.inv_local_inertia = Matrix3x3::zero();
        state_.linear_velocity = {0.0, 0.0, 0.0};
        state_.angular_velocity = {0.0, 0.0, 0.0};
    } else if (mass_props_.mass > 0.0) {
        mass_props_.inv_mass = 1.0 / mass_props_.mass;
        mass_props_.inv_local_inertia = mass_props_.local_inertia.inverse();
    }
}

void RigidBody::set_mass_properties(const MassProperties& props) noexcept {
    mass_props_ = props;
    if (type_ == BodyType::Static) {
        mass_props_.inv_mass = 0.0;
        mass_props_.inv_local_inertia = Matrix3x3::zero();
    }
}

Matrix3x3 RigidBody::world_inv_inertia() const noexcept {
    if (type_ == BodyType::Static || mass_props_.inv_mass == 0.0) {
        return Matrix3x3::zero();
    }
    const Matrix3x3 r = Matrix3x3::from_quaternion(state_.orientation);
    return r * mass_props_.inv_local_inertia * r.transpose();
}

AABB RigidBody::compute_world_aabb() const noexcept {
    AABB aabb;
    std::visit([&](const auto& s) {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, SphereShape>) {
            aabb.min_point = state_.position - Vector3d{s.radius, s.radius, s.radius};
            aabb.max_point = state_.position + Vector3d{s.radius, s.radius, s.radius};
        } else if constexpr (std::is_same_v<T, BoxShape>) {
            const Matrix3x3 r = Matrix3x3::from_quaternion(state_.orientation);
            Vector3d ex = {
                std::abs(r.at(0,0)) * s.half_extents.x + std::abs(r.at(0,1)) * s.half_extents.y + std::abs(r.at(0,2)) * s.half_extents.z,
                std::abs(r.at(1,0)) * s.half_extents.x + std::abs(r.at(1,1)) * s.half_extents.y + std::abs(r.at(1,2)) * s.half_extents.z,
                std::abs(r.at(2,0)) * s.half_extents.x + std::abs(r.at(2,1)) * s.half_extents.y + std::abs(r.at(2,2)) * s.half_extents.z
            };
            aabb.min_point = state_.position - ex;
            aabb.max_point = state_.position + ex;
        } else if constexpr (std::is_same_v<T, CapsuleShape>) {
            const double rad = s.radius + s.half_height;
            aabb.min_point = state_.position - Vector3d{rad, rad, rad};
            aabb.max_point = state_.position + Vector3d{rad, rad, rad};
        } else if constexpr (std::is_same_v<T, PlaneShape>) {
            // Infinite plane approximation for bounding box
            constexpr double huge_extent = 1e6;
            aabb.min_point = {-huge_extent, -huge_extent, -huge_extent};
            aabb.max_point = {huge_extent, huge_extent, huge_extent};
        }
    }, shape_);
    return aabb;
}

void RigidBody::apply_force_center(const Vector3d& force) noexcept {
    if (type_ != BodyType::Dynamic) return;
    force_accum_ = force_accum_ + force;
    is_sleeping_ = false;
    sleep_counter_ = 0;
}

void RigidBody::apply_force_at_world_point(const Vector3d& force, const Vector3d& point) noexcept {
    if (type_ != BodyType::Dynamic) return;
    force_accum_ = force_accum_ + force;
    const Vector3d r = point - state_.position;
    torque_accum_ = torque_accum_ + cross(r, force);
    is_sleeping_ = false;
    sleep_counter_ = 0;
}

void RigidBody::apply_torque(const Vector3d& torque) noexcept {
    if (type_ != BodyType::Dynamic) return;
    torque_accum_ = torque_accum_ + torque;
    is_sleeping_ = false;
    sleep_counter_ = 0;
}

void RigidBody::apply_impulse_center(const Vector3d& impulse) noexcept {
    if (type_ != BodyType::Dynamic) return;
    state_.linear_velocity = state_.linear_velocity + impulse * mass_props_.inv_mass;
    is_sleeping_ = false;
    sleep_counter_ = 0;
}

void RigidBody::apply_impulse_at_world_point(const Vector3d& impulse, const Vector3d& point) noexcept {
    if (type_ != BodyType::Dynamic) return;
    state_.linear_velocity = state_.linear_velocity + impulse * mass_props_.inv_mass;
    const Vector3d r = point - state_.position;
    const Vector3d torque_impulse = cross(r, impulse);
    state_.angular_velocity = state_.angular_velocity + world_inv_inertia() * torque_impulse;
    is_sleeping_ = false;
    sleep_counter_ = 0;
}

void RigidBody::clear_accumulators() noexcept {
    force_accum_ = {0.0, 0.0, 0.0};
    torque_accum_ = {0.0, 0.0, 0.0};
}

Vector3d RigidBody::local_to_world_point(const Vector3d& local_pt) const noexcept {
    const Matrix3x3 r = Matrix3x3::from_quaternion(state_.orientation);
    return state_.position + (r * local_pt);
}

Vector3d RigidBody::world_to_local_point(const Vector3d& world_pt) const noexcept {
    const Matrix3x3 r = Matrix3x3::from_quaternion(state_.orientation);
    return r.transpose() * (world_pt - state_.position);
}

Vector3d RigidBody::point_velocity(const Vector3d& world_point) const noexcept {
    if (type_ == BodyType::Static) {
        return {0.0, 0.0, 0.0};
    }
    const Vector3d r = world_point - state_.position;
    return state_.linear_velocity + cross(state_.angular_velocity, r);
}

// =============================================================================
// PhysicsWorld Implementation
// =============================================================================

PhysicsWorld::PhysicsWorld(PhysicsWorldConfig config)
    : config_(config) {}

void PhysicsWorld::add_body(std::shared_ptr<RigidBody> body) {
    if (body) {
        bodies_.push_back(std::move(body));
    }
}

void PhysicsWorld::remove_body(const std::string& id) {
    bodies_.erase(
        std::remove_if(bodies_.begin(), bodies_.end(),
                       [&](const auto& b) { return b->id() == id; }),
        bodies_.end()
    );
}

std::shared_ptr<RigidBody> PhysicsWorld::get_body(const std::string& id) const {
    for (const auto& b : bodies_) {
        if (b->id() == id) return b;
    }
    return nullptr;
}

void PhysicsWorld::add_joint(const JointDefinition& joint) {
    joints_.push_back(joint);
}

void PhysicsWorld::remove_joint(const std::string& joint_id) {
    joints_.erase(
        std::remove_if(joints_.begin(), joints_.end(),
                       [&](const auto& j) { return j.joint_id == joint_id; }),
        joints_.end()
    );
}

void PhysicsWorld::reset() {
    active_contacts_.clear();
    contact_visuals_.clear();
    telemetry_history_.clear();
    current_time_ = 0.0;
    step_count_ = 0;
    initial_energy_.reset();
}

void PhysicsWorld::record_telemetry() {
    for (const auto& body : bodies_) {
        const double v_sq = dot(body->linear_velocity(), body->linear_velocity());
        const double ke = 0.5 * body->mass_properties().mass * v_sq;
        telemetry_history_[body->id()].push_back(TelemetrySample{
            current_time_,
            body->position(),
            body->linear_velocity(),
            ke
        });
    }
}

const std::vector<TelemetrySample>& PhysicsWorld::body_telemetry(const std::string& body_id) const {
    static const std::vector<TelemetrySample> empty;
    auto it = telemetry_history_.find(body_id);
    return (it != telemetry_history_.end()) ? it->second : empty;
}

// -----------------------------------------------------------------------------
// Force Integration
// -----------------------------------------------------------------------------

void PhysicsWorld::integrate_forces(double dt) {
    for (auto& body : bodies_) {
        if (body->type() != BodyType::Dynamic || body->is_sleeping()) {
            continue;
        }

        // Add gravity force
        const Vector3d gravity_force = config_.gravity * body->mass_props_.mass;
        body->force_accum_ = body->force_accum_ + gravity_force;

        // Linear velocity integration: v += (F / m) * dt
        const Vector3d lin_acc = body->force_accum_ * body->mass_props_.inv_mass;
        body->state_.linear_velocity = body->state_.linear_velocity + lin_acc * dt;

        // Damping
        const double lin_damp = std::clamp(1.0 - body->linear_damping_ * dt, 0.0, 1.0);
        body->state_.linear_velocity = body->state_.linear_velocity * lin_damp;

        // Angular velocity integration: w += (I_inv * torque) * dt
        const Matrix3x3 inv_i = body->world_inv_inertia();
        const Vector3d ang_acc = inv_i * body->torque_accum_;
        body->state_.angular_velocity = body->state_.angular_velocity + ang_acc * dt;

        // Damping
        const double ang_damp = std::clamp(1.0 - body->angular_damping_ * dt, 0.0, 1.0);
        body->state_.angular_velocity = body->state_.angular_velocity * ang_damp;

        body->clear_accumulators();
    }
}

// -----------------------------------------------------------------------------
// Collision Detection (Broad & Narrow Phase)
// -----------------------------------------------------------------------------

void PhysicsWorld::detect_collisions() {
    active_contacts_.clear();
    contact_visuals_.clear();

    const std::size_t count = bodies_.size();
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t j = i + 1; j < count; ++j) {
            auto& bA = bodies_[i];
            auto& bB = bodies_[j];

            // Skip static-static collisions
            if (bA->type() == BodyType::Static && bB->type() == BodyType::Static) {
                continue;
            }
            if (bA->is_sleeping() && bB->is_sleeping()) {
                continue;
            }

            // Broad-phase: AABB intersection
            const AABB aabbA = bA->compute_world_aabb();
            const AABB aabbB = bB->compute_world_aabb();
            if (!aabbA.intersects(aabbB)) {
                continue;
            }

            // Narrow-phase
            ContactManifold manifold;
            manifold.body_a_id = bA->id();
            manifold.body_b_id = bB->id();
            manifold.restitution = std::min(bA->restitution(), bB->restitution());
            manifold.static_friction = std::sqrt(bA->friction() * bB->friction());
            manifold.dynamic_friction = std::sqrt(bA->friction() * bB->friction());

            // 1. Sphere - Sphere
            if (std::holds_alternative<SphereShape>(bA->shape()) &&
                std::holds_alternative<SphereShape>(bB->shape())) {
                const auto& sA = std::get<SphereShape>(bA->shape());
                const auto& sB = std::get<SphereShape>(bB->shape());

                const Vector3d diff = bA->position() - bB->position();
                const double dist = length(diff);
                const double sum_r = sA.radius + sB.radius;

                if (dist < sum_r) {
                    const Vector3d norm = (dist > 1e-8) ? (diff / dist) : Vector3d{0.0, 1.0, 0.0};
                    const double pen = sum_r - dist;
                    const Vector3d pt = bB->position() + norm * sB.radius;

                    manifold.contacts.push_back(ContactPoint{pt, norm, pen, 0.0, 0.0});
                    active_contacts_.push_back(manifold);
                }
            }
            // 2. Sphere - Plane
            else if (std::holds_alternative<SphereShape>(bA->shape()) &&
                     std::holds_alternative<PlaneShape>(bB->shape())) {
                const auto& sA = std::get<SphereShape>(bA->shape());
                const auto& plB = std::get<PlaneShape>(bB->shape());

                const Vector3d p_norm = normalize(plB.normal);
                const double d = dot(bA->position(), p_norm) - plB.offset;

                if (d < sA.radius) {
                    const double pen = sA.radius - d;
                    const Vector3d pt = bA->position() - p_norm * d;
                    manifold.contacts.push_back(ContactPoint{pt, p_norm, pen, 0.0, 0.0});
                    active_contacts_.push_back(manifold);
                }
            }
            else if (std::holds_alternative<PlaneShape>(bA->shape()) &&
                     std::holds_alternative<SphereShape>(bB->shape())) {
                const auto& plA = std::get<PlaneShape>(bA->shape());
                const auto& sB = std::get<SphereShape>(bB->shape());

                const Vector3d p_norm = normalize(plA.normal);
                const double d = dot(bB->position(), p_norm) - plA.offset;

                if (d < sB.radius) {
                    const double pen = sB.radius - d;
                    const Vector3d norm = p_norm * -1.0; // from B to A
                    const Vector3d pt = bB->position() - p_norm * d;
                    manifold.contacts.push_back(ContactPoint{pt, norm, pen, 0.0, 0.0});
                    active_contacts_.push_back(manifold);
                }
            }
            // 3. Box - Box (SAT simplified)
            else if (std::holds_alternative<BoxShape>(bA->shape()) &&
                     std::holds_alternative<BoxShape>(bB->shape())) {
                const auto& bShapeA = std::get<BoxShape>(bA->shape());
                const auto& bShapeB = std::get<BoxShape>(bB->shape());

                const Vector3d delta = bA->position() - bB->position();
                const double overlap_x = (bShapeA.half_extents.x + bShapeB.half_extents.x) - std::abs(delta.x);
                const double overlap_y = (bShapeA.half_extents.y + bShapeB.half_extents.y) - std::abs(delta.y);
                const double overlap_z = (bShapeA.half_extents.z + bShapeB.half_extents.z) - std::abs(delta.z);

                if (overlap_x > 0.0 && overlap_y > 0.0 && overlap_z > 0.0) {
                    Vector3d norm{0.0, 1.0, 0.0};
                    double min_overlap = overlap_y;

                    if (overlap_x < min_overlap) {
                        min_overlap = overlap_x;
                        norm = {delta.x > 0.0 ? 1.0 : -1.0, 0.0, 0.0};
                    }
                    if (overlap_z < min_overlap) {
                        min_overlap = overlap_z;
                        norm = {0.0, 0.0, delta.z > 0.0 ? 1.0 : -1.0};
                    }
                    if (min_overlap == overlap_y) {
                        norm = {0.0, delta.y > 0.0 ? 1.0 : -1.0, 0.0};
                    }

                    const Vector3d pt = (bA->position() + bB->position()) * 0.5;
                    manifold.contacts.push_back(ContactPoint{pt, norm, min_overlap, 0.0, 0.0});
                    active_contacts_.push_back(manifold);
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Constraint & Joint Solver (Sequential Impulses)
// -----------------------------------------------------------------------------

void PhysicsWorld::solve_constraints(double dt) {
    if (dt <= 1e-12) return;

    // 1. Solve Joint constraints
    for (const auto& joint : joints_) {
        auto bA = get_body(joint.body_a_id);
        auto bB = get_body(joint.body_b_id);
        if (!bA && !bB) continue;

        const Vector3d posA = bA ? bA->local_to_world_point(joint.local_anchor_a) : joint.local_anchor_a;
        const Vector3d posB = bB ? bB->local_to_world_point(joint.local_anchor_b) : joint.local_anchor_b;

        if (joint.type == JointType::SpringDamper) {
            const Vector3d diff = posA - posB;
            const double current_dist = length(diff);
            if (current_dist > 1e-8) {
                const Vector3d u = diff / current_dist;
                const double stretch = current_dist - joint.rest_length;
                const Vector3d vA = bA ? bA->point_velocity(posA) : Vector3d{0.0, 0.0, 0.0};
                const Vector3d vB = bB ? bB->point_velocity(posB) : Vector3d{0.0, 0.0, 0.0};
                const double rel_vel = dot(vA - vB, u);

                const double f_mag = -joint.spring_stiffness * stretch - joint.spring_damping * rel_vel;
                const Vector3d f = u * f_mag;
                const Vector3d impulse = f * dt;

                if (bA && bA->type() == BodyType::Dynamic) {
                    bA->apply_impulse_at_world_point(impulse, posA);
                }
                if (bB && bB->type() == BodyType::Dynamic) {
                    bB->apply_impulse_at_world_point(impulse * -1.0, posB);
                }
            }
        } else if (joint.type == JointType::BallSocket || joint.type == JointType::Fixed || joint.type == JointType::Hinge) {
            // Anchor point coincidence constraint (Position correction / Baumgarte impulse)
            const Vector3d err = posA - posB;
            const double err_len = length(err);
            if (err_len > 1e-6) {
                const Vector3d dir = err / err_len;
                const double inv_mA = bA ? bA->mass_properties().inv_mass : 0.0;
                const double inv_mB = bB ? bB->mass_properties().inv_mass : 0.0;
                const double total_inv_m = inv_mA + inv_mB;

                if (total_inv_m > 1e-12) {
                    const double impulse_mag = (err_len * 0.5) / (total_inv_m * dt);
                    const Vector3d impulse = dir * impulse_mag;
                    if (bA && bA->type() == BodyType::Dynamic) {
                        bA->apply_impulse_at_world_point(impulse * -1.0, posA);
                    }
                    if (bB && bB->type() == BodyType::Dynamic) {
                        bB->apply_impulse_at_world_point(impulse, posB);
                    }
                }
            }
        }
    }

    // 2. Solve Contact Constraints (Iterative Sequential Impulses)
    for (int iter = 0; iter < config_.solver_iterations; ++iter) {
        for (auto& manifold : active_contacts_) {
            auto bA = get_body(manifold.body_a_id);
            auto bB = get_body(manifold.body_b_id);
            if (!bA || !bB) continue;

            const double inv_mA = bA->mass_properties().inv_mass;
            const double inv_mB = bB->mass_properties().inv_mass;
            if (inv_mA == 0.0 && inv_mB == 0.0) continue;

            const Matrix3x3 inv_IA = bA->world_inv_inertia();
            const Matrix3x3 inv_IB = bB->world_inv_inertia();

            for (auto& cp : manifold.contacts) {
                const Vector3d rA = cp.world_position - bA->position();
                const Vector3d rB = cp.world_position - bB->position();

                // Relative velocity at contact point
                const Vector3d vA = bA->linear_velocity() + cross(bA->angular_velocity(), rA);
                const Vector3d vB = bB->linear_velocity() + cross(bB->angular_velocity(), rB);
                const Vector3d v_rel = vA - vB;

                const double vn = dot(v_rel, cp.normal);

                // Effective mass in normal direction
                const Vector3d rA_x_n = cross(rA, cp.normal);
                const Vector3d rB_x_n = cross(rB, cp.normal);
                const double kn = inv_mA + inv_mB +
                    dot(cross(inv_IA * rA_x_n, rA), cp.normal) +
                    dot(cross(inv_IB * rB_x_n, rB), cp.normal);

                if (kn < 1e-12) continue;

                // Baumgarte position stabilization
                const double penetration = std::max(0.0, cp.penetration_depth - config_.penetration_slop);
                const double bias = (config_.baumgarte_factor / dt) * penetration;

                // Calculate normal impulse delta
                const double restitution_term = (vn < -0.1) ? manifold.restitution * vn : 0.0;
                double delta_lambda = -(vn + restitution_term - bias) / kn;

                // Accumulate and clamp normal impulse
                const double old_lambda = cp.normal_impulse;
                cp.normal_impulse = std::max(0.0, old_lambda + delta_lambda);
                delta_lambda = cp.normal_impulse - old_lambda;

                const Vector3d impulse_n = cp.normal * delta_lambda;
                bA->apply_impulse_at_world_point(impulse_n, cp.world_position);
                bB->apply_impulse_at_world_point(impulse_n * -1.0, cp.world_position);

                // Tangential Friction Impulse
                Vector3d tangent = v_rel - cp.normal * vn;
                const double t_len = length(tangent);
                if (t_len > 1e-6) {
                    tangent = tangent / t_len;
                    const Vector3d rA_x_t = cross(rA, tangent);
                    const Vector3d rB_x_t = cross(rB, tangent);
                    const double kt = inv_mA + inv_mB +
                        dot(cross(inv_IA * rA_x_t, rA), tangent) +
                        dot(cross(inv_IB * rB_x_t, rB), tangent);

                    if (kt > 1e-12) {
                        const double vt = dot(v_rel, tangent);
                        double delta_tangent = -vt / kt;
                        const double max_friction = manifold.static_friction * cp.normal_impulse;

                        const double old_t_impulse = cp.tangent_impulse;
                        cp.tangent_impulse = std::clamp(old_t_impulse + delta_tangent, -max_friction, max_friction);
                        delta_tangent = cp.tangent_impulse - old_t_impulse;

                        const Vector3d impulse_t = tangent * delta_tangent;
                        bA->apply_impulse_at_world_point(impulse_t, cp.world_position);
                        bB->apply_impulse_at_world_point(impulse_t * -1.0, cp.world_position);
                    }
                }

                // Add to visualizers on final iteration
                if (iter == config_.solver_iterations - 1) {
                    contact_visuals_.push_back(ContactVisualizerData{
                        cp.world_position,
                        cp.normal,
                        cp.normal_impulse
                    });
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Velocity & Position Integration
// -----------------------------------------------------------------------------

void PhysicsWorld::integrate_velocities(double dt) {
    for (auto& body : bodies_) {
        if (body->type() != BodyType::Dynamic || body->is_sleeping()) {
            continue;
        }

        // Integrate linear position: x += v * dt
        body->state_.position = body->state_.position + body->state_.linear_velocity * dt;

        // Integrate orientation: q += 0.5 * w_quat * q * dt
        const Vector3d w = body->state_.angular_velocity;
        const double w_len = length(w);
        if (w_len > 1e-12) {
            const Quaterniond w_quat{0.0, w.x, w.y, w.z};
            const Quaterniond dq = mult_quat(w_quat, body->state_.orientation);
            Quaterniond q_new{
                body->state_.orientation.w + 0.5 * dt * dq.w,
                body->state_.orientation.x + 0.5 * dt * dq.x,
                body->state_.orientation.y + 0.5 * dt * dq.y,
                body->state_.orientation.z + 0.5 * dt * dq.z
            };
            body->set_orientation(q_new);
        }
    }
}

// -----------------------------------------------------------------------------
// Sleeping Bodies (MEC-10)
// -----------------------------------------------------------------------------

void PhysicsWorld::update_sleeping(double /*dt*/) {
    if (!config_.enable_sleeping) return;

    for (auto& body : bodies_) {
        if (body->type() != BodyType::Dynamic) continue;

        const double lin_spd = length(body->linear_velocity());
        const double ang_spd = length(body->angular_velocity());

        if (lin_spd < config_.sleep_linear_threshold &&
            ang_spd < config_.sleep_angular_threshold) {
            body->sleep_counter_++;
            if (body->sleep_counter_ >= config_.sleep_steps_to_deactivate) {
                body->set_sleeping(true);
                body->set_linear_velocity({0.0, 0.0, 0.0});
                body->set_angular_velocity({0.0, 0.0, 0.0});
            }
        } else {
            body->sleep_counter_ = 0;
            body->set_sleeping(false);
        }
    }
}

// -----------------------------------------------------------------------------
// Simulation Step
// -----------------------------------------------------------------------------

void PhysicsWorld::step(double dt) {
    if (dt <= 0.0) return;

    integrate_forces(dt);
    detect_collisions();
    solve_constraints(dt);
    integrate_velocities(dt);
    update_sleeping(dt);

    current_time_ += dt;
    step_count_++;

    record_telemetry();
}

// =============================================================================
// MEC-07: Diagnostics
// =============================================================================

MechanicsDiagnostics PhysicsWorld::compute_diagnostics() const {
    MechanicsDiagnostics diag;

    for (const auto& body : bodies_) {
        if (body->type() == BodyType::Static) continue;

        const double mass = body->mass_properties().mass;
        const Vector3d p = body->linear_velocity() * mass;
        diag.total_linear_momentum = diag.total_linear_momentum + p;

        const Vector3d r = body->position();
        const Matrix3x3 rot = Matrix3x3::from_quaternion(body->orientation());
        const Matrix3x3 world_i = rot * body->mass_properties().local_inertia * rot.transpose();
        const Vector3d l = cross(r, p) + world_i * body->angular_velocity();
        diag.total_angular_momentum = diag.total_angular_momentum + l;

        const double v_sq = dot(body->linear_velocity(), body->linear_velocity());
        const double ke_trans = 0.5 * mass * v_sq;
        diag.kinetic_energy_translational += ke_trans;

        const double ke_rot = 0.5 * dot(body->angular_velocity(), world_i * body->angular_velocity());
        diag.kinetic_energy_rotational += ke_rot;

        // Potential energy: -m * (g . y)
        const double pe = -mass * dot(config_.gravity, body->position());
        diag.potential_energy_gravity += pe;
    }

    diag.total_mechanical_energy = diag.kinetic_energy_translational +
                                   diag.kinetic_energy_rotational +
                                   diag.potential_energy_gravity;

    if (!initial_energy_.has_value()) {
        const_cast<PhysicsWorld*>(this)->initial_energy_ = diag.total_mechanical_energy;
    }

    if (std::abs(*initial_energy_) > 1e-9) {
        diag.energy_drift_ratio = std::abs((diag.total_mechanical_energy - *initial_energy_) / *initial_energy_);
    }

    return diag;
}

// =============================================================================
// MEC-09: Benchmark Factory
// =============================================================================

PhysicsWorld MechanicsBenchmarkFactory::create_free_fall_benchmark(double initial_height, double mass) {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, -9.80665, 0.0};
    cfg.enable_sleeping = false;

    PhysicsWorld world(cfg);
    auto body = std::make_shared<RigidBody>("faller", BodyType::Dynamic);
    body->set_position({0.0, initial_height, 0.0});
    body->set_linear_damping(0.0);
    body->set_angular_damping(0.0);
    body->set_mass_properties(MassProperties::create_solid_sphere(mass, 0.5));
    world.add_body(body);

    return world;
}

PhysicsWorld MechanicsBenchmarkFactory::create_spring_oscillator_benchmark(double mass, double stiffness, double initial_displacement) {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, 0.0, 0.0}; // zero gravity for pure harmonic oscillator
    cfg.enable_sleeping = false;

    PhysicsWorld world(cfg);

    // Anchor body (Static)
    auto anchor = std::make_shared<RigidBody>("anchor", BodyType::Static);
    anchor->set_position({0.0, 0.0, 0.0});
    world.add_body(anchor);

    // Mass body (Dynamic)
    auto bob = std::make_shared<RigidBody>("bob", BodyType::Dynamic);
    bob->set_position({initial_displacement, 0.0, 0.0});
    bob->set_linear_damping(0.0);
    bob->set_angular_damping(0.0);
    bob->set_mass_properties(MassProperties::create_solid_sphere(mass, 0.2));
    world.add_body(bob);

    // Spring joint
    JointDefinition spring;
    spring.joint_id = "spring1";
    spring.type = JointType::SpringDamper;
    spring.body_a_id = "anchor";
    spring.body_b_id = "bob";
    spring.local_anchor_a = {0.0, 0.0, 0.0};
    spring.local_anchor_b = {0.0, 0.0, 0.0};
    spring.rest_length = 0.0;
    spring.spring_stiffness = stiffness;
    spring.spring_damping = 0.0;
    world.add_joint(spring);

    return world;
}

PhysicsWorld MechanicsBenchmarkFactory::create_pendulum_benchmark(double length, double mass, double initial_angle_rad) {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, -9.80665, 0.0};
    cfg.enable_sleeping = false;

    PhysicsWorld world(cfg);

    // Pivot (Static)
    auto pivot = std::make_shared<RigidBody>("pivot", BodyType::Static);
    pivot->set_position({0.0, 0.0, 0.0});
    world.add_body(pivot);

    // Pendulum bob
    auto bob = std::make_shared<RigidBody>("bob", BodyType::Dynamic);
    const double x = length * std::sin(initial_angle_rad);
    const double y = -length * std::cos(initial_angle_rad);
    bob->set_position({x, y, 0.0});
    bob->set_linear_damping(0.0);
    bob->set_angular_damping(0.0);
    bob->set_mass_properties(MassProperties::create_solid_sphere(mass, 0.1));
    world.add_body(bob);

    // Fixed distance spring-damper joint to simulate pendulum rod
    JointDefinition rod;
    rod.joint_id = "rod";
    rod.type = JointType::SpringDamper;
    rod.body_a_id = "pivot";
    rod.body_b_id = "bob";
    rod.rest_length = length;
    rod.spring_stiffness = 50000.0; // stiff constraint
    rod.spring_damping = 2.0;
    world.add_joint(rod);

    return world;
}

PhysicsWorld MechanicsBenchmarkFactory::create_elastic_collision_benchmark(double m1, double v1, double m2, double v2) {
    PhysicsWorldConfig cfg;
    cfg.gravity = {0.0, 0.0, 0.0};
    cfg.enable_sleeping = false;

    PhysicsWorld world(cfg);

    auto b1 = std::make_shared<RigidBody>("ball1", BodyType::Dynamic);
    b1->set_position({-2.0, 0.0, 0.0});
    b1->set_linear_velocity({v1, 0.0, 0.0});
    b1->set_linear_damping(0.0);
    b1->set_restitution(1.0); // perfectly elastic
    b1->set_shape(SphereShape{0.5});
    b1->set_mass_properties(MassProperties::create_solid_sphere(m1, 0.5));
    world.add_body(b1);

    auto b2 = std::make_shared<RigidBody>("ball2", BodyType::Dynamic);
    b2->set_position({2.0, 0.0, 0.0});
    b2->set_linear_velocity({v2, 0.0, 0.0});
    b2->set_linear_damping(0.0);
    b2->set_restitution(1.0); // perfectly elastic
    b2->set_shape(SphereShape{0.5});
    b2->set_mass_properties(MassProperties::create_solid_sphere(m2, 0.5));
    world.add_body(b2);

    return world;
}

} // namespace shunya::core::domain::mechanics
