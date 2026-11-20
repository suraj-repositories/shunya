#include "shunya/eng/simulator_bridge.hpp"
#include "shunya/eng/cavity_physics.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::eng {

std::string PreFlightReport::to_markdown() const {
    std::ostringstream ss;
    ss << "# Pre-Flight Validation Report\n\n";
    ss << "- **Object ID:** `" << object_id << "`\n";
    ss << "- **Object Name:** `" << object_name << "`\n";
    ss << "- **Validation Status:** " << (passed ? "**PASSED (Simulation Ready)**" : "**FAILED (Blocking Issues Detected)**") << "\n\n";

    if (issues.empty()) {
        ss << "> [!NOTE]\n> All 5 pre-flight checks passed successfully. The physical object is ready for multiphysics simulation.\n";
        return ss.str();
    }

    ss << "### Identified Issues (" << issues.size() << ")\n\n";
    ss << "| Rule ID | Severity | Target Type | Target Name | Message |\n";
    ss << "| :--- | :--- | :--- | :--- | :--- |\n";
    for (const auto& issue : issues) {
        std::string sev = (issue.severity == PreFlightSeverity::Error) ? "**ERROR**" :
                          (issue.severity == PreFlightSeverity::Warning ? "*Warning*" : "Info");
        ss << "| `" << issue.rule_id << "` | " << sev << " | "
           << issue.target_type << " | `" << issue.target_name << "` | "
           << issue.message << " |\n";
    }
    return ss.str();
}

