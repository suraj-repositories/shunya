#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/cirsim/mna_system_builder.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::cirsim {

struct ComponentPowerStress {
    std::string component_id;
    double voltage_drop{0.0};
    double current{0.0};
    double power_dissipated_watts{0.0};
    double max_power_rating_watts{0.25}; // default 1/4 W
    double stress_ratio{0.0};             // power / max_rating
    bool is_over_stressed{false};
};

struct CircuitDiagnosticReport {
    enum class Severity { Warning, CriticalError };
    Severity severity{Severity::Warning};
    std::string code;
    std::string message;
    std::string component_id;
    uint32_t node_from{0};
    uint32_t node_to{0};
    double current_level{0.0};
};

struct NonlinearDcResult {
    bool converged{false};
    uint32_t iterations{0};
    double final_residual{0.0};
    bool used_source_stepping{false};
    bool used_gmin_stepping{false};

    // Node index -> solved DC voltage (ground is node 0 = 0V)
    std::unordered_map<uint32_t, double> node_voltages;
    // Component ID -> branch current (A)
    std::unordered_map<std::string, double> branch_currents;

    // Energy & Stress Analysis
    double total_power_delivered_watts{0.0};
    double total_power_dissipated_watts{0.0};
    std::vector<ComponentPowerStress> component_stresses;

    // Diagnostics (Short circuits, over-currents)
    std::vector<CircuitDiagnosticReport> diagnostics;
};

struct NonlinearSolverOptions {
    uint32_t max_iterations{200};
    double voltage_tolerance{1.0e-6}; // 1 uV
    double current_tolerance{1.0e-9}; // 1 nA
    double damping_factor{0.7};       // alpha in (0, 1]
    double nominal_gmin{1.0e-12};     // 1 pS minimum conductance
    bool enable_source_stepping{true};
    uint32_t source_steps{20};
    bool enable_gmin_stepping{true};
    double max_current_limit{50.0};    // 50 A over-current threshold
};

/**
 * @brief Damped Newton-Raphson Non-Linear DC Operating Point Solver with
 * dynamic companion linearizations, source/Gmin stepping, over-current diagnostics,
 * and component power dissipation stress calculations.
 * Fulfills roadmap tasks PH4-08.02.001 through PH4-08.02.006.
 */
class NonlinearDcOperatingPointSolver {
public:
    explicit NonlinearDcOperatingPointSolver(NonlinearSolverOptions options = {})
        : options_(options) {}

    /**
     * @brief Solves the non-linear DC operating point of the circuit.
     */
    [[nodiscard]] core::base::Result<NonlinearDcResult> solve(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components
    );

    /**
     * @brief Helper for SPICE pn-junction voltage limiting to avoid exponential overflow.
     */
    [[nodiscard]] static double limit_pn_voltage(double v_new, double v_old, double vt = 0.02585);

    /**
     * @brief Companion dynamic conductance linearization for Diode.
     */
    static void linearize_diode(
        double v_d,
        double is,
        double vt,
        double n,
        double& out_g_eq,
        double& out_i_eq
    );

    /**
     * @brief Companion dynamic conductance linearization for BJT.
     */
    static void linearize_bjt_npn(
        double v_be,
        double v_ce,
        double is,
        double vt,
        double beta,
        double& out_g_be,
        double& out_g_m,
        double& out_g_ce,
        double& out_i_be_eq,
        double& out_i_c_eq
    );

    /**
     * @brief Companion dynamic conductance linearization for NMOS.
     */
    static void linearize_nmos(
        double v_gs,
        double v_ds,
        double v_th,
        double kp,
        double lambda,
        double& out_g_m,
        double& out_g_ds,
        double& out_i_ds_eq
    );

private:
    NonlinearSolverOptions options_;

    [[nodiscard]] core::base::Result<NonlinearDcResult> solve_newton_internal(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components,
        double source_scale,
        double added_gmin,
        std::unordered_map<uint32_t, double> v_initial
    );

    void detect_short_circuits_and_stress(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components,
        NonlinearDcResult& result
    );
};

} // namespace shunya::cirsim
