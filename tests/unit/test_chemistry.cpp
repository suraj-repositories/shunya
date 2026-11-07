#include "tests/test_framework.hpp"
#include "shunya/core/domain/chemistry.hpp"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

using namespace shunya::core::domain::chem;

// =============================================================================
// CHEM-01: Species & Molar Mass
// =============================================================================

TEST_CASE("CHEM-01: Species Definition and Molar Mass") {
    ChemicalSpecies water{"H2O", "Water", 0.018015, Phase::Liquid, {{"H", 2}, {"O", 1}}};
    ChemicalSpecies methane{"CH4", "Methane", 0.01604, Phase::Gas, {{"C", 1}, {"H", 4}}};

    CHECK(water.formula == "H2O");
    CHECK(water.phase == Phase::Liquid);
    CHECK(water.molar_mass > 0.018);
    CHECK(methane.element_composition.at("H") == 4);
}

// =============================================================================
// CHEM-02: Stoichiometric Balance Validation
// =============================================================================

TEST_CASE("CHEM-02: Reaction Stoichiometric Balance Validation") {
    std::unordered_map<std::string, ChemicalSpecies> species_db;
    species_db["H2"] = ChemicalSpecies{"H2", "Hydrogen", 0.002016, Phase::Gas, {{"H", 2}}};
    species_db["O2"] = ChemicalSpecies{"O2", "Oxygen", 0.032, Phase::Gas, {{"O", 2}}};
    species_db["H2O"] = ChemicalSpecies{"H2O", "Water", 0.018015, Phase::Liquid, {{"H", 2}, {"O", 1}}};

    // Balanced: 2 H2 + 1 O2 -> 2 H2O
    ChemicalReaction balanced_rxn;
    balanced_rxn.id = "combustion";
    balanced_rxn.reactants = {{"H2", 2}, {"O2", 1}};
    balanced_rxn.products = {{"H2O", 2}};

    auto val_res = balanced_rxn.validate_stoichiometric_balance(species_db);
    CHECK(val_res.is_ok());

    // Imbalanced: 1 H2 + 1 O2 -> 1 H2O (missing 1 H)
    ChemicalReaction imbalanced_rxn;
    imbalanced_rxn.id = "bad_combustion";
    imbalanced_rxn.reactants = {{"H2", 1}, {"O2", 1}};
    imbalanced_rxn.products = {{"H2O", 1}};

    auto bad_res = imbalanced_rxn.validate_stoichiometric_balance(species_db);
    CHECK(bad_res.is_err());
}

// =============================================================================
// CHEM-03 & CHEM-04: Batch Reactor Kinetics and Mass Conservation
// =============================================================================

TEST_CASE("CHEM-03 & CHEM-04: Batch Reactor Kinetics and Mass Conservation") {
    BatchReactor reactor(2.0, 298.15); // 2 Liters

    ChemicalSpecies sp_a{"A", "ReactantA", 0.050, Phase::Liquid, {{"A", 1}}};
    ChemicalSpecies sp_b{"B", "ProductB", 0.050, Phase::Liquid, {{"A", 1}}};

    reactor.register_species(sp_a);
    reactor.register_species(sp_b);

    ChemicalReaction rxn;
    rxn.id = "isomerization";
    rxn.reactants = {{"A", 1}};
    rxn.products = {{"B", 1}};
    rxn.rate_constant_forward = 0.5; // s^-1
    rxn.reversible = false;

    reactor.add_reaction(rxn);
    reactor.set_concentration("A", 1.0); // 1.0 mol/L
    reactor.set_concentration("B", 0.0);

    double init_mass = reactor.total_mass_kg();
    CHECK(init_mass > 0.0);

    // Step 10 times with dt = 0.05s
    for (int i = 0; i < 10; ++i) {
        auto step_res = reactor.step(0.05);
        CHECK(step_res.is_ok());
    }

    CHECK(reactor.get_concentration("A") < 1.0);
    CHECK(reactor.get_concentration("B") > 0.0);

    // Mass conservation invariant check
    double final_mass = reactor.total_mass_kg();
    CHECK(std::abs(final_mass - init_mass) < 1e-9);
}

// =============================================================================
// CHEM-05: 1D Reaction-Diffusion Transport
// =============================================================================

