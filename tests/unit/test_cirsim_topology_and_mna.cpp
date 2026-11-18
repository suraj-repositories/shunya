#include "tests/test_framework.hpp"

#include "shunya/cirsim/schematic_topology_parser.hpp"
#include "shunya/cirsim/mna_system_builder.hpp"
#include "shunya/cirsim/sparse_matrix.hpp"
#include "shunya/cirsim/matrix_conditioner.hpp"

#include <cmath>

using namespace shunya::cirsim;
using namespace shunya::core::domain;

TEST_CASE("PH4-08.01.001: Schematic Graph Parsing & Electrical Nodal Netlist Generation") {
    SchematicTopologyParser parser(1.0e-3); // 1mm snap tolerance

    // Component 1: V1 (Pins 1 and 2)
    parser.add_component_pin(SchematicPinAnchor{"V1", "1", 1, Point2D{0.0, 10.0}, ElectricalPinPolarity::Positive});
    parser.add_component_pin(SchematicPinAnchor{"V1", "2", 2, Point2D{0.0, 0.0},  ElectricalPinPolarity::Negative});

    // Component 2: R1 (Pins 1 and 2)
    parser.add_component_pin(SchematicPinAnchor{"R1", "1", 1, Point2D{10.0, 10.0}, ElectricalPinPolarity::PassiveBiDirectional});
    parser.add_component_pin(SchematicPinAnchor{"R1", "2", 2, Point2D{20.0, 10.0}, ElectricalPinPolarity::PassiveBiDirectional});

    // Wires connecting V1:1 to R1:1
    parser.add_wire(SchematicWire{"W1", Point2D{0.0, 10.0}, Point2D{10.0, 10.0}, "NET_VCC"});

    // Wire connecting V1:2 to Ground
    parser.add_wire(SchematicWire{"W2", Point2D{0.0, 0.0}, Point2D{0.0, -5.0}, "GND"});

    auto topology = parser.parse();

    REQUIRE(topology.has_ground_reference);
    REQUIRE(topology.nodes.size() >= 2);

    // V1:2 connects to Ground (Node 0)
    REQUIRE(topology.pin_to_node_map["V1:2"] == 0);
    // V1:1 and R1:1 are in the same non-ground cluster
    uint32_t node_vcc = topology.pin_to_node_map["V1:1"];
    REQUIRE(node_vcc > 0);
    REQUIRE(topology.pin_to_node_map["R1:1"] == node_vcc);
    REQUIRE(topology.nodes[node_vcc].node_name == "NET_VCC");
}

TEST_CASE("PH4-08.01.002: Automated Node Identification, Topological Loop Detection & Floating Node Flagging") {
    SchematicTopologyParser parser(1.0e-3);

    // Closed loop: V1 (0 to 10V) -> R1 -> R2 -> GND
    parser.add_component_pin(SchematicPinAnchor{"V1", "1", 1, Point2D{0.0, 10.0}, ElectricalPinPolarity::Positive});
    parser.add_component_pin(SchematicPinAnchor{"V1", "2", 2, Point2D{0.0, 0.0},  ElectricalPinPolarity::Negative});

    parser.add_component_pin(SchematicPinAnchor{"R1", "1", 1, Point2D{0.0, 10.0}, ElectricalPinPolarity::PassiveBiDirectional});
    parser.add_component_pin(SchematicPinAnchor{"R1", "2", 2, Point2D{20.0, 10.0}, ElectricalPinPolarity::PassiveBiDirectional});

    parser.add_component_pin(SchematicPinAnchor{"R2", "1", 1, Point2D{20.0, 10.0}, ElectricalPinPolarity::PassiveBiDirectional});
    parser.add_component_pin(SchematicPinAnchor{"R2", "2", 2, Point2D{20.0, 0.0}, ElectricalPinPolarity::PassiveBiDirectional});

    // Floating stub resistor R_FLOAT
    parser.add_component_pin(SchematicPinAnchor{"R_FLOAT", "1", 1, Point2D{50.0, 50.0}, ElectricalPinPolarity::PassiveBiDirectional});
    parser.add_component_pin(SchematicPinAnchor{"R_FLOAT", "2", 2, Point2D{60.0, 50.0}, ElectricalPinPolarity::PassiveBiDirectional});

    // Ground wire connecting V1:2 and R2:2
    parser.add_wire(SchematicWire{"W_GND", Point2D{0.0, 0.0}, Point2D{20.0, 0.0}, "0"});

    auto topology = parser.parse();

    // Floating nodes should be detected for R_FLOAT
    auto floating = SchematicTopologyParser::find_floating_nodes(topology);
    REQUIRE(floating.size() >= 2);
    REQUIRE(topology.floating_node_count >= 2);

    // Topological loop detection
    std::vector<ElectricalComponent> comps;
    ElectricalComponent v1; v1.id = "V1"; v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};
    ElectricalComponent r1; r1.id = "R1"; r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};
    ElectricalComponent r2; r2.id = "R2"; r2.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};
    comps = {v1, r1, r2};

    auto loops = SchematicTopologyParser::detect_topological_loops(topology, comps);
    REQUIRE(!loops.empty());
}