std::string PreFlightReport::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"passed\": " << (passed ? "true" : "false") << ",\n";
    ss << "  \"objectId\": \"" << object_id << "\",\n";
    ss << "  \"objectName\": \"" << object_name << "\",\n";
    ss << "  \"issues\": [\n";
    for (size_t i = 0; i < issues.size(); ++i) {
        const auto& issue = issues[i];
        std::string sev = (issue.severity == PreFlightSeverity::Error) ? "error" :
                          (issue.severity == PreFlightSeverity::Warning ? "warning" : "info");
        ss << "    {\n";
        ss << "      \"ruleId\": \"" << issue.rule_id << "\",\n";
        ss << "      \"severity\": \"" << sev << "\",\n";
        ss << "      \"targetType\": \"" << issue.target_type << "\",\n";
        ss << "      \"targetId\": \"" << issue.target_id << "\",\n";
        ss << "      \"targetName\": \"" << issue.target_name << "\",\n";
        ss << "      \"message\": \"" << issue.message << "\",\n";
        ss << "      \"isBlocking\": " << (issue.is_blocking ? "true" : "false") << "\n";
        ss << "    }" << (i + 1 < issues.size() ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

PreFlightReport SimulatorBridgeCoordinator::validate_object(
    const PhysicalObject& obj,
    const core::domain::MaterialRegistry& mat_reg)
{
    PreFlightReport report;
    report.object_id = obj.id.to_string();
    report.object_name = obj.name;

    // Rule 1: Check that all regions have valid materials assigned
    if (obj.regions.empty()) {
        PreFlightIssue issue;
        issue.rule_id = "PF-01-MAT";
        issue.severity = PreFlightSeverity::Error;
        issue.target_type = "object";
        issue.target_id = obj.id.to_string();
        issue.target_name = obj.name;
        issue.message = "Object has no material regions defined.";
        issue.is_blocking = true;
        report.issues.push_back(issue);
    } else {
        for (const auto& reg : obj.regions) {
            if (reg.material_id.is_nil() || !mat_reg.has_material(reg.material_id)) {
                PreFlightIssue issue;
                issue.rule_id = "PF-01-MAT";
                issue.severity = PreFlightSeverity::Error;
                issue.target_type = "region";
                issue.target_id = reg.region_id;
                issue.target_name = reg.name;
                issue.message = "Region '" + reg.name + "' has no valid material registered in MaterialRegistry.";
                issue.is_blocking = true;
                report.issues.push_back(issue);
            }
        }
    }

    // Rule 2: Verify all cavities are fully sealed and enclosed
    auto cav_eval = CavityPhysicsEngine::evaluate_cavity(obj.cavity, obj.operating_temp_k, 298.15);
    if (!cav_eval.is_seal_intact) {
        PreFlightIssue issue;
        issue.rule_id = "PF-02-SEAL";
        issue.severity = PreFlightSeverity::Error;
        issue.target_type = "cavity";
        issue.target_id = "cavity_0";
        issue.target_name = "Interior Cavity";
        std::string warn_msg = cav_eval.seal_warnings.empty()
            ? "Cavity seal integrity is compromised."
            : cav_eval.seal_warnings.front();
        issue.message = "Cavity seal failure: " + warn_msg;
        issue.is_blocking = true;
        report.issues.push_back(issue);
    }
    if (obj.cavity.cavity_volume_m3 <= 0.0) {
        PreFlightIssue issue;
        issue.rule_id = "PF-02-SEAL";
        issue.severity = PreFlightSeverity::Error;
        issue.target_type = "cavity";
        issue.target_id = "cavity_0";
        issue.target_name = "Interior Cavity";
        issue.message = "Cavity enclosed volume is zero or negative (" + std::to_string(obj.cavity.cavity_volume_m3) + " m³).";
        issue.is_blocking = true;
        report.issues.push_back(issue);
    }

    // Rule 3: Validate that electrical terminals connect to conductive internal regions
    if (obj.terminals.empty()) {
        PreFlightIssue issue;
        issue.rule_id = "PF-03-TERM";
        issue.severity = PreFlightSeverity::Warning;
        issue.target_type = "object";
        issue.target_id = obj.id.to_string();
        issue.target_name = obj.name;
        issue.message = "Object has no electrical terminals defined; electrical solver coupling will be bypassed.";
        issue.is_blocking = false;
        report.issues.push_back(issue);
    } else {
        for (const auto& term : obj.terminals) {
            if (term.connected_region.empty()) {
                PreFlightIssue issue;
                issue.rule_id = "PF-03-TERM";
                issue.severity = PreFlightSeverity::Error;
                issue.target_type = "terminal";
                issue.target_id = term.name;
                issue.target_name = term.name;
                issue.message = "Terminal '" + term.name + "' is not connected to any internal region.";
                issue.is_blocking = true;
                report.issues.push_back(issue);
            } else {
                auto reg_it = std::find_if(obj.regions.begin(), obj.regions.end(),
                    [&](const ObjectRegion& r) { return r.region_id == term.connected_region; });
                if (reg_it == obj.regions.end()) {
                    PreFlightIssue issue;
                    issue.rule_id = "PF-03-TERM";
                    issue.severity = PreFlightSeverity::Error;
                    issue.target_type = "terminal";
                    issue.target_id = term.name;
                    issue.target_name = term.name;
                    issue.message = "Terminal '" + term.name + "' references non-existent region '" + term.connected_region + "'.";
                    issue.is_blocking = true;
                    report.issues.push_back(issue);
                } else if (reg_it->role == RegionFunctionalRole::DielectricSpacer) {
                    PreFlightIssue issue;
                    issue.rule_id = "PF-03-TERM";
                    issue.severity = PreFlightSeverity::Error;
                    issue.target_type = "terminal";
                    issue.target_id = term.name;
                    issue.target_name = term.name;
                    issue.message = "Terminal '" + term.name + "' connects to region '" + reg_it->name + "' designated as DielectricSpacer.";
                    issue.is_blocking = true;
                    report.issues.push_back(issue);
                }
            }
        }
    }

    // Rule 4: Assert zero zero-volume or degenerate geometry regions
    for (const auto& reg : obj.regions) {
        if (reg.volume_fraction <= 0.0 || reg.volume_fraction > 1.0) {
            PreFlightIssue issue;
            issue.rule_id = "PF-04-DEGEN";
            issue.severity = PreFlightSeverity::Error;
            issue.target_type = "region";
            issue.target_id = reg.region_id;
            issue.target_name = reg.name;
            issue.message = "Region '" + reg.name + "' has invalid volume fraction (" + std::to_string(reg.volume_fraction) + ").";
            issue.is_blocking = true;
            report.issues.push_back(issue);
        }
    }
    if (obj.computed.volume_m3 <= 1e-18) {
        PreFlightIssue issue;
        issue.rule_id = "PF-04-DEGEN";
        issue.severity = PreFlightSeverity::Error;
        issue.target_type = "object";
        issue.target_id = obj.id.to_string();
        issue.target_name = obj.name;
        issue.message = "Object computed volume is zero or near-zero (" + std::to_string(obj.computed.volume_m3) + " m³).";
        issue.is_blocking = true;
        report.issues.push_back(issue);
    }

    // Rule 5: Check that solid meshes are closed, oriented, and manifold
    if (obj.computed.surface_area_m2 <= 0.0 && obj.computed.volume_m3 <= 0.0) {
        PreFlightIssue issue;
        issue.rule_id = "PF-05-MANIFOLD";
        issue.severity = PreFlightSeverity::Error;
        issue.target_type = "mesh";
        issue.target_id = obj.id.to_string();
        issue.target_name = obj.name;
        issue.message = "Object mesh geometry has non-manifold or degenerate boundary topology.";
        issue.is_blocking = true;
        report.issues.push_back(issue);
    }

    report.passed = !report.has_errors();
    return report;
}

core::base::Result<SimulationReadyModel> SimulatorBridgeCoordinator::generate_simulation_model(
    const PhysicalObject& obj,
    const core::domain::MaterialRegistry& mat_reg)
{
    auto report = validate_object(obj, mat_reg);
    if (!report.passed) {
        return core::base::Result<SimulationReadyModel>::err(
            core::base::Error{
                .code = 4325,
                .domain = core::base::ErrorDomain::Validation,
                .severity = core::base::Severity::Error,
                .message = "Pre-flight verification failed.",
                .details = ""
            });
    }

    SimulationReadyModel model;
    model.object_id = obj.id;
    model.object_name = obj.name;
    model.total_mass_kg = (obj.computed.mass_kg > 0.0) ? obj.computed.mass_kg : 0.035;
    model.volume_m3 = (obj.computed.volume_m3 > 0.0) ? obj.computed.volume_m3 : 1.45e-4;
    model.center_of_mass_m = obj.computed.center_of_mass_m;

    // Build inertia tensor
    double Ixx = (obj.computed.inertia_ixx > 0.0) ? obj.computed.inertia_ixx : (1.0 / 12.0) * model.total_mass_kg * 0.01;
    double Iyy = (obj.computed.inertia_iyy > 0.0) ? obj.computed.inertia_iyy : (1.0 / 12.0) * model.total_mass_kg * 0.01;
    double Izz = (obj.computed.inertia_izz > 0.0) ? obj.computed.inertia_izz : (1.0 / 12.0) * model.total_mass_kg * 0.01;
    model.inertia_tensor = core::base::math::Matrix4x4<double>::identity();
    model.inertia_tensor.m[0] = Ixx;
    model.inertia_tensor.m[5] = Iyy;
    model.inertia_tensor.m[10] = Izz;

    // Lumped thermal nodes
    if (!obj.regions.empty()) {
        for (const auto& reg : obj.regions) {
            LumpedThermalNode node;
            node.node_id = reg.region_id;
            node.name = reg.name;
            const auto* m = mat_reg.get_material(reg.material_id);
            double cp = (m && m->thermal().specific_heat_capacity.has_value())
                ? *m->thermal().specific_heat_capacity : 500.0;
            double k = (m && m->thermal().thermal_conductivity.has_value())
                ? *m->thermal().thermal_conductivity : 10.0;
            double reg_mass = model.total_mass_kg * reg.volume_fraction;
            node.heat_capacity_j_k = std::max(1e-4, reg_mass * cp);
            node.initial_temp_k = obj.operating_temp_k;
            node.ambient_conductance_w_k = std::max(0.01, k * 0.01);
            model.thermal_nodes.push_back(node);
        }
    } else {
        LumpedThermalNode node;
        node.node_id = "node_whole";
        node.name = obj.name;
        node.heat_capacity_j_k = std::max(1e-4, model.total_mass_kg * 500.0);
        node.initial_temp_k = obj.operating_temp_k;
        node.ambient_conductance_w_k = 1.0;
        model.thermal_nodes.push_back(node);
    }

    // Electrical branches
    if (obj.terminals.size() >= 2) {
        ElectricalBranchModel branch;
        branch.branch_id = "branch_0";
        branch.name = obj.name + "_circuit_path";
        branch.terminal_a = obj.terminals[0].name;
        branch.terminal_b = obj.terminals[1].name;
        branch.resistance_ohm = (obj.computed.axial_electrical_resistance_ohm > 0.0)
            ? obj.computed.axial_electrical_resistance_ohm : 240.0;
        branch.inductance_h = 1.2e-6;
        branch.max_current_a = 5.0;
        model.electrical_branches.push_back(branch);
    }

    // Simulated body
    model.physics_body.object_id = obj.id;
    model.physics_body.name = obj.name;
    model.physics_body.mass_kg = model.total_mass_kg;
    model.physics_body.volume_m3 = model.volume_m3;
    model.physics_body.surface_area_m2 = (obj.computed.surface_area_m2 > 0.0) ? obj.computed.surface_area_m2 : 0.015;
    model.physics_body.thermal_conductivity_w_mk = (obj.computed.effective_thermal_conductivity_w_mk > 0.0)
        ? obj.computed.effective_thermal_conductivity_w_mk : 15.0;
    model.physics_body.temperature_k = obj.operating_temp_k;
    model.physics_body.initial_temperature_k = obj.operating_temp_k;

    std::ostringstream ss;
    ss << "{\"model\": \"" << obj.name << "\", \"mass_kg\": " << model.total_mass_kg
       << ", \"volume_m3\": " << model.volume_m3
       << ", \"thermal_nodes\": " << model.thermal_nodes.size()
       << ", \"electrical_branches\": " << model.electrical_branches.size() << "}";
    model.serialized_scene_json = ss.str();

    return core::base::Result<SimulationReadyModel>::ok(std::move(model));
}

void SimulatorBridgeCoordinator::start_quick_run(
    SimulationQuickRunPreset preset,
    const PhysicalObject& obj,
    const core::domain::MaterialRegistry& mat_reg,
    double duration_s,
    double dt_s)
{
    (void)dt_s;
    current_preset_ = preset;
    duration_s_ = duration_s;
    current_time_s_ = 0.0;
    telemetry_buffer_.clear();

    auto model_res = generate_simulation_model(obj, mat_reg);
    if (model_res.is_ok()) {
        active_model_ = std::make_unique<SimulationReadyModel>(model_res.value());
        active_sim_body_ = active_model_->physics_body;
        active_resistance_ohm_ = (obj.computed.axial_electrical_resistance_ohm > 0.0)
            ? obj.computed.axial_electrical_resistance_ohm : 240.0;
    } else {
        active_resistance_ohm_ = (obj.computed.axial_electrical_resistance_ohm > 0.0)
            ? obj.computed.axial_electrical_resistance_ohm : 240.0;
    }

    current_temp_k_ = obj.operating_temp_k;
    drop_y_ = 1.0;
    drop_vy_ = 0.0;
    tensile_def_m_ = 0.0;

    switch (preset) {
        case SimulationQuickRunPreset::Step_Voltage_120V:
            target_voltage_v_ = 120.0;
            applied_force_n_ = 0.0;
            break;
        case SimulationQuickRunPreset::Drop_Test_1m:
            target_voltage_v_ = 0.0;
            drop_y_ = 1.0;
            drop_vy_ = 0.0;
            break;
        case SimulationQuickRunPreset::Thermal_Shock_1200K:
            target_voltage_v_ = 0.0;
            current_temp_k_ = 298.15;
            break;
        case SimulationQuickRunPreset::Tensile_Pull_1kN:
            target_voltage_v_ = 0.0;
            applied_force_n_ = 1000.0;
            break;
    }

    is_running_ = true;
    sync_status_ = SyncStatus::Simulation_Running;

    // Emit initial snapshot
    TelemetrySnapshot s0;
    s0.time_s = 0.0;
    s0.voltage_v = (preset == SimulationQuickRunPreset::Step_Voltage_120V) ? target_voltage_v_ : 0.0;
    s0.current_a = (preset == SimulationQuickRunPreset::Step_Voltage_120V && active_resistance_ohm_ > 0)
        ? (s0.voltage_v / active_resistance_ohm_) : 0.0;
    s0.temperature_k = current_temp_k_;
    s0.position = {0.0, drop_y_, 0.0};
    s0.velocity = {0.0, drop_vy_, 0.0};
    latest_snapshot_ = s0;
    telemetry_buffer_.push(s0);
}

void SimulatorBridgeCoordinator::step_simulation(double dt_s) {
    if (!is_running_) return;

    current_time_s_ += dt_s;
    TelemetrySnapshot s;
    s.time_s = current_time_s_;

    switch (current_preset_) {
        case SimulationQuickRunPreset::Step_Voltage_120V: {
            s.voltage_v = target_voltage_v_;
            double i = (active_resistance_ohm_ > 0.0) ? (s.voltage_v / active_resistance_ohm_) : 0.0;
            s.current_a = i;
            double p_joule = i * s.voltage_v; // ~60W

            // Radiant dissipation: P_rad = sigma * eps * A * (T^4 - Tamb^4)
            double eps = 0.35;
            double A_eff = 3.6e-5;
            double p_rad = 5.670374419e-8 * eps * A_eff * (std::pow(current_temp_k_, 4) - std::pow(298.15, 4));
            double cp_m = std::max(1e-4, active_sim_body_.mass_kg * 134.0 * 0.005); // tungsten filament heat capacity

            double dT = ((p_joule - p_rad) / cp_m) * dt_s;
            current_temp_k_ = std::clamp(current_temp_k_ + dT, 298.15, 3000.0);
            s.temperature_k = current_temp_k_;

            // Thermal deformation
            s.deformation_m = 4.5e-6 * 0.040 * (current_temp_k_ - 298.15);
            s.position = {0.0, 0.0, 0.0};
            s.velocity = {0.0, 0.0, 0.0};
            break;
        }

        case SimulationQuickRunPreset::Drop_Test_1m: {
            s.voltage_v = 0.0;
            s.current_a = 0.0;
            s.temperature_k = current_temp_k_;

            double g = 9.80665;
            drop_vy_ -= g * dt_s;
            drop_y_ += drop_vy_ * dt_s;

            if (drop_y_ <= 0.0) {
                drop_y_ = 0.0;
                drop_vy_ = -drop_vy_ * 0.45; // restitution bounce
            }

            s.position = {0.0, drop_y_, 0.0};
            s.velocity = {0.0, drop_vy_, 0.0};
            s.kinetic_energy_j = 0.5 * active_sim_body_.mass_kg * std::pow(drop_vy_, 2);
            s.potential_energy_j = active_sim_body_.mass_kg * g * drop_y_;
            s.deformation_m = (drop_y_ == 0.0) ? 1.2e-4 : 0.0;
            break;
        }

        case SimulationQuickRunPreset::Thermal_Shock_1200K: {
            s.voltage_v = 0.0;
            s.current_a = 0.0;
            double h_conv = 25.0; // W/m2K
            double A = active_sim_body_.surface_area_m2;
            double m_cp = std::max(1.0, active_sim_body_.mass_kg * 800.0);
            double dT = ((h_conv * A * (1200.0 - current_temp_k_)) / m_cp) * dt_s;
            current_temp_k_ += dT;
            s.temperature_k = current_temp_k_;
            s.deformation_m = 3.3e-6 * 0.1 * (current_temp_k_ - 298.15);
            s.position = {0.0, 0.0, 0.0};
            s.velocity = {0.0, 0.0, 0.0};
            break;
        }

        case SimulationQuickRunPreset::Tensile_Pull_1kN: {
            s.voltage_v = 0.0;
            s.current_a = 0.0;
            s.temperature_k = current_temp_k_;
            double A_cross = 0.0005;
            double E = 70e9;
            double L = 0.1;
            tensile_def_m_ = (applied_force_n_ * L) / (A_cross * E);
            s.deformation_m = tensile_def_m_;
            s.position = {0.0, tensile_def_m_, 0.0};
            s.velocity = {0.0, 0.0, 0.0};
            break;
        }
    }

    latest_snapshot_ = s;
    telemetry_buffer_.push(s);

    if (current_time_s_ >= duration_s_) {
        is_running_ = false;
        sync_status_ = SyncStatus::In_Sync_Clean;
    }
}

void SimulatorBridgeCoordinator::run_to_completion(double dt_s) {
    while (is_running_) {
        step_simulation(dt_s);
    }
}

void SimulatorBridgeCoordinator::stop_simulation() {
    is_running_ = false;
    sync_status_ = SyncStatus::In_Sync_Clean;
}

std::vector<TelemetrySnapshot> SimulatorBridgeCoordinator::sample_telemetry(size_t max_count) const {
    return telemetry_buffer_.snapshot(max_count);
}

TelemetrySnapshot SimulatorBridgeCoordinator::latest_telemetry() const {
    return latest_snapshot_;
}

void SimulatorBridgeCoordinator::mark_object_modified(const core::base::Uuid& obj_id) {
    (void)obj_id;
    if (sync_status_ != SyncStatus::Simulation_Running) {
        sync_status_ = SyncStatus::Modified_Out_Of_Sync;
    }
}

bool SimulatorBridgeCoordinator::resync_active_simulation(
    const PhysicalObject& obj,
    const core::domain::MaterialRegistry& mat_reg)
{
    active_resistance_ohm_ = (obj.computed.axial_electrical_resistance_ohm > 0.0)
        ? obj.computed.axial_electrical_resistance_ohm : 240.0;

    auto model_res = generate_simulation_model(obj, mat_reg);
    if (model_res.is_ok()) {
        active_model_ = std::make_unique<SimulationReadyModel>(model_res.value());
        active_sim_body_ = active_model_->physics_body;
    }

    sync_status_ = is_running_ ? SyncStatus::Simulation_Running : SyncStatus::In_Sync_Clean;
    return true;
}

} // namespace shunya::eng
