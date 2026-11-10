#pragma once

#include "shunya/core/base/result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace shunya::matdb {

struct SubshellOccupancy {
    int n{1};
    int l{0};
    int occupancy{1};

    [[nodiscard]] constexpr char l_char() const noexcept {
        switch (l) {
        case 0: return 's';
        case 1: return 'p';
        case 2: return 'd';
        case 3: return 'f';
        case 4: return 'g';
        default: return '?';
        }
    }

    [[nodiscard]] constexpr int max_capacity() const noexcept {
        return 2 * (2 * l + 1);
    }

    [[nodiscard]] constexpr int orbital_count() const noexcept {
        return 2 * l + 1;
    }

    /**
     * @brief Unpaired electrons per Hund's maximum multiplicity rule.
     */
    [[nodiscard]] constexpr int unpaired_electrons() const noexcept {
        const int orbitals = orbital_count();
        if (occupancy <= orbitals) return occupancy;
        return max_capacity() - occupancy;
    }

    [[nodiscard]] std::string label() const {
        return std::to_string(n) + l_char() + std::to_string(occupancy);
    }
};

struct ElectronConfiguration {
    std::vector<SubshellOccupancy> subshells;

    [[nodiscard]] int total_electrons() const noexcept {
        int sum = 0;
        for (const auto& s : subshells) sum += s.occupancy;
        return sum;
    }

    [[nodiscard]] int unpaired_electrons() const noexcept {
        int sum = 0;
        for (const auto& s : subshells) sum += s.unpaired_electrons();
        return sum;
    }

    [[nodiscard]] int valence_electrons() const noexcept {
        if (subshells.empty()) return 0;
        int max_n = 0;
        for (const auto& s : subshells) {
            if (s.n > max_n) max_n = s.n;
        }
        int sum = 0;
        for (const auto& s : subshells) {
            // Highest principal shell, plus partially filled (n-1)d or (n-2)f valence shells
            if (s.n == max_n) {
                sum += s.occupancy;
            } else if (s.l >= 2 && s.occupancy < s.max_capacity()) {
                sum += s.occupancy;
            }
        }
        return sum;
    }

    [[nodiscard]] double spin_multiplicity() const noexcept {
        const double total_spin = 0.5 * static_cast<double>(unpaired_electrons());
        return 2.0 * total_spin + 1.0;
    }
};

/**
 * @brief Parses condensed noble-gas and full quantum subshell configuration strings.
 */
class ElectronConfigParser {
public:
    [[nodiscard]] static core::base::Result<ElectronConfiguration> parse(std::string_view config_str);
};

} // namespace shunya::matdb
