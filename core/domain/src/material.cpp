#include <shunya/core/domain/material.hpp>

#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_error(std::string msg, int32_t code = 1) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Materials,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

base::Result<MaterialPhysicalState> material_physical_state_from_string(std::string_view str) {
    if (str == "Solid")  return MaterialPhysicalState::Solid;
    if (str == "Liquid") return MaterialPhysicalState::Liquid;
    if (str == "Gas")    return MaterialPhysicalState::Gas;
    if (str == "Plasma") return MaterialPhysicalState::Plasma;
    return make_error(std::string("Unknown MaterialPhysicalState: ") + std::string(str));
}

double MechanicalProperties::effective_shear_modulus() const noexcept {
    if (shear_modulus_pa.has_value()) {
        return *shear_modulus_pa;
    }
    if (youngs_modulus_pa.has_value() && poissons_ratio.has_value()) {
        double nu = *poissons_ratio;
        if (std::abs(1.0 + nu) > 1e-9) {
            return *youngs_modulus_pa / (2.0 * (1.0 + nu));
        }
    }
    return 0.0;
}

double MechanicalProperties::effective_bulk_modulus() const noexcept {
    if (bulk_modulus_pa.has_value()) {
        return *bulk_modulus_pa;
    }
    if (youngs_modulus_pa.has_value() && poissons_ratio.has_value()) {
        double nu = *poissons_ratio;
        if (std::abs(1.0 - 2.0 * nu) > 1e-9) {
            return *youngs_modulus_pa / (3.0 * (1.0 - 2.0 * nu));
        }
    }
    return 0.0;
}

bool MechanicalProperties::operator==(const MechanicalProperties& other) const noexcept {
    return youngs_modulus_pa == other.youngs_modulus_pa &&
           poissons_ratio == other.poissons_ratio &&
           shear_modulus_pa == other.shear_modulus_pa &&
           bulk_modulus_pa == other.bulk_modulus_pa &&
           yield_strength_pa == other.yield_strength_pa &&
           ultimate_strength_pa == other.ultimate_strength_pa &&
           elongation_at_break == other.elongation_at_break &&
           tangent_modulus_pa == other.tangent_modulus_pa &&
           fracture_toughness == other.fracture_toughness;
}

bool ThermalProperties::operator==(const ThermalProperties& other) const noexcept {
    return thermal_conductivity == other.thermal_conductivity &&
           specific_heat_capacity == other.specific_heat_capacity &&
           thermal_expansion_coeff == other.thermal_expansion_coeff &&
           emissivity == other.emissivity &&
           melting_point_k == other.melting_point_k &&
           boiling_point_k == other.boiling_point_k &&
           latent_heat_fusion == other.latent_heat_fusion &&
           latent_heat_vaporization == other.latent_heat_vaporization;
}

double ElectricalProperties::effective_resistivity() const noexcept {
    if (electrical_resistivity.has_value()) {
        return *electrical_resistivity;
    }
    if (electrical_conductivity.has_value() && *electrical_conductivity > 1e-15) {
        return 1.0 / (*electrical_conductivity);
    }
    return 0.0;
}

bool ElectricalProperties::operator==(const ElectricalProperties& other) const noexcept {
    return electrical_conductivity == other.electrical_conductivity &&
           electrical_resistivity == other.electrical_resistivity &&
           relative_permittivity == other.relative_permittivity &&
           relative_permeability == other.relative_permeability &&
           dielectric_breakdown_strength == other.dielectric_breakdown_strength &&
           loss_tangent == other.loss_tangent &&
           temp_coeff_resistance == other.temp_coeff_resistance;
}

bool FluidProperties::operator==(const FluidProperties& other) const noexcept {
    return dynamic_viscosity == other.dynamic_viscosity &&
           kinematic_viscosity == other.kinematic_viscosity &&
           diffusivity == other.diffusivity &&
           surface_tension == other.surface_tension &&
           molecular_weight == other.molecular_weight &&
           chemical_formula == other.chemical_formula;
}

Material::Material()
    : id_(base::Uuid::generate_random()),
      name_("Unnamed Material") {}

Material::Material(base::Uuid id, std::string name, MaterialPhysicalState state, double density_kg_m3)
    : id_(id),
      name_(std::move(name)),
      density_kg_m3_(density_kg_m3),
      state_(state) {}

