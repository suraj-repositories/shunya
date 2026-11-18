#include "shunya/physim/simulation_controller.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::physim {

void SimulationController::add_body(const RigidBodyEntity& body) {
    bodies_.push_back(body);
}

void SimulationController::add_revolute_joint(const RevoluteJointSpec& joint) {
    revolute_joints_.push_back(joint);
}

void SimulationController::add_prismatic_joint(const PrismaticJointSpec& joint) {
    prismatic_joints_.push_back(joint);
}

void SimulationController::add_ball_joint(const BallAndSocketJointSpec& joint) {
    ball_joints_.push_back(joint);
}

void SimulationController::add_wheel_joint(const WheelAxleJointSpec& joint) {
    wheel_joints_.push_back(joint);
}

void SimulationController::add_gear(const GearConstraintSpec& gear) {
    gears_.push_back(gear);
}

void SimulationController::add_pulley_belt(const PulleyBeltConstraintSpec& belt) {
    belts_.push_back(belt);
}

void SimulationController::add_manifold(const PhysimContactManifold& manifold) {
    manifolds_.push_back(manifold);
}

const RigidBodyEntity* SimulationController::find_body(const std::string& id) const noexcept {
    for (const auto& b : bodies_) {
        if (b.id() == id) return &b;
    }
    return nullptr;
}

RigidBodyEntity* SimulationController::find_body(const std::string& id) noexcept {
    for (auto& b : bodies_) {
        if (b.id() == id) return &b;
    }
    return nullptr;
}

void SimulationController::capture_initial_state() {
    initial_snapshot_.time_s = 0.0;
    initial_snapshot_.step_count = 0;
    initial_snapshot_.bodies = bodies_;
    initial_snapshot_.revolute_joints = revolute_joints_;
    initial_snapshot_.prismatic_joints = prismatic_joints_;
    initial_snapshot_.ball_joints = ball_joints_;
    initial_snapshot_.wheel_joints = wheel_joints_;
}

void SimulationController::reset_to_initial_state() {
    if (!initial_snapshot_.bodies.empty()) {
        bodies_ = initial_snapshot_.bodies;
        revolute_joints_ = initial_snapshot_.revolute_joints;
        prismatic_joints_ = initial_snapshot_.prismatic_joints;
        ball_joints_ = initial_snapshot_.ball_joints;
        wheel_joints_ = initial_snapshot_.wheel_joints;
    }
    current_time_s_ = 0.0;
    step_count_ = 0;
    history_.clear();
    release_grab();
}

void SimulationController::set_state(SimulationState new_state) {
    if (state_ != new_state) {
        const SimulationState old_state = state_;
        state_ = new_state;
        if (on_state_change_) {
            on_state_change_(old_state, new_state);
        }
    }
}

void SimulationController::play() {
    if (state_ == SimulationState::Stopped) {
        capture_initial_state();
    }
    set_state(SimulationState::Running);
}

void SimulationController::pause() {
    if (state_ == SimulationState::Running) {
        set_state(SimulationState::Paused);
    }
}

void SimulationController::stop() {
    set_state(SimulationState::Stopped);
    reset_to_initial_state();
}

void SimulationController::set_speed_scale(double scale) noexcept {
    config_.speed_scale = std::clamp(scale, 0.1, 10.0);
}

double SimulationController::calculate_effective_dt(double base_dt_s) const noexcept {
    double dt = base_dt_s * config_.speed_scale;

    if (config_.stepping_mode == TimeSteppingMode::Adaptive) {
        double max_speed = 0.0;
        for (const auto& b : bodies_) {
            if (b.is_dynamic()) {
                max_speed = std::max(max_speed, b.state().linear_velocity().length());
            }
        }
        if (max_speed > 1e-3) {
            const double adaptive_dt = config_.adaptive_cfl_factor / max_speed;
            dt = std::clamp(adaptive_dt, config_.min_adaptive_dt, config_.max_adaptive_dt);
        }
    }
    return dt;
}

void SimulationController::step_forward(double dt_s) {
    if (dt_s <= 0.0) return;
    if (state_ == SimulationState::Stopped && initial_snapshot_.bodies.empty()) {
        capture_initial_state();
    }
    const double eff_dt = calculate_effective_dt(dt_s);
    advance_physics_step(eff_dt);
}

