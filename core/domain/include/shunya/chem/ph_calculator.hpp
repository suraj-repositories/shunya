#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace shunya::chem {

class PhCalculator {
public:
    static constexpr double kKw25C = 1.0e-14;

    [[nodiscard]] static double pH_from_h(double h_conc_mol_l) noexcept {
        return -std::log10(std::max(1e-20, h_conc_mol_l));
    }

    [[nodiscard]] static double h_from_pH(double pH) noexcept {
        return std::pow(10.0, -pH);
    }

    [[nodiscard]] static double henderson_hasselbalch(
        double pka,
        double weak_acid_mol_l,
        double conjugate_base_mol_l) noexcept {
        if (weak_acid_mol_l <= 0.0 || conjugate_base_mol_l <= 0.0) return pka;
        return pka + std::log10(conjugate_base_mol_l / weak_acid_mol_l);
    }
};

} // namespace shunya::chem
