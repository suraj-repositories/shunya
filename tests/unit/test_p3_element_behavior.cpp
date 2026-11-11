#include "tests/test_framework.hpp"

#include "shunya/mat/element_behavior.hpp"

#include <cmath>

using namespace shunya::mat;

TEST_CASE("MATBEH: Phase transitions, Clausius-Clapeyron boiling, and supercritical state") {
    // Copper at STP -> Solid
    auto cu_stp = ElementBehaviorEngine::evaluate_by_symbol("Cu", 293.15, 101325.0);
    REQUIRE(cu_stp.is_ok());
    CHECK(cu_stp.value().phase == EvaluatedPhase::Solid);
    REQUIRE(cu_stp.value().density_kg_m3.available());
    CHECK(std::abs(*cu_stp.value().density_kg_m3.value - 8920.0) < 1e-6);

    // Copper at 1500 K -> Liquid (MP = 1357.77 K, BP = 3200.15 K)
    auto cu_liq = ElementBehaviorEngine::evaluate_by_symbol("Cu", 1500.0, 101325.0);
    REQUIRE(cu_liq.is_ok());
    CHECK(cu_liq.value().phase == EvaluatedPhase::Liquid);
    CHECK(!cu_liq.value().warnings.empty()); // melted from STP solid

    // Hydrogen at high T and high P -> Supercritical (Tc = 32.97 K, Pc = 1.293e6 Pa)
    auto h_sc = ElementBehaviorEngine::evaluate_by_symbol("H", 100.0, 5.0e6);
    REQUIRE(h_sc.is_ok());
    CHECK(h_sc.value().phase == EvaluatedPhase::Supercritical);

    // Hydrogen at STP -> Diatomic ideal gas density ~ 0.083 kg/m^3 at 293.15 K
    auto h_gas = ElementBehaviorEngine::evaluate_by_symbol("H", 293.15, 101325.0);
    REQUIRE(h_gas.is_ok());
    CHECK(h_gas.value().phase == EvaluatedPhase::Gas);
    REQUIRE(h_gas.value().density_kg_m3.available());
    CHECK(*h_gas.value().density_kg_m3.value > 0.05);
    CHECK(*h_gas.value().density_kg_m3.value < 0.12);
}

TEST_CASE("MATBEH: Superconductivity, TCR resistivity, magnetic transitions & combustion hazards") {
    // Aluminum superconducting below 1.175 K
    auto al_sc = ElementBehaviorEngine::evaluate_by_symbol("Al", 1.0, 101325.0);
    REQUIRE(al_sc.is_ok());
    CHECK(al_sc.value().is_superconducting);
    CHECK(al_sc.value().electrical_resistivity_ohm_m.value == 0.0);

    // Copper resistivity increases with temperature
    auto cu_cold = ElementBehaviorEngine::evaluate_by_symbol("Cu", 293.15, 101325.0);
    auto cu_hot = ElementBehaviorEngine::evaluate_by_symbol("Cu", 500.0, 101325.0);
    REQUIRE(cu_cold.is_ok());
    REQUIRE(cu_hot.is_ok());
    CHECK(*cu_hot.value().electrical_resistivity_ohm_m.value >
          *cu_cold.value().electrical_resistivity_ohm_m.value);

    // Iron above Curie point (1043 K) transitions from Ferromagnetic to Paramagnetic
    auto fe_cool = ElementBehaviorEngine::evaluate_by_symbol("Fe", 300.0, 101325.0);
    auto fe_hot = ElementBehaviorEngine::evaluate_by_symbol("Fe", 1200.0, 101325.0);
    REQUIRE(fe_cool.is_ok());
    REQUIRE(fe_hot.is_ok());
    CHECK(fe_cool.value().magnetic_state == "Ferromagnetic");
    CHECK(fe_hot.value().magnetic_state.find("Paramagnetic") != std::string::npos);

    // Lithium combustion risk above Autoignition_Point (452.15 K) in oxidizing atmosphere
    auto li_burn = ElementBehaviorEngine::evaluate_by_symbol("Li", 500.0, 101325.0, true);
    REQUIRE(li_burn.is_ok());
    CHECK(li_burn.value().combustion_risk);
    CHECK_FALSE(li_burn.value().stable_at_conditions);

    // In inert atmosphere, combustion risk is false
    auto li_inert = ElementBehaviorEngine::evaluate_by_symbol("Li", 500.0, 101325.0, false);
    REQUIRE(li_inert.is_ok());
    CHECK_FALSE(li_inert.value().combustion_risk);

    // Noble gas Neon is inert
    auto ne = ElementBehaviorEngine::evaluate_by_symbol("Ne", 300.0, 101325.0);
    REQUIRE(ne.is_ok());
    CHECK(ne.value().reactivity_index < 0.1);
}

TEST_CASE("MATBEH: Unsupported properties are reported transparently without fabrication") {
    auto og = ElementBehaviorEngine::evaluate_by_symbol("Og", 300.0, 101325.0);
    REQUIRE(og.is_ok());
    CHECK_FALSE(og.value().thermal_conductivity_w_mk.available());
    CHECK_FALSE(og.value().electrical_conductivity_s_m.available());
    CHECK(!og.value().unsupported_properties.empty());

    // Invalid symbol or invalid T/P
    CHECK(ElementBehaviorEngine::evaluate_by_symbol("Xx", 300.0, 101325.0).is_err());
    auto bad_t = ElementBehaviorEngine::evaluate_by_symbol("Cu", -5.0, 101325.0);
    REQUIRE(bad_t.is_ok());
    CHECK_FALSE(bad_t.value().stable_at_conditions);
}
