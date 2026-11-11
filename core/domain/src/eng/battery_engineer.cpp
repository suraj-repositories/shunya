#include "shunya/eng/battery_engineer.hpp"

#include "shunya/chem/li_ion_soc_tables.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::eng {

namespace {

constexpr double kFaradayCPerMol = 96485.33212;
constexpr double kGasConstantR = 8.31446261815324;
constexpr double kRefTempK = 298.15;

core::base::Error make_bat_error(int32_t code, std::string msg, std::string details = {}) {
    return core::base::Error{
        .code = code,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = std::move(details)
    };
}

std::string to_lower_copy(std::string_view sv) {
    std::string out(sv);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

bool contains_token(const core::domain::Material* m, std::string_view tok) {
    if (!m) return false;
    const std::string t = to_lower_copy(tok);
    return to_lower_copy(m->name()).find(t) != std::string::npos ||
           to_lower_copy(m->composition()).find(t) != std::string::npos;
}

double effective_sigma(const core::domain::Material* m) {
    if (!m) return 0.0;
    if (m->electrical().electrical_conductivity.has_value() &&
        *m->electrical().electrical_conductivity > 0.0) {
        return *m->electrical().electrical_conductivity;
    }
    if (m->electrical().electrical_resistivity.has_value() &&
        *m->electrical().electrical_resistivity > 0.0) {
        return 1.0 / (*m->electrical().electrical_resistivity);
    }
    return 0.0;
}

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += c;
    }
    return out;
}

std::string json_str(std::string_view json, std::string_view key) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*\"([^\"]*)\"");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) return m[1].str();
    return {};
}

double json_num(std::string_view json, std::string_view key, double fallback) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*([-+0-9eE\\.]+)");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        try { return std::stod(m[1].str()); } catch (...) { return fallback; }
    }
    return fallback;
}

} // namespace

