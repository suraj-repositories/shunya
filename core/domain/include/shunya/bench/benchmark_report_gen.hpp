#pragma once

#include <cmath>
#include <span>

namespace shunya::bench {

struct ErrorNorms {
    double l2_relative{0.0};
    double linf_absolute{0.0};
    double max_relative{0.0};
};

class BenchmarkReportGen {
public:
    [[nodiscard]] static ErrorNorms compute_error_norms(
        std::span<const double> reference,
        std::span<const double> actual) noexcept {
        const size_t n = std::min(reference.size(), actual.size());
        double sum_sq_err = 0.0;
        double sum_sq_ref = 0.0;
        double max_abs = 0.0;
        double max_rel = 0.0;

        for (size_t i = 0; i < n; ++i) {
            const double e = std::abs(actual[i] - reference[i]);
            sum_sq_err += e * e;
            sum_sq_ref += reference[i] * reference[i];
            max_abs = std::max(max_abs, e);
            if (std::abs(reference[i]) > 1e-12) {
                max_rel = std::max(max_rel, e / std::abs(reference[i]));
            }
        }

        ErrorNorms out;
        out.linf_absolute = max_abs;
        out.max_relative = max_rel;
        out.l2_relative = (sum_sq_ref > 1e-24) ? std::sqrt(sum_sq_err / sum_sq_ref) : std::sqrt(sum_sq_err);
        return out;
    }
};

} // namespace shunya::bench
