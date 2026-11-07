#include "shunya/core/domain/chemistry.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::core::domain::chem {

// =============================================================================
// ChemicalReaction Implementation
// =============================================================================

double ChemicalReaction::rate_constant_at_temperature(double temp_kelvin) const noexcept {
    if (activation_energy > 0.0 && temp_kelvin > 0.0) {
        constexpr double R_GAS = 8.314462618; // J / (mol * K)
        return pre_exponential_factor * std::exp(-activation_energy / (R_GAS * temp_kelvin));
    }
    return rate_constant_forward;
}

base::Result<void> ChemicalReaction::validate_stoichiometric_balance(
    const std::unordered_map<std::string, ChemicalSpecies>& species_db) const {
    std::unordered_map<std::string, int> reactant_elements;
    std::unordered_map<std::string, int> product_elements;

    for (const auto& r : reactants) {
        auto it = species_db.find(r.species_formula);
        if (it == species_db.end()) {
            return base::Result<void>::err(base::Error{
                .code = 1,
                .domain = base::ErrorDomain::Simulation,
                .message = "Unknown reactant species: " + r.species_formula,
                .details = {}
            });
        }
        for (const auto& [elem, count] : it->second.element_composition) {
            reactant_elements[elem] += r.stoichiometric_coefficient * count;
        }
    }

    for (const auto& p : products) {
        auto it = species_db.find(p.species_formula);
        if (it == species_db.end()) {
            return base::Result<void>::err(base::Error{
                .code = 2,
                .domain = base::ErrorDomain::Simulation,
                .message = "Unknown product species: " + p.species_formula,
                .details = {}
            });
        }
        for (const auto& [elem, count] : it->second.element_composition) {
            product_elements[elem] += p.stoichiometric_coefficient * count;
        }
    }

    // Verify all reactant elements match product elements
    for (const auto& [elem, count] : reactant_elements) {
        auto it = product_elements.find(elem);
        if (it == product_elements.end() || it->second != count) {
            return base::Result<void>::err(base::Error{
                .code = 3,
                .domain = base::ErrorDomain::Simulation,
                .message = "Stoichiometric imbalance for element: " + elem,
                .details = {}
            });
        }
    }

    for (const auto& [elem, count] : product_elements) {
        auto it = reactant_elements.find(elem);
        if (it == reactant_elements.end() || it->second != count) {
            return base::Result<void>::err(base::Error{
                .code = 4,
                .domain = base::ErrorDomain::Simulation,
                .message = "Stoichiometric imbalance for element: " + elem,
                .details = {}
            });
        }
    }

    return base::Result<void>::ok();
}

// =============================================================================
// BatchReactor Implementation
// =============================================================================

void BatchReactor::register_species(const ChemicalSpecies& species) {
    species_db_[species.formula] = species;
    if (concentrations_.find(species.formula) == concentrations_.end()) {
        concentrations_[species.formula] = 0.0;
        initial_moles_[species.formula] = 0.0;
    }
}

void BatchReactor::add_reaction(ChemicalReaction rxn) {
    reactions_.push_back(std::move(rxn));
}

void BatchReactor::set_concentration(const std::string& formula, double molarity_mol_per_l) {
    concentrations_[formula] = molarity_mol_per_l;
    initial_moles_[formula] = molarity_mol_per_l * volume_;
}

double BatchReactor::get_concentration(const std::string& formula) const {
    auto it = concentrations_.find(formula);
    if (it != concentrations_.end()) {
        return it->second;
    }
    return 0.0;
}

double BatchReactor::evaluate_reaction_rate(const ChemicalReaction& rxn) const {
    double k_f = rxn.rate_constant_at_temperature(temperature_);
    double rate_forward = k_f;

    for (const auto& r : rxn.reactants) {
        double c = get_concentration(r.species_formula);
        rate_forward *= std::pow(std::max(0.0, c), r.stoichiometric_coefficient);
    }

    double rate_reverse = 0.0;
    if (rxn.reversible && rxn.rate_constant_reverse > 0.0) {
        rate_reverse = rxn.rate_constant_reverse;
        for (const auto& p : rxn.products) {
            double c = get_concentration(p.species_formula);
            rate_reverse *= std::pow(std::max(0.0, c), p.stoichiometric_coefficient);
        }
    }

    return rate_forward - rate_reverse;
}

void BatchReactor::compute_derivatives(
    const std::unordered_map<std::string, double>& conc,
    std::unordered_map<std::string, double>& dC_dt,
    double& dT_dt) const {
    for (const auto& [formula, _] : conc) {
        dC_dt[formula] = 0.0;
    }
    dT_dt = 0.0;

    for (const auto& rxn : reactions_) {
        double k_f = rxn.rate_constant_at_temperature(temperature_);
        double r_fwd = k_f;
        for (const auto& r : rxn.reactants) {
            auto it = conc.find(r.species_formula);
            double c = (it != conc.end()) ? it->second : 0.0;
            r_fwd *= std::pow(std::max(0.0, c), r.stoichiometric_coefficient);
        }

        double r_rev = 0.0;
        if (rxn.reversible && rxn.rate_constant_reverse > 0.0) {
            r_rev = rxn.rate_constant_reverse;
            for (const auto& p : rxn.products) {
                auto it = conc.find(p.species_formula);
                double c = (it != conc.end()) ? it->second : 0.0;
                r_rev *= std::pow(std::max(0.0, c), p.stoichiometric_coefficient);
            }
        }

        double net_rate = r_fwd - r_rev;

        // Reactants consumed
        for (const auto& r : rxn.reactants) {
            dC_dt[r.species_formula] -= r.stoichiometric_coefficient * net_rate;
        }
        // Products produced
        for (const auto& p : rxn.products) {
            dC_dt[p.species_formula] += p.stoichiometric_coefficient * net_rate;
        }

        // Exothermic / endothermic temperature coupling
        if (rxn.enthalpy_delta != 0.0) {
            constexpr double C_HEAT_LIQUID = 4184.0; // J / (L * K) approx water
            dT_dt += (-rxn.enthalpy_delta * net_rate) / C_HEAT_LIQUID;
        }
    }
}

