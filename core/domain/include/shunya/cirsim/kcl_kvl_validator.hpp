#pragma once

#include "shunya/cirsim/mna_system_builder.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <string>
#include <vector>

namespace shunya::cirsim {

struct ConservationCheckResult {
    bool kcl_conserved{false};
    bool kvl_conserved{false};
    double max_kcl_residual_amperes{0.0};
    double max_kvl_residual_volts{0.0};
    std::vector<std::string> violation_reports;
};

/**
 * @brief Rigorous physical conservation law verifier (Kirchhoff's Current Law & Kirchhoff's Voltage Law).
 * Fulfills roadmap task PH4-08.01.006.
 */
class KclKvlValidator {
public:
    /**
     * @brief Verifies KCL at every node (sum of currents entering/leaving node = 0)
     * and KVL around every closed fundamental loop (sum of branch voltages = 0).
     */
    [[nodiscard]] static ConservationCheckResult verify_conservation(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components,
        const std::vector<double>& solved_node_voltages,
        const std::vector<double>& solved_branch_currents,
        double current_tolerance = 1.0e-5,
        double voltage_tolerance = 1.0e-5
    );
};

} // namespace shunya::cirsim
