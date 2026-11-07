#include <shunya/core/domain/material_dictionary.hpp>

#include <algorithm>
#include <cmath>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_dict_error(std::string msg, int32_t code = 4001) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Materials,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

base::Result<FieldDataType> field_data_type_from_string(std::string_view s) {
    if (s == "Numeric")   return FieldDataType::Numeric;
    if (s == "Text")      return FieldDataType::Text;
    if (s == "Boolean")   return FieldDataType::Boolean;
    if (s == "Quantity")  return FieldDataType::Quantity;
    if (s == "Vector")    return FieldDataType::Vector;
    if (s == "Tensor")    return FieldDataType::Tensor;
    if (s == "Curve")     return FieldDataType::Curve;
    if (s == "Composite") return FieldDataType::Composite;
    return make_dict_error(std::string("Unknown FieldDataType: ") + std::string(s));
}

base::Result<FieldApplicability> field_applicability_from_string(std::string_view s) {
    if (s == "Universal")     return FieldApplicability::Universal;
    if (s == "SolidOnly")     return FieldApplicability::SolidOnly;
    if (s == "FluidOnly")     return FieldApplicability::FluidOnly;
    if (s == "GasOnly")       return FieldApplicability::GasOnly;
    if (s == "PlasmaOnly")    return FieldApplicability::PlasmaOnly;
    if (s == "Semiconductor") return FieldApplicability::Semiconductor;
    if (s == "Conductor")     return FieldApplicability::Conductor;
    if (s == "Nuclear")       return FieldApplicability::Nuclear;
    return make_dict_error(std::string("Unknown FieldApplicability: ") + std::string(s));
}

base::Result<LicenceType> licence_type_from_string(std::string_view s) {
    if (s == "PublicDomain")          return LicenceType::PublicDomain;
    if (s == "CC0")                   return LicenceType::CC0;
    if (s == "OpenDataCommons_ODbL")  return LicenceType::OpenDataCommons_ODbL;
    if (s == "CC_BY_4_0")             return LicenceType::CC_BY_4_0;
    if (s == "ProprietaryCommercial") return LicenceType::ProprietaryCommercial;
    if (s == "AcademicOnly")          return LicenceType::AcademicOnly;
    if (s == "Unknown")               return LicenceType::Unknown;
    return make_dict_error(std::string("Unknown LicenceType: ") + std::string(s));
}

bool FieldDefinition::matches(std::string_view name_or_alias) const noexcept {
    if (canonical_name == name_or_alias) return true;
    for (const auto& a : aliases) {
        if (a == name_or_alias) return true;
    }
    return false;
}

DataDictionary::DataDictionary() {
    seed_default_dictionary();
}

DataDictionary::DataDictionary(base::Version version)
    : version_(version) {
    seed_default_dictionary();
}

base::Result<void> DataDictionary::register_field(FieldDefinition field) {
    if (field.canonical_name.empty()) {
        return make_dict_error("Cannot register field definition with empty canonical name");
    }

    if (fields_.find(field.canonical_name) != fields_.end()) {
        return make_dict_error("Field definition already registered: " + field.canonical_name);
    }

    std::string c_name = field.canonical_name;
    for (const auto& alias : field.aliases) {
        if (alias_to_canonical_.find(alias) != alias_to_canonical_.end()) {
            return make_dict_error("Alias '" + alias + "' already assigned to field: " + alias_to_canonical_[alias]);
        }
        alias_to_canonical_[alias] = c_name;
    }

    fields_[c_name] = std::move(field);
    return {};
}

bool DataDictionary::has_field(std::string_view name_or_alias) const noexcept {
    std::string key(name_or_alias);
    if (fields_.find(key) != fields_.end()) return true;
    return alias_to_canonical_.find(key) != alias_to_canonical_.end();
}

const FieldDefinition* DataDictionary::get_field(std::string_view name_or_alias) const noexcept {
    std::string key(name_or_alias);
    auto it = fields_.find(key);
    if (it != fields_.end()) return &it->second;

    auto a_it = alias_to_canonical_.find(key);
    if (a_it != alias_to_canonical_.end()) {
        auto c_it = fields_.find(a_it->second);
        if (c_it != fields_.end()) return &c_it->second;
    }
    return nullptr;
}