TEST_CASE("PH4-08.01.003: Standard Modified Nodal Analysis (MNA) Linear System Formulation") {
    // Construct standard DC voltage divider: V1 = 12V, R1 = 1k, R2 = 1k
    ParsedSchematicTopology topology;

    CircuitNodeInfo gnd_node;
    gnd_node.node_index = 0;
    gnd_node.node_name = "0";
    gnd_node.is_ground = true;
    topology.nodes[0] = gnd_node;

    CircuitNodeInfo node1;
    node1.node_index = 1;
    node1.node_name = "NET_1";
    topology.nodes[1] = node1;

    CircuitNodeInfo node2;
    node2.node_index = 2;
    node2.node_name = "NET_2";
    topology.nodes[2] = node2;

    topology.pin_to_node_map["V1:1"] = 1;
    topology.pin_to_node_map["V1:2"] = 0;
    topology.pin_to_node_map["R1:1"] = 1;
    topology.pin_to_node_map["R1:2"] = 2;
    topology.pin_to_node_map["R2:1"] = 2;
    topology.pin_to_node_map["R2:2"] = 0;

    std::vector<ElectricalComponent> comps;
    ElectricalComponent v1;
    v1.id = "V1";
    v1.type = ElectricalComponentType::VoltageSourceDC;
    v1.value_primary = 12.0;
    v1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};

    ElectricalComponent r1;
    r1.id = "R1";
    r1.type = ElectricalComponentType::Resistor;
    r1.value_primary = 1000.0;
    r1.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "2")};

    ElectricalComponent r2;
    r2.id = "R2";
    r2.type = ElectricalComponentType::Resistor;
    r2.value_primary = 1000.0;
    r2.pins = {CircuitPin::make("1", "2"), CircuitPin::make("2", "0")};

    comps = {v1, r1, r2};

    auto res = MnaSystemBuilder::build_dc_system(topology, comps);
    REQUIRE(res.is_ok());

    auto sys = res.value();
    REQUIRE(sys.num_nodes == 2);    // NET_1 and NET_2
    REQUIRE(sys.num_branches == 1); // V1 branch current
    REQUIRE(sys.A.rows() == 3);
    REQUIRE(sys.A.cols() == 3);

    // Solve [A] [x] = [z] using LU decomposition
    auto sol_res = sys.A.solve_lu(sys.z);
    REQUIRE(sol_res.is_ok());

    auto x = sol_res.value();
    REQUIRE(x.size() == 3);

    // Node 1 voltage must equal 12.0 V
    REQUIRE(std::abs(x[0] - 12.0) < 1e-6);

    // Node 2 voltage (midpoint of 1k / 1k divider) must equal 6.0 V
    REQUIRE(std::abs(x[1] - 6.0) < 1e-6);

    // Branch current of V1: I = -12V / 2000 Ohm = -6.0 mA (-0.006 A)
    REQUIRE(std::abs(x[2] - (-0.006)) < 1e-6);
}

