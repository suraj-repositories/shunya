#pragma once

#include "shunya/core/domain/mechanics.hpp"

#include <cmath>

namespace shunya::phy {

struct SymmetricStressTensor {
    double sxx{0.0};
    double syy{0.0};
    double szz{0.0};
    double sxy{0.0};
    double sxz{0.0};
    double syz{0.0};
};

/**
 * @brief Computes von Mises equivalent stress and hydrostatic/deviatoric invariants from 3D stress tensors.
 */
class VonMisesEvaluator {
public:
    [[nodiscard]] static double hydrostatic_stress(const SymmetricStressTensor& s) noexcept {
        return (s.sxx + s.syy + s.szz) / 3.0;
    }

    [[nodiscard]] static double von_mises_stress(const SymmetricStressTensor& s) noexcept {
        const double d1 = s.sxx - s.syy;
        const double d2 = s.syy - s.szz;
        const double d3 = s.szz - s.sxx;
        const double shear2 = s.sxy * s.sxy + s.syz * s.syz + s.sxz * s.sxz;
        return std::sqrt(std::max(0.0, 0.5 * (d1 * d1 + d2 * d2 + d3 * d3 + 6.0 * shear2)));
    }
};

} // namespace shunya::phy