std::string BatteryDesignSpec::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id.to_string() << "\",\n";
    ss << "  \"name\": \"" << escape_json(name) << "\",\n";
    ss << "  \"chemistry_label\": \"" << escape_json(chemistry_label) << "\",\n";
    ss << "  \"ambient_temp_k\": " << ambient_temp_k << ",\n";
    ss << "  \"initial_soc\": " << initial_soc << ",\n";
    ss << "  \"convective_h_w_m2k\": " << convective_h_w_m2k << ",\n";
    ss << "  \"footprint_area_m2\": " << geometry.footprint_area_m2 << ",\n";
    ss << "  \"stacked_layers\": " << geometry.stacked_layers << ",\n";
    ss << "  \"anode_thickness_m\": " << geometry.anode_thickness_m << ",\n";
    ss << "  \"cathode_thickness_m\": " << geometry.cathode_thickness_m << ",\n";
    ss << "  \"separator_thickness_m\": " << geometry.separator_thickness_m << ",\n";
    ss << "  \"electrolyte_thickness_m\": " << geometry.electrolyte_thickness_m << ",\n";
    ss << "  \"anode_collector_thickness_m\": " << geometry.anode_collector_thickness_m << ",\n";
    ss << "  \"cathode_collector_thickness_m\": " << geometry.cathode_collector_thickness_m << ",\n";
    ss << "  \"casing_thickness_m\": " << geometry.casing_thickness_m << ",\n";
    ss << "  \"active_mass_fraction\": " << geometry.active_mass_fraction << ",\n";
    ss << "  \"electrolyte_ionic_conductivity_s_m\": " << geometry.electrolyte_ionic_conductivity_s_m << ",\n";
    ss << "  \"anode_material_id\": \"" << materials.anode_material_id.to_string() << "\",\n";
    ss << "  \"cathode_material_id\": \"" << materials.cathode_material_id.to_string() << "\",\n";
    ss << "  \"electrolyte_material_id\": \"" << materials.electrolyte_material_id.to_string() << "\",\n";
    ss << "  \"separator_material_id\": \"" << materials.separator_material_id.to_string() << "\",\n";
    ss << "  \"anode_collector_material_id\": \"" << materials.anode_collector_material_id.to_string() << "\",\n";
    ss << "  \"cathode_collector_material_id\": \"" << materials.cathode_collector_material_id.to_string() << "\",\n";
    ss << "  \"casing_material_id\": \"" << materials.casing_material_id.to_string() << "\",\n";
    ss << "  \"positive_terminal_material_id\": \"" << materials.positive_terminal_material_id.to_string() << "\",\n";
    ss << "  \"negative_terminal_material_id\": \"" << materials.negative_terminal_material_id.to_string() << "\"\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<BatteryDesignSpec> BatteryDesignSpec::from_json(std::string_view src) {
    if (src.empty()) return make_bat_error(4501, "Empty BatteryDesignSpec JSON");
    BatteryDesignSpec s;
    if (auto id_opt = core::base::Uuid::from_string(json_str(src, "id"))) s.id = *id_opt;
    std::string nm = json_str(src, "name");
    if (!nm.empty()) s.name = std::move(nm);
    std::string ch = json_str(src, "chemistry_label");
    if (!ch.empty()) s.chemistry_label = std::move(ch);
    s.ambient_temp_k = json_num(src, "ambient_temp_k", 298.15);
    s.initial_soc = json_num(src, "initial_soc", 0.8);
    s.convective_h_w_m2k = json_num(src, "convective_h_w_m2k", 12.0);
    s.geometry.footprint_area_m2 = json_num(src, "footprint_area_m2", 0.02);
    s.geometry.stacked_layers = static_cast<int>(json_num(src, "stacked_layers", 12.0));
    s.geometry.anode_thickness_m = json_num(src, "anode_thickness_m", 80e-6);
    s.geometry.cathode_thickness_m = json_num(src, "cathode_thickness_m", 90e-6);
    s.geometry.separator_thickness_m = json_num(src, "separator_thickness_m", 25e-6);
    s.geometry.electrolyte_thickness_m = json_num(src, "electrolyte_thickness_m", 25e-6);
    s.geometry.anode_collector_thickness_m = json_num(src, "anode_collector_thickness_m", 12e-6);
    s.geometry.cathode_collector_thickness_m = json_num(src, "cathode_collector_thickness_m", 15e-6);
    s.geometry.casing_thickness_m = json_num(src, "casing_thickness_m", 0.5e-3);
    s.geometry.active_mass_fraction = json_num(src, "active_mass_fraction", 0.85);
    s.geometry.electrolyte_ionic_conductivity_s_m = json_num(src, "electrolyte_ionic_conductivity_s_m", 1.0);

    auto read_uuid = [&](const char* key, core::base::Uuid& dst) {
        if (auto u = core::base::Uuid::from_string(json_str(src, key))) dst = *u;
    };
    read_uuid("anode_material_id", s.materials.anode_material_id);
    read_uuid("cathode_material_id", s.materials.cathode_material_id);
    read_uuid("electrolyte_material_id", s.materials.electrolyte_material_id);
    read_uuid("separator_material_id", s.materials.separator_material_id);
    read_uuid("anode_collector_material_id", s.materials.anode_collector_material_id);
    read_uuid("cathode_collector_material_id", s.materials.cathode_collector_material_id);
    read_uuid("casing_material_id", s.materials.casing_material_id);
    read_uuid("positive_terminal_material_id", s.materials.positive_terminal_material_id);
    read_uuid("negative_terminal_material_id", s.materials.negative_terminal_material_id);
    return s;
}