TEST_CASE("PH4-08.01.004: Sparse Matrix Representations (CSR/CSC) for Large Electronic Schematics (>500 nodes)") {
    // Construct a large 600-node resistive ladder network
    constexpr size_t kNodes = 600;
    std::vector<MatrixEntryCOO> coo;

    // Tridiagonal structure typical of circuit ladder meshes
    for (size_t i = 0; i < kNodes; ++i) {
        coo.push_back(MatrixEntryCOO{i, i, 2.0}); // Diagonal
        if (i > 0) coo.push_back(MatrixEntryCOO{i, i - 1, -1.0}); // Sub-diagonal
        if (i + 1 < kNodes) coo.push_back(MatrixEntryCOO{i, i + 1, -1.0}); // Super-diagonal
    }

    auto csr = SparseMatrixCSR::from_coo(kNodes, kNodes, coo);

    REQUIRE(csr.rows() == kNodes);
    REQUIRE(csr.cols() == kNodes);
    REQUIRE(csr.non_zeros() == (3 * kNodes - 2));

    // Matrix density must be well under 1% for 600 nodes
    REQUIRE(csr.density() < 0.01);

    // Memory savings compared to full 600x600 dense matrix
    REQUIRE(csr.memory_savings_percent() > 95.0);

    // Fast SpMV product: A * [1, 1, ..., 1]^T
    std::vector<double> ones(kNodes, 1.0);
    auto y = csr.spmv(ones);
    REQUIRE(y.size() == kNodes);
    // Row 0 has entries 2.0 and -1.0 -> y[0] = 1.0
    REQUIRE(std::abs(y[0] - 1.0) < 1e-6);
    // Interior rows have 2.0 - 1.0 - 1.0 = 0.0
    REQUIRE(std::abs(y[1] - 0.0) < 1e-6);
    // Last row has entries -1.0 and 2.0 -> y[N-1] = 1.0
    REQUIRE(std::abs(y[kNodes - 1] - 1.0) < 1e-6);

    // Test conversion to CSC and SpMV equivalence
    auto csc = csr.to_csc();
    REQUIRE(csc.rows() == kNodes);
    REQUIRE(csc.cols() == kNodes);
    REQUIRE(csc.non_zeros() == csr.non_zeros());

    auto y_csc = csc.spmv(ones);
    for (size_t i = 0; i < kNodes; ++i) {
        REQUIRE(std::abs(y[i] - y_csc[i]) < 1e-12);
    }
}

TEST_CASE("PH4-08.01.005: Automated Matrix Conditioning, Row Scaling & Singularity Detection") {
    // 1. Ill-conditioned system with varying magnitudes (1e6 and 1e-3)
    DenseMatrix A(2, 2);
    A(0, 0) = 1.0e6; A(0, 1) = 2.0e6;
    A(1, 0) = 1.0e-3; A(1, 1) = 3.0e-3;
    std::vector<double> z = {3.0e6, 4.0e-3};

    auto rep = MatrixConditioner::condition_system(A, z, {"Row_Mega", "Row_Milli"});

    REQUIRE(!rep.is_singular);
    REQUIRE(rep.row_scale_factors.size() == 2);
    // Row 0 scaled by 1/2e6
    REQUIRE(std::abs(rep.row_scale_factors[0] - 5.0e-7) < 1e-12);
    // Row 1 scaled by 1/3e-3
    REQUIRE(std::abs(rep.row_scale_factors[1] - (1.0 / 3.0e-3)) < 1e-6);

    // Max absolute element in each scaled row must be exactly 1.0
    double r0_max = std::max(std::abs(rep.scaled_matrix(0, 0)), std::abs(rep.scaled_matrix(0, 1)));
    double r1_max = std::max(std::abs(rep.scaled_matrix(1, 0)), std::abs(rep.scaled_matrix(1, 1)));
    REQUIRE(std::abs(r0_max - 1.0) < 1e-9);
    REQUIRE(std::abs(r1_max - 1.0) < 1e-9);

    // 2. Singular system detection (zero row from floating/disconnected node)
    DenseMatrix A_sing(2, 2);
    A_sing(0, 0) = 5.0; A_sing(0, 1) = -5.0;
    A_sing(1, 0) = 0.0; A_sing(1, 1) = 0.0; // All zeros
    std::vector<double> z_sing = {5.0, 0.0};

    auto rep_sing = MatrixConditioner::condition_system(A_sing, z_sing, {"Node1", "FloatingNode"});
    REQUIRE(rep_sing.is_singular);
    REQUIRE(!rep_sing.diagnostic_messages.empty());
}
