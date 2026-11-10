#pragma once

#include "shunya/chem/ph_calculator.hpp"
#include "shunya/core/base/result.hpp"

#include <string>
#include <vector>

namespace shunya::chem {

struct PolyproticAcidSpec {
    std::string name;
    double total_conc_mol_l{0.0};
    std::vector<double> pka_values; // e.g. Acetic: {4.76}, Carbonic: {6.35, 10.33}, Phosphoric: {2.15, 7.20, 12.35}
    int neutral_charge{0};          // Charge of fully protonated H_n A form (0 for H3PO4)
};

struct AqueousEquilibriumResult {
    double pH{7.0};
    double pOH{7.0};
    double h_plus_mol_l{1e-7};
    double oh_minus_mol_l{1e-7};
    std::vector<std::vector<double>> acid_species_fractions;
};

/**
 * @brief Charge-neutrality & mass-balance aqueous solver for strong/weak acids, polyprotic buffers, and titrations.
 */
class AqueousSolutionEngine {
public:
    void add_strong_acid(double conc_mol_l); // monoprotic e.g. HCl
    void add_strong_base(double conc_mol_l); // monoprotic e.g. NaOH
    void add_polyprotic_acid(PolyproticAcidSpec acid);
    void set_spectator_net_charge(double eq_per_l) { spectator_charge_ += eq_per_l; }
    void reset();

    [[nodiscard]] core::base::Result<AqueousEquilibriumResult> solve_equilibrium(
        double kw = PhCalculator::kKw25C) const;

    /**
     * @brief Simulates titration of acid_vol_l of strong acid (or weak acid) by strong base of titrant_conc_mol_l.
     */
    [[nodiscard]] static std::vector<std::pair<double, double>> titrate_strong_acid_base(
        double acid_conc_mol_l,
        double acid_vol_l,
        double base_conc_mol_l,
        const std::vector<double>& added_base_vols_l);

private:
    double spectator_charge_{0.0}; // [Na+] - [Cl-] etc.
    std::vector<PolyproticAcidSpec> acids_;
};

} // namespace shunya::chem
