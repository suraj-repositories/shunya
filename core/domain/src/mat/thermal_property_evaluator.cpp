#include "shunya/mat/thermal_property_evaluator.hpp"

#include <algorithm>

namespace shunya::mat {

double ThermalPropertyEvaluator::thermal_conductivity_w_mk(double temp_k) const noexcept {
    return std::max(1e-9, k_spline_.evaluate(temp_k));
}

double ThermalPropertyEvaluator::specific_heat_j_kg_k(double temp_k) const noexcept {
    return std::max(1e-9, cp_spline_.evaluate(temp_k));
}

ThermalPropertyEvaluator ThermalPropertyEvaluator::copper_nist_cryogenic() {
    // NIST Cryogenic Data for OFHC Copper (RRR ~ 50) from 4 K to 1356 K (W/(m*K))
    std::vector<double> t_k = {
        4.0, 10.0, 20.0, 30.0, 50.0, 77.0, 100.0, 150.0, 200.0, 298.15,
        400.0, 600.0, 800.0, 1000.0, 1200.0, 1356.0
    };
    std::vector<double> k_vals = {
        320.0, 760.0, 1050.0, 960.0, 700.0, 520.0, 482.0, 429.0, 413.0, 401.0,
        393.0, 379.0, 366.0, 352.0, 339.0, 330.0
    };
    std::vector<double> t_cp = {
        4.0, 10.0, 20.0, 50.0, 100.0, 200.0, 298.15, 500.0, 800.0, 1200.0, 1356.0
    };
    std::vector<double> cp_vals = {
        0.10, 0.85, 7.7, 99.0, 252.0, 356.0, 385.0, 408.0, 434.0, 472.0, 490.0
    };
    return ThermalPropertyEvaluator(
        MonotonicCubicSpline::create(std::move(t_k), std::move(k_vals)).value(),
        MonotonicCubicSpline::create(std::move(t_cp), std::move(cp_vals)).value());
}

ThermalPropertyEvaluator ThermalPropertyEvaluator::aluminum_6061() {
    std::vector<double> t = {4.0, 20.0, 77.0, 150.0, 298.15, 500.0, 800.0};
    std::vector<double> k = {4.2, 24.0, 85.0, 135.0, 167.0, 184.0, 196.0};
    std::vector<double> cp = {0.2, 8.5, 336.0, 675.0, 896.0, 1020.0, 1140.0};
    return ThermalPropertyEvaluator(
        MonotonicCubicSpline::create(t, std::move(k)).value(),
        MonotonicCubicSpline::create(std::move(t), std::move(cp)).value());
}

} // namespace shunya::mat
