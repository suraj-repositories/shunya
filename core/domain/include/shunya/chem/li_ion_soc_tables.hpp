#pragma once

#include <algorithm>
#include <array>
#include <utility>

namespace shunya::chem {

class LiIonSocTables {
public:
    // 18650 NMC/Graphite OCV(SOC) curve
    static constexpr std::array<std::pair<double, double>, 11> kOcvTable{{
        {0.00, 2.85},
        {0.05, 3.30},
        {0.10, 3.45},
        {0.20, 3.56},
        {0.35, 3.63},
        {0.50, 3.70},
        {0.65, 3.80},
        {0.80, 3.94},
        {0.90, 4.06},
        {0.95, 4.13},
        {1.00, 4.20}
    }};

    [[nodiscard]] static double lookup_ocv_v(double soc) noexcept {
        const double s = std::clamp(soc, 0.0, 1.0);
        for (size_t i = 1; i < kOcvTable.size(); ++i) {
            if (s <= kOcvTable[i].first) {
                const double s0 = kOcvTable[i - 1].first;
                const double s1 = kOcvTable[i].first;
                const double v0 = kOcvTable[i - 1].second;
                const double v1 = kOcvTable[i].second;
                const double t = (s - s0) / std::max(1e-9, s1 - s0);
                return v0 + t * (v1 - v0);
            }
        }
        return kOcvTable.back().second;
    }
};

} // namespace shunya::chem
