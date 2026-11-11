#include "shunya/eng/component_generator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::eng {

namespace {

constexpr double kEps0 = 8.8541878128e-12;
constexpr double kMu0  = 1.25663706212e-6;

core::base::Error make_cmp_error(int32_t code, std::string msg, std::string details = {}) {
    return core::base::Error{
        .code = code,
        .domain = core::base::ErrorDomain::Validation,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = std::move(details)
    };
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

bool json_bool(std::string_view json, std::string_view key, bool fallback) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        return m[1].str() == "true";
    }
    return fallback;
}

} // namespace

core::base::Result<ComponentKind> component_kind_from_string(std::string_view s) {
    if (s == "Resistor") return ComponentKind::Resistor;
    if (s == "ConductorWire") return ComponentKind::ConductorWire;
    if (s == "Switch") return ComponentKind::Switch;
    if (s == "Capacitor") return ComponentKind::Capacitor;
    if (s == "Inductor") return ComponentKind::Inductor;
    if (s == "Battery") return ComponentKind::Battery;
    if (s == "Connector") return ComponentKind::Connector;
    if (s == "HeatingElement") return ComponentKind::HeatingElement;
    if (s == "Motor") return ComponentKind::Motor;
    return make_cmp_error(4401, "Unknown ComponentKind: " + std::string(s));
}

