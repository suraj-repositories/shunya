#include "shunya/chem/reaction_network.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

using namespace core::domain::chem;

void ReactionNetworkEngine::add_species(const ChemicalSpecies& species, double initial_molar_conc) {
    if (auto it = species_index_.find(species.formula); it != species_index_.end()) {
        species_list_[it->second] = species;
        species_map_[species.formula] = species;
        concentrations_[it->second] = std::max(0.0, initial_molar_conc);
        return;
    }
    species_index_[species.formula] = species_list_.size();
    species_list_.push_back(species);
    species_map_[species.formula] = species;
    concentrations_.push_back(std::max(0.0, initial_molar_conc));
}

core::base::Result<void> ReactionNetworkEngine::add_reaction(const ChemicalReaction& rxn) {
    auto val = rxn.validate_stoichiometric_balance(species_map_);
    if (!val.is_ok()) {
        return val;
    }
    reactions_.push_back(rxn);
    return core::base::Result<void>::ok();
}

void ReactionNetworkEngine::set_concentration(std::string_view formula, double conc_mol_l) {
    if (auto it = species_index_.find(std::string(formula)); it != species_index_.end()) {
        concentrations_[it->second] = std::max(0.0, conc_mol_l);
    }
}

double ReactionNetworkEngine::concentration(std::string_view formula) const noexcept {
    if (auto it = species_index_.find(std::string(formula)); it != species_index_.end()) {
        return concentrations_[it->second];
    }
    return 0.0;
}

double ReactionNetworkEngine::element_moles_per_l(std::string_view element) const noexcept {
    double total = 0.0;
    for (size_t i = 0; i < species_list_.size(); ++i) {
        if (auto it = species_list_[i].element_composition.find(std::string(element));
            it != species_list_[i].element_composition.end()) {
            total += static_cast<double>(it->second) * concentrations_[i];
        }
    }
    return total;
}

double ReactionNetworkEngine::total_mass_kg_per_l() const noexcept {
    double total = 0.0;
    for (size_t i = 0; i < species_list_.size(); ++i) {
        total += species_list_[i].molar_mass * concentrations_[i];
    }
    return total;
}

core::base::Result<std::vector<std::vector<double>>> ReactionNetworkEngine::integrate(
    double t_end_s,
    double dt_s,
    bool use_arrhenius_rates) {

    const size_t ns = species_list_.size();
    if (ns == 0) {
        return core::base::Result<std::vector<std::vector<double>>>::err(
            core::base::Error{92, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "ReactionNetworkEngine requires at least one species", ""});
    }

    struct CompiledTerm {
        size_t idx;
        int coeff;
    };
    struct CompiledRxn {
        double kf;
        double kr;
        bool reversible;
        std::vector<CompiledTerm> reactants;
        std::vector<CompiledTerm> products;
    };

    std::vector<CompiledRxn> compiled;
    compiled.reserve(reactions_.size());
    for (const auto& r : reactions_) {
        CompiledRxn cr;
        cr.kf = use_arrhenius_rates ? r.rate_constant_at_temperature(temperature_k_) : r.rate_constant_forward;
        cr.kr = r.rate_constant_reverse;
        cr.reversible = r.reversible;
        for (const auto& c : r.reactants) {
            if (auto it = species_index_.find(c.species_formula); it != species_index_.end()) {
                cr.reactants.push_back({it->second, c.stoichiometric_coefficient});
            }
        }
        for (const auto& c : r.products) {
            if (auto it = species_index_.find(c.species_formula); it != species_index_.end()) {
                cr.products.push_back({it->second, c.stoichiometric_coefficient});
            }
        }
        compiled.push_back(std::move(cr));
    }

    auto rhs = [&compiled, ns](double, const std::vector<double>& y, std::vector<double>& dydt) {
        dydt.assign(ns, 0.0);
        for (const auto& cr : compiled) {
            double rf = cr.kf;
            for (const auto& term : cr.reactants) {
                const double c = std::max(0.0, y[term.idx]);
                for (int p = 0; p < term.coeff; ++p) rf *= c;
            }
            double rr = 0.0;
            if (cr.reversible && cr.kr > 0.0) {
                rr = cr.kr;
                for (const auto& term : cr.products) {
                    const double c = std::max(0.0, y[term.idx]);
                    for (int p = 0; p < term.coeff; ++p) rr *= c;
                }
            }
            const double net_rate = rf - rr;
            for (const auto& term : cr.reactants) {
                dydt[term.idx] -= static_cast<double>(term.coeff) * net_rate;
            }
            for (const auto& term : cr.products) {
                dydt[term.idx] += static_cast<double>(term.coeff) * net_rate;
            }
        }
    };

    return StiffOdeSolver::integrate(t_end_s, dt_s, concentrations_, rhs);
}

} // namespace shunya::chem