void Material::add_curve(MaterialCurve curve) {
    std::string key = curve.name();
    curves_[key] = std::move(curve);
}

const MaterialCurve* Material::get_curve(std::string_view property_name) const noexcept {
    auto it = curves_.find(std::string(property_name));
    if (it != curves_.end()) return &it->second;
    return nullptr;
}

bool Material::has_curve(std::string_view property_name) const noexcept {
    return curves_.find(std::string(property_name)) != curves_.end();
}

void Material::remove_curve(std::string_view property_name) {
    curves_.erase(std::string(property_name));
}

base::Result<double> Material::evaluate_thermal_conductivity(double temperature_k) const {
    const MaterialCurve* curve = get_curve("thermal_conductivity");
    if (!curve) curve = get_curve("k");
    if (curve) {
        return curve->evaluate(temperature_k);
    }
    if (thermal_.thermal_conductivity.has_value()) {
        return *thermal_.thermal_conductivity;
    }
    return make_error("Thermal conductivity not defined for material: " + name_);
}

base::Result<double> Material::evaluate_specific_heat(double temperature_k) const {
    const MaterialCurve* curve = get_curve("specific_heat");
    if (!curve) curve = get_curve("cp");
    if (curve) {
        return curve->evaluate(temperature_k);
    }
    if (thermal_.specific_heat_capacity.has_value()) {
        return *thermal_.specific_heat_capacity;
    }
    return make_error("Specific heat capacity not defined for material: " + name_);
}

base::Result<double> Material::evaluate_electrical_resistivity(double temperature_k) const {
    const MaterialCurve* curve = get_curve("electrical_resistivity");
    if (!curve) curve = get_curve("resistivity");
    if (curve) {
        return curve->evaluate(temperature_k);
    }
    double rho0 = electrical_.effective_resistivity();
    if (rho0 > 0.0) {
        if (electrical_.temp_coeff_resistance.has_value()) {
            double alpha = *electrical_.temp_coeff_resistance;
            // Reference T0 = 293.15 K (20 C)
            double t0 = 293.15;
            return rho0 * (1.0 + alpha * (temperature_k - t0));
        }
        return rho0;
    }
    return make_error("Electrical resistivity not defined for material: " + name_);
}

base::Result<double> Material::evaluate_stress(double strain) const {
    const MaterialCurve* curve = get_curve("stress_strain");
    if (!curve) curve = get_curve("stress");
    if (curve) {
        return curve->evaluate(strain);
    }
    if (mechanical_.youngs_modulus_pa.has_value()) {
        double E = *mechanical_.youngs_modulus_pa;
        double stress = E * strain;
        if (mechanical_.yield_strength_pa.has_value()) {
            double sy = *mechanical_.yield_strength_pa;
            if (stress > sy) {
                if (mechanical_.tangent_modulus_pa.has_value()) {
                    double Et = *mechanical_.tangent_modulus_pa;
                    double elastic_strain = sy / E;
                    stress = sy + Et * (strain - elastic_strain);
                } else {
                    stress = sy; // Perfect plasticity
                }
            }
        }
        return stress;
    }
    return make_error("Elastic modulus not defined for material: " + name_);
}

