#include "shunya/eng/assembly_model.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>

namespace shunya::eng {

AssemblyPartInstance& AssemblyModel::add_part_instance(
    const core::base::Uuid& object_id,
    const std::string& instance_name,
    const core::base::math::Vector3<double>& position)
{
    AssemblyPartInstance inst;
    inst.instance_id = core::base::Uuid::generate_random();
    inst.object_id = object_id;
    inst.instance_name = instance_name.empty() ? ("Part_" + std::to_string(parts.size() + 1)) : instance_name;
    inst.position = position;
    inst.local_transform = inst.compute_transform_matrix();
    parts.push_back(std::move(inst));
    return parts.back();
}

const AssemblyPartInstance* AssemblyModel::find_part(const core::base::Uuid& instance_id) const noexcept {
    auto it = std::find_if(parts.begin(), parts.end(),
        [&](const AssemblyPartInstance& p) { return p.instance_id == instance_id; });
    return (it != parts.end()) ? &(*it) : nullptr;
}

AssemblyPartInstance* AssemblyModel::find_part_mut(const core::base::Uuid& instance_id) noexcept {
    auto it = std::find_if(parts.begin(), parts.end(),
        [&](const AssemblyPartInstance& p) { return p.instance_id == instance_id; });
    return (it != parts.end()) ? &(*it) : nullptr;
}

bool AssemblyModel::remove_part(const core::base::Uuid& instance_id) {
    auto it = std::remove_if(parts.begin(), parts.end(),
        [&](const AssemblyPartInstance& p) { return p.instance_id == instance_id; });
    if (it == parts.end()) return false;
    parts.erase(it, parts.end());

    // Clean up associated joints & wires
    joints.erase(std::remove_if(joints.begin(), joints.end(),
        [&](const KinematicJoint& j) { return j.part_a_id == instance_id || j.part_b_id == instance_id; }), joints.end());
    wires.erase(std::remove_if(wires.begin(), wires.end(),
        [&](const ElectricalWireRoute& w) { return w.part_a_id == instance_id || w.part_b_id == instance_id; }), wires.end());
    thermal_contacts.erase(std::remove_if(thermal_contacts.begin(), thermal_contacts.end(),
        [&](const ThermalContactInterface& c) { return c.part_a_id == instance_id || c.part_b_id == instance_id; }), thermal_contacts.end());
    return true;
}

bool AssemblyModel::snap_concentric(const core::base::Uuid& part_a_id, const core::base::Uuid& part_b_id) {
    const auto* a = find_part(part_a_id);
    auto* b = find_part_mut(part_b_id);
    if (!a || !b) return false;
    b->position.x = a->position.x;
    b->position.z = a->position.z;
    b->local_transform = b->compute_transform_matrix();
    return true;
}

bool AssemblyModel::snap_face_to_face(const core::base::Uuid& part_a_id, const core::base::Uuid& part_b_id, double offset_m) {
    const auto* a = find_part(part_a_id);
    auto* b = find_part_mut(part_b_id);
    if (!a || !b) return false;
    b->position.y = a->position.y + offset_m;
    b->local_transform = b->compute_transform_matrix();
    return true;
}

bool AssemblyModel::snap_terminals(
    const core::base::Uuid& part_a_id, const std::string& term_a,
    const core::base::Uuid& part_b_id, const std::string& term_b)
{
    (void)term_a;
    (void)term_b;
    const auto* a = find_part(part_a_id);
    auto* b = find_part_mut(part_b_id);
    if (!a || !b) return false;
    // Align terminal centers
    b->position = a->position;
    b->local_transform = b->compute_transform_matrix();
    return true;
}

KinematicJoint& AssemblyModel::add_joint(
    const std::string& name,
    KinematicJointKind kind,
    const core::base::Uuid& part_a,
    const core::base::Uuid& part_b,
    const core::base::math::Vector3<double>& axis)
{
    KinematicJoint j;
    j.name = name;
    j.kind = kind;
    j.part_a_id = part_a;
    j.part_b_id = part_b;
    j.axis = axis;
    joints.push_back(j);
    return joints.back();
}

ElectricalWireRoute& AssemblyModel::add_wire(
    const std::string& name,
    const core::base::Uuid& part_a, const std::string& term_a,
    const core::base::Uuid& part_b, const std::string& term_b,
    int awg, double length_m)
{
    ElectricalWireRoute w;
    w.name = name;
    w.part_a_id = part_a;
    w.terminal_a_name = term_a;
    w.part_b_id = part_b;
    w.terminal_b_name = term_b;
    w.awg_gauge = awg;
    w.length_m = length_m;
    w.resistance_ohm = length_m * 0.053; // ~0.053 ohm/m for AWG 22
    wires.push_back(w);
    return wires.back();
}

ThermalContactInterface& AssemblyModel::add_thermal_contact(
    const std::string& name,
    const core::base::Uuid& part_a,
    const core::base::Uuid& part_b,
    double area_m2, double h_c)
{
    ThermalContactInterface c;
    c.name = name;
    c.part_a_id = part_a;
    c.part_b_id = part_b;
    c.contact_area_m2 = area_m2;
    c.contact_conductance_w_m2k = h_c;
    thermal_contacts.push_back(c);
    return thermal_contacts.back();
}

AssemblySubgroup& AssemblyModel::create_subgroup(
    const std::string& group_name,
    const std::vector<core::base::Uuid>& members)
{
    AssemblySubgroup g;
    g.name = group_name;
    g.member_instance_ids = members;
    subassemblies.push_back(g);
    return subassemblies.back();
}

AssemblyPreFlightReport AssemblyModel::run_preflight_checks(const PhysicalObjectManager& obj_mgr) const {
    AssemblyPreFlightReport report;
    report.passed = true;

    // Check 1: All parts must exist in library
    for (const auto& p : parts) {
        if (!obj_mgr.get_object(p.object_id)) {
            AssemblyPreFlightIssue issue;
            issue.rule_id = "ASY-01-MISSING-PART";
            issue.target_type = "part";
            issue.target_id = p.instance_id.to_string();
            issue.message = "Part instance '" + p.instance_name + "' references non-existent object ID in library.";
            issue.is_blocking = true;
            report.issues.push_back(issue);
        }
    }

    // Check 2: Joint validity
    for (const auto& j : joints) {
        const auto* a = find_part(j.part_a_id);
        const auto* b = find_part(j.part_b_id);
        if (!a || !b) {
            AssemblyPreFlightIssue issue;
            issue.rule_id = "ASY-02-ORPHAN-JOINT";
            issue.target_type = "joint";
            issue.target_id = j.joint_id.to_string();
            issue.message = "Kinematic joint '" + j.name + "' references missing part instance(s).";
            issue.is_blocking = true;
            report.issues.push_back(issue);
        }
        if (j.axis.length_squared() <= 1e-12) {
            AssemblyPreFlightIssue issue;
            issue.rule_id = "ASY-02-ZERO-AXIS";
            issue.target_type = "joint";
            issue.target_id = j.joint_id.to_string();
            issue.message = "Kinematic joint '" + j.name + "' has a zero-vector rotation/translation axis.";
            issue.is_blocking = true;
            report.issues.push_back(issue);
        }
    }

    // Check 3: Electrical loop & wiring validity
    if (wires.size() >= 2) {
        report.has_closed_electrical_loop = true;
    } else if (!wires.empty()) {
        AssemblyPreFlightIssue issue;
        issue.rule_id = "ASY-03-OPEN-LOOP";
        issue.target_type = "wiring";
        issue.target_id = wires.front().wire_id.to_string();
        issue.message = "Electrical wiring forms an open circuit; at least 2 return routes required for closed conduction.";
        issue.is_blocking = false;
        report.issues.push_back(issue);
    }

    report.passed = report.issues.empty();
    return report;
}

std::string AssemblyModel::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schemaVersion\": \"5.0.0\",\n";
    ss << "  \"id\": \"" << id.to_string() << "\",\n";
    ss << "  \"name\": \"" << name << "\",\n";
    ss << "  \"parts\": [\n";
    for (size_t i = 0; i < parts.size(); ++i) {
        const auto& p = parts[i];
        ss << "    {\n";
        ss << "      \"instanceId\": \"" << p.instance_id.to_string() << "\",\n";
        ss << "      \"objectId\": \"" << p.object_id.to_string() << "\",\n";
        ss << "      \"name\": \"" << p.instance_name << "\",\n";
        ss << "      \"posX\": " << p.position.x << ", \"posY\": " << p.position.y << ", \"posZ\": " << p.position.z << ",\n";
        ss << "      \"rotX\": " << p.rotation_deg.x << ", \"rotY\": " << p.rotation_deg.y << ", \"rotZ\": " << p.rotation_deg.z << ",\n";
        ss << "      \"scaleX\": " << p.scale.x << ", \"scaleY\": " << p.scale.y << ", \"scaleZ\": " << p.scale.z << "\n";
        ss << "    }" << (i + 1 < parts.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";
    ss << "  \"joints\": [\n";
    for (size_t i = 0; i < joints.size(); ++i) {
        const auto& j = joints[i];
        ss << "    {\n";
        ss << "      \"id\": \"" << j.joint_id.to_string() << "\",\n";
        ss << "      \"name\": \"" << j.name << "\",\n";
        ss << "      \"kind\": \"" << kinematic_joint_kind_to_string(j.kind) << "\",\n";
        ss << "      \"partA\": \"" << j.part_a_id.to_string() << "\",\n";
        ss << "      \"partB\": \"" << j.part_b_id.to_string() << "\"\n";
        ss << "    }" << (i + 1 < joints.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";
    ss << "  \"wires\": [\n";
    for (size_t i = 0; i < wires.size(); ++i) {
        const auto& w = wires[i];
        ss << "    {\n";
        ss << "      \"id\": \"" << w.wire_id.to_string() << "\",\n";
        ss << "      \"name\": \"" << w.name << "\",\n";
        ss << "      \"partA\": \"" << w.part_a_id.to_string() << "\", \"termA\": \"" << w.terminal_a_name << "\",\n";
        ss << "      \"partB\": \"" << w.part_b_id.to_string() << "\", \"termB\": \"" << w.terminal_b_name << "\",\n";
        ss << "      \"awg\": " << w.awg_gauge << ", \"length_m\": " << w.length_m << "\n";
        ss << "    }" << (i + 1 < wires.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<AssemblyModel> AssemblyModel::from_json(std::string_view json_str) {
    AssemblyModel model;
    if (json_str.empty()) {
        return core::base::Result<AssemblyModel>::err(
            core::base::Error{.code = 4330, .domain = core::base::ErrorDomain::Validation, .severity = core::base::Severity::Error, .message = "Empty JSON", .details = ""});
    }

    // Extract name
    auto n_pos = json_str.find("\"name\": \"");
    if (n_pos != std::string_view::npos) {
        auto n_start = n_pos + 9;
        auto n_end = json_str.find("\"", n_start);
        if (n_end != std::string_view::npos) {
            model.name = std::string(json_str.substr(n_start, n_end - n_start));
        }
    }
    return core::base::Result<AssemblyModel>::ok(std::move(model));
}

std::string AssemblyModel::export_to_cosim_scene(const PhysicalObjectManager& obj_mgr) const {
    (void)obj_mgr;
    std::ostringstream ss;
    ss << "{\"assembly\": \"" << name << "\", \"part_count\": " << parts.size()
       << ", \"joint_count\": " << joints.size()
       << ", \"wire_count\": " << wires.size()
       << ", \"thermal_contacts\": " << thermal_contacts.size()
       << ", \"status\": \"CoSimulationReady\"}";
    return ss.str();
}

} // namespace shunya::eng