void SimulationController::apply_mouse_grab_force() noexcept {
    if (!mouse_grab_.active) return;
    auto* b = find_body(mouse_grab_.body_id);
    if (!b || !b->is_dynamic()) return;

    auto& st = b->state();
    const Vector3d anchor_world = st.local_to_world_point(mouse_grab_.local_anchor);
    const Vector3d delta_pos = mouse_grab_.target_world_pos - anchor_world;
    const Vector3d anchor_vel = st.point_velocity(anchor_world);

    // Spring-damper force F = k * delta_pos - c * v
    const Vector3d f_grab = delta_pos * mouse_grab_.stiffness - anchor_vel * mouse_grab_.damping;
    st.apply_force_at_world_point(f_grab, anchor_world);
}

void SimulationController::advance_physics_step(double dt_s) {
    // 1. Record frame into history buffer for rewinding (PH4-09.04.006)
    SimulationSnapshot frame_snapshot;
    frame_snapshot.time_s = current_time_s_;
    frame_snapshot.step_count = step_count_;
    frame_snapshot.bodies = bodies_;
    frame_snapshot.revolute_joints = revolute_joints_;
    frame_snapshot.prismatic_joints = prismatic_joints_;
    frame_snapshot.ball_joints = ball_joints_;
    frame_snapshot.wheel_joints = wheel_joints_;

    if (history_.size() >= max_history_frames_) {
        history_.pop_front();
    }
    history_.push_back(std::move(frame_snapshot));

    // 2. Apply interactive mouse grab forces (PH4-09.04.004)
    apply_mouse_grab_force();

    // 3. Solve suspension forces before integration
    if (!wheel_joints_.empty()) {
        JointConstraintSolver::solve_wheel_suspension(bodies_, wheel_joints_, dt_s);
    }

    // 4. Step integration for all bodies
    RigidBodyIntegrator::step_system(bodies_, dt_s, IntegratorScheme::SymplecticEuler, config_.gravity);

    // 5. Solve multi-body joint constraints
    if (!revolute_joints_.empty()) {
        JointConstraintSolver::solve_revolute_joints(bodies_, revolute_joints_, dt_s, config_.joint_iterations);
    }
    if (!prismatic_joints_.empty()) {
        JointConstraintSolver::solve_prismatic_joints(bodies_, prismatic_joints_, dt_s, config_.joint_iterations);
    }
    if (!ball_joints_.empty()) {
        JointConstraintSolver::solve_ball_joints(bodies_, ball_joints_, dt_s, config_.joint_iterations);
    }
    if (!gears_.empty()) {
        JointConstraintSolver::solve_gear_constraints(bodies_, gears_, dt_s);
    }
    if (!belts_.empty()) {
        JointConstraintSolver::solve_pulley_belt_constraints(bodies_, belts_, dt_s);
    }

    // 6. Solve contacts if any
    if (!manifolds_.empty()) {
        PgsSolverOptions opts;
        opts.iterations = config_.pgs_iterations;
        opts.penetration_slop = config_.penetration_slop;
        opts.restitution_velocity_threshold = config_.restitution_threshold;
        opts.baumgarte_factor = config_.baumgarte_beta;
        opts.enable_warm_starting = config_.enable_warm_starting;
        PgsContactSolver::solve_contacts(bodies_, manifolds_, dt_s, opts);
    }

    current_time_s_ += dt_s;
    ++step_count_;

    if (on_step_) {
        on_step_(current_time_s_, step_count_);
    }
}

// =============================================================================
// Visual Debug Rendering (PH4-09.04.003)
// =============================================================================

DebugRenderData SimulationController::extract_debug_render_data() const noexcept {
    DebugRenderData data;

    // 1. Center of mass glyphs and velocity vectors
    for (const auto& b : bodies_) {
        const auto& st = b.state();
        DebugGlyph glyph;
        glyph.position = st.position;
        glyph.radius = 0.03;
        glyph.label = b.id() + "_com";
        data.com_glyphs.push_back(glyph);

        const Vector3d vel = st.linear_velocity();
        if (vel.length() > 0.01) {
            DebugArrow arr;
            arr.origin = st.position;
            arr.vector = vel * 0.2; // scale for visualization
            arr.label = b.id() + "_vel";
            arr.color_rgba = 0x00FF00FF; // Green
            data.velocity_vectors.push_back(arr);
        }
    }

    // 2. Contact normal arrows
    for (const auto& m : manifolds_) {
        for (const auto& cp : m.contacts) {
            DebugArrow arr;
            arr.origin = cp.world_position;
            arr.vector = cp.normal * 0.3;
            arr.label = "contact_n";
            arr.color_rgba = 0xFF0000FF; // Red
            data.contact_normals.push_back(arr);
        }
    }

    // 3. Joint axes
    for (const auto& j : revolute_joints_) {
        const auto* a = find_body(j.body_a_id);
        if (a) {
            DebugArrow arr;
            arr.origin = a->state().local_to_world_point(j.local_anchor_a);
            arr.vector = a->state().local_to_world_vector(j.local_axis_a) * 0.25;
            arr.label = "hinge_axis";
            arr.color_rgba = 0x0000FFFF; // Blue
            data.joint_axes.push_back(arr);
        }
    }

    return data;
}