BatteryDerivedParameters BatteryEngineer::evaluate_and_validate(
    const BatteryDesignSpec& spec,
    const core::domain::MaterialRegistry& reg) {
    BatteryDerivedParameters d;

    if (spec.geometry.footprint_area_m2 <= 0.0 || spec.geometry.stacked_layers <= 0) {
        d.compatibility_errors.push_back("Battery footprint area and stacked layer count must be positive.");
        return d;
    }

    auto fetch = [&](const core::base::Uuid& id, const char* region_name) -> const core::domain::Material* {
        const auto* m = reg.get_material(id);
        if (!m) {
            d.compatibility_errors.push_back(std::string("Missing material assignment for ") + region_name + ".");
        }
        return m;
    };

    const auto* m_anode   = fetch(spec.materials.anode_material_id, "Anode");
    const auto* m_cathode = fetch(spec.materials.cathode_material_id, "Cathode");
    const auto* m_elec    = fetch(spec.materials.electrolyte_material_id, "Electrolyte");
    const auto* m_sep     = fetch(spec.materials.separator_material_id, "Separator");
    const auto* m_ac      = fetch(spec.materials.anode_collector_material_id, "Anode Current Collector");
    const auto* m_cc      = fetch(spec.materials.cathode_collector_material_id, "Cathode Current Collector");
    const auto* m_case    = fetch(spec.materials.casing_material_id, "Casing");
    const auto* m_tpos    = fetch(spec.materials.positive_terminal_material_id, "Positive Terminal");
    const auto* m_tneg    = fetch(spec.materials.negative_terminal_material_id, "Negative Terminal");

    if (!d.compatibility_errors.empty()) {
        return d;
    }

    // Compatibility rules
    // 1. Separator must be electrically insulating to prevent internal short-circuit
    const double sig_sep = effective_sigma(m_sep);
    if (sig_sep >= 1e-2) {
        d.compatibility_errors.push_back(
            "Separator material '" + m_sep->name() + "' is electrically conductive (sigma=" +
            std::to_string(sig_sep) + " S/m), which causes an internal short circuit.");
    }

    // 2. Current collectors and terminals must be high-conductivity metals
    if (effective_sigma(m_ac) < 1e4) {
        d.compatibility_errors.push_back("Anode current collector '" + m_ac->name() + "' is not sufficiently conductive.");
    }
    if (effective_sigma(m_cc) < 1e4) {
        d.compatibility_errors.push_back("Cathode current collector '" + m_cc->name() + "' is not sufficiently conductive.");
    }
    if (effective_sigma(m_tpos) < 1e4 || effective_sigma(m_tneg) < 1e4) {
        d.compatibility_errors.push_back("Battery electrical terminals must use conductive materials.");
    }

    // 3. Electrochemical pairing check (Lithium-based chemistry validation)
    const bool has_li_cathode = contains_token(m_cathode, "li");
    const bool has_li_anode = contains_token(m_anode, "li");
    const bool has_carbon_anode = contains_token(m_anode, "carbon") ||
                                  contains_token(m_anode, "graphite") ||
                                  contains_token(m_anode, "c (") ||
                                  m_anode->name() == "C";
    if (!has_li_cathode && !has_li_anode) {
        d.compatibility_errors.push_back(
            "Lithium-based battery requires Lithium in cathode or anode material (neither '" +
            m_cathode->name() + "' nor '" + m_anode->name() + "' indicates Li).");
    }
    if (!has_carbon_anode && !has_li_anode && !contains_token(m_anode, "si")) {
        d.warnings.push_back(
            "Anode '" + m_anode->name() + "' is not standard graphite/Li/Si; capacity and OCV are approximate.");
    }

    // Check Al alloying hazard if Aluminum is used on the low-potential anode collector
    if (contains_token(m_ac, "aluminum") || m_ac->name() == "Al") {
        d.warnings.push_back(
            "Anode collector uses Aluminum: Al forms a destructive Li-Al alloy below ~0.3 V vs Li/Li+ (Copper is recommended for anode collector).");
    }

    // Check separator melting point
    if (m_sep->thermal().melting_point_k.has_value()) {
        d.max_safe_temp_k = std::min(d.max_safe_temp_k, (*m_sep->thermal().melting_point_k) - 20.0);
    }
    if (spec.ambient_temp_k >= d.max_safe_temp_k) {
        d.warnings.push_back("Ambient temperature exceeds recommended safe cell thermal limit.");
    }
    if (spec.ambient_temp_k >= d.thermal_runaway_onset_k) {
        d.compatibility_errors.push_back("Ambient temperature exceeds thermal runaway onset (393.15 K).");
    }

    // 4. Geometry, mass, capacity, internal resistance, thermal mass
    const double area = spec.geometry.footprint_area_m2;
    const double N = static_cast<double>(spec.geometry.stacked_layers);
    const double total_stack_area = area * N;

    auto region_mass_cp = [&](const core::domain::Material* m, double vol, double& mass_acc, double& cp_acc) {
        const double rho = std::max(1.0, m->density());
        const double mk = rho * vol;
        const double cp = m->thermal().specific_heat_capacity.value_or(800.0);
        mass_acc += mk;
        cp_acc += mk * cp;
        return mk;
    };

    const double v_an  = total_stack_area * spec.geometry.anode_thickness_m;
    const double v_ca  = total_stack_area * spec.geometry.cathode_thickness_m;
    const double v_sep = total_stack_area * spec.geometry.separator_thickness_m;
    const double v_el  = total_stack_area * spec.geometry.electrolyte_thickness_m;
    const double v_ac  = total_stack_area * spec.geometry.anode_collector_thickness_m;
    const double v_cc  = total_stack_area * spec.geometry.cathode_collector_thickness_m;

    const double stack_height = N * (spec.geometry.anode_thickness_m + spec.geometry.cathode_thickness_m +
                                     spec.geometry.separator_thickness_m + spec.geometry.anode_collector_thickness_m +
                                     spec.geometry.cathode_collector_thickness_m);
    const double side = std::sqrt(area);
    const double outer_surface_area = 2.0 * area + 4.0 * side * stack_height;
    const double v_case = outer_surface_area * spec.geometry.casing_thickness_m;

    double total_mass = 0.0;
    double total_cp_j_k = 0.0;
    (void)region_mass_cp(m_anode, v_an, total_mass, total_cp_j_k);
    const double m_ca = region_mass_cp(m_cathode, v_ca, total_mass, total_cp_j_k);
    (void)region_mass_cp(m_sep, v_sep, total_mass, total_cp_j_k);
    (void)region_mass_cp(m_elec, v_el, total_mass, total_cp_j_k);
    (void)region_mass_cp(m_ac, v_ac, total_mass, total_cp_j_k);
    (void)region_mass_cp(m_cc, v_cc, total_mass, total_cp_j_k);
    (void)region_mass_cp(m_case, v_case, total_mass, total_cp_j_k);

    d.total_volume_m3 = v_an + v_ca + v_sep + v_el + v_ac + v_cc + v_case;
    d.total_mass_kg = total_mass;
    d.thermal_mass_j_k = std::max(1.0, total_cp_j_k);
    d.cooling_h_area_w_k = std::max(0.05, spec.convective_h_w_m2k * outer_surface_area);

    // Faraday theoretical capacity from active cathode mass:
    // Typical LiFePO4 ~ 170 mAh/g, LiCoO2/NMC ~ 155-180 mAh/g; derived from 1 mol e- / 0.158 kg active host -> ~170 mAh/g
    d.active_cathode_mass_kg = m_ca * std::clamp(spec.geometry.active_mass_fraction, 0.1, 1.0);
    constexpr double kRepresentativeMolarHostKgPerMol = 0.15776; // LiFePO4 molar mass = 157.76 g/mol
    const double mol_active = d.active_cathode_mass_kg / kRepresentativeMolarHostKgPerMol;
    const double theoretical_coulombs = mol_active * kFaradayCPerMol;
    d.nominal_capacity_ah = std::max(0.05, theoretical_coulombs / 3600.0);

    // Voltage profile: LiFePO4 ~3.2-3.65V vs NMC/LCO ~3.7-4.2V
    if (contains_token(m_cathode, "lfp") || contains_token(m_cathode, "lifepo4") ||
        (contains_token(m_cathode, "fe") && contains_token(m_cathode, "p"))) {
        d.nominal_voltage_v = 3.3;
        d.max_charge_voltage_v = 3.65;
        d.min_cutoff_voltage_v = 2.5;
    } else {
        d.nominal_voltage_v = 3.7;
        d.max_charge_voltage_v = 4.2;
        d.min_cutoff_voltage_v = 2.8;
    }

    // Internal resistance R0 = ionic electrolyte/separator path + collector/terminal ohmic resistance
    const double r_ionic = spec.geometry.electrolyte_thickness_m /
                           (std::max(0.01, spec.geometry.electrolyte_ionic_conductivity_s_m) * total_stack_area);
    const double r_tpos = spec.geometry.terminal_length_m /
                          (std::max(1.0, effective_sigma(m_tpos)) * spec.geometry.terminal_area_m2);
    const double r_tneg = spec.geometry.terminal_length_m /
                          (std::max(1.0, effective_sigma(m_tneg)) * spec.geometry.terminal_area_m2);
    const double r_collectors = (side / (effective_sigma(m_ac) * spec.geometry.anode_collector_thickness_m * side * N)) +
                                (side / (effective_sigma(m_cc) * spec.geometry.cathode_collector_thickness_m * side * N));
    constexpr double kContactAndSeiOhm = 0.012;
    d.internal_r0_25c_ohm = std::max(0.005, r_ionic + r_tpos + r_tneg + r_collectors + kContactAndSeiOhm);

    d.specific_energy_wh_kg = (d.total_mass_kg > 1e-6)
        ? (d.nominal_capacity_ah * d.nominal_voltage_v) / d.total_mass_kg
        : 0.0;

    d.is_compatible = d.compatibility_errors.empty();
    return d;
}

