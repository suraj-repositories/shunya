#pragma once

#include <cmath>
#include <vector>

namespace shunya::chem {

struct IonConductivityContribution {
    int stoich_count{1};
    double limiting_molar_cond_s_m2_mol{0.0}; // e.g. H+: 0.03498, Na+: 0.00501, Cl-: 0.00763, Ag+: 0.00619 S*m^2/mol
};

/**
 * @brief Kohlrausch law aqueous electrolyte molar and specific electrical conductivity evaluator.
 */
class ElectrolyteConductivityModel {
public:
    [[nodiscard]] static double limiting_molar_conductivity(
        const std::vector<IonConductivityContribution>& ions) noexcept;

    /**
     * @brief Evaluates specific conductivity kappa [S/m] from Kohlrausch law Lambda_m = Lambda_0 - K * sqrt(c).
     * @param concentration_mol_l Molar concentration c [mol/L] (converted internally to mol/m^3 via 1000 * c)
     * @param kohlrausch_coeff_k Kohlrausch Onsager slope K [S*m^2/(mol^(3/2)*L^(-1/2))]
     */
    [[nodiscard]] static double specific_conductivity_s_m(
        double lambda_0_s_m2_mol,
        double kohlrausch_coeff_k,
        double concentration_mol_l) noexcept;
};

} // namespace shunya::chem