void DataDictionary::seed_default_dictionary() {
    // 1. Atomic / Nuclear
    register_field({
        .canonical_name = "atomic_number",
        .aliases = {"Z", "atomic_no"},
        .data_type = FieldDataType::Numeric,
        .canonical_unit = "dimensionless",
        .dimension = base::units::Dimensionless,
        .applicability = FieldApplicability::Universal,
        .description = "Number of protons in atomic nucleus",
        .is_required = true,
        .default_value = "1",
        .min_value = 1.0,
        .max_value = 118.0
    });

    register_field({
        .canonical_name = "atomic_mass",
        .aliases = {"A", "atomic_weight", "m_a"},
        .data_type = FieldDataType::Numeric,
        .canonical_unit = "u",
        .dimension = base::units::Mass,
        .applicability = FieldApplicability::Universal,
        .description = "Standard relative atomic mass or isotopic mass in unified atomic mass units",
        .is_required = true,
        .default_value = "1.008",
        .min_value = 1.0,
        .max_value = 300.0
    });

    register_field({
        .canonical_name = "electronegativity",
        .aliases = {"chi", "pauling_electronegativity"},
        .data_type = FieldDataType::Numeric,
        .canonical_unit = "dimensionless",
        .dimension = base::units::Dimensionless,
        .applicability = FieldApplicability::Universal,
        .description = "Pauling scale dimensionless electronegativity",
        .is_required = false,
        .default_value = "0.0",
        .min_value = 0.0,
        .max_value = 4.5
    });

    // 2. Physical & Mechanical
    register_field({
        .canonical_name = "density",
        .aliases = {"rho", "mass_density"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "kg/m^3",
        .dimension = base::units::Mass / base::units::Volume,
        .applicability = FieldApplicability::Universal,
        .description = "Mass per unit volume",
        .is_required = true,
        .default_value = "1000.0",
        .min_value = 1e-4,
        .max_value = 1e5
    });

    register_field({
        .canonical_name = "youngs_modulus",
        .aliases = {"E", "elastic_modulus"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "Pa",
        .dimension = base::units::Pressure,
        .applicability = FieldApplicability::SolidOnly,
        .description = "Tensile elastic modulus",
        .is_required = false,
        .default_value = "200e9",
        .min_value = 1.0,
        .max_value = 2e12
    });

    register_field({
        .canonical_name = "poissons_ratio",
        .aliases = {"nu"},
        .data_type = FieldDataType::Numeric,
        .canonical_unit = "dimensionless",
        .dimension = base::units::Dimensionless,
        .applicability = FieldApplicability::SolidOnly,
        .description = "Ratio of transverse contraction strain to longitudinal extension strain",
        .is_required = false,
        .default_value = "0.3",
        .min_value = -0.99,
        .max_value = 0.499
    });

    register_field({
        .canonical_name = "yield_strength",
        .aliases = {"sigma_y", "yield_stress"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "Pa",
        .dimension = base::units::Pressure,
        .applicability = FieldApplicability::SolidOnly,
        .description = "Stress at which permanent plastic deformation begins",
        .is_required = false,
        .default_value = "250e6",
        .min_value = 1.0,
        .max_value = 1e11
    });

    // 3. Thermal
    register_field({
        .canonical_name = "thermal_conductivity",
        .aliases = {"k", "kappa"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "W/(m*K)",
        .dimension = base::units::Power / (base::units::Length * base::units::Temperature),
        .applicability = FieldApplicability::Universal,
        .description = "Rate of heat transfer through unit cross-sectional area per unit temperature gradient",
        .is_required = false,
        .default_value = "1.0",
        .min_value = 1e-4,
        .max_value = 5e3
    });

    register_field({
        .canonical_name = "specific_heat_capacity",
        .aliases = {"c_p", "cp", "heat_capacity"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "J/(kg*K)",
        .dimension = base::units::Energy / (base::units::Mass * base::units::Temperature),
        .applicability = FieldApplicability::Universal,
        .description = "Heat energy required to raise temperature of unit mass by one kelvin",
        .is_required = false,
        .default_value = "1000.0",
        .min_value = 1.0,
        .max_value = 2e5
    });

    register_field({
        .canonical_name = "melting_point",
        .aliases = {"T_m", "melting_temp"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "K",
        .dimension = base::units::Temperature,
        .applicability = FieldApplicability::Universal,
        .description = "Temperature at which solid transitions to liquid phase at 1 atm",
        .is_required = false,
        .default_value = "273.15",
        .min_value = 0.0,
        .max_value = 1e4
    });

    register_field({
        .canonical_name = "boiling_point",
        .aliases = {"T_b", "boiling_temp"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "K",
        .dimension = base::units::Temperature,
        .applicability = FieldApplicability::Universal,
        .description = "Temperature at which liquid transitions to gas phase at 1 atm",
        .is_required = false,
        .default_value = "373.15",
        .min_value = 0.0,
        .max_value = 1e4
    });

    // 4. Electrical
    register_field({
        .canonical_name = "electrical_conductivity",
        .aliases = {"sigma", "elec_cond"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "S/m",
        .dimension = base::units::ElectricalConductance / base::units::Length,
        .applicability = FieldApplicability::Universal,
        .description = "Electric current density per unit electric field",
        .is_required = false,
        .default_value = "1.0",
        .min_value = 0.0,
        .max_value = 1e9
    });

    register_field({
        .canonical_name = "electrical_resistivity",
        .aliases = {"rho_e", "resistivity"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "ohm*m",
        .dimension = base::units::ElectricalResistance * base::units::Length,
        .applicability = FieldApplicability::Universal,
        .description = "Resistance of unit length and unit cross-sectional area conductor",
        .is_required = false,
        .default_value = "1e-6",
        .min_value = 0.0,
        .max_value = 1e20
    });

    // 5. Fluid / Viscosity
    register_field({
        .canonical_name = "dynamic_viscosity",
        .aliases = {"mu", "viscosity"},
        .data_type = FieldDataType::Quantity,
        .canonical_unit = "Pa*s",
        .dimension = base::units::Pressure * base::units::Time,
        .applicability = FieldApplicability::FluidOnly,
        .description = "Fluid internal resistance to shear flow",
        .is_required = false,
        .default_value = "1e-3",
        .min_value = 1e-7,
        .max_value = 1e6
    });
}

base::ValidationReport DataDictionary::validate_audited_value(const AuditedValue& val) const {
    base::ValidationReport report;
    const FieldDefinition* def = get_field(val.field_name);
    if (!def) {
        report.add_warning(val.field_name, "Audited field not in standard data dictionary schema");
        return report;
    }

    // Unit verification
    if (!val.canonical_unit.empty() && !def->canonical_unit.empty()) {
        if (val.canonical_unit != def->canonical_unit) {
            report.add_error(val.field_name, "Unit mismatch: expected " + def->canonical_unit +
                                              " but normalized to " + val.canonical_unit);
        }
    }

    // Range checks
    if (def->min_value.has_value() && val.normalized_value < *def->min_value) {
        report.add_error(val.field_name, "Normalized value (" + std::to_string(val.normalized_value) +
                                          ") below physical minimum (" + std::to_string(*def->min_value) + ")");
    }
    if (def->max_value.has_value() && val.normalized_value > *def->max_value) {
        report.add_error(val.field_name, "Normalized value (" + std::to_string(val.normalized_value) +
                                          ") exceeds physical maximum (" + std::to_string(*def->max_value) + ")");
    }

    return report;
}

base::Result<AuditedValue> DataDictionary::audit_and_normalize(std::string_view field_name,
                                                               double raw_num,
                                                               std::string_view raw_unit,
                                                               FieldProvenance prov) const {
    const FieldDefinition* def = get_field(field_name);
    if (!def) {
        return make_dict_error("Unknown field for normalization: " + std::string(field_name));
    }

    AuditedValue av;
    av.field_name = def->canonical_name;
    av.raw_value = std::to_string(raw_num);
    av.raw_unit = std::string(raw_unit);
    av.provenance = std::move(prov);

    // Default 1:1 if unit is already canonical or dimensionless
    double norm_val = raw_num;
    double factor = 1.0;
    std::string canon_unit = def->canonical_unit;

    std::string r_u(raw_unit);

    // Common scientific unit conversions
    if (r_u == "g/cm^3") {
        factor = 1000.0;
        norm_val = raw_num * factor;
        canon_unit = "kg/m^3";
    } else if (r_u == "g/mL" || r_u == "g/cm3") {
        factor = 1000.0;
        norm_val = raw_num * factor;
        canon_unit = "kg/m^3";
    } else if (r_u == "degC" || r_u == "°C" || r_u == "C") {
        // Temperature affine conversion T_K = T_C + 273.15
        norm_val = raw_num + 273.15;
        factor = 1.0;
        canon_unit = "K";
    } else if (r_u == "MPa") {
        factor = 1e6;
        norm_val = raw_num * factor;
        canon_unit = "Pa";
    } else if (r_u == "GPa") {
        factor = 1e9;
        norm_val = raw_num * factor;
        canon_unit = "Pa";
    } else if (r_u == "bar") {
        factor = 1e5;
        norm_val = raw_num * factor;
        canon_unit = "Pa";
    } else if (r_u == "mbar") {
        factor = 100.0;
        norm_val = raw_num * factor;
        canon_unit = "Pa";
    } else if (r_u == "cal/(g*K)") {
        factor = 4184.0;
        norm_val = raw_num * factor;
        canon_unit = "J/(kg*K)";
    } else if (r_u == "MS/m") {
        factor = 1e6;
        norm_val = raw_num * factor;
        canon_unit = "S/m";
    } else if (r_u == "uOhm*cm") {
        factor = 1e-8;
        norm_val = raw_num * factor;
        canon_unit = "ohm*m";
    } else if (r_u == "cP" || r_u == "centipoise") {
        factor = 1e-3;
        norm_val = raw_num * factor;
        canon_unit = "Pa*s";
    }

    av.normalized_value = norm_val;
    av.canonical_unit = canon_unit;
    av.conversion_factor = factor;

    return av;
}

std::string DataDictionary::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": \"" << version_.as_string() << "\",\n";
    ss << "  \"fields\": [\n";

    size_t i = 0;
    for (const auto& pair : fields_) {
        const auto& f = pair.second;
        ss << "    {\n";
        ss << "      \"canonical_name\": \"" << f.canonical_name << "\",\n";
        ss << "      \"data_type\": \"" << field_data_type_to_string(f.data_type) << "\",\n";
        ss << "      \"canonical_unit\": \"" << f.canonical_unit << "\",\n";
        ss << "      \"applicability\": \"" << field_applicability_to_string(f.applicability) << "\",\n";
        ss << "      \"description\": \"" << f.description << "\",\n";
        ss << "      \"is_required\": " << (f.is_required ? "true" : "false") << ",\n";
        ss << "      \"aliases\": [";
        for (size_t a_idx = 0; a_idx < f.aliases.size(); ++a_idx) {
            ss << "\"" << f.aliases[a_idx] << "\"";
            if (a_idx + 1 < f.aliases.size()) ss << ", ";
        }
        ss << "]\n";
        ss << "    }";
        if (++i < fields_.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<DataDictionary> DataDictionary::from_json(std::string_view json_str) {
    std::string s(json_str);
    std::regex ver_re(R"re("version"\s*:\s*"([^"]+)")re");
    std::smatch m;
    base::Version ver{1, 0, 0};
    if (std::regex_search(s, m, ver_re)) {
        auto v_res = base::Version::parse(m[1].str());
        if (v_res.is_ok()) ver = v_res.value();
    }

    DataDictionary dict(ver);

    std::regex field_re(R"re(\{\s*"canonical_name"\s*:\s*"([^"]+)"\s*,\s*"data_type"\s*:\s*"([^"]+)"\s*,\s*"canonical_unit"\s*:\s*"([^"]*)"\s*,\s*"applicability"\s*:\s*"([^"]+)"\s*,\s*"description"\s*:\s*"([^"]*)")re");
    auto it = std::sregex_iterator(s.begin(), s.end(), field_re);
    auto end = std::sregex_iterator();

    for (; it != end; ++it) {
        FieldDefinition fd;
        fd.canonical_name = (*it)[1].str();
        auto dt_res = field_data_type_from_string((*it)[2].str());
        if (dt_res.is_ok()) fd.data_type = dt_res.value();
        fd.canonical_unit = (*it)[3].str();
        auto ap_res = field_applicability_from_string((*it)[4].str());
        if (ap_res.is_ok()) fd.applicability = ap_res.value();
        fd.description = (*it)[5].str();

        if (!dict.has_field(fd.canonical_name)) {
            dict.register_field(fd);
        }
    }

    return dict;
}

} // namespace shunya::core::domain