base::Result<void> BatchReactor::step(double dt) {
    if (dt <= 0.0) {
        return base::Result<void>::err(base::Error{
            .code = 1,
            .domain = base::ErrorDomain::Simulation,
            .message = "dt must be positive",
            .details = {}
        });
    }

    // RK4 numerical integration
    std::unordered_map<std::string, double> k1, k2, k3, k4;
    double dT1 = 0.0, dT2 = 0.0, dT3 = 0.0, dT4 = 0.0;

    // Stage 1
    compute_derivatives(concentrations_, k1, dT1);

    // Stage 2
    auto c2 = concentrations_;
    for (auto& [sp, val] : c2) {
        val = std::max(0.0, val + 0.5 * dt * k1[sp]);
    }
    compute_derivatives(c2, k2, dT2);

    // Stage 3
    auto c3 = concentrations_;
    for (auto& [sp, val] : c3) {
        val = std::max(0.0, val + 0.5 * dt * k2[sp]);
    }
    compute_derivatives(c3, k3, dT3);

    // Stage 4
    auto c4 = concentrations_;
    for (auto& [sp, val] : c4) {
        val = std::max(0.0, val + dt * k3[sp]);
    }
    compute_derivatives(c4, k4, dT4);

    // Combine RK4 updates
    for (auto& [sp, val] : concentrations_) {
        double delta = (dt / 6.0) * (k1[sp] + 2.0 * k2[sp] + 2.0 * k3[sp] + k4[sp]);
        val = std::max(0.0, val + delta); // Positivity constraint
    }

    double delta_T = (dt / 6.0) * (dT1 + 2.0 * dT2 + 2.0 * dT3 + dT4);
    temperature_ = std::max(0.0, temperature_ + delta_T);

    return base::Result<void>::ok();
}

double BatchReactor::reaction_extent(const std::string& /*rxn_id*/, const std::string& species_formula) const {
    auto it_curr = concentrations_.find(species_formula);
    auto it_init = initial_moles_.find(species_formula);
    if (it_curr == concentrations_.end() || it_init == initial_moles_.end()) {
        return 0.0;
    }
    double current_moles = it_curr->second * volume_;
    return current_moles - it_init->second;
}

double BatchReactor::total_mass_kg() const {
    double total = 0.0;
    for (const auto& [formula, conc] : concentrations_) {
        auto it = species_db_.find(formula);
        if (it != species_db_.end()) {
            double moles = conc * volume_;
            total += moles * it->second.molar_mass;
        }
    }
    return total;
}

// =============================================================================
// ReactionDiffusion1D Implementation
// =============================================================================

ReactionDiffusion1D::ReactionDiffusion1D(ReactionDiffusion1DConfig config, double k_decay)
    : config_(config), k_decay_(k_decay) {
    c_.assign(config_.num_nodes, 0.0);
}

void ReactionDiffusion1D::set_boundary_conditions(double c_left, double c_right) {
    c_left_ = c_left;
    c_right_ = c_right;
}

void ReactionDiffusion1D::initialize_uniform(double initial_c) {
    c_.assign(config_.num_nodes, initial_c);
    if (!c_.empty()) {
        c_.front() = c_left_;
        c_.back() = c_right_;
    }
}

base::Result<void> ReactionDiffusion1D::step(double dt) {
    if (dt <= 0.0 || config_.num_nodes < 2) {
        return base::Result<void>::ok();
    }

    double dx = config_.length / static_cast<double>(config_.num_nodes - 1);
    double dx2 = dx * dx;
    double d = config_.diffusion_coefficient;

    std::vector<double> c_next = c_;

    for (size_t i = 1; i < config_.num_nodes - 1; ++i) {
        double d2c_dx2 = (c_[i + 1] - 2.0 * c_[i] + c_[i - 1]) / dx2;
        double reaction = -k_decay_ * c_[i];
        double dc_dt = d * d2c_dx2 + reaction;

        c_next[i] = std::max(0.0, c_[i] + dt * dc_dt);
    }

    c_next.front() = c_left_;
    c_next.back() = c_right_;
    c_ = c_next;

    return base::Result<void>::ok();
}

double ReactionDiffusion1D::concentration(size_t node_idx) const {
    if (node_idx < c_.size()) {
        return c_[node_idx];
    }
    return 0.0;
}

// =============================================================================
// ChemicalValidationBenchmark Implementation
// =============================================================================

double ChemicalValidationBenchmark::analytical_first_order(double c_a0, double k, double t) noexcept {
    return c_a0 * std::exp(-k * t);
}

double ChemicalValidationBenchmark::analytical_second_order(double c_a0, double k, double t) noexcept {
    if (c_a0 <= 0.0) return 0.0;
    return 1.0 / ((1.0 / c_a0) + 2.0 * k * t);
}

double ChemicalValidationBenchmark::analytical_reversible_equilibrium(
    double c_total, double k_f, double k_r) noexcept {
    double sum = k_f + k_r;
    return sum > 0.0 ? (k_r * c_total / sum) : 0.0;
}

} // namespace shunya::core::domain::chem