core::base::Result<BatterySimulationResult> BatteryEngineer::simulate_discharge_and_charge(
    const BatteryDesignSpec& spec,
    const core::domain::MaterialRegistry& reg,
    double discharge_c_rate,
    double discharge_duration_s,
    double charge_c_rate,
    double charge_duration_s,
    double dt_s) {
    if (dt_s <= 0.0 || discharge_c_rate < 0.0 || charge_c_rate < 0.0 ||
        discharge_duration_s < 0.0 || charge_duration_s < 0.0) {
        return make_bat_error(4510, "Battery simulation rates, durations, and dt must be valid non-negative numbers (dt > 0).");
    }

    auto derived = evaluate_and_validate(spec, reg);
    if (!derived.is_compatible) {
        std::string msg = derived.compatibility_errors.empty() ? "Incompatible battery materials" : derived.compatibility_errors.front();
        return make_bat_error(4511, "Battery compatibility validation failed", std::move(msg));
    }

    const double q_cap_c = derived.nominal_capacity_ah * 3600.0;
    const double r1 = 0.015;
    const double c1 = 800.0;
    const double r2 = 0.025;
    const double c2 = 4000.0;
    const double ea = 15000.0;

    double soc = std::clamp(spec.initial_soc, 0.0, 1.0);
    double v1 = 0.0;
    double v2 = 0.0;
    double temp_k = spec.ambient_temp_k;
    double t_s = 0.0;
    double cap_ah = 0.0;

    BatterySimulationResult res;
    res.warnings = derived.warnings;
    res.peak_temperature_k = temp_k;
    res.min_voltage_v = 1e9;
    res.max_voltage_v = -1e9;

    const double v_scale = derived.nominal_voltage_v / 3.7;
    auto ocv_at = [&](double s) -> double {
        return chem::LiIonSocTables::lookup_ocv_v(s) * v_scale;
    };

    auto step_current = [&](double current_a, bool is_charging) {
        const double t_safe = std::max(150.0, temp_k);
        const double r0 = derived.internal_r0_25c_ohm *
                          std::exp(std::clamp((ea / kGasConstantR) * ((1.0 / t_safe) - (1.0 / kRefTempK)), -10.0, 10.0));
        double i_actual = current_a;

        // CC-CV protection on charge if terminal voltage reaches max_charge_voltage_v
        const double ocv = ocv_at(soc);
        if (is_charging) {
            const double v_pred = ocv - i_actual * r0 - v1 - v2;
            if (v_pred > derived.max_charge_voltage_v) {
                i_actual = -std::max(0.0, (derived.max_charge_voltage_v - ocv + v1 + v2) / std::max(1e-4, r0));
            }
            if (soc >= 0.9999) i_actual = 0.0;
        } else {
            if (soc <= 0.0001) i_actual = 0.0;
        }

        soc = std::clamp(soc - (i_actual * dt_s) / q_cap_c, 0.0, 1.0);
        const double a1 = std::exp(-dt_s / (r1 * c1));
        const double a2 = std::exp(-dt_s / (r2 * c2));
        v1 = v1 * a1 + (i_actual * r1) * (1.0 - a1);
        v2 = v2 * a2 + (i_actual * r2) * (1.0 - a2);

        const double ocv_new = ocv_at(soc);
        const double v_term = std::clamp(ocv_new - i_actual * r0 - v1 - v2, 0.0, derived.max_charge_voltage_v + 0.5);
        const double q_joule = i_actual * i_actual * r0 + (v1 * v1) / r1 + (v2 * v2) / r2;
        double q_tot = q_joule;
        if (temp_k >= derived.thermal_runaway_onset_k) {
            q_tot += 120.0 * std::exp(0.04 * std::min(temp_k - derived.thermal_runaway_onset_k, 80.0));
        }
        const double q_cool = derived.cooling_h_area_w_k * (temp_k - spec.ambient_temp_k);
        temp_k = std::max(100.0, temp_k + ((q_tot - q_cool) / derived.thermal_mass_j_k) * dt_s);
        t_s += dt_s;
        cap_ah += std::abs(i_actual) * dt_s / 3600.0;

        BatterySimSample smp;
        smp.time_s = t_s;
        smp.soc = soc;
        smp.ocv_v = ocv_new;
        smp.voltage_v = v_term;
        smp.current_a = i_actual;
        smp.power_w = v_term * i_actual;
        smp.capacity_ah = cap_ah;
        smp.temperature_k = temp_k;
        smp.heat_generation_w = q_tot;
        res.samples.push_back(smp);

        res.peak_temperature_k = std::max(res.peak_temperature_k, temp_k);
        res.min_voltage_v = std::min(res.min_voltage_v, v_term);
        res.max_voltage_v = std::max(res.max_voltage_v, v_term);
        res.total_energy_wh += std::abs(smp.power_w) * dt_s / 3600.0;
    };

    const int n_dis = static_cast<int>(std::round(discharge_duration_s / dt_s));
    const double i_dis = discharge_c_rate * derived.nominal_capacity_ah;
    for (int k = 0; k < n_dis; ++k) {
        step_current(i_dis, false);
    }

    const int n_chg = static_cast<int>(std::round(charge_duration_s / dt_s));
    const double i_chg = -charge_c_rate * derived.nominal_capacity_ah;
    for (int k = 0; k < n_chg; ++k) {
        step_current(i_chg, true);
    }

    if (res.peak_temperature_k >= derived.max_safe_temp_k) {
        res.had_limit_warnings = true;
        res.warnings.push_back("THERMAL WARNING: Peak cell temperature (" +
                               std::to_string(res.peak_temperature_k - 273.15) +
                               " C) exceeded safe separator limit.");
    }
    if (res.min_voltage_v <= derived.min_cutoff_voltage_v && discharge_duration_s > 0.0) {
        res.had_limit_warnings = true;
        res.warnings.push_back("UNDER-VOLTAGE WARNING: Terminal voltage reached cutoff threshold (" +
                               std::to_string(derived.min_cutoff_voltage_v) + " V).");
    }
    if (discharge_c_rate > 3.0 || charge_c_rate > 2.0) {
        res.had_limit_warnings = true;
        res.warnings.push_back("HIGH C-RATE WARNING: Operating C-rate accelerates heating and lithium plating risk.");
    }

    res.completed = true;
    return res;
}

