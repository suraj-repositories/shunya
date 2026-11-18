#include "shunya/physim/simulation_controller.hpp"

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

void SimulationController::step_forward(double dt_s) {
    if (dt_s <= 0.0) return;
    if (state_ == SimulationState::Stopped && initial_snapshot_.bodies.empty()) {
        capture_initial_state();
    }
    advance_physics_step(dt_s);
}

void SimulationController::advance_physics_step(double dt_s) {
    // 1. Solve suspension forces before integration
    if (!wheel_joints_.empty()) {
        JointConstraintSolver::solve_wheel_suspension(bodies_, wheel_joints_, dt_s);
    }

    // 2. Step integration for all bodies
    RigidBodyIntegrator::step_system(bodies_, dt_s, IntegratorScheme::SymplecticEuler, gravity_);

    // 3. Solve multi-body joint constraints
    if (!revolute_joints_.empty()) {
        JointConstraintSolver::solve_revolute_joints(bodies_, revolute_joints_, dt_s, 20);
    }
    if (!prismatic_joints_.empty()) {
        JointConstraintSolver::solve_prismatic_joints(bodies_, prismatic_joints_, dt_s, 20);
    }
    if (!ball_joints_.empty()) {
        JointConstraintSolver::solve_ball_joints(bodies_, ball_joints_, dt_s, 20);
    }
    if (!gears_.empty()) {
        JointConstraintSolver::solve_gear_constraints(bodies_, gears_, dt_s);
    }
    if (!belts_.empty()) {
        JointConstraintSolver::solve_pulley_belt_constraints(bodies_, belts_, dt_s);
    }

    current_time_s_ += dt_s;
    ++step_count_;

    if (on_step_) {
        on_step_(current_time_s_, step_count_);
    }
}

} // namespace shunya::physim
