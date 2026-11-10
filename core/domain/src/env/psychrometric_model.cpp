#include "shunya/env/psychrometric_model.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::env {

namespace {
constexpr double kMagnusC0 = 610.94;
constexpr double kMagnusA = 17.625;
constexpr double kMagnusB = 243.04;
constexpr double kPsychrometricGamma = 6.62e-4; // 1/K
} // namespace

double PsychrometricModel::saturation_vapor_pressure_pa(double temp_k) noexcept {
    const double t_c = std::clamp(temp_k - 273.15, -80.0, 150.0);
    return kMagnusC0 * std::exp((kMagnusA * t_c) / (kMagnusB + t_c));
}

core::base::Result<double> PsychrometricModel::dew_point_k(
    double dry_bulb_temp_k,
    double relative_humidity_pct) {
    if (dry_bulb_temp_k <= 150.0 || !std::isfinite(dry_bulb_temp_k)) {
        return core::base::Error{
            311, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Dry-bulb temperature must be > 150 K", ""
        };
    }
    if (relative_humidity_pct <= 0.0 || relative_humidity_pct > 100.0 ||
        !std::isfinite(relative_humidity_pct)) {
        return core::base::Error{
            312, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Relative humidity must be in (0, 100] % for dew point", ""
        };
    }
    const double e_sat = saturation_vapor_pressure_pa(dry_bulb_temp_k);
    const double e_v = e_sat * (relative_humidity_pct / 100.0);
    const double gamma = std::log(e_v / kMagnusC0);
    const double td_c = (kMagnusB * gamma) / (kMagnusA - gamma);
    return td_c + 273.15;
}

core::base::Result<double> PsychrometricModel::wet_bulb_k(
    double dry_bulb_temp_k,
    double relative_humidity_pct,
    double pressure_pa) {
    if (dry_bulb_temp_k <= 150.0 || pressure_pa <= 0.0 ||
        relative_humidity_pct < 0.0 || relative_humidity_pct > 100.0) {
        return core::base::Error{
            313, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid psychrometric input parameters", ""
        };
    }
    const double e_sat_db = saturation_vapor_pressure_pa(dry_bulb_temp_k);
    const double e_v = e_sat_db * (relative_humidity_pct / 100.0);

    auto f = [&](double t_wb) {
        return saturation_vapor_pressure_pa(t_wb) -
               kPsychrometricGamma * pressure_pa * (dry_bulb_temp_k - t_wb) - e_v;
    };

    double lo = 170.0;
    double hi = dry_bulb_temp_k;
    for (int iter = 0; iter < 60; ++iter) {
        const double mid = 0.5 * (lo + hi);
        if (f(mid) > 0.0) {
            hi = mid;
        } else {
            lo = mid;
        }
    }
    return 0.5 * (lo + hi);
}

core::base::Result<PsychrometricState> PsychrometricModel::evaluate(
    double dry_bulb_temp_k,
    double relative_humidity_pct,
    double pressure_pa) {
    if (dry_bulb_temp_k <= 150.0 || !std::isfinite(dry_bulb_temp_k)) {
        return core::base::Error{
            314, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Dry-bulb temperature must be > 150 K", ""
        };
    }
    if (relative_humidity_pct < 0.0 || relative_humidity_pct > 100.0 ||
        !std::isfinite(relative_humidity_pct)) {
        return core::base::Error{
            315, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Relative humidity must be in [0, 100] %", ""
        };
    }
    if (pressure_pa <= 0.0 || !std::isfinite(pressure_pa)) {
        return core::base::Error{
            316, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Ambient pressure must be positive", ""
        };
    }

    const double e_sat = saturation_vapor_pressure_pa(dry_bulb_temp_k);
    const double e_v = std::min(0.999 * pressure_pa, e_sat * (relative_humidity_pct / 100.0));
    const double p_dry = std::max(1.0, pressure_pa - e_v);

    const double w = kMolecularWeightRatio * (e_v / p_dry);
    const double r_moist = kDryAirGasR * ((1.0 + (kWaterVaporGasR / kDryAirGasR) * w) / (1.0 + w));
    const double rho_moist = (p_dry / (kDryAirGasR * dry_bulb_temp_k)) +
                             (e_v / (kWaterVaporGasR * dry_bulb_temp_k));

    double td_k = 0.0;
    if (relative_humidity_pct > 0.0) {
        auto td_res = dew_point_k(dry_bulb_temp_k, relative_humidity_pct);
        if (!td_res.is_ok()) {
            return td_res.error();
        }
        td_k = td_res.value();
    }

    auto twb_res = wet_bulb_k(dry_bulb_temp_k, relative_humidity_pct, pressure_pa);
    if (!twb_res.is_ok()) {
        return twb_res.error();
    }

    PsychrometricState s;
    s.dry_bulb_temp_k = dry_bulb_temp_k;
    s.relative_humidity_pct = relative_humidity_pct;
    s.pressure_pa = pressure_pa;
    s.saturation_vapor_pressure_pa = e_sat;
    s.vapor_pressure_pa = e_v;
    s.dew_point_k = td_k;
    s.wet_bulb_k = twb_res.value();
    s.humidity_ratio_kg_kg = w;
    s.specific_gas_constant_moist = r_moist;
    s.moist_air_density_kg_m3 = rho_moist;
    return s;
}

} // namespace shunya::env
