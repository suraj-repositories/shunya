#pragma once

#include "shunya/mat/varshni_model.hpp"

namespace shunya::mat {

/**
 * @brief Evaluates temperature-dependent metallic resistivity, Joule self-heating, and Varshni bandgaps.
 */
class ElectricalPropertyEvaluator {
public:
    [[nodiscard]] static double metal_resistivity_ohm_m(const MetalResistivityParams& p,
                                                        double temp_k) noexcept;

    /**
     * @brief Explicit Joule self-heating step for resistor wire at fixed current (thermal runaway model).
     */
    [[nodiscard]] static double step_resistor_temperature_k(const MetalResistivityParams& p,
                                                            double current_temp_k,
                                                            double current_a,
                                                            double length_m,
                                                            double area_m2,
                                                            double thermal_mass_j_k,
                                                            double ambient_conductance_w_k,
                                                            double ambient_temp_k,
                                                            double dt_s) noexcept;

    [[nodiscard]] static double varshni_bandgap_ev(const SemiconductorVarshniParams& p,
                                                   double temp_k) noexcept;

    [[nodiscard]] static double intrinsic_carrier_cm3(const SemiconductorVarshniParams& p,
                                                      double temp_k) noexcept;

    [[nodiscard]] static MetalResistivityParams copper();
    [[nodiscard]] static SemiconductorVarshniParams silicon();
    [[nodiscard]] static SemiconductorVarshniParams germanium();
    [[nodiscard]] static SemiconductorVarshniParams gallium_arsenide();
};

} // namespace shunya::mat
