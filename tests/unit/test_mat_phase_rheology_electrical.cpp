#include "tests/test_framework.hpp"

#include "shunya/mat/electrical_properties.hpp"
#include "shunya/mat/phase_change_enthalpy.hpp"
#include "shunya/mat/viscosity_evaluator.hpp"

#include <cmath>

using namespace shunya::mat;

TEST_CASE("PhaseChangeEnthalpyModel: mushy-zone latent heat recovery and Neumann Stefan analytical front") {
    auto water = PhaseChangeEnthalpyModel::water_ice();
    const auto& p = water.parameters();

    // Integrating excess apparent Cp over [Ts, Tl] recovers L_f = 334000 J/kg
    const int n = 1000;
    const double dt = (p.liquidus_temp_k - p.solidus_temp_k) / static_cast<double>(n);
    double integral_cp_excess = 0.0;
    for (int i = 0; i < n; ++i) {
        const double t = p.solidus_temp_k + (static_cast<double>(i) + 0.5) * dt;
        const double fl = water.liquid_fraction(t);
        const double cp_base = (1.0 - fl) * p.cp_solid_j_kg_k + fl * p.cp_liquid_j_kg_k;
        integral_cp_excess += (water.apparent_cp_j_kg_k(t) - cp_base) * dt;
    }
    REQUIRE(std::abs(integral_cp_excess - p.latent_heat_fusion_j_kg) / p.latent_heat_fusion_j_kg < 1e-4);

    // Stefan solidification of water at -10 degC (263.15 K) grows monotonically with sqrt(t) (~3.77 mm at 100 s)
    const double s_100 = water.neumann_solidification_depth_m(263.15, 100.0);
    const double s_400 = water.neumann_solidification_depth_m(263.15, 400.0);
    REQUIRE(s_100 > 0.003 && s_100 < 0.005);
    REQUIRE(std::abs(s_400 - 2.0 * s_100) < 1e-7);

    REQUIRE(water.specific_enthalpy_j_kg(280.0, 265.0) > p.latent_heat_fusion_j_kg);
}

TEST_CASE("ViscosityEvaluator: liquid water < 1.0% error 0..100 degC, SAE 10W-30, and Ga Andrade") {
    auto water = ViscosityEvaluator::liquid_water();
    // 0 degC = 1.787 mPa*s, 20 degC = 1.002 mPa*s, 100 degC = 0.282 mPa*s
    const double mu_0   = water.base_viscosity_pa_s(273.15) * 1000.0;
    const double mu_20  = water.base_viscosity_pa_s(293.15) * 1000.0;
    const double mu_100 = water.base_viscosity_pa_s(373.15) * 1000.0;
    REQUIRE(100.0 * std::abs(mu_0 - 1.787) / 1.787 < 1.0);
    REQUIRE(100.0 * std::abs(mu_20 - 1.002) / 1.002 < 1.0);
    REQUIRE(100.0 * std::abs(mu_100 - 0.282) / 0.282 < 1.0);

    // SAE 10W-30 oil shear-thinning decreases viscosity at higher shear rates
    auto oil = ViscosityEvaluator::sae_10w30_oil();
    REQUIRE(oil.effective_viscosity_pa_s(310.0, 1000.0) < oil.effective_viscosity_pa_s(310.0, 100.0));

    // Molten Gallium strictly decreases in viscosity with temperature
    auto ga = ViscosityEvaluator::liquid_gallium();
    REQUIRE(ga.base_viscosity_pa_s(310.0) > ga.base_viscosity_pa_s(500.0));
}

TEST_CASE("ElectricalPropertyEvaluator: Si Varshni bandgap, n_i(T) scaling, and Cu Joule self-heating") {
    const auto si = ElectricalPropertyEvaluator::silicon();
    REQUIRE(std::abs(ElectricalPropertyEvaluator::varshni_bandgap_ev(si, 0.0) - 1.17) < 0.005);
    REQUIRE(std::abs(ElectricalPropertyEvaluator::varshni_bandgap_ev(si, 300.0) - 1.124) < 0.005);

    const double ni_300 = ElectricalPropertyEvaluator::intrinsic_carrier_cm3(si, 300.0);
    const double ni_400 = ElectricalPropertyEvaluator::intrinsic_carrier_cm3(si, 400.0);
    REQUIRE(ni_300 > 5e9 && ni_300 < 2e10);
    REQUIRE(ni_400 > ni_300 * 100.0);

    // Copper positive TCR increases temperature rapidly at insufficient ambient cooling (thermal runaway trend)
    const auto cu = ElectricalPropertyEvaluator::copper();
    REQUIRE(ElectricalPropertyEvaluator::metal_resistivity_ohm_m(cu, 400.0) >
            ElectricalPropertyEvaluator::metal_resistivity_ohm_m(cu, 293.15));
    double temp = 293.15;
    for (int step = 0; step < 50; ++step) {
        temp = ElectricalPropertyEvaluator::step_resistor_temperature_k(
            cu, temp, 150.0, 1.0, 1e-6, 5.0, 0.05, 293.15, 0.1);
    }
    REQUIRE(temp > 500.0);
}
