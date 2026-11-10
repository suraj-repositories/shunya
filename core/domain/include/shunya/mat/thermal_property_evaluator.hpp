#pragma once

#include "shunya/mat/monotonic_spline.hpp"

#include <string_view>

namespace shunya::mat {

/**
 * @brief Evaluates non-oscillatory temperature-dependent thermal conductivity k(T) and cp(T).
 */
class ThermalPropertyEvaluator {
public:
    ThermalPropertyEvaluator() = default;
    ThermalPropertyEvaluator(MonotonicCubicSpline k_spline, MonotonicCubicSpline cp_spline)
        : k_spline_(std::move(k_spline)), cp_spline_(std::move(cp_spline)) {}

    [[nodiscard]] double thermal_conductivity_w_mk(double temp_k) const noexcept;
    [[nodiscard]] double specific_heat_j_kg_k(double temp_k) const noexcept;

    /**
     * @brief Standard NIST Cryogenics RRR=50 Copper k(T) & cp(T) from 4 K to 1356 K.
     */
    [[nodiscard]] static ThermalPropertyEvaluator copper_nist_cryogenic();

    /**
     * @brief Standard Aluminum 6061-T6 k(T) & cp(T) from 4 K to 800 K.
     */
    [[nodiscard]] static ThermalPropertyEvaluator aluminum_6061();

private:
    MonotonicCubicSpline k_spline_;
    MonotonicCubicSpline cp_spline_;
};

} // namespace shunya::mat
