#pragma once

#include "shunya/physim/collision_pipeline.hpp"
#include "shunya/physim/integrator.hpp"
#include "shunya/physim/joint_constraints.hpp"
#include "shunya/physim/pgs_solver.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace shunya::physim {

enum class SimulationState : uint8_t {
    Stopped,
    Running,
    Paused
};

struct SimulationSnapshot {
    double time_s{0.0};
    uint64_t step_count{0};
    std::vector<RigidBodyEntity> bodies;
    std::vector<RevoluteJointSpec> revolute_joints;
    std::vector<PrismaticJointSpec> prismatic_joints;
    std::vector<BallAndSocketJointSpec> ball_joints;
    std::vector<WheelAxleJointSpec> wheel_joints;
};

/**
 * @brief Interactive Machine Simulation Controller (PH4-09.04.001).
 *        Provides Play, Pause, Stop, Step Forward, and Reset to Initial State controls.
 */
class SimulationController {
public:
    using StepCallback = std::function<void(double time_s, uint64_t step_count)>;
    using StateChangeCallback = std::function<void(SimulationState old_state, SimulationState new_state)>;

    SimulationController() = default;

    // Entity and Joint Registration
    void add_body(const RigidBodyEntity& body);
    void add_revolute_joint(const RevoluteJointSpec& joint);
    void add_prismatic_joint(const PrismaticJointSpec& joint);
    void add_ball_joint(const BallAndSocketJointSpec& joint);
    void add_wheel_joint(const WheelAxleJointSpec& joint);
    void add_gear(const GearConstraintSpec& gear);
    void add_pulley_belt(const PulleyBeltConstraintSpec& belt);

    // Initial State Capture & Baseline Setup
    void capture_initial_state();
    void reset_to_initial_state();

    // Simulation Controls
    void play();
    void pause();
    void stop();
    void step_forward(double dt_s = 0.01);

    // Time & State Queries
    [[nodiscard]] SimulationState state() const noexcept { return state_; }
    [[nodiscard]] bool is_running() const noexcept { return state_ == SimulationState::Running; }
    [[nodiscard]] bool is_paused() const noexcept { return state_ == SimulationState::Paused; }
    [[nodiscard]] bool is_stopped() const noexcept { return state_ == SimulationState::Stopped; }

    [[nodiscard]] double current_time_s() const noexcept { return current_time_s_; }
    [[nodiscard]] uint64_t total_steps() const noexcept { return step_count_; }

    [[nodiscard]] const std::vector<RigidBodyEntity>& bodies() const noexcept { return bodies_; }
    [[nodiscard]] std::vector<RigidBodyEntity>& bodies() noexcept { return bodies_; }

    [[nodiscard]] const RigidBodyEntity* find_body(const std::string& id) const noexcept;
    [[nodiscard]] RigidBodyEntity* find_body(const std::string& id) noexcept;

    // Environmental Parameters
    void set_gravity(const Vector3d& g) noexcept { gravity_ = g; }
    [[nodiscard]] const Vector3d& gravity() const noexcept { return gravity_; }

    void set_on_step_callback(StepCallback cb) { on_step_ = std::move(cb); }
    void set_on_state_change_callback(StateChangeCallback cb) { on_state_change_ = std::move(cb); }

private:
    void set_state(SimulationState new_state);
    void advance_physics_step(double dt_s);

    SimulationState state_{SimulationState::Stopped};
    double current_time_s_{0.0};
    uint64_t step_count_{0};
    Vector3d gravity_{0.0, -9.80665, 0.0};

    std::vector<RigidBodyEntity> bodies_;
    std::vector<RevoluteJointSpec> revolute_joints_;
    std::vector<PrismaticJointSpec> prismatic_joints_;
    std::vector<BallAndSocketJointSpec> ball_joints_;
    std::vector<WheelAxleJointSpec> wheel_joints_;
    std::vector<GearConstraintSpec> gears_;
    std::vector<PulleyBeltConstraintSpec> belts_;

    SimulationSnapshot initial_snapshot_;
    StepCallback on_step_;
    StateChangeCallback on_state_change_;
};

} // namespace shunya::physim