// =============================================================================
// Interactive Mouse-Drag Tool (PH4-09.04.004)
// =============================================================================

void SimulationController::grab_body(
    const std::string& body_id,
    const Vector3d& local_anchor,
    const Vector3d& target_world_pos,
    double stiffness,
    double damping) noexcept {
    mouse_grab_.active = true;
    mouse_grab_.body_id = body_id;
    mouse_grab_.local_anchor = local_anchor;
    mouse_grab_.target_world_pos = target_world_pos;
    mouse_grab_.stiffness = stiffness;
    mouse_grab_.damping = damping;
}

void SimulationController::update_grab_target(const Vector3d& new_target_world_pos) noexcept {
    if (mouse_grab_.active) {
        mouse_grab_.target_world_pos = new_target_world_pos;
    }
}

void SimulationController::release_grab() noexcept {
    mouse_grab_.active = false;
    mouse_grab_.body_id.clear();
}

// =============================================================================
// Snapshotting & Rewinding (PH4-09.04.006)
// =============================================================================

void SimulationController::save_named_snapshot(const std::string& name) {
    SimulationSnapshot snap;
    snap.time_s = current_time_s_;
    snap.step_count = step_count_;
    snap.bodies = bodies_;
    snap.revolute_joints = revolute_joints_;
    snap.prismatic_joints = prismatic_joints_;
    snap.ball_joints = ball_joints_;
    snap.wheel_joints = wheel_joints_;
    named_snapshots_[name] = std::move(snap);
}

bool SimulationController::restore_named_snapshot(const std::string& name) {
    const auto it = named_snapshots_.find(name);
    if (it == named_snapshots_.end()) {
        return false;
    }
    const auto& snap = it->second;
    bodies_ = snap.bodies;
    revolute_joints_ = snap.revolute_joints;
    prismatic_joints_ = snap.prismatic_joints;
    ball_joints_ = snap.ball_joints;
    wheel_joints_ = snap.wheel_joints;
    current_time_s_ = snap.time_s;
    step_count_ = snap.step_count;
    return true;
}

void SimulationController::rewind_steps(size_t steps) {
    if (history_.empty() || steps == 0) return;

    size_t steps_to_pop = std::min(steps, history_.size());
    SimulationSnapshot target_snap;
    for (size_t s = 0; s < steps_to_pop; ++s) {
        target_snap = std::move(history_.back());
        history_.pop_back();
    }

    bodies_ = std::move(target_snap.bodies);
    revolute_joints_ = std::move(target_snap.revolute_joints);
    prismatic_joints_ = std::move(target_snap.prismatic_joints);
    ball_joints_ = std::move(target_snap.ball_joints);
    wheel_joints_ = std::move(target_snap.wheel_joints);
    current_time_s_ = target_snap.time_s;
    step_count_ = target_snap.step_count;
}

// =============================================================================
// Energy Conservation Auditing (PH4-09.04.007)
// =============================================================================

double SimulationController::total_kinetic_energy() const noexcept {
    double total = 0.0;
    for (const auto& b : bodies_) {
        if (b.is_dynamic()) {
            total += b.state().kinetic_energy();
        }
    }
    return total;
}

double SimulationController::total_potential_energy() const noexcept {
    double total = 0.0;
    // Gravitational potential energy: - m * g . r (since g points downward, e.g. -9.80665)
    for (const auto& b : bodies_) {
        if (b.is_dynamic()) {
            total += -b.state().mass_kg * config_.gravity.dot(b.state().position);
        }
    }
    return total;
}

double SimulationController::total_mechanical_energy() const noexcept {
    return total_kinetic_energy() + total_potential_energy();
}

} // namespace shunya::physim
