#pragma once

#include "shunya/core/base/result.hpp"

#include <optional>
#include <string>
#include <vector>

namespace shunya::matdb {

inline constexpr double kEvToKjPerMol = 96.48533212;
inline constexpr double kEvToJoules = 1.602176634e-19;

[[nodiscard]] constexpr double ev_to_kj_per_mol(double ev) noexcept {
    return ev * kEvToKjPerMol;
}

[[nodiscard]] constexpr double ev_to_joules(double ev) noexcept {
    return ev * kEvToJoules;
}

struct IonizationProfile {
    int atomic_number{1};
    std::optional<double> electron_affinity_ev;
    /// Sequential ionization energies IE_1, IE_2, ... (up to min(Z, 8)) in eV
    std::vector<double> ionization_energies_ev;
};

/**
 * @brief Repository of NIST ASD ionization energies (eV, kJ/mol, SI J) and electron affinities.
 */
class IonizationEnergyRepository {
public:
    static const std::vector<IonizationProfile>& all_profiles();
    [[nodiscard]] static const IonizationProfile* find_by_z(int atomic_number) noexcept;
    [[nodiscard]] static std::optional<double> first_ionization_ev(int atomic_number) noexcept;
    [[nodiscard]] static std::string to_json();
    [[nodiscard]] static core::base::ValidationReport audit_monotonicity();
};

} // namespace shunya::matdb