DerivedComponentSpecs ComponentGenerator::qualify_and_derive(
    const PhysicalObject& obj,
    ComponentKind kind,
    const ComponentTypeConfig& cfg,
    const std::vector<ComponentTerminal>& terminals,
    const core::domain::MaterialRegistry& reg) {
    DerivedComponentSpecs out;

    auto props_res = PhysicalObjectManager::evaluate_properties(obj, reg);
    if (props_res.is_err()) {
        out.qualification_errors.push_back(props_res.error().message);
        return out;
    }
    const auto& p = props_res.value();
    if (!p.is_solid_at_operating_temp) {
        out.qualification_errors.push_back(
            "Object is not structurally solid at operating temperature " +
            std::to_string(obj.operating_temp_k) + " K.");
    }

    if (terminals.size() < 2) {
        out.qualification_errors.push_back("Component must define at least 2 electrical terminals.");
    }

    const double len = (cfg.conductor_length_m > 0.0)
        ? cfg.conductor_length_m : std::max(1e-6, obj.geometry.compute_characteristic_length_m());
    const double area = (cfg.cross_section_area_m2 > 0.0)
        ? cfg.cross_section_area_m2 : std::max(1e-12, obj.geometry.compute_cross_section_area_m2());

    out.thermal_mass_j_k = p.thermal_mass_j_k;
    out.thermal_conductance_w_k = 10.0 * std::max(1e-6, p.surface_area_m2); // h=10 W/m^2K convective baseline

    // Inspect material regions
    bool has_insulator_region = false;
    double eps_r = cfg.relative_permittivity;
    double breakdown_v_m = 1.5e7; // conservative air/polymer dielectric default only if region is insulator
    double min_melt_k = 1e9;
    for (const auto& r : obj.regions) {
        const auto* mat = reg.get_material(r.material_id);
        if (!mat) continue;
        double sig = mat->electrical().electrical_conductivity.value_or(0.0);
        if (sig <= 0.0 && mat->electrical().electrical_resistivity.has_value() &&
            *mat->electrical().electrical_resistivity > 0.0) {
            sig = 1.0 / (*mat->electrical().electrical_resistivity);
        }
        if (sig < 1e-3) {
            has_insulator_region = true;
            if (mat->electrical().relative_permittivity.has_value()) {
                eps_r = *mat->electrical().relative_permittivity;
            }
            if (mat->electrical().dielectric_breakdown_strength.has_value()) {
                breakdown_v_m = *mat->electrical().dielectric_breakdown_strength;
            }
        }
        if (mat->thermal().melting_point_k.has_value()) {
            min_melt_k = std::min(min_melt_k, *mat->thermal().melting_point_k);
        }
    }

    const double max_temp_limit = std::min(cfg.max_operating_temp_k, min_melt_k * 0.85);
    const double delta_t_max = std::max(10.0, max_temp_limit - obj.operating_temp_k);
    out.max_power_w = out.thermal_conductance_w_k * delta_t_max;

    switch (kind) {
    case ComponentKind::ConductorWire:
    case ComponentKind::Connector: {
        if (!p.has_conductive_path || p.effective_electrical_conductivity_s_m < 1.0e4) {
            out.qualification_errors.push_back(
                "ConductorWire/Connector requires a continuous high-conductivity metallic path (sigma >= 1e4 S/m).");
        } else {
            const double r_wire = p.effective_electrical_resistivity_ohm_m * (len / area);
            const double r_tot = r_wire + (kind == ComponentKind::Connector ? cfg.contact_resistance_ohm : 0.0);
            out.primary_value = std::max(1e-6, r_tot);
            out.series_resistance_ohm = out.primary_value;
            out.max_current_a = std::sqrt(out.max_power_w / std::max(1e-6, out.series_resistance_ohm));
        }
        break;
    }
    case ComponentKind::Resistor:
    case ComponentKind::HeatingElement: {
        if (p.effective_electrical_conductivity_s_m < 1e-3) {
            out.qualification_errors.push_back(
                "Object electrical conductivity is too low (< 1e-3 S/m; insulator) to serve as a Resistor/HeatingElement.");
        } else {
            out.primary_value = std::max(1e-4, p.effective_electrical_resistivity_ohm_m * (len / area));
            out.series_resistance_ohm = out.primary_value;
            out.max_current_a = std::sqrt(out.max_power_w / std::max(1e-6, out.series_resistance_ohm));
            if (kind == ComponentKind::Resistor && out.primary_value < 0.05) {
                out.warnings.push_back("Very low resistance (< 0.05 Ohm) for a Resistor; consider ConductorWire or longer/thinner geometry.");
            }
        }
        break;
    }
    case ComponentKind::Switch: {
        if (!p.has_conductive_path || p.effective_electrical_conductivity_s_m < 1.0e3) {
            out.qualification_errors.push_back("Switch requires conductive contact material.");
        } else {
            out.primary_value = std::max(1e-4, cfg.contact_resistance_ohm);
            out.series_resistance_ohm = out.primary_value;
            out.max_current_a = std::sqrt(out.max_power_w / std::max(1e-6, out.series_resistance_ohm));
        }
        break;
    }
    case ComponentKind::Capacitor: {
        if (!has_insulator_region) {
            out.qualification_errors.push_back(
                "Capacitor requires an insulating dielectric region separating electrodes (purely conductive block would short-circuit).");
        } else if (cfg.dielectric_thickness_m <= 0.0) {
            out.qualification_errors.push_back("Capacitor dielectric thickness must be strictly positive.");
        } else {
            out.primary_value = kEps0 * std::max(1.0, eps_r) * area / cfg.dielectric_thickness_m;
            out.series_resistance_ohm = 0.05; // ESR estimate
            out.max_voltage_v = breakdown_v_m * cfg.dielectric_thickness_m;
            out.max_current_a = 10.0;
        }
        break;
    }
    case ComponentKind::Inductor: {
        if (!p.has_conductive_path || p.effective_electrical_conductivity_s_m < 1.0e4) {
            out.qualification_errors.push_back("Inductor requires conductive winding material.");
        } else if (cfg.coil_turns <= 0) {
            out.qualification_errors.push_back("Inductor coil_turns must be positive.");
        } else {
            const double n = static_cast<double>(cfg.coil_turns);
            out.primary_value = kMu0 * std::max(1.0, cfg.core_relative_permeability) * (n * n) * area / len;
            out.series_resistance_ohm = std::max(1e-4, p.effective_electrical_resistivity_ohm_m * (len * n / area));
            out.max_current_a = std::sqrt(out.max_power_w / std::max(1e-6, out.series_resistance_ohm));
        }
        break;
    }
    case ComponentKind::Battery: {
        if (cfg.nominal_voltage_v <= 0.0 || cfg.capacity_ah <= 0.0) {
            out.qualification_errors.push_back("Battery nominal voltage and capacity must be positive.");
        } else {
            out.primary_value = cfg.nominal_voltage_v;
            out.series_resistance_ohm = std::max(1e-4, cfg.internal_resistance_ohm);
            out.max_current_a = cfg.capacity_ah * 5.0; // 5C conservative limit
        }
        break;
    }
    case ComponentKind::Motor: {
        if (!p.has_conductive_path || p.effective_electrical_conductivity_s_m < 1.0e4) {
            out.qualification_errors.push_back("Motor winding requires conductive material.");
        } else {
            out.primary_value = std::max(0.1, p.effective_electrical_resistivity_ohm_m * (len * std::max(1, cfg.coil_turns) / area));
            out.series_resistance_ohm = out.primary_value;
            out.max_current_a = std::sqrt(out.max_power_w / std::max(1e-6, out.series_resistance_ohm));
            out.warnings.push_back("Motor modeled at L1/L2 simplified winding + inertia level.");
        }
        break;
    }
    }

    out.is_qualified = out.qualification_errors.empty();
    return out;
}

