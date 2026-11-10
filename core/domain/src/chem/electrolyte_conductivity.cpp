#include "shunya/chem/electrolyte_conductivity.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::chem {

double ElectrolyteConductivityModel::limiting_molar_conductivity(
    const std::vector<IonConductivityContribution>& ions) noexcept {
    double sum = 0.0;
    for (const auto& ion : ions) {
        sum += static_cast<double>(ion.stoich_count) * ion.limiting_molar_cond_s_m2_mol;
    }
    return sum;
}

double ElectrolyteConductivityModel::specific_conductivity_s_m(
    double lambda_0_s_m2_mol,
    double kohlrausch_coeff_k,
    double concentration_mol_l) noexcept {
    if (concentration_mol_l <= 0.0 || lambda_0_s_m2_mol <= 0.0) return 0.0;
    const double lambda_m = std::max(0.0, lambda_0_s_m2_mol - kohlrausch_coeff_k * std::sqrt(concentration_mol_l));
    // 1 mol/L = 1000 mol/m^3 => kappa [S/m] = (1000 * c) [mol/m^3] * Lambda_m [S*m^2/mol]
    return (1000.0 * concentration_mol_l) * lambda_m;
}

} // namespace shunya::chem
