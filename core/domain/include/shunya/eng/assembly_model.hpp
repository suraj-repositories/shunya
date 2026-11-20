#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/eng/physical_object.hpp"

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace shunya::eng {

struct AssemblyPartInstance {
    core::base::Uuid instance_id{core::base::Uuid::generate_random()};
    core::base::Uuid object_id;
    std::string instance_name{"Part_1"};
    uint32_t object_version{1};
    core::base::math::Vector3<double> position{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> rotation_deg{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> scale{1.0, 1.0, 1.0};
    core::base::math::Matrix4x4<double> local_transform{};
    std::string parent_subassembly_id;

    [[nodiscard]] core::base::math::Matrix4x4<double> compute_transform_matrix() const noexcept {
        core::base::math::Matrix4x4<double> t = core::base::math::Matrix4x4<double>::translation(position);
        double deg2rad = 3.14159265358979323846 / 180.0;
        core::base::math::Quaternion<double> q = core::base::math::Quaternion<double>::from_euler_angles(
            rotation_deg.x * deg2rad, rotation_deg.y * deg2rad, rotation_deg.z * deg2rad);
        core::base::math::Matrix4x4<double> r = core::base::math::Matrix4x4<double>::from_quaternion(q);
        core::base::math::Matrix4x4<double> s = core::base::math::Matrix4x4<double>::scaling(scale);
        return t * r * s;
    }
};

enum class KinematicJointKind : uint8_t {
    Fixed,
    Revolute,       // 1-DOF Hinge
    Prismatic,      // 1-DOF Slider
    Ball,           // 3-DOF Spherical
    ThreadedScrew   // Helical coupling
};

[[nodiscard]] constexpr std::string_view kinematic_joint_kind_to_string(KinematicJointKind k) noexcept {
    switch (k) {
        case KinematicJointKind::Fixed:         return "Fixed";
        case KinematicJointKind::Revolute:      return "Revolute";
        case KinematicJointKind::Prismatic:     return "Prismatic";
        case KinematicJointKind::Ball:          return "Ball";
        case KinematicJointKind::ThreadedScrew: return "ThreadedScrew";
    }
    return "Fixed";
}

struct KinematicJoint {
    core::base::Uuid joint_id{core::base::Uuid::generate_random()};
    std::string name{"Joint_1"};
    KinematicJointKind kind{KinematicJointKind::Fixed};
    core::base::Uuid part_a_id;
    core::base::Uuid part_b_id;
    core::base::math::Vector3<double> anchor_point{0.0, 0.0, 0.0};
    core::base::math::Vector3<double> axis{0.0, 1.0, 0.0};
    double lower_limit{-180.0}; // deg for revolute, m for prismatic
    double upper_limit{180.0};
    double screw_pitch_m_per_rev{0.002}; // 2 mm/rev for bulb screw
    double current_coordinate{0.0};
};

struct ElectricalWireRoute {
    core::base::Uuid wire_id{core::base::Uuid::generate_random()};
    std::string name{"Wire_1"};
    core::base::Uuid part_a_id;
    std::string terminal_a_name;
    core::base::Uuid part_b_id;
    std::string terminal_b_name;
    int awg_gauge{22};
    double length_m{0.25};
    double resistance_ohm{0.013}; // AWG 22 ~ 0.053 ohm/m
    std::string insulation_material{"PVC"};
    std::vector<core::base::math::Vector3<double>> waypoints;
};

struct ThermalContactInterface {
    core::base::Uuid contact_id{core::base::Uuid::generate_random()};
    std::string name{"ThermalContact_1"};
    core::base::Uuid part_a_id;
    core::base::Uuid part_b_id;
    double contact_area_m2{1e-4};
    double contact_conductance_w_m2k{5000.0}; // h_c
    [[nodiscard]] double thermal_resistance_k_w() const noexcept {
        return (contact_conductance_w_m2k * contact_area_m2 > 0.0)
            ? (1.0 / (contact_conductance_w_m2k * contact_area_m2)) : 1e6;
    }
};

struct AssemblySubgroup {
    core::base::Uuid group_id{core::base::Uuid::generate_random()};
    std::string name{"SubAssembly_1"};
    core::base::math::Vector3<double> local_origin{0.0, 0.0, 0.0};
    std::vector<core::base::Uuid> member_instance_ids;
};

struct AssemblyPreFlightIssue {
    std::string rule_id;
    std::string message;
    std::string target_type;
    std::string target_id;
    bool is_blocking{true};
};

struct AssemblyPreFlightReport {
    bool passed{true};
    std::vector<AssemblyPreFlightIssue> issues;
    bool has_closed_electrical_loop{false};
    bool has_collisions{false};
};

class AssemblyModel {
public:
    AssemblyModel() = default;

    core::base::Uuid id{core::base::Uuid::generate_random()};
    std::string name{"Desk_Lamp_Assembly"};
    uint32_t schema_version{5};

    std::vector<AssemblyPartInstance> parts;
    std::vector<KinematicJoint> joints;
    std::vector<ElectricalWireRoute> wires;
    std::vector<ThermalContactInterface> thermal_contacts;
    std::vector<AssemblySubgroup> subassemblies;

    // Part operations
    AssemblyPartInstance& add_part_instance(
        const core::base::Uuid& object_id,
        const std::string& instance_name,
        const core::base::math::Vector3<double>& position = {0.0, 0.0, 0.0});

    [[nodiscard]] const AssemblyPartInstance* find_part(const core::base::Uuid& instance_id) const noexcept;
    [[nodiscard]] AssemblyPartInstance* find_part_mut(const core::base::Uuid& instance_id) noexcept;
    bool remove_part(const core::base::Uuid& instance_id);

    // Precision mating snaps
    bool snap_concentric(const core::base::Uuid& part_a_id, const core::base::Uuid& part_b_id);
    bool snap_face_to_face(const core::base::Uuid& part_a_id, const core::base::Uuid& part_b_id, double offset_m = 0.0);
    bool snap_terminals(
        const core::base::Uuid& part_a_id, const std::string& term_a,
        const core::base::Uuid& part_b_id, const std::string& term_b);

    // Joints & Wires
    KinematicJoint& add_joint(
        const std::string& name,
        KinematicJointKind kind,
        const core::base::Uuid& part_a,
        const core::base::Uuid& part_b,
        const core::base::math::Vector3<double>& axis = {0, 1, 0});

    ElectricalWireRoute& add_wire(
        const std::string& name,
        const core::base::Uuid& part_a, const std::string& term_a,
        const core::base::Uuid& part_b, const std::string& term_b,
        int awg = 22, double length_m = 0.25);

    ThermalContactInterface& add_thermal_contact(
        const std::string& name,
        const core::base::Uuid& part_a,
        const core::base::Uuid& part_b,
        double area_m2, double h_c);

    // Grouping
    AssemblySubgroup& create_subgroup(const std::string& group_name, const std::vector<core::base::Uuid>& members);

    // Validation & Simulation Pre-flight
    [[nodiscard]] AssemblyPreFlightReport run_preflight_checks(const PhysicalObjectManager& obj_mgr) const;

    // Serialization & Model Export
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static core::base::Result<AssemblyModel> from_json(std::string_view json_str);
    [[nodiscard]] std::string export_to_cosim_scene(const PhysicalObjectManager& obj_mgr) const;
};

} // namespace shunya::eng
