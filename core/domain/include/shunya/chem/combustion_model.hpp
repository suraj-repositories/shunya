#pragma once

#include "shunya/chem/flammability_limits.hpp"
#include "shunya/core/base/result.hpp"

#include <vector>

namespace shunya::chem {

struct CombustionMixtureState {
    double fuel_mol_m3{0.0};
    double o2_mol_m3{0.0};
    double n2_mol_m3{0.0};
    double co_mol_m3{0.0};
    double co2_mol_m3{0.0};
    double h2o_mol_m3{0.0};
    double temperature_k{298.15};
    bool ignited{false};
};

/**
 * @brief Multi-step Westbrook-Dryer hydrocarbon combustion kinetics with LFL/UFL and auto-ignition checks.
 */
class CombustionKineticsModel {
public:
    [[nodiscard]] static core::base::Result<CombustionMixtureState> simulate_combustion(
        FuelKind fuel,
        double fuel_mol_m3,
        double o2_mol_m3,
        double n2_mol_m3,
        double temperature_k,
        double duration_s,
        double dt_s = 0.001);
};

} // namespace shunya::chem