base::ValidationReport Material::validate() const {
    base::ValidationReport report;

    if (id_.is_nil()) {
        report.add_error("MAT_NIL_ID", "Material has nil UUID");
    }
    if (name_.empty()) {
        report.add_error("MAT_EMPTY_NAME", "Material name cannot be empty");
    }
    if (density_kg_m3_ <= 0.0) {
        report.add_error("MAT_INVALID_DENSITY", "Material density must be strictly positive (> 0.0 kg/m^3)");
    } else if (density_kg_m3_ > 30000.0) {
        report.add_warning("MAT_HIGH_DENSITY", "Material density is unusually high (> 30000 kg/m^3)");
    }

    // Mechanical checks
    if (mechanical_.youngs_modulus_pa.has_value() && *mechanical_.youngs_modulus_pa <= 0.0) {
        report.add_error("MAT_INVALID_YOUNGS_MODULUS", "Young's modulus must be strictly positive");
    }
    if (mechanical_.poissons_ratio.has_value()) {
        double nu = *mechanical_.poissons_ratio;
        if (nu <= -1.0 || nu >= 0.5) {
            report.add_error("MAT_INVALID_POISSON_RATIO", "Poisson's ratio must satisfy -1.0 < nu < 0.5 for 3D isotropic materials");
        }
    }
    if (mechanical_.shear_modulus_pa.has_value() && *mechanical_.shear_modulus_pa <= 0.0) {
        report.add_error("MAT_INVALID_SHEAR_MODULUS", "Shear modulus must be strictly positive");
    }
    if (mechanical_.bulk_modulus_pa.has_value() && *mechanical_.bulk_modulus_pa <= 0.0) {
        report.add_error("MAT_INVALID_BULK_MODULUS", "Bulk modulus must be strictly positive");
    }
    if (mechanical_.yield_strength_pa.has_value() && *mechanical_.yield_strength_pa <= 0.0) {
        report.add_error("MAT_INVALID_YIELD_STRENGTH", "Yield strength must be strictly positive");
    }
    if (mechanical_.ultimate_strength_pa.has_value() && *mechanical_.ultimate_strength_pa <= 0.0) {
        report.add_error("MAT_INVALID_ULTIMATE_STRENGTH", "Ultimate strength must be strictly positive");
    }
    if (mechanical_.yield_strength_pa.has_value() && mechanical_.ultimate_strength_pa.has_value()) {
        if (*mechanical_.ultimate_strength_pa < *mechanical_.yield_strength_pa) {
            report.add_error("MAT_STRENGTH_INVERSION", "Ultimate tensile strength cannot be lower than yield strength");
        }
    }
    if (mechanical_.elongation_at_break.has_value() && *mechanical_.elongation_at_break < 0.0) {
        report.add_error("MAT_INVALID_ELONGATION", "Elongation at break cannot be negative");
    }

    // Thermal checks
    if (thermal_.thermal_conductivity.has_value() && *thermal_.thermal_conductivity <= 0.0) {
        report.add_error("MAT_INVALID_THERMAL_CONDUCTIVITY", "Thermal conductivity must be strictly positive");
    }
    if (thermal_.specific_heat_capacity.has_value() && *thermal_.specific_heat_capacity <= 0.0) {
        report.add_error("MAT_INVALID_SPECIFIC_HEAT", "Specific heat capacity must be strictly positive");
    }
    if (thermal_.emissivity.has_value()) {
        double eps = *thermal_.emissivity;
        if (eps < 0.0 || eps > 1.0) {
            report.add_error("MAT_INVALID_EMISSIVITY", "Emissivity must be in range [0.0, 1.0]");
        }
    }
    if (thermal_.melting_point_k.has_value() && *thermal_.melting_point_k < 0.0) {
        report.add_error("MAT_BELOW_ABSOLUTE_ZERO", "Melting point cannot be below absolute zero (0 K)");
    }
    if (thermal_.boiling_point_k.has_value() && *thermal_.boiling_point_k < 0.0) {
        report.add_error("MAT_BELOW_ABSOLUTE_ZERO", "Boiling point cannot be below absolute zero (0 K)");
    }
    if (thermal_.melting_point_k.has_value() && thermal_.boiling_point_k.has_value()) {
        if (*thermal_.boiling_point_k < *thermal_.melting_point_k) {
            report.add_error("MAT_PHASE_TEMP_INVERSION", "Boiling point cannot be lower than melting point");
        }
    }

    // Electrical checks
    if (electrical_.electrical_conductivity.has_value() && *electrical_.electrical_conductivity < 0.0) {
        report.add_error("MAT_INVALID_ELECTRICAL_CONDUCTIVITY", "Electrical conductivity cannot be negative");
    }
    if (electrical_.electrical_resistivity.has_value() && *electrical_.electrical_resistivity < 0.0) {
        report.add_error("MAT_INVALID_RESISTIVITY", "Electrical resistivity cannot be negative");
    }
    if (electrical_.relative_permittivity.has_value() && *electrical_.relative_permittivity < 1.0) {
        report.add_error("MAT_INVALID_PERMITTIVITY", "Relative permittivity must be >= 1.0");
    }
    if (electrical_.relative_permeability.has_value() && *electrical_.relative_permeability < 0.0) {
        report.add_error("MAT_INVALID_PERMEABILITY", "Relative permeability must be >= 0.0");
    }
    if (electrical_.loss_tangent.has_value() && *electrical_.loss_tangent < 0.0) {
        report.add_error("MAT_INVALID_LOSS_TANGENT", "Loss tangent cannot be negative");
    }

    // Fluid checks
    if (fluid_.dynamic_viscosity.has_value() && *fluid_.dynamic_viscosity <= 0.0) {
        report.add_error("MAT_INVALID_VISCOSITY", "Dynamic viscosity must be strictly positive");
    }
    if (fluid_.kinematic_viscosity.has_value() && *fluid_.kinematic_viscosity <= 0.0) {
        report.add_error("MAT_INVALID_KINEMATIC_VISCOSITY", "Kinematic viscosity must be strictly positive");
    }
    if (fluid_.diffusivity.has_value() && *fluid_.diffusivity < 0.0) {
        report.add_error("MAT_INVALID_DIFFUSIVITY", "Diffusivity cannot be negative");
    }
    if (fluid_.surface_tension.has_value() && *fluid_.surface_tension < 0.0) {
        report.add_error("MAT_INVALID_SURFACE_TENSION", "Surface tension cannot be negative");
    }

    return report;
}

