#pragma once

#include "shunya/core/base/result.hpp"

#include <cmath>

namespace shunya::pcb {

struct MicrostripParams {
    double trace_width_mm{0.35};
    double trace_thickness_mm{0.035}; // 1 oz copper ~ 0.035 mm
    double dielectric_height_mm{0.20};
    double epsilon_r{4.4};
};

struct StriplineParams {
    double trace_width_mm{0.18};
    double trace_thickness_mm{0.035};
    double plane_spacing_mm{0.40}; // Total distance b between reference planes
    double epsilon_r{4.4};
};

/**
 * @brief IPC-2141 controlled-impedance microstrip and stripline calculators.
 */
class ImpedanceCalculator {
public:
    /**
     * @brief Computes single-ended surface microstrip Z_0 (Ohms) per IPC-2141A.
     */
    [[nodiscard]] static core::base::Result<double> microstrip_z0_ipc2141(const MicrostripParams& p);

    /**
     * @brief Computes single-ended symmetric stripline Z_0 (Ohms) per IPC-2141.
     */
    [[nodiscard]] static core::base::Result<double> stripline_z0_ipc2141(const StriplineParams& p);

    /**
     * @brief Solves microstrip trace width (mm) required to achieve target Z_0 (Ohms).
     */
    [[nodiscard]] static core::base::Result<double> solve_microstrip_width_mm(
        double target_z0_ohm,
        double trace_thickness_mm,
        double dielectric_height_mm,
        double epsilon_r);
};

} // namespace shunya::pcb
