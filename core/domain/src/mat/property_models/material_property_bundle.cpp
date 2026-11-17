#include "shunya/mat/property_models/material_property_bundle.hpp"

#include <cmath>
#include <sstream>
#include <iomanip>

namespace shunya::mat {

BundleValidationResult MaterialPropertyBundle::validate() const {
    BundleValidationResult result;

    // 1. Mechanical SI Constraints
    if (mechanical.density_kg_m3 <= 0.0) {
        result.errors.push_back("Mechanical: mass density must be strictly positive (> 0 kg/m^3).");
    }
    if (mechanical.young_modulus_pa < 0.0) {
        result.errors.push_back("Mechanical: Young's modulus must be non-negative (Pa).");
    }
    if (mechanical.shear_modulus_pa < 0.0) {
        result.errors.push_back("Mechanical: shear modulus must be non-negative (Pa).");
    }
    if (mechanical.bulk_modulus_pa < 0.0) {
        result.errors.push_back("Mechanical: bulk modulus must be non-negative (Pa).");
    }
    if (mechanical.yield_strength_pa < 0.0) {
        result.errors.push_back("Mechanical: yield strength must be non-negative (Pa).");
    }
    if (mechanical.ultimate_tensile_strength_pa < 0.0) {
        result.errors.push_back("Mechanical: ultimate tensile strength must be non-negative (Pa).");
    }
    if (mechanical.fracture_toughness_k1c_mpa_m05 < 0.0) {
        result.errors.push_back("Mechanical: fracture toughness must be non-negative (MPa*m^0.5).");
    }
    if (mechanical.poisson_ratio != 0.0) {
        if (mechanical.poisson_ratio <= -1.0 || mechanical.poisson_ratio >= 0.5) {
            result.warnings.push_back("Mechanical: Poisson's ratio typically falls in (-1.0, 0.5) for stable isotropic solids.");
        }
    }
    // Cross-check isotropic elasticity relations if E, G, nu are defined
    if (mechanical.young_modulus_pa > 0.0 && mechanical.poisson_ratio > 0.0 && mechanical.shear_modulus_pa > 0.0) {
        double expected_g = mechanical.young_modulus_pa / (2.0 * (1.0 + mechanical.poisson_ratio));
        double rel_diff = std::abs(expected_g - mechanical.shear_modulus_pa) / mechanical.shear_modulus_pa;
        if (rel_diff > 0.30) {
            result.warnings.push_back("Mechanical: shear modulus deviates >30% from isotropic relation G = E/(2*(1+nu)). Anisotropic material?");
        }
    }

    // 2. Thermal SI Constraints
    if (thermal.melting_point_k < 0.0) {
        result.errors.push_back("Thermal: melting point cannot be negative (Kelvin).");
    }
    if (thermal.boiling_point_k < 0.0) {
        result.errors.push_back("Thermal: boiling point cannot be negative (Kelvin).");
    }
    if (thermal.melting_point_k > 0.0 && thermal.boiling_point_k > 0.0 &&
        thermal.melting_point_k > thermal.boiling_point_k) {
        result.errors.push_back("Thermal: melting point cannot exceed boiling point.");
    }
    if (thermal.heat_of_fusion_j_kg < 0.0) {
        result.errors.push_back("Thermal: heat of fusion cannot be negative (J/kg).");
    }
    if (thermal.heat_of_vaporization_j_kg < 0.0) {
        result.errors.push_back("Thermal: heat of vaporization cannot be negative (J/kg).");
    }
    if (thermal.base_specific_heat_j_kgk < 0.0 || thermal.specific_heat_at(293.15) < 0.0) {
        result.errors.push_back("Thermal: specific heat capacity cannot be negative (J/(kg*K)).");
    }
    if (thermal.base_thermal_conductivity_w_mk < 0.0 || thermal.thermal_conductivity_at(293.15) < 0.0) {
        result.errors.push_back("Thermal: thermal conductivity cannot be negative (W/(m*K)).");
    }

    // 3. Electrical SI Constraints
    if (electrical.base_conductivity_s_m < 0.0) {
        result.errors.push_back("Electrical: conductivity cannot be negative (S/m).");
    }
    if (electrical.base_resistivity_ohm_m < 0.0) {
        result.errors.push_back("Electrical: resistivity cannot be negative (Ohm*m).");
    }
    if (electrical.carrier_mobility_m2_vs < 0.0) {
        result.errors.push_back("Electrical: carrier mobility cannot be negative (m^2/(V*s)).");
    }
    if (electrical.carrier_density_m3 < 0.0) {
        result.errors.push_back("Electrical: charge carrier density cannot be negative (m^-3).");
    }
    if (electrical.relative_permittivity < 1.0 && electrical.relative_permittivity > 0.0) {
        result.warnings.push_back("Electrical: relative dielectric constant is typically >= 1.0.");
    }
    if (electrical.breakdown_field_v_m < 0.0) {
        result.errors.push_back("Electrical: dielectric breakdown field cannot be negative (V/m).");
    }

    // Reciprocity check between conductivity and resistivity at reference temperature
    double cond_ref = electrical.conductivity_at(electrical.reference_temp_k);
    double res_ref = electrical.resistivity_at(electrical.reference_temp_k);
    if (cond_ref > 0.0 && res_ref > 0.0) {
        double prod = cond_ref * res_ref;
        if (std::abs(prod - 1.0) > 0.05) {
            result.warnings.push_back("Electrical: conductivity * resistivity deviates from 1.0 at reference temperature.");
        }
    }

    // 4. Environmental SI Constraints
    if (environmental.saturation_moisture_content_pct < 0.0) {
        result.errors.push_back("Environmental: saturation moisture content cannot be negative.");
    }
    if (environmental.moisture_diffusion_coefficient_m2_s < 0.0) {
        result.errors.push_back("Environmental: moisture diffusion coefficient cannot be negative (m^2/s).");
    }
    if (environmental.oxidation_activation_energy_j_mol < 0.0) {
        result.errors.push_back("Environmental: oxidation activation energy cannot be negative (J/mol).");
    }

    result.is_valid = result.errors.empty();
    return result;
}

double MaterialPropertyBundle::lorenz_number(double temp_k) const {
    if (temp_k <= 0.0) {
        return 0.0;
    }
    double sigma = electrical.conductivity_at(temp_k);
    double k = thermal.thermal_conductivity_at(temp_k);
    if (sigma <= 0.0 || k <= 0.0) {
        return 0.0;
    }
    return k / (sigma * temp_k);
}

std::string MaterialPropertyBundle::summary() const {
    std::ostringstream oss;
    oss << "=== Material Property Bundle: " << (material_name.empty() ? material_id : material_name) << " ===\n";
    oss << "[Chemical] Formula: " << chemical.chemical_formula 
        << ", MolMass: " << chemical.molecular_weight_g_mol << " g/mol\n";
    oss << "[Mechanical] Density: " << mechanical.density_kg_m3 << " kg/m^3"
        << ", E: " << (mechanical.young_modulus_pa / 1e9) << " GPa"
        << ", Yield: " << (mechanical.yield_strength_pa / 1e6) << " MPa\n";
    oss << "[Thermal] Melting: " << thermal.melting_point_k << " K"
        << ", k(293K): " << thermal.thermal_conductivity_at(293.15) << " W/(m*K)\n";
    oss << "[Electrical] sigma(293K): " << electrical.conductivity_at(293.15) << " S/m"
        << ", rho(293K): " << electrical.resistivity_at(293.15) << " Ohm*m\n";
    oss << "[Environmental] Galvanic standard potential: " 
        << environmental.standard_reduction_potential_v << " V vs SHE\n";
    return oss.str();
}

} // namespace shunya::mat
