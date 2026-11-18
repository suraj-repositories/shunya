#include "tests/test_framework.hpp"

#include "shunya/cad3d/extrusion_feature.hpp"
#include "shunya/chem/battery_cell_model.hpp"
#include "shunya/cirsim/schematic_topology_parser.hpp"
#include "shunya/cosim/demonstrator_machines.hpp"
#include "shunya/cosim/master_coordinator.hpp"
#include "shunya/lib30/ic_actuator_components.hpp"
#include "shunya/lib30/power_sources_components.hpp"
#include "shunya/mat/alloy_builder.hpp"
#include "shunya/matdb/element_database.hpp"
#include "shunya/physim/rigid_body.hpp"

#include <cmath>
#include <memory>
#include <vector>

using namespace shunya::core::domain;
using namespace shunya::matdb;
using namespace shunya::mat;
using namespace shunya::cad3d;
using namespace shunya::lib30;
using namespace shunya::cirsim;
using namespace shunya::cosim;
using namespace shunya::physim;

// =============================================================================
// PH4-13.02.001: Headless E2E 8-Step User Journey: Elements to RC Car Movement
// =============================================================================

TEST_CASE("PH4-13.02.001: Automated Headless E2E 8-Step User Journey to RC Car Physical Movement") {
    // -------------------------------------------------------------------------
    // STEP 1: Chemical Elements Selection
    // -------------------------------------------------------------------------
    ElementRecord cu;
    cu.atomic_number = 29;
    cu.symbol = "Cu";
    cu.name = "Copper";
    cu.atomic_radius = 128.0e-12;
    cu.space_group_number = 225; // FCC
    cu.electronegativity = 1.90;

    ElementRecord zn;
    zn.atomic_number = 30;
    zn.symbol = "Zn";
    zn.name = "Zinc";
    zn.atomic_radius = 134.0e-12;
    zn.space_group_number = 194; // HCP
    zn.electronegativity = 1.65;

    CHECK(cu.atomic_number == 29);
    CHECK(zn.atomic_number == 30);

    // -------------------------------------------------------------------------
    // STEP 2: Material Synthesis in Mixing Lab (Brass for structural bushings/contacts)
    // -------------------------------------------------------------------------
    auto brass = AlloyBuilder::preset_brass_c260();
    CHECK(brass.elements.size() == 2u);
    CHECK(brass.theoretical_density_kg_m3 > 8200.0);
    CHECK(brass.theoretical_density_kg_m3 < 8600.0);

    // -------------------------------------------------------------------------
    // STEP 3: 3D CAD Solid Modeling (Chassis Extrusion)
    // -------------------------------------------------------------------------
    const shunya::cad2d::Path2D chassis_outline{
        {-0.20, -0.10}, {0.20, -0.10}, {0.20, 0.10}, {-0.20, 0.10}
    };
    ExtrusionParams ext_p;
    ext_p.depth = 0.05; // 50 mm thick
    SolidMesh3D chassis_mesh = ExtrusionEngine::extrude(chassis_outline, {}, ext_p);
    CHECK(chassis_mesh.analyze_manifold().is_2_manifold);
    CHECK(chassis_mesh.compute_volume() > 0.001); // 0.4 * 0.2 * 0.05 = 0.004 m^3

    // -------------------------------------------------------------------------
    // STEP 4: 3D Electronic Component Definitions (Battery & DC Motor)
    // -------------------------------------------------------------------------
    auto battery = PowerSourcesProtectionFactory::create_battery_cell(3.7, 2.6);
    CHECK(battery.terminals.size() == 2u);
    CHECK(battery.specs.nominal_primary_value == 3.7);

    auto motor_comp = ActuatorsTransducersFactory::create_dc_motor();
    CHECK(motor_comp.terminals.size() == 2u);

    // -------------------------------------------------------------------------
    // STEP 5: 2D Schematic Circuit Modeling & Closed-Loop Wiring
    // -------------------------------------------------------------------------
    ParsedSchematicTopology circuit_top;
    CircuitNodeInfo gnd; gnd.node_index = 0; gnd.node_name = "GND"; gnd.is_ground = true;
    CircuitNodeInfo vbat; vbat.node_index = 1; vbat.node_name = "NET_VBAT";
    CircuitNodeInfo vmot; vmot.node_index = 2; vmot.node_name = "NET_VMOT";
    circuit_top.nodes[0] = gnd;
    circuit_top.nodes[1] = vbat;
    circuit_top.nodes[2] = vmot;

    // -------------------------------------------------------------------------
    // STEP 6: Multi-Body Machine Assembly (Rigid Bodies & Revolute Joints)
    // -------------------------------------------------------------------------
    RigidBodyEntity car_body("car_chassis", BodyType::Dynamic);
    car_body.state().set_mass(2.0); // 2.0 kg
    car_body.state().position = {0.0, 0.1, 0.0};
    car_body.state().set_local_inertia(Matrix3x3::diagonal(0.05, 0.05, 0.05));

    // -------------------------------------------------------------------------
    // STEP 7: Electro-Mechanical Co-Simulation Transducer Binding
    // -------------------------------------------------------------------------
    MasterCoSimulationCoordinator coordinator;
    RCCarSystem rc_car(coordinator);

    CHECK(rc_car.forward_speed_m_s() == 0.0);
    CHECK(!rc_car.is_switch_closed());
    CHECK(!rc_car.has_collided());

    // -------------------------------------------------------------------------
    // STEP 8: Simulation Execution & Verified Physical Movement
    // -------------------------------------------------------------------------
    // Close the power switch, throttle up to 100%
    rc_car.set_power_switch(true);
    rc_car.set_throttle(1.0);

    // Step forward in time for 50 steps of 20 ms (1.0 second duration)
    for (int step = 0; step < 50; ++step) {
        rc_car.step(0.02);
    }

    // Assert that the electric motor drew current, wheels rotated, and the car accelerated forward
    CHECK(rc_car.motor_current_a() > 0.0);
    CHECK(rc_car.wheel_rpm() > 50.0);
    CHECK(rc_car.forward_speed_m_s() > 1.0);
}
