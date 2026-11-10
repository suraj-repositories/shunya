#pragma once

#include <algorithm>
#include <cmath>

namespace shunya::env {

struct SolarIrradianceSample {
    double extraterrestrial_w_m2{1361.0};
    double air_mass{1.0};
    double direct_normal_w_m2{0.0};
    double diffuse_horizontal_w_m2{0.0};
    double global_horizontal_w_m2{0.0};
};

/**
 * @brief Clear-sky solar irradiance model combining Kasten air mass, Meinel beam attenuation, and Haurwitz diffuse sky flux.
 */
class SolarFluxModel {
public:
    static constexpr double kSolarConstantWPerM2 = 1361.0;

    [[nodiscard]] static SolarIrradianceSample evaluate_clear_sky(
        double elevation_deg,
        int day_of_year = 80,
        double altitude_m = 0.0) noexcept {
        constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
        const double b = (2.0 * 3.14159265358979323846 * static_cast<double>(day_of_year - 1)) / 365.0;
        const double i0 = kSolarConstantWPerM2 * (1.00011 + 0.034221 * std::cos(b) +
                                                  0.00128 * std::sin(b));

        SolarIrradianceSample s;
        s.extraterrestrial_w_m2 = i0;

        if (elevation_deg <= 0.0) {
            s.air_mass = 0.0;
            return s;
        }

        const double elev_clamped = std::min(90.0, elevation_deg);
        const double zenith_deg = 90.0 - elev_clamped;
        const double cos_z = std::max(1e-4, std::sin(elev_clamped * kDegToRad));

        // Kasten & Young (1989) relative optical air mass with pressure altitude scaling
        const double p_ratio = std::exp(-std::clamp(altitude_m, 0.0, 50000.0) / 8400.0);
        const double am_sea = 1.0 / (cos_z + 0.50572 * std::pow(96.07995 - zenith_deg, -1.6364));
        const double am = am_sea * p_ratio;

        // Meinel clear-sky DNI & Haurwitz clear-sky GHI reference calibration
        const double dni = i0 * std::pow(0.7, std::pow(std::max(0.05, am), 0.678));
        const double ghi_haurwitz = 1098.0 * cos_z * std::exp(-0.057 / cos_z);
        const double direct_horiz = dni * cos_z;
        const double dhi = std::max(0.0, ghi_haurwitz - direct_horiz);

        s.air_mass = am;
        s.direct_normal_w_m2 = dni;
        s.diffuse_horizontal_w_m2 = dhi;
        s.global_horizontal_w_m2 = direct_horiz + dhi;
        return s;
    }
};

} // namespace shunya::env