TEST_CASE("CHEM-05: 1D Reaction-Diffusion Transport Field") {
    ReactionDiffusion1DConfig cfg;
    cfg.num_nodes = 10;
    cfg.length = 0.1;
    cfg.diffusion_coefficient = 1.0e-5;

    ReactionDiffusion1D diff(cfg, 0.2); // decay rate k = 0.2
    diff.set_boundary_conditions(1.0, 0.0);
    diff.initialize_uniform(0.0);

    for (int i = 0; i < 20; ++i) {
        auto step_res = diff.step(0.01);
        CHECK(step_res.is_ok());
    }

    CHECK(diff.concentration(0) == 1.0);
    CHECK(diff.concentration(9) == 0.0);
    CHECK(diff.concentration(1) > 0.0); // species diffused into domain
}

// =============================================================================
// CHEM-06: Temperature-Dependent Arrhenius Kinetics & Exothermic Heat
// =============================================================================

TEST_CASE("CHEM-06: Arrhenius Kinetics and Exothermic Temperature Rise") {
    ChemicalReaction rxn;
    rxn.activation_energy = 50000.0;       // 50 kJ/mol
    rxn.pre_exponential_factor = 1.0e8;    // A
    rxn.enthalpy_delta = -200000.0;        // Exothermic: -200 kJ/mol

    double k_room = rxn.rate_constant_at_temperature(298.15); // ~298 K
    double k_hot = rxn.rate_constant_at_temperature(350.0);   // ~350 K

    CHECK(k_hot > k_room); // Rate accelerates with temperature

    // Reactor thermal rise
    BatchReactor reactor(1.0, 300.0);
    ChemicalSpecies a{"A", "A", 0.05, Phase::Liquid, {{"A", 1}}};
    ChemicalSpecies b{"B", "B", 0.05, Phase::Liquid, {{"A", 1}}};
    reactor.register_species(a);
    reactor.register_species(b);

    rxn.reactants = {{"A", 1}};
    rxn.products = {{"B", 1}};
    reactor.add_reaction(rxn);
    reactor.set_concentration("A", 2.0);

    auto step_res = reactor.step(0.01);
    CHECK(step_res.is_ok());
    CHECK(reactor.temperature() >= 300.0); // Exothermic temperature rise
}

// =============================================================================
// CHEM-07 & CHEM-08: Positivity Constraints and Reaction Extent
// =============================================================================

TEST_CASE("CHEM-07 & CHEM-08: Positivity Enforcement and Reaction Extent") {
    BatchReactor reactor(1.0, 298.15);
    ChemicalSpecies sp_a{"A", "A", 0.02, Phase::Liquid, {{"A", 1}}};
    ChemicalSpecies sp_b{"B", "B", 0.02, Phase::Liquid, {{"A", 1}}};
    reactor.register_species(sp_a);
    reactor.register_species(sp_b);

    ChemicalReaction rxn;
    rxn.id = "fast_rxn";
    rxn.reactants = {{"A", 1}};
    rxn.products = {{"B", 1}};
    rxn.rate_constant_forward = 1000.0; // Fast rate
    reactor.add_reaction(rxn);

    reactor.set_concentration("A", 0.1);
    reactor.set_concentration("B", 0.0);

    // Large time step: would overshoot negative without positivity constraint
    auto step_res = reactor.step(1.0);
    CHECK(step_res.is_ok());
    CHECK(reactor.get_concentration("A") >= 0.0);
    CHECK(reactor.get_concentration("B") >= 0.0);

    double extent = reactor.reaction_extent("fast_rxn", "B");
    CHECK(extent > 0.0); // B was generated
}

// =============================================================================
// CHEM-09: Analytical Kinetics Benchmark
// =============================================================================

TEST_CASE("CHEM-09: Analytical Reaction Kinetics Validation") {
    // 1st order decay: C(t) = C0 * exp(-k * t)
    double c0 = 2.0;
    double k = 0.1;
    double t = 5.0;
    double expected = 2.0 * std::exp(-0.5);
    CHECK(std::abs(ChemicalValidationBenchmark::analytical_first_order(c0, k, t) - expected) < 1e-9);

    // 2nd order reaction: 1/C = 1/C0 + 2*k*t
    double c2 = ChemicalValidationBenchmark::analytical_second_order(1.0, 0.5, 2.0);
    // 1/C = 1/1.0 + 2 * 0.5 * 2.0 = 1 + 2 = 3 => C = 1/3
    CHECK(std::abs(c2 - (1.0 / 3.0)) < 1e-9);

    // Reversible equilibrium
    double c_eq = ChemicalValidationBenchmark::analytical_reversible_equilibrium(10.0, 2.0, 1.0);
    // C_A_eq = 1.0 * 10 / (2.0 + 1.0) = 10/3
    CHECK(std::abs(c_eq - (10.0 / 3.0)) < 1e-9);
}
