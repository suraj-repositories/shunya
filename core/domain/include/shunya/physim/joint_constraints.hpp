#pragma once

#include "shunya/physim/rigid_body.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace shunya::physim {

// =============================================================================
// Revolve / Hinge Joint (PH4-09.03.001)
// =============================================================================
struct RevoluteJointSpec {
    std::string body_a_id;
    std::string body_b_id;
    Vector3d local_anchor_a{0.0, 0.0, 0.0};
    Vector3d local_anchor_b{0.0, 0.0, 0.0};
    Vector3d local_axis_a{0.0, 0.0, 1.0};
    Vector3d local_axis_b{0.0, 0.0, 1.0};

    bool enable_limits{false};
    double lower_angle_rad{-std::numbers::pi};
    double upper_angle_rad{std::numbers::pi};

    double joint_friction_torque{0.0}; ///< Resisting dry friction torque (N*m)
    double joint_damping{0.01};         ///< Viscous rotational damping (N*m*s/rad)

    bool enable_motor{false};
    double motor_target_velocity_rad_s{0.0};
    double motor_max_torque_nm{100.0};

    // Accumulated impulses for warm-starting
    Vector3d impulse_linear{0.0, 0.0, 0.0};
    Vector3d impulse_angular{0.0, 0.0, 0.0};
    double impulse_motor{0.0};
};

// =============================================================================
// Prismatic / Slider Joint (PH4-09.03.002)
// =============================================================================
struct PrismaticJointSpec {
    std::string body_a_id;
    std::string body_b_id;
    Vector3d local_anchor_a{0.0, 0.0, 0.0};
    Vector3d local_anchor_b{0.0, 0.0, 0.0};
    Vector3d local_axis_a{1.0, 0.0, 0.0};   ///< Slider translation axis
    Vector3d local_axis_b{1.0, 0.0, 0.0};

    bool enable_limits{false};
    double lower_limit_m{-1.0};
    double upper_limit_m{1.0};

    double joint_damping{0.05};            ///< Viscous linear damping (N*s/m)
    bool enable_motor{false};
    double motor_target_velocity_mps{0.0};
    double motor_max_force_n{200.0};

    Vector3d impulse_linear{0.0, 0.0, 0.0};
    Vector3d impulse_angular{0.0, 0.0, 0.0};
    double impulse_motor{0.0};
};

// =============================================================================
// Ball-and-Socket Joint with Cone Limits (PH4-09.03.003)
// =============================================================================
struct BallAndSocketJointSpec {
    std::string body_a_id;
    std::string body_b_id;
    Vector3d local_anchor_a{0.0, 0.0, 0.0};
    Vector3d local_anchor_b{0.0, 0.0, 0.0};

    Vector3d primary_axis_a{0.0, 1.0, 0.0}; ///< Primary axis on body A for cone limit
    Vector3d primary_axis_b{0.0, 1.0, 0.0}; ///< Primary axis on body B for cone limit
    bool enable_cone_limit{false};
    double max_cone_angle_rad{std::numbers::pi / 4.0}; ///< Max cone half-angle (e.g. 45 deg)

    Vector3d impulse_linear{0.0, 0.0, 0.0};
};

// =============================================================================
// Wheel Axle Joint with Suspension (PH4-09.03.004)
// =============================================================================
struct WheelAxleJointSpec {
    std::string chassis_body_id;
    std::string wheel_body_id;
    Vector3d local_anchor_chassis{0.0, 0.0, 0.0};
    Vector3d local_anchor_wheel{0.0, 0.0, 0.0};
    Vector3d spin_axis_local{0.0, 0.0, 1.0};       ///< Wheel spin axis (+Z)
    Vector3d suspension_axis_local{0.0, 1.0, 0.0}; ///< Shock travel axis (+Y)

    double suspension_rest_length_m{0.4};
    double suspension_stiffness_n_m{25000.0};      ///< Spring rate k (N/m)
    double suspension_damping_n_s_m{1500.0};       ///< Shock damping rate c (N*s/m)
    double suspension_min_travel_m{-0.15};
    double suspension_max_travel_m{0.15};

    double current_travel_m{0.0};
    double spring_force_n{0.0};
};