core::base::Result<GeneratedComponentDefinition> ComponentGenerator::generate(
    std::string name,
    const PhysicalObject& obj,
    ComponentKind kind,
    ComponentTypeConfig cfg,
    std::vector<ComponentTerminal> terminals,
    const core::domain::MaterialRegistry& reg) {
    if (name.empty()) {
        return make_cmp_error(4402, "Component name cannot be empty");
    }
    if (terminals.empty()) {
        terminals = {
            {"T1", core::domain::ElectricalPinPolarity::Positive, {-0.5 * obj.geometry.dim_x_m, 0, 0}, "whole"},
            {"T2", core::domain::ElectricalPinPolarity::Negative, { 0.5 * obj.geometry.dim_x_m, 0, 0}, "whole"}
        };
    }

    auto derived = qualify_and_derive(obj, kind, cfg, terminals, reg);
    if (!derived.is_qualified) {
        std::string reason = derived.qualification_errors.empty()
            ? "Object failed component qualification"
            : derived.qualification_errors.front();
        return make_cmp_error(4403, "Cannot generate " + std::string(component_kind_to_string(kind)) +
                                    " from object '" + obj.name + "'", std::move(reason));
    }

    GeneratedComponentDefinition comp;
    comp.name = std::move(name);
    comp.kind = kind;
    comp.source_object_id = obj.id;
    comp.terminals = std::move(terminals);
    comp.config = cfg;
    comp.derived = std::move(derived);
    return comp;
}

