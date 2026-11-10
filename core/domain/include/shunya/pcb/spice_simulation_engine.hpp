#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/circuit.hpp"

#include <cstdint>
#include <string>

namespace shunya::pcb {

enum class SpiceIntegrationMethod : uint8_t {
    BackwardEuler,
    Trapezoidal,
    Gear2
};

struct SpiceTransientOptions {
    double t_stop_s{1e-3};
    double dt_s{1e-6};
    SpiceIntegrationMethod method{SpiceIntegrationMethod::Trapezoidal};
    bool use_dc_op_initial{true};
    uint32_t max_newton_iters{60};
    double voltage_tol_v{1e-6};
    double reltol{1e-4};
    double gmin{1e-12};
};

struct SpiceAcOptions {
    double f_start_hz{1.0};
    double f_stop_hz{1e6};
    uint32_t points_per_decade{20};
    std::string input_source_id{"V1"};
    std::string output_node{"out"};
};

/**
 * @brief Full MNA SPICE engine supporting Newton-Raphson DC OP, BE/Trapezoidal/Gear-2 Transient, and AC small-signal sweeps.
 */
class SpiceSimulationEngine {
public:
    [[nodiscard]] static core::base::Result<core::domain::DCOperatingPointResult> solve_dc(
        const core::domain::CircuitNetlist& netlist,
        uint32_t max_newton_iters = 100,
        double voltage_tol_v = 1e-6,
        double gmin = 1e-12);

    [[nodiscard]] static core::base::Result<core::domain::TransientResult> solve_transient(
        const core::domain::CircuitNetlist& netlist,
        const SpiceTransientOptions& opts = {});

    [[nodiscard]] static core::base::Result<core::domain::AcAnalysisResult> solve_ac(
        const core::domain::CircuitNetlist& netlist,
        const SpiceAcOptions& opts = {});
};

} // namespace shunya::pcb