std::string Material::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id_.to_string() << "\",\n";
    ss << "  \"name\": \"" << name_ << "\",\n";
    ss << "  \"description\": \"" << description_ << "\",\n";
    ss << "  \"composition\": \"" << composition_ << "\",\n";
    ss << "  \"density_kg_m3\": " << density_kg_m3_ << ",\n";
    ss << "  \"state\": \"" << material_physical_state_to_string(state_) << "\",\n";
    ss << "  \"provenance\": \"" << provenance_ << "\",\n";
    ss << "  \"source\": \"" << source_ << "\",\n";
    ss << "  \"version\": \"" << version_.as_string() << "\",\n";

    // Mechanical
    ss << "  \"mechanical\": {\n";
    if (mechanical_.youngs_modulus_pa) ss << "    \"youngs_modulus_pa\": " << *mechanical_.youngs_modulus_pa << ",\n";
    if (mechanical_.poissons_ratio) ss << "    \"poissons_ratio\": " << *mechanical_.poissons_ratio << ",\n";
    if (mechanical_.shear_modulus_pa) ss << "    \"shear_modulus_pa\": " << *mechanical_.shear_modulus_pa << ",\n";
    if (mechanical_.bulk_modulus_pa) ss << "    \"bulk_modulus_pa\": " << *mechanical_.bulk_modulus_pa << ",\n";
    if (mechanical_.yield_strength_pa) ss << "    \"yield_strength_pa\": " << *mechanical_.yield_strength_pa << ",\n";
    if (mechanical_.ultimate_strength_pa) ss << "    \"ultimate_strength_pa\": " << *mechanical_.ultimate_strength_pa << ",\n";
    if (mechanical_.elongation_at_break) ss << "    \"elongation_at_break\": " << *mechanical_.elongation_at_break << ",\n";
    if (mechanical_.tangent_modulus_pa) ss << "    \"tangent_modulus_pa\": " << *mechanical_.tangent_modulus_pa << ",\n";
    if (mechanical_.fracture_toughness) ss << "    \"fracture_toughness\": " << *mechanical_.fracture_toughness << ",\n";
    ss << "    \"_valid\": true\n";
    ss << "  },\n";

    // Thermal
    ss << "  \"thermal\": {\n";
    if (thermal_.thermal_conductivity) ss << "    \"thermal_conductivity\": " << *thermal_.thermal_conductivity << ",\n";
    if (thermal_.specific_heat_capacity) ss << "    \"specific_heat_capacity\": " << *thermal_.specific_heat_capacity << ",\n";
    if (thermal_.thermal_expansion_coeff) ss << "    \"thermal_expansion_coeff\": " << *thermal_.thermal_expansion_coeff << ",\n";
    if (thermal_.emissivity) ss << "    \"emissivity\": " << *thermal_.emissivity << ",\n";
    if (thermal_.melting_point_k) ss << "    \"melting_point_k\": " << *thermal_.melting_point_k << ",\n";
    if (thermal_.boiling_point_k) ss << "    \"boiling_point_k\": " << *thermal_.boiling_point_k << ",\n";
    if (thermal_.latent_heat_fusion) ss << "    \"latent_heat_fusion\": " << *thermal_.latent_heat_fusion << ",\n";
    if (thermal_.latent_heat_vaporization) ss << "    \"latent_heat_vaporization\": " << *thermal_.latent_heat_vaporization << ",\n";
    ss << "    \"_valid\": true\n";
    ss << "  },\n";

    // Electrical
    ss << "  \"electrical\": {\n";
    if (electrical_.electrical_conductivity) ss << "    \"electrical_conductivity\": " << *electrical_.electrical_conductivity << ",\n";
    if (electrical_.electrical_resistivity) ss << "    \"electrical_resistivity\": " << *electrical_.electrical_resistivity << ",\n";
    if (electrical_.relative_permittivity) ss << "    \"relative_permittivity\": " << *electrical_.relative_permittivity << ",\n";
    if (electrical_.relative_permeability) ss << "    \"relative_permeability\": " << *electrical_.relative_permeability << ",\n";
    if (electrical_.dielectric_breakdown_strength) ss << "    \"dielectric_breakdown_strength\": " << *electrical_.dielectric_breakdown_strength << ",\n";
    if (electrical_.loss_tangent) ss << "    \"loss_tangent\": " << *electrical_.loss_tangent << ",\n";
    if (electrical_.temp_coeff_resistance) ss << "    \"temp_coeff_resistance\": " << *electrical_.temp_coeff_resistance << ",\n";
    ss << "    \"_valid\": true\n";
    ss << "  },\n";

    // Fluid
    ss << "  \"fluid\": {\n";
    if (fluid_.dynamic_viscosity) ss << "    \"dynamic_viscosity\": " << *fluid_.dynamic_viscosity << ",\n";
    if (fluid_.kinematic_viscosity) ss << "    \"kinematic_viscosity\": " << *fluid_.kinematic_viscosity << ",\n";
    if (fluid_.diffusivity) ss << "    \"diffusivity\": " << *fluid_.diffusivity << ",\n";
    if (fluid_.surface_tension) ss << "    \"surface_tension\": " << *fluid_.surface_tension << ",\n";
    if (fluid_.molecular_weight) ss << "    \"molecular_weight\": " << *fluid_.molecular_weight << ",\n";
    ss << "    \"chemical_formula\": \"" << fluid_.chemical_formula << "\"\n";
    ss << "  },\n";

    // Curves
    ss << "  \"curves\": [\n";
    size_t c_idx = 0;
    for (const auto& pair : curves_) {
        ss << pair.second.to_json();
        if (++c_idx < curves_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

base::Result<Material> Material::from_json(std::string_view json_str) {
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
    auto id_res = base::Uuid::from_string(id_str);
    base::Uuid id = id_res.has_value() ? *id_res : base::Uuid::generate_random();

    std::string name = extract_str("name");
    std::string desc = extract_str("description");
    std::string comp = extract_str("composition");
    double rho = extract_num("density_kg_m3").value_or(0.0);

    MaterialPhysicalState state = MaterialPhysicalState::Solid;
    std::string state_str = extract_str("state");
    if (!state_str.empty()) {
        auto res = material_physical_state_from_string(state_str);
        if (res.is_ok()) state = res.value();
    }

    Material mat(id, name, state, rho);
    mat.set_description(std::move(desc));
    mat.set_composition(std::move(comp));
    mat.set_provenance(extract_str("provenance"));
    mat.set_source(extract_str("source"));

    std::string ver_str = extract_str("version");
    if (!ver_str.empty()) {
        auto ver_res = base::Version::parse(ver_str);
        if (ver_res.is_ok()) mat.set_version(ver_res.value());
    }

    // Mechanical
    MechanicalProperties mech;
    mech.youngs_modulus_pa = extract_num("youngs_modulus_pa");
    mech.poissons_ratio = extract_num("poissons_ratio");
    mech.shear_modulus_pa = extract_num("shear_modulus_pa");
    mech.bulk_modulus_pa = extract_num("bulk_modulus_pa");
    mech.yield_strength_pa = extract_num("yield_strength_pa");
    mech.ultimate_strength_pa = extract_num("ultimate_strength_pa");
    mech.elongation_at_break = extract_num("elongation_at_break");
    mech.tangent_modulus_pa = extract_num("tangent_modulus_pa");
    mech.fracture_toughness = extract_num("fracture_toughness");
    mat.set_mechanical(mech);

    // Thermal
    ThermalProperties therm;
    therm.thermal_conductivity = extract_num("thermal_conductivity");
    therm.specific_heat_capacity = extract_num("specific_heat_capacity");
    therm.thermal_expansion_coeff = extract_num("thermal_expansion_coeff");
    therm.emissivity = extract_num("emissivity");
    therm.melting_point_k = extract_num("melting_point_k");
    therm.boiling_point_k = extract_num("boiling_point_k");
    therm.latent_heat_fusion = extract_num("latent_heat_fusion");
    therm.latent_heat_vaporization = extract_num("latent_heat_vaporization");
    mat.set_thermal(therm);

    // Electrical
    ElectricalProperties elec;
    elec.electrical_conductivity = extract_num("electrical_conductivity");
    elec.electrical_resistivity = extract_num("electrical_resistivity");
    elec.relative_permittivity = extract_num("relative_permittivity");
    elec.relative_permeability = extract_num("relative_permeability");
    elec.dielectric_breakdown_strength = extract_num("dielectric_breakdown_strength");
    elec.loss_tangent = extract_num("loss_tangent");
    elec.temp_coeff_resistance = extract_num("temp_coeff_resistance");
    mat.set_electrical(elec);

    // Fluid
    FluidProperties fl;
    fl.dynamic_viscosity = extract_num("dynamic_viscosity");
    fl.kinematic_viscosity = extract_num("kinematic_viscosity");
    fl.diffusivity = extract_num("diffusivity");
    fl.surface_tension = extract_num("surface_tension");
    fl.molecular_weight = extract_num("molecular_weight");
    fl.chemical_formula = extract_str("chemical_formula");
    mat.set_fluid(fl);

    // Parse curves if present
    size_t curves_pos = s.find("\"curves\"");
    if (curves_pos != std::string::npos) {
        size_t arr_start = s.find('[', curves_pos);
        if (arr_start != std::string::npos) {
            size_t depth = 0;
            size_t obj_start = std::string::npos;
            for (size_t i = arr_start; i < s.size(); ++i) {
                if (s[i] == '{') {
                    if (depth == 0) obj_start = i;
                    depth++;
                } else if (s[i] == '}') {
                    depth--;
                    if (depth == 0 && obj_start != std::string::npos) {
                        std::string curve_json = s.substr(obj_start, i - obj_start + 1);
                        auto c_res = MaterialCurve::from_json(curve_json);
                        if (c_res.is_ok()) {
                            mat.add_curve(c_res.value());
                        }
                        obj_start = std::string::npos;
                    }
                } else if (s[i] == ']' && depth == 0) {
                    break;
                }
            }
        }
    }

    return mat;
}

// Preset standard engineering materials
Material Material::structural_steel() {
    Material mat(base::Uuid::generate_random(), "Structural Steel", MaterialPhysicalState::Solid, 7850.0);
    mat.set_composition("Fe > 98%, C 0.2%, Mn 1.4%, Si 0.3%");
    mat.set_provenance("EN 10025-2 S275 / ASTM A36 standard engineering reference");
    mat.set_source("Engineering Databook");
    mat.set_version({1, 0, 0});

    MechanicalProperties mech;
    mech.youngs_modulus_pa = 200.0e9;      // 200 GPa
    mech.poissons_ratio = 0.30;
    mech.shear_modulus_pa = 76.9e9;        // 76.9 GPa
    mech.bulk_modulus_pa = 166.7e9;        // 166.7 GPa
    mech.yield_strength_pa = 250.0e6;      // 250 MPa
    mech.ultimate_strength_pa = 400.0e6;   // 400 MPa
    mech.elongation_at_break = 0.20;       // 20%
    mech.tangent_modulus_pa = 2.0e9;       // 2 GPa hardening
    mech.fracture_toughness = 50.0e6;      // 50 MPa*m^0.5
    mat.set_mechanical(mech);

    ThermalProperties therm;
    therm.thermal_conductivity = 50.0;     // 50 W/(m*K)
    therm.specific_heat_capacity = 486.0;  // 486 J/(kg*K)
    therm.thermal_expansion_coeff = 1.2e-5;// 12 ppm/K
    therm.emissivity = 0.80;
    therm.melting_point_k = 1693.15;       // ~1420 C
    mat.set_thermal(therm);

    ElectricalProperties elec;
    elec.electrical_conductivity = 6.99e6; // S/m
    elec.electrical_resistivity = 1.43e-7; // Ohm*m
    elec.relative_permittivity = 1.0;
    elec.relative_permeability = 200.0;    // Ferromagnetic
    elec.temp_coeff_resistance = 0.005;    // 0.005 1/K
    mat.set_electrical(elec);

    return mat;
}

Material Material::aluminium_6061() {
    Material mat(base::Uuid::generate_random(), "Aluminium 6061-T6", MaterialPhysicalState::Solid, 2700.0);
    mat.set_composition("Al 97.9%, Mg 1.0%, Si 0.6%, Cu 0.28%, Cr 0.2%");
    mat.set_provenance("ASM Aerospace Specification Metals");
    mat.set_source("MIL-HDBK-5J");
    mat.set_version({1, 0, 0});

    MechanicalProperties mech;
    mech.youngs_modulus_pa = 68.9e9;       // 68.9 GPa
    mech.poissons_ratio = 0.33;
    mech.shear_modulus_pa = 26.0e9;        // 26 GPa
    mech.yield_strength_pa = 276.0e6;      // 276 MPa
    mech.ultimate_strength_pa = 310.0e6;   // 310 MPa
    mech.elongation_at_break = 0.12;       // 12%
    mat.set_mechanical(mech);

    ThermalProperties therm;
    therm.thermal_conductivity = 167.0;    // 167 W/(m*K)
    therm.specific_heat_capacity = 896.0;  // 896 J/(kg*K)
    therm.thermal_expansion_coeff = 2.32e-5;
    therm.emissivity = 0.09;               // Polished
    therm.melting_point_k = 855.15;        // 582 C
    mat.set_thermal(therm);

    ElectricalProperties elec;
    elec.electrical_conductivity = 2.5e7;  // 25 MS/m
    elec.electrical_resistivity = 3.99e-8; // Ohm*m
    elec.relative_permittivity = 1.0;
    elec.relative_permeability = 1.0;
    mat.set_electrical(elec);

    return mat;
}

Material Material::copper() {
    Material mat(base::Uuid::generate_random(), "Copper (Electrolytic Tough Pitch)", MaterialPhysicalState::Solid, 8960.0);
    mat.set_composition("Cu > 99.9%, O 0.04%");
    mat.set_provenance("CDA Alloy C11000 / ASTM B152");
    mat.set_source("Copper Development Association");
    mat.set_version({1, 0, 0});

    MechanicalProperties mech;
    mech.youngs_modulus_pa = 117.0e9;      // 117 GPa
    mech.poissons_ratio = 0.34;
    mech.shear_modulus_pa = 44.0e9;
    mech.yield_strength_pa = 70.0e6;       // Annealed 70 MPa
    mech.ultimate_strength_pa = 220.0e6;   // 220 MPa
    mech.elongation_at_break = 0.45;       // 45%
    mat.set_mechanical(mech);

    ThermalProperties therm;
    therm.thermal_conductivity = 401.0;    // 401 W/(m*K)
    therm.specific_heat_capacity = 385.0;  // 385 J/(kg*K)
    therm.thermal_expansion_coeff = 1.65e-5;
    therm.emissivity = 0.05;
    therm.melting_point_k = 1357.77;       // 1084.62 C
    mat.set_thermal(therm);

    ElectricalProperties elec;
    elec.electrical_conductivity = 5.96e7; // 59.6 MS/m (100% IACS)
    elec.electrical_resistivity = 1.68e-8; // 16.8 nOhm*m
    elec.relative_permittivity = 1.0;
    elec.relative_permeability = 0.999994; // Diamagnetic
    elec.temp_coeff_resistance = 0.00393;  // 0.393% / K
    mat.set_electrical(elec);

    return mat;
}

Material Material::silicon() {
    Material mat(base::Uuid::generate_random(), "Silicon (Monocrystalline)", MaterialPhysicalState::Solid, 2329.0);
    mat.set_composition("Si > 99.9999%");
    mat.set_provenance("Semiconductor Grade Wafer");
    mat.set_source("Ioffe Institute");
    mat.set_version({1, 0, 0});

    MechanicalProperties mech;
    mech.youngs_modulus_pa = 130.0e9;      // 130 GPa <100>
    mech.poissons_ratio = 0.28;
    mech.shear_modulus_pa = 50.8e9;
    mat.set_mechanical(mech);

    ThermalProperties therm;
    therm.thermal_conductivity = 148.0;    // 148 W/(m*K)
    therm.specific_heat_capacity = 705.0;  // 705 J/(kg*K)
    therm.thermal_expansion_coeff = 2.6e-6;// 2.6 ppm/K
    therm.melting_point_k = 1687.0;        // 1414 C
    mat.set_thermal(therm);

    ElectricalProperties elec;
    elec.electrical_conductivity = 1.0e-3; // Undoped ~10^-3 S/m
    elec.electrical_resistivity = 1000.0;  // 1000 Ohm*m
    elec.relative_permittivity = 11.68;    // 11.68
    elec.relative_permeability = 1.0;
    elec.dielectric_breakdown_strength = 3.0e7; // 30 MV/m
    mat.set_electrical(elec);

    FluidProperties fl;
    fl.chemical_formula = "Si";
    fl.molecular_weight = 0.0280855;       // kg/mol
    mat.set_fluid(fl);

    return mat;
}

Material Material::air_standard() {
    Material mat(base::Uuid::generate_random(), "Air (Standard Sea Level)", MaterialPhysicalState::Gas, 1.225);
    mat.set_composition("N2 78.08%, O2 20.95%, Ar 0.93%, CO2 0.04%");
    mat.set_provenance("US Standard Atmosphere 1976");
    mat.set_source("NASA-TM-X-74335");
    mat.set_version({1, 0, 0});

    ThermalProperties therm;
    therm.thermal_conductivity = 0.026;    // 0.026 W/(m*K)
    therm.specific_heat_capacity = 1005.0; // 1005 J/(kg*K)
    therm.thermal_expansion_coeff = 3.43e-3;// ~1/T0
    mat.set_thermal(therm);

    ElectricalProperties elec;
    elec.electrical_conductivity = 1.0e-14;
    elec.electrical_resistivity = 1.0e14;
    elec.relative_permittivity = 1.00059;
    elec.relative_permeability = 1.0;
    elec.dielectric_breakdown_strength = 3.0e6; // 3 kV/mm = 3 MV/m
    mat.set_electrical(elec);

    FluidProperties fl;
    fl.dynamic_viscosity = 1.81e-5;        // 1.81e-5 Pa*s
    fl.kinematic_viscosity = 1.48e-5;      // 1.48e-5 m^2/s
    fl.diffusivity = 2.0e-5;               // m^2/s
    fl.molecular_weight = 0.0289647;       // 28.96 g/mol
    fl.chemical_formula = "Air";
    mat.set_fluid(fl);

    return mat;
}

Material Material::water_liquid() {
    Material mat(base::Uuid::generate_random(), "Water (Pure Liquid 20 C)", MaterialPhysicalState::Liquid, 998.2);
    mat.set_composition("H2O");
    mat.set_provenance("IAPWS-95 Formulation");
    mat.set_source("NIST Chemistry WebBook");
    mat.set_version({1, 0, 0});

    MechanicalProperties mech;
    mech.bulk_modulus_pa = 2.2e9;          // 2.2 GPa
    mat.set_mechanical(mech);

    ThermalProperties therm;
    therm.thermal_conductivity = 0.606;    // 0.606 W/(m*K)
    therm.specific_heat_capacity = 4184.0; // 4184 J/(kg*K)
    therm.thermal_expansion_coeff = 2.07e-4;
    therm.emissivity = 0.96;
    therm.melting_point_k = 273.15;        // 0 C
    therm.boiling_point_k = 373.15;        // 100 C
    therm.latent_heat_fusion = 3.34e5;     // 334 kJ/kg
    therm.latent_heat_vaporization = 2.26e6;// 2260 kJ/kg
    mat.set_thermal(therm);

    ElectricalProperties elec;
    elec.electrical_conductivity = 5.5e-6; // Deionized water ~ 0.055 uS/cm
    elec.electrical_resistivity = 1.8e5;   // 18 MOhm*cm = 180 kOhm*m
    elec.relative_permittivity = 80.1;     // 80.1 at 20 C
    elec.relative_permeability = 0.999992;
    elec.loss_tangent = 0.04;
    mat.set_electrical(elec);

    FluidProperties fl;
    fl.dynamic_viscosity = 1.002e-3;       // 1.002 mPa*s
    fl.kinematic_viscosity = 1.004e-6;     // 1.004e-6 m^2/s
    fl.diffusivity = 1.4e-7;
    fl.surface_tension = 0.0728;           // 72.8 mN/m
    fl.molecular_weight = 0.018015;        // 18.015 g/mol
    fl.chemical_formula = "H2O";
    mat.set_fluid(fl);

    return mat;
}

} // namespace shunya::core::domain
