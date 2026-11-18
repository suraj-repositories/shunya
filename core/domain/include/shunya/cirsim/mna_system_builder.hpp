#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"

#include <string>
#include <vector>

namespace shunya::cirsim {

struct MnaBlockMatrix {
    size_t num_nodes{0};    // N (non-ground nodes)
    size_t num_branches{0}; // M (auxiliary branch currents)

    // Block matrices
    core::domain::DenseMatrix G; // N x N conductance matrix
    core::domain::DenseMatrix B; // N x M voltage source connection matrix
    core::domain::DenseMatrix C; // M x N constraint matrix (B^T for independent sources)
    core::domain::DenseMatrix D; // M x M source impedance matrix

    // RHS vectors
    std::vector<double> i; // N x 1 nodal injected currents
    std::vector<double> e; // M x 1 branch voltage specifications

    // Unified system [A] [x] = [z]
    core::domain::DenseMatrix A; // (N+M) x (N+M)
    std::vector<double> z;       // (N+M) x 1

    std::vector<std::string> branch_labels; // "V_SRC1", "V_BAT", etc.
    std::vector<std::string> node_labels;   // "NET_1", "NET_2", etc.
};

/**
 * @brief Standard Modified Nodal Analysis (MNA) linear system formulator.
 * Fulfills roadmap task PH4-08.01.003.
 */
class MnaSystemBuilder {
public:
    [[nodiscard]] static core::base::Result<MnaBlockMatrix> build_dc_system(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components
    );

    [[nodiscard]] static core::base::Result<MnaBlockMatrix> build_transient_companion_system(
        const ParsedSchematicTopology& topology,
        const std::vector<core::domain::ElectricalComponent>& components,
        double dt,
        const std::unordered_map<std::string, double>& cap_voltages_prev,
        const std::unordered_map<std::string, double>& ind_currents_prev
    );
};

} // namespace shunya::cirsim
