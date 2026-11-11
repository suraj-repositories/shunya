#pragma once

#include <algorithm>
#include <cmath>
#include <span>

namespace shunya::bench {

struct WaveformComparison {
    double rms_error{0.0};
    double normalized_rms_error{0.0};
    double max_abs_error{0.0};
    double cross_correlation{1.0};
};

/**
 * @brief Compares simulated SPICE analog waveforms against reference ngspice/analytical traces.
 */
class WaveformComparator {
public:
    [[nodiscard]] static WaveformComparison compare(
        std::span<const double> expected,
        std::span<const double> actual) noexcept {
        const size_t n = std::min(expected.size(), actual.size());
        if (n == 0) return {};

        double sum_sq_err = 0.0;
        double sum_sq_ref = 0.0;
        double max_abs = 0.0;
        double mean_e = 0.0, mean_a = 0.0;

        for (size_t i = 0; i < n; ++i) {
            mean_e += expected[i];
            mean_a += actual[i];
            const double diff = actual[i] - expected[i];
            sum_sq_err += diff * diff;
            sum_sq_ref += expected[i] * expected[i];
            max_abs = std::max(max_abs, std::abs(diff));
        }
        mean_e /= static_cast<double>(n);
        mean_a /= static_cast<double>(n);

        double cov = 0.0, var_e = 0.0, var_a = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double de = expected[i] - mean_e;
            const double da = actual[i] - mean_a;
            cov += de * da;
            var_e += de * de;
            var_a += da * da;
        }

        WaveformComparison res;
        res.rms_error = std::sqrt(sum_sq_err / static_cast<double>(n));
        res.normalized_rms_error = std::sqrt(sum_sq_err / std::max(1e-24, sum_sq_ref));
        res.max_abs_error = max_abs;
        const double denom = std::sqrt(var_e * var_a);
        res.cross_correlation = (denom > 1e-24) ? (cov / denom) : 1.0;
        return res;
    }
};

} // namespace shunya::bench