core::base::Result<GeneratedComponentDefinition> BatteryEngineer::generate_component(
    BatteryDesignSpec& spec,
    const core::domain::MaterialRegistry& reg,
    PhysicalObjectManager* obj_mgr) {
    spec.derived = evaluate_and_validate(spec, reg);
    if (!spec.derived.is_compatible) {
        std::string msg = spec.derived.compatibility_errors.empty()
            ? "Battery failed material compatibility validation"
            : spec.derived.compatibility_errors.front();
        return make_bat_error(4520, "Cannot generate Battery component", std::move(msg));
    }

    core::base::Uuid source_obj_id;
    if (obj_mgr) {
        PhysicalObject cell_obj;
        cell_obj.name = spec.name + "_PhysicalAssembly";
        cell_obj.category = "BatteryAssembly";
        const double side = std::sqrt(spec.geometry.footprint_area_m2);
        const double height = std::max(1e-3, spec.derived.total_volume_m3 / spec.geometry.footprint_area_m2);
        cell_obj.geometry.kind = ObjectShapeKind::Box;
        cell_obj.geometry.dim_x_m = side;
        cell_obj.geometry.dim_y_m = height;
        cell_obj.geometry.dim_z_m = side;
        cell_obj.operating_temp_k = spec.ambient_temp_k;
        cell_obj.regions = {
            {"anode",     "Anode",     spec.materials.anode_material_id,     0.25, spec.geometry.anode_thickness_m, false},
            {"cathode",   "Cathode",   spec.materials.cathode_material_id,   0.30, spec.geometry.cathode_thickness_m, false},
            {"separator", "Separator", spec.materials.separator_material_id, 0.10, spec.geometry.separator_thickness_m, false},
            {"casing",    "Casing",    spec.materials.casing_material_id,    0.35, spec.geometry.casing_thickness_m, true}
        };
        auto add_res = obj_mgr->add_object(std::move(cell_obj), reg);
        if (add_res.is_ok()) {
            source_obj_id = add_res.value();
        }
    }

    GeneratedComponentDefinition comp;
    comp.name = spec.name;
    comp.kind = ComponentKind::Battery;
    comp.source_object_id = source_obj_id;
    comp.battery_spec_id = spec.id;
    comp.terminals = {
        {"+", core::domain::ElectricalPinPolarity::Positive, { 0.02, 0.0, 0.0}, "positive_terminal"},
        {"-", core::domain::ElectricalPinPolarity::Negative, {-0.02, 0.0, 0.0}, "negative_terminal"}
    };
    comp.config.nominal_voltage_v = spec.derived.nominal_voltage_v;
    comp.config.capacity_ah = spec.derived.nominal_capacity_ah;
    comp.config.internal_resistance_ohm = spec.derived.internal_r0_25c_ohm;
    comp.config.initial_soc = spec.initial_soc;

    comp.derived.is_qualified = true;
    comp.derived.primary_value = spec.derived.nominal_voltage_v;
    comp.derived.series_resistance_ohm = spec.derived.internal_r0_25c_ohm;
    comp.derived.thermal_mass_j_k = spec.derived.thermal_mass_j_k;
    comp.derived.thermal_conductance_w_k = spec.derived.cooling_h_area_w_k;
    comp.derived.max_current_a = spec.derived.nominal_capacity_ah * 5.0;
    comp.derived.max_voltage_v = spec.derived.max_charge_voltage_v;
    comp.derived.warnings = spec.derived.warnings;
    return comp;
}

} // namespace shunya::eng
