#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <vector>

namespace shunya::matdb {

struct BohrShellCount {
    int principal_n{1};   // 1..7 (K..Q)
    char shell_letter{'K'};
    int electrons{0};
    int max_capacity{2};
};

struct BohrAtomModel {
    int atomic_number{1};
    std::string symbol;
    std::vector<BohrShellCount> shells;
    int total_electrons{1};
};

/**
 * @brief Computes concentric Bohr principal shell occupancies and hydrogenic angular probability densities.
 */
class BohrOrbitalModel {
public:
    [[nodiscard]] static core::base::Result<BohrAtomModel> compute_for_element(int atomic_number);

    /**
     * @brief Evaluates real spherical harmonic angular probability |Y_l^m(theta, phi)|^2 (s/p/d/f).
     */
    [[nodiscard]] static double spherical_harmonic_density(int l, int m, double theta_rad, double phi_rad) noexcept;

    [[nodiscard]] static std::string angular_orbital_glsl_fragment();
    [[nodiscard]] static core::base::ValidationReport audit_all_118_bohr_counts();
};

} // namespace shunya::matdb
