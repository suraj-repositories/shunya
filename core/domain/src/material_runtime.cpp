#include <shunya/core/domain/material_runtime.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_rt_error(std::string msg, int32_t code = 4301) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Materials,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

base::Result<MaterialClassification> material_classification_from_string(std::string_view s) {
    if (s == "PureElement")      return MaterialClassification::PureElement;
    if (s == "ChemicalCompound") return MaterialClassification::ChemicalCompound;
    if (s == "Alloy")            return MaterialClassification::Alloy;
    if (s == "Mixture")          return MaterialClassification::Mixture;
    if (s == "Composite")        return MaterialClassification::Composite;
    return make_rt_error(std::string("Unknown MaterialClassification: ") + std::string(s));
}

base::Result<FractionBasis> fraction_basis_from_string(std::string_view s) {
    if (s == "MassFraction")   return FractionBasis::MassFraction;
    if (s == "MoleFraction")   return FractionBasis::MoleFraction;
    if (s == "VolumeFraction") return FractionBasis::VolumeFraction;
    return make_rt_error(std::string("Unknown FractionBasis: ") + std::string(s));
}

base::Result<MaterialPhase> material_phase_from_string(std::string_view s) {
    if (s == "Solid")         return MaterialPhase::Solid;
    if (s == "Liquid")        return MaterialPhase::Liquid;
    if (s == "Gas")           return MaterialPhase::Gas;
    if (s == "Supercritical") return MaterialPhase::Supercritical;
    if (s == "Plasma")        return MaterialPhase::Plasma;
    return make_rt_error(std::string("Unknown MaterialPhase: ") + std::string(s));
}

bool ChemicalElementDefinition::has_isotope(int mass_number) const noexcept {
    for (const auto& iso : isotopes) {
        if (iso.mass_number == mass_number) return true;
    }
    return false;
}

const IsotopeDefinition* ChemicalElementDefinition::get_isotope(int mass_number) const noexcept {
    for (const auto& iso : isotopes) {
        if (iso.mass_number == mass_number) return &iso;
    }
    return nullptr;
}

MaterialDefinition::MaterialDefinition()
    : id_(base::Uuid::generate_random()),
      name_("Unnamed Material"),
      classification_(MaterialClassification::PureElement) {}

MaterialDefinition::MaterialDefinition(base::Uuid id, std::string name, MaterialClassification classification)
    : id_(id),
      name_(std::move(name)),
      classification_(classification) {}

void MaterialDefinition::add_constituent(MaterialConstituent constituent) {
    constituents_.push_back(std::move(constituent));
}

base::ValidationReport MaterialDefinition::validate_composition() const {
    base::ValidationReport report;
    std::string path = "Material[" + name_ + "]";

    if (constituents_.empty()) {
        report.add_warning(path, "Material definition has no specified constituents");
        return report;
    }

    double sum = 0.0;
    for (const auto& c : constituents_) {
        if (c.identifier.empty()) {
            report.add_error(path, "Constituent has empty identifier");
        }
        if (c.fraction <= 0.0 || c.fraction > 1.0) {
            report.add_error(path, "Constituent fraction out of range (0, 1]: " + std::to_string(c.fraction));
        }
        sum += c.fraction;
    }

    if (std::abs(sum - 1.0) > 1e-3) {
        report.add_error(path, "Sum of constituent fractions must equal 1.0 (got " + std::to_string(sum) + ")");
    }

    if (classification_ == MaterialClassification::PureElement && constituents_.size() > 1) {
        report.add_error(path, "PureElement material cannot contain more than one constituent");
    }

    return report;
}

void MaterialDefinition::set_scalar_property(std::string name, double value) {
    scalar_properties_[std::move(name)] = value;
}

std::optional<double> MaterialDefinition::get_scalar_property(std::string_view name) const {
    auto it = scalar_properties_.find(std::string(name));
    if (it != scalar_properties_.end()) return it->second;
    return std::nullopt;
}

void MaterialDefinition::add_curve(MaterialCurve curve) {
    std::string key = curve.name();
    curves_[key] = std::move(curve);
}

const MaterialCurve* MaterialDefinition::get_curve(std::string_view name) const {
    auto it = curves_.find(std::string(name));
    if (it != curves_.end()) return &it->second;
    return nullptr;
}

