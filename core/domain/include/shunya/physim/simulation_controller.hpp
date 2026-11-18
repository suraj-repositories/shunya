#pragma once

#include "shunya/physim/collision_pipeline.hpp"
#include "shunya/physim/integrator.hpp"
#include "shunya/physim/joint_constraints.hpp"
#include "shunya/physim/pgs_solver.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <cstdint>
#include <deque>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::physim {

enum class SimulationState : uint8_t {
    Stopped,
    Running,
    Paused
};

enum class TimeSteppingMode : uint8_t {
    Fixed,
    Adaptive
};

// =============================================================================
// Physics Configuration Panel (PH4-09.04.005)
// =============================================================================
struct PhysicsConfig {
    Vector3d gravity{0.0, -9.80665, 0.0};
    int pgs_iterations{30};
    int joint_iterations{20};
    double penetration_slop{0.001};
    double restitution_threshold{0.2};
    double baumgarte_beta{0.2};
    bool enable_warm_starting{true};
    bool enable_ccd{true};

    // Time-stepping controls (PH4-09.04.002)
    double speed_scale{1.0};               ///< Simulation speed scaling (0.1x to 10.0x)
    TimeSteppingMode stepping_mode{TimeSteppingMode::Fixed};
    double adaptive_cfl_factor{0.2};       ///< CFL ratio for adaptive sub-stepping
    double min_adaptive_dt{1e-4};          ///< Minimum time-step clamp (s)
    double max_adaptive_dt{0.02};          ///< Maximum time-step clamp (s)
};

// =============================================================================
// Visual Debug Rendering Primitives (PH4-09.04.003)
// =============================================================================
struct DebugGlyph {
    Vector3d position{0.0, 0.0, 0.0};
    double radius{0.02};
    std::string label;
};

struct DebugArrow {
    Vector3d origin{0.0, 0.0, 0.0};
    Vector3d vector{0.0, 1.0, 0.0};
    std::string label;
    uint32_t color_rgba{0xFFFFFFFF};
};

struct DebugRenderData {
    std::vector<DebugGlyph> com_glyphs;
    std::vector<DebugArrow> velocity_vectors;
    std::vector<DebugArrow> contact_normals;
    std::vector<DebugArrow> joint_axes;
};

// =============================================================================
// Interactive Mouse-Drag Tool (PH4-09.04.004)
// =============================================================================
struct MouseGrabState {
    bool active{false};
    std::string body_id;
    Vector3d local_anchor{0.0, 0.0, 0.0};
    Vector3d target_world_pos{0.0, 0.0, 0.0};
    double stiffness{1500.0}; ///< Grab spring constant (N/m)
    double damping{80.0};     ///< Grab damping coefficient (N*s/m)
};

// =============================================================================
// State Snapshotting & Rewinding (PH4-09.04.006)
// =============================================================================
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
 * @brief Interactive Machine Simulation Controller (PH4-09.04.001 - PH4-09.04.007).
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
    void add_manifold(const PhysimContactManifold& manifold);

    // Initial State Capture & Baseline Setup
    void capture_initial_state();
    void reset_to_initial_state();

    // Simulation Controls (PH4-09.04.001)
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

    // Configuration & Speed Scaling (PH4-09.04.002, PH4-09.04.005)
    [[nodiscard]] const PhysicsConfig& config() const noexcept { return config_; }
    [[nodiscard]] PhysicsConfig& config() noexcept { return config_; }
    void set_config(const PhysicsConfig& cfg) noexcept { config_ = cfg; }
    void set_speed_scale(double scale) noexcept;
    [[nodiscard]] double speed_scale() const noexcept { return config_.speed_scale; }

    // Visual Debug Rendering (PH4-09.04.003)
    [[nodiscard]] DebugRenderData extract_debug_render_data() const noexcept;

    // Interactive Mouse-Drag Tool (PH4-09.04.004)
    void grab_body(const std::string& body_id, const Vector3d& local_anchor,
                   const Vector3d& target_world_pos, double stiffness = 1500.0, double damping = 80.0) noexcept;
    void update_grab_target(const Vector3d& new_target_world_pos) noexcept;
    void release_grab() noexcept;
    [[nodiscard]] bool is_grabbing() const noexcept { return mouse_grab_.active; }

    // Snapshotting & Rewinding (PH4-09.04.006)
    void save_named_snapshot(const std::string& name);
    bool restore_named_snapshot(const std::string& name);
    void rewind_steps(size_t steps);
    [[nodiscard]] bool can_rewind() const noexcept { return !history_.empty(); }
    [[nodiscard]] size_t history_size() const noexcept { return history_.size(); }
    void set_max_history_frames(size_t max_frames) noexcept { max_history_frames_ = max_frames; }

    // Energy Conservation Auditing (PH4-09.04.007)
    [[nodiscard]] double total_kinetic_energy() const noexcept;
    [[nodiscard]] double total_potential_energy() const noexcept;
    [[nodiscard]] double total_mechanical_energy() const noexcept;

    void set_on_step_callback(StepCallback cb) { on_step_ = std::move(cb); }
    void set_on_state_change_callback(StateChangeCallback cb) { on_state_change_ = std::move(cb); }

private:
    void set_state(SimulationState new_state);
    void advance_physics_step(double dt_s);
    void apply_mouse_grab_force() noexcept;
    [[nodiscard]] double calculate_effective_dt(double base_dt_s) const noexcept;

    SimulationState state_{SimulationState::Stopped};
    double current_time_s_{0.0};
    uint64_t step_count_{0};
    PhysicsConfig config_;

    std::vector<RigidBodyEntity> bodies_;
    std::vector<RevoluteJointSpec> revolute_joints_;
    std::vector<PrismaticJointSpec> prismatic_joints_;
    std::vector<BallAndSocketJointSpec> ball_joints_;
    std::vector<WheelAxleJointSpec> wheel_joints_;
    std::vector<GearConstraintSpec> gears_;
    std::vector<PulleyBeltConstraintSpec> belts_;
    std::vector<PhysimContactManifold> manifolds_;

    MouseGrabState mouse_grab_;
    SimulationSnapshot initial_snapshot_;
    std::deque<SimulationSnapshot> history_;
    size_t max_history_frames_{500};
    std::unordered_map<std::string, SimulationSnapshot> named_snapshots_;

    StepCallback on_step_;
    StateChangeCallback on_state_change_;
};

} // namespace shunya::physim
