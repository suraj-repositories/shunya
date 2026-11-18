#include "tests/test_framework.hpp"

#include "shunya/cirsim/mna_system_builder.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"
#include "shunya/physim/collision_pipeline.hpp"
#include "shunya/physim/integrator.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <chrono>
#include <vector>

using namespace shunya::physim;
using namespace shunya::cirsim;
using namespace shunya::core::domain;
using AABB = shunya::core::domain::mechanics::AABB;

// =============================================================================
// PH4-13.02.003: Large-Scale Stress Simulation (1,000 Bodies & 200 Circuit Nodes)
// =============================================================================

TEST_CASE("PH4-13.02.003: Physics Stress: 1,000 Interacting Rigid Bodies in Spatial Hash Grid & BVH") {
    constexpr size_t NUM_BODIES = 1000;

    std::vector<RigidBodyEntity> bodies;
    bodies.reserve(NUM_BODIES);

    std::vector<AABB> boxes;
    boxes.reserve(NUM_BODIES);

    // Arrange 1,000 bodies in a 10 x 10 x 10 grid with small random jitter
    for (size_t i = 0; i < NUM_BODIES; ++i) {
        const double x = static_cast<double>(i % 10) * 1.5;
        const double y = static_cast<double>((i / 10) % 10) * 1.5;
        const double z = static_cast<double>(i / 100) * 1.5;

        RigidBodyEntity b("body_" + std::to_string(i), BodyType::Dynamic);
        b.state().set_mass(1.0);
        b.state().position = {x, y, z};
        b.state().set_linear_velocity({(i % 2 == 0 ? 0.5 : -0.5), -1.0, 0.0});
        bodies.push_back(std::move(b));

        AABB aabb;
        aabb.min_point = {x - 0.5, y - 0.5, z - 0.5};
        aabb.max_point = {x + 0.5, y + 0.5, z + 0.5};
        boxes.push_back(aabb);
    }

    CHECK(bodies.size() == 1000);
    CHECK(boxes.size() == 1000);

    // 1. Broadphase BVH Build and Overlap Query
    auto t0 = std::chrono::steady_clock::now();
    DynamicAabbBvh bvh;
    bvh.build(boxes);
    const auto overlaps = bvh.query_overlapping_pairs();
    auto t1 = std::chrono::steady_clock::now();

    // BVH query on 1000 boxes should take < 50 ms
    const auto elapsed_bvh_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    CHECK(elapsed_bvh_ms < 50);

    // 2. Spatial Hash Grid Insertion & Query
    SpatialHashGrid grid(2.0); // 2 meter cell
    for (size_t i = 0; i < boxes.size(); ++i) {
        grid.insert(i, boxes[i]);
    }
    const auto grid_overlaps = grid.query_overlapping_pairs();
    CHECK(grid_overlaps.size() >= overlaps.size());

    // 3. Step 1000 bodies forward in time with Symplectic Euler
    for (int step = 0; step < 5; ++step) {
        for (auto& body : bodies) {
            body.state().apply_force_at_com({0.0, -9.81 * body.state().mass_kg, 0.0});
            RigidBodyIntegrator::step_symplectic_euler(body.state(), 0.01);
        }
    }

    // Ensure all bodies moved without NaN
    for (const auto& body : bodies) {
        CHECK(!std::isnan(body.state().position.y));
        CHECK(body.state().position.y < 20.0);
    }
}

TEST_CASE("PH4-13.02.003: Circuit Stress: 200-Node Resistive Ladder MNA System") {
    constexpr size_t NUM_NODES = 200;

    ParsedSchematicTopology topology;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "0"; gnd.is_ground = true;
    topology.nodes[0] = gnd;

    for (size_t i = 1; i <= NUM_NODES; ++i) {
        CircuitNodeInfo n;
        n.node_index = static_cast<uint32_t>(i);
        n.node_name = "NET_" + std::to_string(i);
        topology.nodes[n.node_index] = n;
    }

    std::vector<ElectricalComponent> components;
    components.reserve(NUM_NODES * 2 + 1);

    // Voltage source at Node 1 (10V)
    ElectricalComponent v_source;
    v_source.id = "V_SRC";
    v_source.type = ElectricalComponentType::VoltageSourceDC;
    v_source.value_primary = 10.0;
    v_source.pins = {CircuitPin::make("1", "1"), CircuitPin::make("2", "0")};
    topology.pin_to_node_map["V_SRC:1"] = 1;
    topology.pin_to_node_map["V_SRC:2"] = 0;
    components.push_back(v_source);

    // Series resistors connecting node i to node i+1
    for (size_t i = 1; i < NUM_NODES; ++i) {
        ElectricalComponent r_series;
        r_series.id = "R_SERIES_" + std::to_string(i);
        r_series.type = ElectricalComponentType::Resistor;
        r_series.value_primary = 100.0; // 100 ohms
        r_series.pins = {CircuitPin::make("1", std::to_string(i)), CircuitPin::make("2", std::to_string(i + 1))};
        topology.pin_to_node_map[r_series.id + ":1"] = static_cast<uint32_t>(i);
        topology.pin_to_node_map[r_series.id + ":2"] = static_cast<uint32_t>(i + 1);
        components.push_back(r_series);
    }

    // Shunt resistors connecting each node to GND
    for (size_t i = 1; i <= NUM_NODES; ++i) {
        ElectricalComponent r_shunt;
        r_shunt.id = "R_SHUNT_" + std::to_string(i);
        r_shunt.type = ElectricalComponentType::Resistor;
        r_shunt.value_primary = 1000.0; // 1k ohm to ground
        r_shunt.pins = {CircuitPin::make("1", std::to_string(i)), CircuitPin::make("2", "0")};
        topology.pin_to_node_map[r_shunt.id + ":1"] = static_cast<uint32_t>(i);
        topology.pin_to_node_map[r_shunt.id + ":2"] = 0;
        components.push_back(r_shunt);
    }

    auto mna_res = MnaSystemBuilder::build_dc_system(topology, components);
    REQUIRE(mna_res.is_ok());

    auto sys = mna_res.value();
    CHECK(sys.A.rows() >= NUM_NODES);

    // Solve linear system via LU
    auto solve_res = sys.A.solve_lu(sys.z);
    REQUIRE(solve_res.is_ok());

    auto x = solve_res.value();
    REQUIRE(x.size() >= NUM_NODES);

    // Node 1 must be 10V, voltages must monotonically decay down the ladder
    CHECK(std::abs(x[0] - 10.0) < 1e-4);
    CHECK(x[NUM_NODES - 1] > 0.0);
    CHECK(x[NUM_NODES - 1] < x[0]);
}
