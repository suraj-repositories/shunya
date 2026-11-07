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
#include <unordered_map>
#include <vector>

namespace shunya::core::domain::chem {

// =============================================================================
// CHEM-01: Chemical Species, Phase, and Molar Mass
// =============================================================================

enum class Phase {
    Gas,
    Liquid,
    Solid,
    Aqueous
};

struct ChemicalSpecies {
    std::string formula;       // e.g. "H2O", "O2", "CH4", "CO2"
    std::string name;          // e.g. "Water", "Oxygen"
    double molar_mass{0.018};  // kg / mol (e.g. H2O = 0.018015 kg/mol)
    Phase phase{Phase::Liquid};
    std::unordered_map<std::string, int> element_composition; // e.g. {"H": 2, "O": 1}
};

// =============================================================================
// CHEM-02: Stoichiometric Reaction & Balance Validation
// =============================================================================

struct ReactionComponent {
    std::string species_formula;
    int stoichiometric_coefficient{1}; // positive integer (e.g. 2 for 2H2)
};

struct ChemicalReaction {
    std::string id;
    std::vector<ReactionComponent> reactants;
    std::vector<ReactionComponent> products;
    bool reversible{false};

    // Forward and reverse rate parameters
    double rate_constant_forward{1.0};  // k_f
    double rate_constant_reverse{0.0};  // k_r
    double activation_energy{0.0};      // J / mol (Arrhenius Ea)
    double pre_exponential_factor{1.0}; // A in k = A * exp(-Ea / RT)
    double enthalpy_delta{0.0};         // J / mol (negative for exothermic)

    [[nodiscard]] double rate_constant_at_temperature(double temp_kelvin) const noexcept;
    [[nodiscard]] base::Result<void> validate_stoichiometric_balance(
        const std::unordered_map<std::string, ChemicalSpecies>& species_db) const;
};

// =============================================================================
// CHEM-03 & CHEM-04: Batch Reactor & Well-Mixed Chemical Kinetics
// =============================================================================

class BatchReactor {
public:
    explicit BatchReactor(double volume_liters = 1.0, double temperature_kelvin = 298.15)
        : volume_(volume_liters), temperature_(temperature_kelvin) {}

    void register_species(const ChemicalSpecies& species);
    void add_reaction(ChemicalReaction rxn);

    void set_concentration(const std::string& formula, double molarity_mol_per_l);
    [[nodiscard]] double get_concentration(const std::string& formula) const;

    [[nodiscard]] double temperature() const noexcept { return temperature_; }
    void set_temperature(double temp_kelvin) noexcept { temperature_ = temp_kelvin; }

    [[nodiscard]] double volume() const noexcept { return volume_; }

    // Evaluates reaction rate r_j = k_f * prod(C_reactants^nu) - k_r * prod(C_products^nu)
    [[nodiscard]] double evaluate_reaction_rate(const ChemicalReaction& rxn) const;

    // Advances the reactor by time step dt using Runge-Kutta 4th order (RK4) integration
    [[nodiscard]] base::Result<void> step(double dt);

    // Calculates reaction extent xi = (n_i - n_i0) / nu_i
    [[nodiscard]] double reaction_extent(const std::string& rxn_id, const std::string& species_formula) const;

    // Total mass in reactor = sum(C_i * V * M_w)
    [[nodiscard]] double total_mass_kg() const;

private:
    double volume_{1.0}; // Liters
    double temperature_{298.15}; // Kelvin
    std::unordered_map<std::string, ChemicalSpecies> species_db_;
    std::unordered_map<std::string, double> concentrations_;    // mol / L
    std::unordered_map<std::string, double> initial_moles_;     // mol
    std::vector<ChemicalReaction> reactions_;

    void compute_derivatives(
        const std::unordered_map<std::string, double>& conc,
        std::unordered_map<std::string, double>& dC_dt,
        double& dT_dt) const;
};

// =============================================================================
// CHEM-05: 1D Reaction-Diffusion Transport
// =============================================================================

struct ReactionDiffusion1DConfig {
    size_t num_nodes{20};
    double length{0.1}; // 10 cm
    double diffusion_coefficient{1.0e-9}; // m^2 / s
};

class ReactionDiffusion1D {
public:
    explicit ReactionDiffusion1D(ReactionDiffusion1DConfig config, double k_decay = 0.1);

    void set_boundary_conditions(double c_left, double c_right);
    void initialize_uniform(double initial_c);

    // Advances 1D diffusion-reaction: dC/dt = D * d^2 C / dx^2 - k * C
    [[nodiscard]] base::Result<void> step(double dt);

    [[nodiscard]] double concentration(size_t node_idx) const;
    [[nodiscard]] const std::vector<double>& field() const noexcept { return c_; }

private:
    ReactionDiffusion1DConfig config_;
    double k_decay_{0.1};
    std::vector<double> c_;
    double c_left_{1.0};
    double c_right_{0.0};
};

// =============================================================================
// CHEM-09: Analytical Kinetics Benchmark
// =============================================================================

class ChemicalValidationBenchmark {
public:
    // 1st order irreversible decay A -> B: C_A(t) = C_A0 * exp(-k * t)
    static double analytical_first_order(double c_a0, double k, double t) noexcept;

    // 2nd order reaction 2A -> B: 1/C_A(t) = 1/C_A0 + 2*k*t
    static double analytical_second_order(double c_a0, double k, double t) noexcept;

    // Reversible reaction A <-> B: C_A_eq = (k_r * C_total) / (k_f + k_r)
    static double analytical_reversible_equilibrium(double c_total, double k_f, double k_r) noexcept;
};

} // namespace shunya::core::domain::chem
