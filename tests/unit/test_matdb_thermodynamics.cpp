#include "tests/test_framework.hpp"

#include "shunya/matdb/shomate_evaluator.hpp"
#include "shunya/matdb/thermo_registry.hpp"

#include <cmath>

using namespace shunya::matdb;

TEST_CASE("ThermodynamicStateRegistry: reference state zeros, graphite vs diamond Delta_f H, and JSON export") {
    auto audit = ThermodynamicStateRegistry::audit_reference_states();
    REQUIRE(audit.is_valid());

    for (const char* ref_id : {"O2(g)", "N2(g)", "H2(g)", "Fe(s)", "Cu(s)", "C(graphite)"}) {
        const auto* rec = ThermodynamicStateRegistry::find_by_id(ref_id);
        REQUIRE(rec != nullptr);
        REQUIRE(rec->is_reference_state);
        REQUIRE(std::abs(rec->delta_f_h_298_kj_mol) < 1e-9);
        REQUIRE(std::abs(rec->delta_f_g_298_kj_mol) < 1e-9);
        REQUIRE(rec->s0_298_j_mol_k > 0.0);
    }

    const auto* gra = ThermodynamicStateRegistry::find_by_id("C(graphite)");
    const auto* dia = ThermodynamicStateRegistry::find_by_id("C(diamond)");
    REQUIRE(gra != nullptr);
    REQUIRE(dia != nullptr);
    REQUIRE(std::abs((dia->delta_f_h_298_kj_mol - gra->delta_f_h_298_kj_mol) - 1.895) < 1e-6);
    REQUIRE(dia->delta_f_g_298_kj_mol > 0.0);
    REQUIRE(!ThermodynamicStateRegistry::to_json().empty());
}

TEST_CASE("ShomateEvaluator: NIST heat capacity accuracy (<0.25% error) and segment continuity up to 6000 K") {
    auto continuity = ShomateEvaluator::audit_segment_continuity(0.5);
    REQUIRE(continuity.is_valid());

    // N2(g) at 298.15 K (~29.12 J/(mol*K)), 1000 K (~32.70 J/(mol*K)), 5000 K (~38.13 J/(mol*K))
    auto n2_298 = ShomateEvaluator::evaluate_cp_j_mol_k("N2(g)", 298.15);
    auto n2_1000 = ShomateEvaluator::evaluate_cp_j_mol_k("N2(g)", 1000.0);
    auto n2_5000 = ShomateEvaluator::evaluate_cp_j_mol_k("N2(g)", 5000.0);
    REQUIRE(n2_298.is_ok());
    REQUIRE(n2_1000.is_ok());
    REQUIRE(n2_5000.is_ok());
    REQUIRE(100.0 * std::abs(n2_298.value() - 29.124) / 29.124 < 0.25);
    REQUIRE(100.0 * std::abs(n2_1000.value() - 32.696) / 32.696 < 0.25);

    // Enthalpy H(298.15 K) - H(298.15 K) ~ 0.0 kJ/mol
    auto dh_298 = ShomateEvaluator::evaluate_delta_h_kj_mol("N2(g)", 298.15);
    REQUIRE(dh_298.is_ok());
    REQUIRE(std::abs(dh_298.value()) < 0.05);

    // O2(g) at 298.15 K (~29.38 J/(mol*K))
    auto o2_298 = ShomateEvaluator::evaluate_cp_j_mol_k("O2(g)", 298.15);
    REQUIRE(o2_298.is_ok());
    REQUIRE(100.0 * std::abs(o2_298.value() - 29.378) / 29.378 < 0.25);

    // Cu(s) at 298.15 K (~24.44 J/(mol*K)) & 1000 K (~28.62 J/(mol*K))
    auto cu_298 = ShomateEvaluator::evaluate_cp_j_mol_k("Cu(s/l)", 298.15);
    auto cu_1000 = ShomateEvaluator::evaluate_cp_j_mol_k("Cu(s/l)", 1000.0);
    REQUIRE(cu_298.is_ok());
    REQUIRE(cu_1000.is_ok());
    REQUIRE(100.0 * std::abs(cu_298.value() - 24.44) / 24.44 < 0.25);
    REQUIRE(100.0 * std::abs(cu_1000.value() - 28.66) / 28.66 < 0.25);

    // Specific heat for Al(s) at 298.15 K (~900 J/(kg*K))
    auto al_cp_mass = ShomateEvaluator::evaluate_specific_heat_j_kg_k("Al(s/l)", 298.15);
    REQUIRE(al_cp_mass.is_ok());
    REQUIRE(std::abs(al_cp_mass.value() - 897.0) < 5.0);

    // Fe(s) entropy S(298.15 K) ~ 27.28 J/(mol*K)
    auto fe_s = ShomateEvaluator::evaluate_entropy_j_mol_k("Fe(s/l)", 298.15);
    REQUIRE(fe_s.is_ok());
    REQUIRE(std::abs(fe_s.value() - 27.285) < 0.2);

    // Invalid bounds rejected
    REQUIRE(ShomateEvaluator::evaluate_cp_j_mol_k("N2(g)", 10000.0).is_err());
    REQUIRE(!ShomateEvaluator::to_json().empty());
}
