#pragma once

#include <cmath>
#include <vector>

namespace shunya::bench {

/**
 * @brief 500-term analytical Fourier series solver for 1D transient slab heat conduction across arbitrary Biot numbers.
 */
class BenchmarkThermalDiffusion {
public:
    static constexpr double kPi = 3.14159265358979323846;

    /**
     * @brief Exact dimensionless temperature theta(x/L, Fo) on symmetric slab [-L, L] with convective Bi = h*L/k.
     */
    [[nodiscard]] static double slab_convective_fourier_500(
        double x_over_l,
        double fourier_fo,
        double biot_bi) noexcept {
        if (biot_bi >= 1e5) {
            return slab_dirichlet_fourier_500(x_over_l, fourier_fo);
        }
        double theta = 0.0;
        for (int n = 1; n <= 500; ++n) {
            // Root zeta_n of zeta * tan(zeta) = Bi lies in ((n-1)*pi, (n-0.5)*pi)
            double lo = static_cast<double>(n - 1) * kPi + 1e-12;
            double hi = (static_cast<double>(n) - 0.5) * kPi - 1e-12;
            for (int it = 0; it < 36; ++it) {
                const double mid = 0.5 * (lo + hi);
                const double f = mid * std::tan(mid) - biot_bi;
                if (f > 0.0) {
                    hi = mid;
                } else {
                    lo = mid;
                }
            }
            const double zeta = 0.5 * (lo + hi);
            const double c_n = (2.0 * std::sin(zeta)) / (zeta + std::sin(zeta) * std::cos(zeta));
            const double decay = std::exp(-zeta * zeta * fourier_fo);
            theta += c_n * decay * std::cos(zeta * x_over_l);
            if (decay < 1e-16 && n > 10) break;
        }
        return theta;
    }

    /**
     * @brief Exact Dirichlet 0-to-1 unit bar T(x, t) for 0 <= x/L <= 1 with T(0,t)=0, T(1,t)=0, T(x,0)=1.
     */
    [[nodiscard]] static double bar_sine_fourier_500(double x_over_l, double fourier_fo) noexcept {
        double theta = 0.0;
        for (int n = 1; n <= 999; n += 2) {
            const double lam = static_cast<double>(n) * kPi;
            const double decay = std::exp(-lam * lam * fourier_fo);
            theta += (4.0 / lam) * decay * std::sin(lam * x_over_l);
            if (decay < 1e-16 && n > 15) break;
        }
        return theta;
    }

    [[nodiscard]] static double slab_dirichlet_fourier_500(double x_over_l, double fourier_fo) noexcept {
        double theta = 0.0;
        for (int n = 1; n <= 500; ++n) {
            const double lam = (static_cast<double>(n) - 0.5) * kPi;
            const double sign = ((n - 1) % 2 == 0) ? 1.0 : -1.0;
            const double decay = std::exp(-lam * lam * fourier_fo);
            theta += (2.0 * sign / lam) * decay * std::cos(lam * x_over_l);
            if (decay < 1e-16 && n > 15) break;
        }
        return theta;
    }
};

} // namespace shunya::bench
