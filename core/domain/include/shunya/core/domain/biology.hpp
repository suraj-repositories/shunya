#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::core::domain::bio {

// =============================================================================
// BIO-01 & BIO-02: Cell Agent State, Cell Cycle & Compartments
// =============================================================================

enum class CellCyclePhase {
    G1, // Growth
    S,  // DNA synthesis
    G2, // Preparation for division
    M,  // Mitosis / Division
    G0, // Quiescent / Arrested
    Apoptotic // Dying
};

struct CellAgent {
    uint64_t id{0};
    uint64_t lineage_id{0};
    double x{0.0};
    double y{0.0};
    double mass{1.0};           // Normalized mass (divides at ~2.0)
    double internal_energy{1.0};
    CellCyclePhase phase{CellCyclePhase::G1};
    double age{0.0};
    bool is_alive{true};
};

struct PopulationMetrics {
    size_t live_count{0};
    size_t dead_count{0};
    double total_biomass{0.0};
    double mean_cell_mass{0.0};
};

// =============================================================================
// BIO-03 & BIO-04 & BIO-05: Cellular Colony Simulator (Agent-Based + Nutrient Field)
// =============================================================================

struct BioSimConfig {
    double domain_width{1.0};
    double domain_height{1.0};
    double mu_max{0.5};          // Maximum specific growth rate (1/s)
    double k_s{0.1};             // Monod substrate affinity constant (g/L)
    double division_mass{2.0};   // Mass threshold for cell division
    double death_rate{0.01};     // Base death rate
    uint64_t random_seed{42};    // Seed for reproducible stochastic runs
};

class CellColonySimulator {
public:
    explicit CellColonySimulator(BioSimConfig config);

    void add_cell(double x, double y, double mass = 1.0);
    [[nodiscard]] const std::vector<CellAgent>& cells() const noexcept { return cells_; }

    void set_nutrient_level(double substrate_conc) noexcept { substrate_ = substrate_conc; }
    [[nodiscard]] double nutrient_level() const noexcept { return substrate_; }

    // Advances cell population by time step dt
    [[nodiscard]] base::Result<void> step(double dt);

    [[nodiscard]] PopulationMetrics calculate_metrics() const noexcept;
    void reset(uint64_t new_seed) noexcept;

private:
    BioSimConfig config_;
    double substrate_{1.0}; // Environmental nutrient level (g/L)
    std::vector<CellAgent> cells_;
    uint64_t next_cell_id_{1};
    uint64_t rng_state_{42};

    // Deterministic xorshift64* PRNG
    uint64_t next_random();
    double next_uniform(); // [0.0, 1.0)
};

// =============================================================================
// BIO-07: Population Kinetics Benchmarks (Logistic & Lotka-Volterra)
// =============================================================================

class PopulationValidationBenchmark {
public:
    // Logistic growth: N(t) = K / (1 + ((K - N0)/N0) * exp(-r * t))
    static double analytical_logistic_growth(double n0, double k_capacity, double r_growth, double t) noexcept;

    // Lotka-Volterra Predator-Prey derivatives:
    // dx/dt = alpha * x - beta * x * y (Prey)
    // dy/dt = delta * x * y - gamma * y (Predator)
    struct LotkaVolterraParams {
        double alpha{1.0}; // Prey birth rate
        double beta{0.1};  // Predation rate
        double delta{0.05}; // Predator growth rate
        double gamma{0.5}; // Predator mortality
    };

    static std::pair<double, double> lotka_volterra_step(
        double prey,
        double predator,
        const LotkaVolterraParams& params,
        double dt) noexcept;
};

} // namespace shunya::core::domain::bio
