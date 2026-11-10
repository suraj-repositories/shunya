#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <vector>

namespace shunya::mat {

struct ElasticPhase {
    std::string name;
    double volume_fraction{0.5};
    double bulk_modulus_gpa{100.0};
    double shear_modulus_gpa{40.0};
};

struct HomogenizationBounds {
    double bulk_reuss_gpa{0.0};
    double bulk_hs_lower_gpa{0.0};
    double bulk_hs_upper_gpa{0.0};
    double bulk_voigt_gpa{0.0};
    double bulk_hill_gpa{0.0};

    double shear_reuss_gpa{0.0};
    double shear_hs_lower_gpa{0.0};
    double shear_hs_upper_gpa{0.0};
    double shear_voigt_gpa{0.0};
    double shear_hill_gpa{0.0};

    double effective_youngs_gpa{0.0};
    double effective_poisson{0.0};
};

/**
 * @brief Evaluates Voigt, Reuss, Hill, and Hashin-Shtrikman elastic homogenization bounds.
 */
class CompositeHomogenizer {
public:
    [[nodiscard]] static core::base::Result<HomogenizationBounds> evaluate(
        const std::vector<ElasticPhase>& phases);
};

} // namespace shunya::mat