std::string GeneratedComponentDefinition::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id.to_string() << "\",\n";
    ss << "  \"name\": \"" << escape_json(name) << "\",\n";
    ss << "  \"kind\": \"" << component_kind_to_string(kind) << "\",\n";
    ss << "  \"source_object_id\": \"" << source_object_id.to_string() << "\",\n";
    if (battery_spec_id.has_value()) {
        ss << "  \"battery_spec_id\": \"" << battery_spec_id->to_string() << "\",\n";
    }
    ss << "  \"conductor_length_m\": " << config.conductor_length_m << ",\n";
    ss << "  \"cross_section_area_m2\": " << config.cross_section_area_m2 << ",\n";
    ss << "  \"dielectric_thickness_m\": " << config.dielectric_thickness_m << ",\n";
    ss << "  \"relative_permittivity\": " << config.relative_permittivity << ",\n";
    ss << "  \"coil_turns\": " << config.coil_turns << ",\n";
    ss << "  \"core_relative_permeability\": " << config.core_relative_permeability << ",\n";
    ss << "  \"contact_resistance_ohm\": " << config.contact_resistance_ohm << ",\n";
    ss << "  \"initially_closed\": " << (config.initially_closed ? "true" : "false") << ",\n";
    ss << "  \"nominal_voltage_v\": " << config.nominal_voltage_v << ",\n";
    ss << "  \"capacity_ah\": " << config.capacity_ah << ",\n";
    ss << "  \"internal_resistance_ohm\": " << config.internal_resistance_ohm << ",\n";
    ss << "  \"initial_soc\": " << config.initial_soc << ",\n";
    ss << "  \"primary_value\": " << std::setprecision(10) << derived.primary_value << ",\n";
    ss << "  \"series_resistance_ohm\": " << std::setprecision(10) << derived.series_resistance_ohm << ",\n";
    ss << "  \"terminals\": [\n";
    for (size_t i = 0; i < terminals.size(); ++i) {
        const auto& t = terminals[i];
        ss << "    {\"name\": \"" << escape_json(t.name)
           << "\", \"target_region_id\": \"" << escape_json(t.target_region_id) << "\"}";
        if (i + 1 < terminals.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<GeneratedComponentDefinition> GeneratedComponentDefinition::from_json(std::string_view src) {
    if (src.empty()) return make_cmp_error(4410, "Empty GeneratedComponentDefinition JSON");
    GeneratedComponentDefinition c;
    if (auto id_opt = core::base::Uuid::from_string(json_str(src, "id"))) c.id = *id_opt;
    std::string nm = json_str(src, "name");
    if (!nm.empty()) c.name = std::move(nm);
    if (auto k = component_kind_from_string(json_str(src, "kind")); k.is_ok()) c.kind = k.value();
    if (auto oid = core::base::Uuid::from_string(json_str(src, "source_object_id"))) c.source_object_id = *oid;
    std::string bid = json_str(src, "battery_spec_id");
    if (!bid.empty()) c.battery_spec_id = core::base::Uuid::from_string(bid);

    c.config.conductor_length_m = json_num(src, "conductor_length_m", 0.0);
    c.config.cross_section_area_m2 = json_num(src, "cross_section_area_m2", 0.0);
    c.config.dielectric_thickness_m = json_num(src, "dielectric_thickness_m", 1e-3);
    c.config.relative_permittivity = json_num(src, "relative_permittivity", 3.5);
    c.config.coil_turns = static_cast<int>(json_num(src, "coil_turns", 100.0));
    c.config.core_relative_permeability = json_num(src, "core_relative_permeability", 1.0);
    c.config.contact_resistance_ohm = json_num(src, "contact_resistance_ohm", 1e-3);
    c.config.initially_closed = json_bool(src, "initially_closed", true);
    c.config.nominal_voltage_v = json_num(src, "nominal_voltage_v", 3.7);
    c.config.capacity_ah = json_num(src, "capacity_ah", 2.5);
    c.config.internal_resistance_ohm = json_num(src, "internal_resistance_ohm", 0.05);
    c.config.initial_soc = json_num(src, "initial_soc", 1.0);
    c.derived.primary_value = json_num(src, "primary_value", 0.0);
    c.derived.series_resistance_ohm = json_num(src, "series_resistance_ohm", 0.0);
    c.derived.is_qualified = true;

    const size_t tpos = src.find("\"terminals\"");
    if (tpos != std::string_view::npos) {
        std::string_view sub = src.substr(tpos);
        std::regex item_re(R"(\{[^\{\}]*\})");
        auto begin = std::regex_iterator(sub.begin(), sub.end(), item_re);
        auto end = std::regex_iterator<std::string_view::const_iterator>();
        for (auto it = begin; it != end; ++it) {
            std::string item = it->str();
            ComponentTerminal t;
            t.name = json_str(item, "name");
            t.target_region_id = json_str(item, "target_region_id");
            if (!t.name.empty()) c.terminals.push_back(std::move(t));
        }
    }
    return c;
}

core::base::Result<core::base::Uuid> ComponentLibraryManager::register_component(GeneratedComponentDefinition comp) {
    if (!comp.derived.is_qualified) {
        return make_cmp_error(4420, "Cannot register unqualified component");
    }
    const core::base::Uuid id = comp.id;
    components_.push_back(std::move(comp));
    return id;
}

bool ComponentLibraryManager::remove_component(const core::base::Uuid& id) {
    auto it = std::remove_if(components_.begin(), components_.end(), [&](const auto& c) {
        return c.id == id;
    });
    if (it == components_.end()) return false;
    components_.erase(it, components_.end());
    return true;
}

const GeneratedComponentDefinition* ComponentLibraryManager::get_component(const core::base::Uuid& id) const noexcept {
    for (const auto& c : components_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

GeneratedComponentDefinition* ComponentLibraryManager::get_component_mut(const core::base::Uuid& id) noexcept {
    for (auto& c : components_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

const GeneratedComponentDefinition* ComponentLibraryManager::find_by_name(std::string_view name) const noexcept {
    for (const auto& c : components_) {
        if (c.name == name) return &c;
    }
    return nullptr;
}

void ComponentLibraryManager::recalculate_for_object(
    const PhysicalObject& obj,
    const core::domain::MaterialRegistry& reg) {
    for (auto& c : components_) {
        if (c.source_object_id == obj.id && c.kind != ComponentKind::Battery) {
            c.derived = ComponentGenerator::qualify_and_derive(obj, c.kind, c.config, c.terminals, reg);
        }
    }
}

void ComponentLibraryManager::recalculate_all(
    const PhysicalObjectManager& objs,
    const core::domain::MaterialRegistry& reg) {
    for (auto& c : components_) {
        if (c.kind == ComponentKind::Battery) continue;
        if (const auto* obj = objs.get_object(c.source_object_id)) {
            c.derived = ComponentGenerator::qualify_and_derive(*obj, c.kind, c.config, c.terminals, reg);
        }
    }
}

} // namespace shunya::eng
