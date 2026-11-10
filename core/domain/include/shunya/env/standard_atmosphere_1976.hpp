#pragma once

#include <array>
#include <cstddef>

namespace shunya::env {

struct AtmosphereLayerSpec {
    double base_geopotential_m;
    double base_temperature_k;
    double base_pressure_pa;
    double lapse_rate_k_per_m;
};

/**
 * @brief Tabulated US Standard Atmosphere 1976 atmospheric layers (0 to 84.852 km geopotential / 86 km geometric).
 */
struct StandardAtmosphere1976 {
    static constexpr double kEarthRadiusM = 6356766.0;
    static constexpr double kStandardGravity = 9.80665;
    static constexpr double kMolarMassAir = 0.0289644;
    static constexpr double kUniversalGasR = 8.3144598;
    static constexpr double kSpecificGasAirR = kUniversalGasR / kMolarMassAir; // ~287.05287 J/(kg*K)
    static constexpr double kGammaAir = 1.4;
    static constexpr double kMaxGeopotentialM = 84852.0;

    static constexpr std::array<AtmosphereLayerSpec, 7> kLayers{{
        {0.0,     288.15, 101325.0,       -0.0065}, // Layer 0: Troposphere (0 - 11 km)
        {11000.0, 216.65, 22632.064,       0.0000}, // Layer 1: Tropopause (11 - 20 km)
        {20000.0, 216.65, 5474.8886,      +0.0010}, // Layer 2: Stratosphere I (20 - 32 km)
        {32000.0, 228.65, 868.01868,      +0.0028}, // Layer 3: Stratosphere II (32 - 47 km)
        {47000.0, 270.65, 110.90631,       0.0000}, // Layer 4: Stratopause (47 - 51 km)
        {51000.0, 270.65, 66.938873,      -0.0028}, // Layer 5: Mesosphere I (51 - 71 km)
        {71000.0, 214.65, 3.9564204,      -0.0020}  // Layer 6: Mesosphere II (71 - 84.852 km)
    }};

    [[nodiscard]] static constexpr double geometric_to_geopotential(double z_m) noexcept {
        return (kEarthRadiusM * z_m) / (kEarthRadiusM + z_m);
    }

    [[nodiscard]] static constexpr double geopotential_to_geometric(double h_m) noexcept {
        return (kEarthRadiusM * h_m) / (kEarthRadiusM - h_m);
    }
};

} // namespace shunya::env
