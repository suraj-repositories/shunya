#pragma once

#include "shunya/core/base/error.hpp"

#include <array>

namespace shunya::mat {

using Matrix6x6 = std::array<std::array<double, 6>, 6>;

struct StiffnessStabilityResult {
    bool is_symmetric{true};
    bool is_positive_definite{true};
    std::array<double, 6> eigenvalues_gpa{};
    double min_eigenvalue_gpa{0.0};
};

/**
 * @brief Validates symmetry and thermodynamic positive definiteness of 6x6 Voigt stiffness matrices.
 */
class TensorValidator {
public:
    [[nodiscard]] static StiffnessStabilityResult analyze_stability(const Matrix6x6& c_gpa);
    [[nodiscard]] static core::base::ValidationReport validate_stiffness(const Matrix6x6& c_gpa);
};

} // namespace shunya::mat
