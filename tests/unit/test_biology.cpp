#include "tests/test_framework.hpp"
#include "shunya/core/domain/biology.hpp"

#include <cmath>
#include <vector>

using namespace shunya::core::domain::bio;

// =============================================================================
// BIO-01 & BIO-02: Cell Agent Lifecycle and Cycle Phases
// =============================================================================

TEST_CASE("BIO-01 & BIO-02: Cell Agent State and Cell Cycle Progression") {
    BioSimConfig cfg;
    cfg.domain_width = 1.0;
    cfg.domain_height = 1.0;
    cfg.mu_max = 1.0;
    cfg.k_s = 0.1;
    cfg.division_mass = 2.0;
    cfg.death_rate = 0.0;
    cfg.random_seed = 12345;

    CellColonySimulator sim(cfg);
    sim.add_cell(0.5, 0.5, 1.0); // Starts in G1
    REQUIRE(sim.cells().size() == 1u);
    CHECK(sim.cells()[0].phase == CellCyclePhase::G1);
    CHECK(sim.cells()[0].mass == 1.0);

    // Grow cell
    sim.set_nutrient_level(1.0);
    for (int i = 0; i < 5; ++i) {
        auto step_res = sim.step(0.1);
        CHECK(step_res.is_ok());
    }

    CHECK(sim.cells()[0].mass > 1.0);
    CHECK(sim.cells()[0].phase == CellCyclePhase::S || sim.cells()[0].phase == CellCyclePhase::G2);
}

// =============================================================================
// BIO-03 & BIO-04: Mitosis Division and Nutrient Coupling
// =============================================================================

TEST_CASE("BIO-03 & BIO-04: Cell Mitosis Division and Substrate Depletion") {
    BioSimConfig cfg;
    cfg.division_mass = 2.0;
    cfg.mu_max = 2.0;
    cfg.death_rate = 0.0;
    cfg.random_seed = 999;

    CellColonySimulator sim(cfg);
    sim.set_nutrient_level(2.0); // Rich broth
    sim.add_cell(0.5, 0.5, 1.95); // Near division mass

    double init_substrate = sim.nutrient_level();

    // Step to trigger division
    auto step_res = sim.step(0.1);
    CHECK(step_res.is_ok());

    // 1 cell should have divided into 2
    CHECK(sim.cells().size() == 2u);
    CHECK(sim.cells()[0].mass < 2.0); // Shared mass
    CHECK(sim.cells()[1].mass < 2.0);

    // Substrate should have been consumed
    CHECK(sim.nutrient_level() < init_substrate);
}

// =============================================================================
// BIO-05: Deterministic PRNG Reproducibility
// =============================================================================

TEST_CASE("BIO-05: Reproducible Stochastic Simulation with Random Seed") {
    BioSimConfig cfg;
    cfg.random_seed = 777;
    cfg.mu_max = 0.5;
    cfg.death_rate = 0.05;

    CellColonySimulator sim1(cfg);
    sim1.add_cell(0.2, 0.3, 1.0);
    sim1.add_cell(0.7, 0.8, 1.0);

    for (int i = 0; i < 10; ++i) {
        auto res1 = sim1.step(0.1);
        CHECK(res1.is_ok());
    }

    CellColonySimulator sim2(cfg);
    sim2.add_cell(0.2, 0.3, 1.0);
    sim2.add_cell(0.7, 0.8, 1.0);

    for (int i = 0; i < 10; ++i) {
        auto res2 = sim2.step(0.1);
        CHECK(res2.is_ok());
    }

    // Both runs must yield identical state
    REQUIRE(sim1.cells().size() == sim2.cells().size());
    for (size_t i = 0; i < sim1.cells().size(); ++i) {
        CHECK(sim1.cells()[i].x == sim2.cells()[i].x);
        CHECK(sim1.cells()[i].y == sim2.cells()[i].y);
        CHECK(sim1.cells()[i].mass == sim2.cells()[i].mass);
        CHECK(sim1.cells()[i].is_alive == sim2.cells()[i].is_alive);
    }
}

// =============================================================================
// BIO-06: Population Metrics
// =============================================================================

TEST_CASE("BIO-06: Population-Level Summary Metrics") {
    BioSimConfig cfg;
    CellColonySimulator sim(cfg);

    sim.add_cell(0.1, 0.1, 1.2);
    sim.add_cell(0.2, 0.2, 1.4);

    auto metrics = sim.calculate_metrics();
    CHECK(metrics.live_count == 2u);
    CHECK(metrics.dead_count == 0u);
    CHECK(std::abs(metrics.total_biomass - 2.6) < 1e-6);
    CHECK(std::abs(metrics.mean_cell_mass - 1.3) < 1e-6);
}

// =============================================================================
// BIO-07: Validation against Analytical Logistic Growth & Lotka-Volterra
// =============================================================================

TEST_CASE("BIO-07: Population Kinetics Benchmarks") {
    // 1. Logistic Growth: N0 = 10, K = 1000, r = 0.5
    double n0 = 10.0;
    double k = 1000.0;
    double r = 0.5;

    // At t = 0: N(0) = N0
    double n_0 = PopulationValidationBenchmark::analytical_logistic_growth(n0, k, r, 0.0);
    CHECK(std::abs(n_0 - n0) < 1e-6);

    // At large t -> N(t) approaches K
    double n_inf = PopulationValidationBenchmark::analytical_logistic_growth(n0, k, r, 100.0);
    CHECK(std::abs(n_inf - k) < 1e-3);

    // 2. Lotka-Volterra Predator-Prey Step
    PopulationValidationBenchmark::LotkaVolterraParams lv;
    lv.alpha = 1.0;
    lv.beta = 0.1;
    lv.delta = 0.05;
    lv.gamma = 0.5;

    double prey = 10.0;
    double pred = 5.0;

    auto [next_prey, next_pred] = PopulationValidationBenchmark::lotka_volterra_step(prey, pred, lv, 0.01);
    CHECK(next_prey > 0.0);
    CHECK(next_pred > 0.0);
}