std::string MaterialDefinition::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id_.to_string() << "\",\n";
    ss << "  \"name\": \"" << name_ << "\",\n";
    ss << "  \"classification\": \"" << material_classification_to_string(classification_) << "\",\n";
    ss << "  \"chemical_formula\": \"" << chemical_formula_ << "\",\n";
    ss << "  \"molecular_weight\": " << molecular_weight_ << ",\n";
    ss << "  \"reference_density\": " << reference_density_ << ",\n";
    ss << "  \"constituents\": [\n";
    for (size_t i = 0; i < constituents_.size(); ++i) {
        const auto& c = constituents_[i];
        ss << "    {\"id\": \"" << c.identifier << "\", \"basis\": \""
           << fraction_basis_to_string(c.basis) << "\", \"fraction\": " << c.fraction << "}";
        if (i + 1 < constituents_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"scalar_properties\": {\n";
    size_t s_idx = 0;
    for (const auto& pair : scalar_properties_) {
        ss << "    \"" << pair.first << "\": " << pair.second;
        if (++s_idx < scalar_properties_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

base::Result<MaterialDefinition> MaterialDefinition::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::string {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            return m[1].str();
        }
        return "";
    };

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            try {
                return std::stod(m[1].str());
            } catch (...) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    };

    std::string id_str = extract_str("id");
    auto id_opt = base::Uuid::from_string(id_str);
    base::Uuid id = id_opt.has_value() ? *id_opt : base::Uuid::generate_random();

    std::string name = extract_str("name");
    std::string cls_str = extract_str("classification");
    MaterialClassification cls = MaterialClassification::PureElement;
    if (!cls_str.empty()) {
        auto res = material_classification_from_string(cls_str);
        if (res.is_ok()) cls = res.value();
    }

    MaterialDefinition def(id, std::move(name), cls);
    def.set_chemical_formula(extract_str("chemical_formula"));
    def.set_molecular_weight(extract_num("molecular_weight").value_or(0.0));
    def.set_reference_density(extract_num("reference_density").value_or(1000.0));

    // Parse constituents
    std::regex const_re(R"re(\{\s*"id"\s*:\s*"([^"]+)"\s*,\s*"basis"\s*:\s*"([^"]+)"\s*,\s*"fraction"\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)\s*\})re");
    auto it = std::sregex_iterator(s.begin(), s.end(), const_re);
    auto end = std::sregex_iterator();

    for (; it != end; ++it) {
        MaterialConstituent c;
        c.identifier = (*it)[1].str();
        auto b_res = fraction_basis_from_string((*it)[2].str());
        if (b_res.is_ok()) c.basis = b_res.value();
        c.fraction = std::stod((*it)[3].str());
        def.add_constituent(std::move(c));
    }

    return def;
}

double MaterialState::get_phase_fraction(std::string_view phase_name) const noexcept {
    auto it = phase_fractions.find(std::string(phase_name));
    if (it != phase_fractions.end()) return it->second;
    return 0.0;
}

bool MaterialState::operator==(const MaterialState& o) const noexcept {
    return temperature_k == o.temperature_k &&
           pressure_pa == o.pressure_pa &&
           primary_phase == o.primary_phase &&
           phase_fractions == o.phase_fractions &&
           accumulated_plastic_strain == o.accumulated_plastic_strain &&
           internal_energy_j_kg == o.internal_energy_j_kg &&
           history_variables == o.history_variables;
}

std::string MaterialState::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"temperature_k\": " << temperature_k << ",\n";
    ss << "  \"pressure_pa\": " << pressure_pa << ",\n";
    ss << "  \"primary_phase\": \"" << material_phase_to_string(primary_phase) << "\",\n";
    ss << "  \"accumulated_plastic_strain\": " << accumulated_plastic_strain << ",\n";
    ss << "  \"internal_energy_j_kg\": " << internal_energy_j_kg << ",\n";
    ss << "  \"state_version\": " << state_version << ",\n";
    ss << "  \"phase_fractions\": {\n";
    size_t pf_i = 0;
    for (const auto& pair : phase_fractions) {
        ss << "    \"" << pair.first << "\": " << pair.second;
        if (++pf_i < phase_fractions.size()) ss << ",";
        ss << "\n";
    }
    ss << "  },\n";
    ss << "  \"history_variables\": {\n";
    size_t hv_i = 0;
    for (const auto& pair : history_variables) {
        ss << "    \"" << pair.first << "\": " << pair.second;
        if (++hv_i < history_variables.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";
    ss << "}";
    return ss.str();
}

base::Result<MaterialState> MaterialState::from_json(std::string_view json_str) {
    std::string s(json_str);
    MaterialState st;

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            try { return std::stod(m[1].str()); } catch (...) { return std::nullopt; }
        }
        return std::nullopt;
    };

    st.temperature_k = extract_num("temperature_k").value_or(293.15);
    st.pressure_pa = extract_num("pressure_pa").value_or(101325.0);
    st.accumulated_plastic_strain = extract_num("accumulated_plastic_strain").value_or(0.0);
    st.internal_energy_j_kg = extract_num("internal_energy_j_kg").value_or(0.0);

    // Primary phase
    std::regex ph_re(R"re("primary_phase"\s*:\s*"([^"]*)")re");
    std::smatch ph_m;
    if (std::regex_search(s, ph_m, ph_re)) {
        auto p_res = material_phase_from_string(ph_m[1].str());
        if (p_res.is_ok()) st.primary_phase = p_res.value();
    }

    // Parse phase fractions block
    size_t pf_pos = s.find("\"phase_fractions\"");
    if (pf_pos != std::string::npos) {
        size_t blk_start = s.find('{', pf_pos);
        size_t blk_end = s.find('}', blk_start);
        if (blk_start != std::string::npos && blk_end != std::string::npos) {
            std::string sub = s.substr(blk_start, blk_end - blk_start + 1);
            std::regex pair_re(R"re("([^"]+)"\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
            auto it = std::sregex_iterator(sub.begin(), sub.end(), pair_re);
            auto end = std::sregex_iterator();
            for (; it != end; ++it) {
                st.phase_fractions[(*it)[1].str()] = std::stod((*it)[2].str());
            }
        }
    }

    // Parse history variables block
    size_t hv_pos = s.find("\"history_variables\"");
    if (hv_pos != std::string::npos) {
        size_t blk_start = s.find('{', hv_pos);
        size_t blk_end = s.find('}', blk_start);
        if (blk_start != std::string::npos && blk_end != std::string::npos) {
            std::string sub = s.substr(blk_start, blk_end - blk_start + 1);
            std::regex pair_re(R"re("([^"]+)"\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
            auto it = std::sregex_iterator(sub.begin(), sub.end(), pair_re);
            auto end = std::sregex_iterator();
            for (; it != end; ++it) {
                st.history_variables[(*it)[1].str()] = std::stod((*it)[2].str());
            }
        }
    }

    return st;
}

MaterialRegion::MaterialRegion()
    : id_(base::Uuid::generate_random()),
      name_("Unnamed Region") {}

MaterialRegion::MaterialRegion(base::Uuid id, std::string name, base::Uuid material_id, std::string geometry_ref)
    : id_(id),
      name_(std::move(name)),
      material_id_(material_id),
      geometry_ref_(std::move(geometry_ref)) {}

std::string MaterialRegion::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id_.to_string() << "\",\n";
    ss << "  \"name\": \"" << name_ << "\",\n";
    ss << "  \"material_id\": \"" << material_id_.to_string() << "\",\n";
    ss << "  \"geometry_ref\": \"" << geometry_ref_ << "\",\n";
    ss << "  \"volume_m3\": " << volume_m3_ << ",\n";
    ss << "  \"state\": " << state_.to_json() << "\n";
    ss << "}";
    return ss.str();
}

base::Result<MaterialRegion> MaterialRegion::from_json(std::string_view json_str) {
    std::string s(json_str);

    auto extract_str = [&](const std::string& key) -> std::string {
        std::regex re("\"" + key + R"re("\s*:\s*"([^"]*)")re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            return m[1].str();
        }
        return "";
    };

    auto extract_num = [&](const std::string& key) -> std::optional<double> {
        std::regex re("\"" + key + R"re("\s*:\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?))re");
        std::smatch m;
        if (std::regex_search(s, m, re)) {
            try { return std::stod(m[1].str()); } catch (...) { return std::nullopt; }
        }
        return std::nullopt;
    };

    auto id_opt = base::Uuid::from_string(extract_str("id"));
    base::Uuid id = id_opt.has_value() ? *id_opt : base::Uuid::generate_random();

    auto mid_opt = base::Uuid::from_string(extract_str("material_id"));
    base::Uuid mid = mid_opt.has_value() ? *mid_opt : base::Uuid::generate_random();

    std::string name = extract_str("name");
    std::string geom = extract_str("geometry_ref");
    double vol = extract_num("volume_m3").value_or(1.0);

    MaterialRegion reg(id, std::move(name), mid, std::move(geom));
    reg.set_volume_m3(vol);

    // Parse state sub-object
    size_t st_pos = s.find("\"state\"");
    if (st_pos != std::string::npos) {
        size_t obj_start = s.find('{', st_pos);
        if (obj_start != std::string::npos) {
            size_t depth = 0;
            size_t end_pos = std::string::npos;
            for (size_t i = obj_start; i < s.size(); ++i) {
                if (s[i] == '{') depth++;
                else if (s[i] == '}') {
                    depth--;
                    if (depth == 0) {
                        end_pos = i;
                        break;
                    }
                }
            }
            if (end_pos != std::string::npos) {
                auto st_res = MaterialState::from_json(s.substr(obj_start, end_pos - obj_start + 1));
                if (st_res.is_ok()) {
                    reg.set_state(st_res.value());
                }
            }
        }
    }

    return reg;
}

double PhaseTransitionModel::compute_liquid_fraction(double temperature_k) const noexcept {
    if (temperature_k <= solidus_temperature_k) return 0.0;
    if (temperature_k >= liquidus_temperature_k) return 1.0;
    double range = liquidus_temperature_k - solidus_temperature_k;
    if (range < 1e-6) return 1.0;
    return (temperature_k - solidus_temperature_k) / range;
}

double PhaseTransitionModel::compute_apparent_heat_capacity(double temperature_k, double base_cp) const noexcept {
    if (temperature_k < solidus_temperature_k || temperature_k > liquidus_temperature_k) {
        return base_cp;
    }
    double range = liquidus_temperature_k - solidus_temperature_k;
    if (range < 1e-6) return base_cp;
    return base_cp + (latent_heat_fusion_j_kg / range);
}

std::optional<double> PropertyCache::get(const base::Uuid& mat_id, std::string_view prop, const MaterialState& state) const {
    CacheKey key{
        mat_id,
        std::string(prop),
        static_cast<int64_t>(std::round(state.temperature_k * 100.0)),
        static_cast<int64_t>(std::round(state.pressure_pa / 10.0)),
        state.state_version
    };

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        ++hits_;
        return it->second;
    }
    ++misses_;
    return std::nullopt;
}

void PropertyCache::put(const base::Uuid& mat_id, std::string_view prop, const MaterialState& state, double val) {
    CacheKey key{
        mat_id,
        std::string(prop),
        static_cast<int64_t>(std::round(state.temperature_k * 100.0)),
        static_cast<int64_t>(std::round(state.pressure_pa / 10.0)),
        state.state_version
    };
    cache_[key] = val;
}

void PropertyCache::invalidate_material(const base::Uuid& mat_id) {
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->first.material_id == mat_id) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

base::ValidationReport RegionDiagnosticsEngine::diagnose_region(
    const MaterialRegion& region,
    const MaterialDefinition& def,
    const std::vector<std::string>& active_solver_domains) {
    base::ValidationReport report;
    std::string path = "Region[" + region.name() + "]";

    if (region.volume_m3() <= 0.0) {
        report.add_error(path, "Material region volume must be strictly positive (> 0.0 m^3)");
    }

    const auto& st = region.state();
    if (st.temperature_k <= 0.0) {
        report.add_error(path, "Region temperature below absolute zero: " + std::to_string(st.temperature_k) + " K");
    }

    if (st.pressure_pa < 0.0) {
        report.add_warning(path, "Negative absolute pressure detected: " + std::to_string(st.pressure_pa) + " Pa");
    }

    // Inspect requirements for active solver domains
    for (const auto& domain : active_solver_domains) {
        if (domain == "Thermal") {
            if (!def.get_scalar_property("thermal_conductivity").has_value() &&
                !def.get_curve("thermal_conductivity")) {
                report.add_error(path, "Missing thermal conductivity required by Thermal solver");
            }
            if (!def.get_scalar_property("specific_heat_capacity").has_value() &&
                !def.get_curve("specific_heat_capacity")) {
                report.add_error(path, "Missing specific heat capacity required by Thermal solver");
            }
        } else if (domain == "Mechanical") {
            if (!def.get_scalar_property("youngs_modulus").has_value() &&
                !def.get_curve("stress_strain")) {
                report.add_error(path, "Missing Young's modulus required by Mechanical solver");
            }
            if (!def.get_scalar_property("poissons_ratio").has_value()) {
                report.add_error(path, "Missing Poisson's ratio required by Mechanical solver");
            }
        } else if (domain == "Electrical") {
            if (!def.get_scalar_property("electrical_conductivity").has_value() &&
                !def.get_scalar_property("electrical_resistivity").has_value() &&
                !def.get_curve("electrical_conductivity")) {
                report.add_error(path, "Missing electrical conductivity/resistivity required by Electrical solver");
            }
        } else if (domain == "Fluid") {
            if (!def.get_scalar_property("dynamic_viscosity").has_value() &&
                !def.get_curve("dynamic_viscosity")) {
                report.add_error(path, "Missing dynamic viscosity required by Fluid solver");
            }
        }
    }

    return report;
}

} // namespace shunya::core::domain