// =============================================================================
// Mechanical Gear Constraint (PH4-09.03.005)
// =============================================================================
struct GearConstraintSpec {
    std::string gear1_body_id;
    std::string gear2_body_id;
    Vector3d gear1_axis_local{0.0, 0.0, 1.0};
    Vector3d gear2_axis_local{0.0, 0.0, 1.0};
    int teeth_count_1{20};
    int teeth_count_2{40};
    double pitch_radius_1_m{0.1};
    double pitch_radius_2_m{0.2};

    [[nodiscard]] double gear_ratio() const noexcept {
        return (teeth_count_1 > 0) ? (static_cast<double>(teeth_count_2) / teeth_count_1) : 1.0;
    }
};

// =============================================================================
// Pulley and Belt Constraint (PH4-09.03.006)
// =============================================================================
struct PulleyBeltConstraintSpec {
    std::string pulley1_body_id;
    std::string pulley2_body_id;
    Vector3d pulley1_axis_local{0.0, 0.0, 1.0};
    Vector3d pulley2_axis_local{0.0, 0.0, 1.0};
    double pulley1_radius_m{0.1};
    double pulley2_radius_m{0.2};
    bool is_crossed_belt{false}; ///< True: counter-rotating crossed belt; False: open belt
    double max_torque_capacity_nm{50.0}; ///< Slip threshold
};

// =============================================================================
// 4-Bar and Multi-Bar Mechanical Linkage (PH4-09.03.007)
// =============================================================================
enum class GrashofClass : uint8_t {
    CrankRocker,   ///< Shortest link is crank, can rotate 360 deg
    DoubleCrank,   ///< Shortest link is ground link, both input & output rotate 360 deg (drag link)
    DoubleRocker,  ///< Shortest link is coupler, both input & output oscillate
    NonGrashof     ///< S + L > P + Q: triple rocker, no link can make a complete revolution
};

struct FourBarKinematicsResult {
    bool has_solution{false};
    double rocker_angle_rad{0.0};
    double coupler_angle_rad{0.0};
    double loop_closure_error_m{0.0};
};

class FourBarLinkageAssembly {
public:
    double ground_length_m{0.4};   ///< L1: fixed frame distance
    double crank_length_m{0.1};    ///< L2: driving link
    double coupler_length_m{0.35}; ///< L3: floating connecting link
    double rocker_length_m{0.3};   ///< L4: driven oscillating link

    [[nodiscard]] GrashofClass classify_grashof() const noexcept;
    [[nodiscard]] bool satisfies_grashof() const noexcept;

    /**
     * @brief Computes exact output rocker and coupler angles for a given input crank angle
     *        via analytical Freudenstein closure equations.
     */
    [[nodiscard]] FourBarKinematicsResult solve_forward_kinematics(
        double crank_angle_rad, bool open_branch = true) const noexcept;
};

// =============================================================================
// Joint Constraint PGS Solver
// =============================================================================
class JointConstraintSolver {
public:
    static void solve_revolute_joints(
        std::vector<RigidBodyEntity>& bodies,
        std::vector<RevoluteJointSpec>& joints,
        double dt_s,
        int iterations = 20) noexcept;

    static void solve_prismatic_joints(
        std::vector<RigidBodyEntity>& bodies,
        std::vector<PrismaticJointSpec>& joints,
        double dt_s,
        int iterations = 20) noexcept;

    static void solve_ball_joints(
        std::vector<RigidBodyEntity>& bodies,
        std::vector<BallAndSocketJointSpec>& joints,
        double dt_s,
        int iterations = 20) noexcept;

    static void solve_wheel_suspension(
        std::vector<RigidBodyEntity>& bodies,
        std::vector<WheelAxleJointSpec>& joints,
        double dt_s) noexcept;

    static void solve_gear_constraints(
        std::vector<RigidBodyEntity>& bodies,
        const std::vector<GearConstraintSpec>& gears,
        double dt_s) noexcept;

    static void solve_pulley_belt_constraints(
        std::vector<RigidBodyEntity>& bodies,
        const std::vector<PulleyBeltConstraintSpec>& belts,
        double dt_s) noexcept;

    [[nodiscard]] static double compute_max_joint_anchor_error(
        const std::vector<RigidBodyEntity>& bodies,
        const std::vector<RevoluteJointSpec>& revolute_joints,
        const std::vector<BallAndSocketJointSpec>& ball_joints) noexcept;
};

} // namespace shunya::physim
