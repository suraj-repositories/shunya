#pragma once

#include <string>
#include <string_view>

namespace shunya::chem {

enum class FuelKind {
    Methane,
    Propane,
    Hydrogen
};

struct FuelFlammabilitySpec {
    FuelKind kind{FuelKind::Methane};
    std::string formula{"CH4"};
    double lfl_vol_fraction{0.050};      // Lower Flammability Limit (vol/mol fraction in air)
    double ufl_vol_fraction{0.150};      // Upper Flammability Limit
    double auto_ignition_temp_k{873.15}; // 600 C for Methane
    double stoich_o2_per_fuel{2.0};      // Moles O2 per mole fuel
    double activation_j_mol{125000.0};
    double pre_exponential{2.0e9};
};

class FlammabilityLimits {
public:
    [[nodiscard]] static FuelFlammabilitySpec spec_for(FuelKind kind) noexcept {
        switch (kind) {
            case FuelKind::Methane:
                return {FuelKind::Methane, "CH4", 0.050, 0.150, 873.15, 2.0, 90000.0, 5.0e6};
            case FuelKind::Propane:
                return {FuelKind::Propane, "C3H8", 0.021, 0.095, 743.15, 5.0, 80000.0, 5.0e6};
            case FuelKind::Hydrogen:
                return {FuelKind::Hydrogen, "H2", 0.040, 0.750, 773.15, 0.5, 70000.0, 8.0e6};
        }
        return {};
    }

    [[nodiscard]] static bool is_flammable(const FuelFlammabilitySpec& spec, double fuel_vol_frac_in_air) noexcept {
        return fuel_vol_frac_in_air >= spec.lfl_vol_fraction && fuel_vol_frac_in_air <= spec.ufl_vol_fraction;
    }

    [[nodiscard]] static double equivalence_ratio(
        const FuelFlammabilitySpec& spec,
        double fuel_moles,
        double o2_moles) noexcept {
        if (o2_moles <= 0.0) return 1e9;
        return (fuel_moles / o2_moles) * spec.stoich_o2_per_fuel;
    }
};

} // namespace shunya::chem
