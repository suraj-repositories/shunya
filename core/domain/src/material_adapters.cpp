#include <shunya/core/domain/material_adapters.hpp>

#include <cmath>

namespace shunya::core::domain {

namespace {

base::Error make_adapt_error(std::string msg, int32_t code = 4401) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::Materials,
        .severity = base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

} // namespace

base::Result<ThermalDomainProperties> MaterialDomainAdapter::extract_thermal(
    const MaterialDefinition& mat, const MaterialState& state) {
    ThermalDomainProperties p;

    // 1. Thermal conductivity
    const auto* k_curve = mat.get_curve("thermal_conductivity");
    if (!k_curve) k_curve = mat.get_curve("k");
    if (k_curve) {
        auto eval_res = k_curve->evaluate(state.temperature_k);
        if (eval_res.is_err()) return eval_res.error();
        p.thermal_conductivity = eval_res.value();
    } else {
        auto val = mat.get_scalar_property("thermal_conductivity");
        if (val) p.thermal_conductivity = *val;
        else return make_adapt_error("Missing thermal conductivity for " + mat.name());
    }

    // 2. Specific heat capacity
    const auto* cp_curve = mat.get_curve("specific_heat_capacity");
    if (!cp_curve) cp_curve = mat.get_curve("cp");
    if (cp_curve) {
        auto eval_res = cp_curve->evaluate(state.temperature_k);
        if (eval_res.is_err()) return eval_res.error();
        p.specific_heat_capacity = eval_res.value();
    } else {
        auto val = mat.get_scalar_property("specific_heat_capacity");
        if (val) p.specific_heat_capacity = *val;
        else p.specific_heat_capacity = 1000.0; // fallback
    }

    // 3. Density
    p.density = mat.get_scalar_property("density").value_or(mat.reference_density());

    // 4. Thermal expansion
    p.thermal_expansion_coeff = mat.get_scalar_property("thermal_expansion_coeff").value_or(1.2e-5);

    // 5. Emissivity
    p.emissivity = mat.get_scalar_property("emissivity").value_or(0.8);

    return p;
}

base::Result<MechanicalDomainProperties> MaterialDomainAdapter::extract_mechanical(
    const MaterialDefinition& mat, const MaterialState& state) {
    MechanicalDomainProperties p;

    // 1. Young's modulus
    auto e_val = mat.get_scalar_property("youngs_modulus");
    if (e_val) {
        p.youngs_modulus = *e_val;
    } else {
        return make_adapt_error("Missing Young's modulus for " + mat.name());
    }

    // 2. Poisson's ratio
    auto nu_val = mat.get_scalar_property("poissons_ratio");
    if (nu_val) {
        p.poissons_ratio = *nu_val;
    } else {
        p.poissons_ratio = 0.3;
    }

    // 3. Derived shear and bulk moduli if not explicitly specified
    p.shear_modulus = mat.get_scalar_property("shear_modulus").value_or(
        p.youngs_modulus / (2.0 * (1.0 + p.poissons_ratio))
    );

    double denom = 3.0 * (1.0 - 2.0 * p.poissons_ratio);
    p.bulk_modulus = mat.get_scalar_property("bulk_modulus").value_or(
        (std::abs(denom) > 1e-9) ? (p.youngs_modulus / denom) : p.youngs_modulus
    );

    // 4. Strengths
    p.yield_strength = mat.get_scalar_property("yield_strength").value_or(250.0e6);
    p.tangent_modulus = mat.get_scalar_property("tangent_modulus").value_or(p.youngs_modulus * 0.01);

    // 5. Density
    p.density = mat.get_scalar_property("density").value_or(mat.reference_density());

    (void)state;
    return p;
}

base::Result<ElectricalDomainProperties> MaterialDomainAdapter::extract_electrical(
    const MaterialDefinition& mat, const MaterialState& state) {
    ElectricalDomainProperties p;

    auto sigma_opt = mat.get_scalar_property("electrical_conductivity");
    auto rho_opt = mat.get_scalar_property("electrical_resistivity");

    if (sigma_opt) {
        p.electrical_conductivity = *sigma_opt;
        p.electrical_resistivity = (*sigma_opt > 1e-15) ? (1.0 / *sigma_opt) : 1e12;
    } else if (rho_opt) {
        p.electrical_resistivity = *rho_opt;
        p.electrical_conductivity = (*rho_opt > 1e-15) ? (1.0 / *rho_opt) : 0.0;
    } else {
        return make_adapt_error("Missing electrical conductivity/resistivity for " + mat.name());
    }

    p.relative_permittivity = mat.get_scalar_property("relative_permittivity").value_or(1.0);
    p.relative_permeability = mat.get_scalar_property("relative_permeability").value_or(1.0);
    p.loss_tangent = mat.get_scalar_property("loss_tangent").value_or(0.0);

    (void)state;
    return p;
}

base::Result<FluidDomainProperties> MaterialDomainAdapter::extract_fluid(
    const MaterialDefinition& mat, const MaterialState& state) {
    FluidDomainProperties p;

    auto mu_opt = mat.get_scalar_property("dynamic_viscosity");
    if (mu_opt) {
        p.dynamic_viscosity = *mu_opt;
    } else {
        return make_adapt_error("Missing dynamic viscosity for " + mat.name());
    }

    p.density = mat.get_scalar_property("density").value_or(mat.reference_density());
    p.kinematic_viscosity = (p.density > 1e-6) ? (p.dynamic_viscosity / p.density) : 1e-6;
    p.bulk_modulus = mat.get_scalar_property("bulk_modulus").value_or(2.2e9);
    p.speed_of_sound = std::sqrt(p.bulk_modulus / p.density);

    (void)state;
    return p;
}

base::Result<OpticalDomainProperties> MaterialDomainAdapter::extract_optical(
    const MaterialDefinition& mat, const MaterialState& state) {
    OpticalDomainProperties p;

    p.refractive_index_real = mat.get_scalar_property("refractive_index_real").value_or(1.5);
    p.refractive_index_imag = mat.get_scalar_property("refractive_index_imag").value_or(0.0);
    p.absorption_coeff = mat.get_scalar_property("absorption_coefficient").value_or(0.0);

    // Fresnel normal incidence reflectivity: R = ((n - 1)^2 + k^2) / ((n + 1)^2 + k^2)
    double n = p.refractive_index_real;
    double k = p.refractive_index_imag;
    double num = (n - 1.0) * (n - 1.0) + k * k;
    double den = (n + 1.0) * (n + 1.0) + k * k;
    p.reflectivity = (den > 1e-12) ? (num / den) : 0.0;

    (void)state;
    return p;
}

} // namespace shunya::core::domain
