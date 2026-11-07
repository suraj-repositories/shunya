#include "tests/test_framework.hpp"
#include "shunya/core/domain/device.hpp"

#include <cmath>
#include <vector>

using namespace shunya::core::domain::device;
using namespace shunya::core::base::math;

// =============================================================================
// DEV-01: Device Assembly Structure & Ports
// =============================================================================

TEST_CASE("DEV-01: Device Assembly Hierarchy and Ports") {
    DeviceAssembly assembly("dev_heat_sink_board");
    CHECK(assembly.assembly_id() == "dev_heat_sink_board");

    DevicePart pcb;
    pcb.part_id = "pcb_main";
    pcb.name = "Main Board FR4";
    pcb.primitive.type = GeometricPrimitive::Type::Box;
    pcb.primitive.dimensions = {0.1, 0.0016, 0.1}; // 100 mm x 1.6 mm x 100 mm

    DevicePort p1;
    p1.port_id = "vin";
    p1.domain = "electrical";
    p1.connected_net = "VCC";
    pcb.ports.push_back(p1);

    assembly.add_part(pcb);
    CHECK(assembly.parts().size() == 1u);

    const auto* retrieved = assembly.get_part("pcb_main");
    REQUIRE(retrieved != nullptr);
    CHECK(retrieved->name == "Main Board FR4");
    CHECK(retrieved->ports.size() == 1u);
    CHECK(retrieved->ports[0].connected_net == "VCC");
}

// =============================================================================
// DEV-02: CAD Import Pipeline and Mesh Representation
// =============================================================================

TEST_CASE("DEV-02: CAD Import and Mesh Generation") {
    auto res = CadImportPipeline::import_mock_cad("models/enclosure.step", CadFormat::Step, {0.1, 0.05, 0.02});
    REQUIRE(res.is_ok());

    const auto& mesh = res.value();
    CHECK(mesh.is_valid());
    CHECK(mesh.format == CadFormat::Step);
    CHECK(mesh.vertices.size() == 8u);
    CHECK(mesh.triangle_indices.size() == 12u);
    CHECK(approx_equal(mesh.estimated_volume, 0.1 * 0.05 * 0.02));

    // Negative bounding box should fail
    auto fail_res = CadImportPipeline::import_mock_cad("bad.obj", CadFormat::Obj, {-1.0, 1.0, 1.0});
    CHECK(fail_res.is_err());
}

// =============================================================================
// DEV-03: Geometry-to-Physics Mapping & Mass Calculation
// =============================================================================

TEST_CASE("DEV-03: Geometry Primitives Volume and Mass") {
    DevicePart sphere_part;
    sphere_part.part_id = "copper_ball";
    sphere_part.primitive.type = GeometricPrimitive::Type::Sphere;
    sphere_part.primitive.radius = 0.05; // 5 cm radius
    sphere_part.density = 8960.0;        // Copper ~8960 kg/m^3

    // Volume = (4/3) * pi * r^3 = (4/3) * pi * 0.000125 ~ 5.235987e-4 m^3
    const double expected_vol = (4.0 / 3.0) * PI * 0.05 * 0.05 * 0.05;
    CHECK(approx_equal(sphere_part.compute_volume(), expected_vol, 1e-6));

    // Mass = vol * density
    const double expected_mass = expected_vol * 8960.0;
    CHECK(approx_equal(sphere_part.compute_mass(), expected_mass, 1e-3));
}

// =============================================================================
// DEV-04: Layered & Composite Construction
// =============================================================================

TEST_CASE("DEV-04: Composite Stackup Properties and Areal Capacitance") {
    CompositeStackup stack;
    stack.stackup_id = "pcb_4layer";

    DeviceLayer cu_top{"Cu_Top", LayerType::Conductor, 35e-6, 400.0, 5.8e7, 1.0, 8960.0};
    DeviceLayer fr4{"FR4_Core", LayerType::Dielectric, 1.5e-3, 0.3, 1e-12, 4.4, 1850.0};
    DeviceLayer cu_bot{"Cu_Bot", LayerType::Conductor, 35e-6, 400.0, 5.8e7, 1.0, 8960.0};

    stack.layers = {cu_top, fr4, cu_bot};

    // Total thickness = 35 um + 1.5 mm + 35 um = 1.57 mm
    CHECK(approx_equal(stack.total_thickness(), 1.57e-3, 1e-6));

    // Effective thermal conductivity through thickness: L_tot / sum(L_i / k_i)
    // R_th = 35e-6 / 400 + 1.5e-3 / 0.3 + 35e-6 / 400 = 8.75e-8 + 0.005 + 8.75e-8 ~ 0.005000175 K*m^2/W
    // k_eff = 1.57e-3 / 0.005000175 ~ 0.31398 W/(m*K)
    CHECK(approx_equal(stack.effective_thermal_conductivity(), 0.31398, 0.01));

    // Areal capacitance across dielectric: eps_0 * eps_r / t = 8.854187e-12 * 4.4 / 1.5e-3 ~ 2.597e-8 F/m^2
    CHECK(stack.areal_capacitance() > 1e-9);
}

// =============================================================================
// DEV-05: Conductor Paths & Trace Resistance
// =============================================================================

TEST_CASE("DEV-05: Conductor Path Geometry and Resistance") {
    ConductorPath trace;
    trace.trace_id = "t1";
    trace.net_name = "NET_PWM";
    trace.width = 0.25e-3;   // 0.25 mm
    trace.thickness = 35e-6; // 35 um
    trace.resistivity = 1.68e-8; // Copper

    // L-shaped trace: (0,0,0) -> (0.05, 0, 0) -> (0.05, 0.05, 0) => total length = 0.10 m (100 mm)
    trace.waypoints = {
        {0.0, 0.0, 0.0},
        {0.05, 0.0, 0.0},
        {0.05, 0.05, 0.0}
    };

    CHECK(approx_equal(trace.length(), 0.10, 1e-6));

    // R = rho * L / (w * t) = 1.68e-8 * 0.10 / (0.25e-3 * 35e-6) = 1.68e-9 / 8.75e-9 = 0.192 Ohms
    CHECK(approx_equal(trace.resistance(), 0.192, 0.005));
}

// =============================================================================
// DEV-06: Interface Models (Electrical, Thermal, Mechanical)
// =============================================================================

TEST_CASE("DEV-06: Multi-Domain Interfaces and Thermal Pads") {
    DeviceContactInterface tim;
    tim.interface_id = "tim_cpu_heatsink";
    tim.domain = ContactDomain::Thermal;
    tim.part_a_id = "cpu_package";
    tim.part_b_id = "heatsink";
    tim.thermal_interface_r = 0.15; // 0.15 K/W
    tim.contact_area = 4e-4;        // 20 mm x 20 mm

    CHECK(tim.interface_id == "tim_cpu_heatsink");
    CHECK(tim.domain == ContactDomain::Thermal);
    CHECK(approx_equal(tim.thermal_interface_r, 0.15));
}

// =============================================================================
// DEV-07 & DEV-08: Assembly Mates & Attachment
// =============================================================================

TEST_CASE("DEV-08: Assembly Mates and Alignment") {
    AssemblyMate mate;
    mate.mate_id = "m1";
    mate.type = MateType::Concentric;
    mate.part_a_id = "shaft";
    mate.part_b_id = "bearing";
    mate.target_distance = 0.0;
    mate.is_satisfied = true;

    CHECK(mate.type == MateType::Concentric);
    CHECK(mate.is_satisfied);
}

// =============================================================================
// DEV-09: Sensors and Probes
// =============================================================================

TEST_CASE("DEV-09: Sensors and Non-Invasive Instrumentation") {
    auto sensor = std::make_shared<DeviceSensor>("temp_sense", SensorType::Temperature, "cpu_die");
    CHECK(sensor->sensor_id() == "temp_sense");
    CHECK(sensor->type() == SensorType::Temperature);

    sensor->record_scalar(0.0, 25.0, "C");
    sensor->record_scalar(1.0, 45.0, "C");

    CHECK(sensor->readings().size() == 2u);
    auto latest = sensor->latest_reading();
    REQUIRE(latest.has_value());
    CHECK(latest->timestamp == 1.0);
    CHECK(latest->scalar_value == 45.0);
    CHECK(latest->unit == "C");

    // Vector sensor (Accelerometer)
    auto accel = std::make_shared<DeviceSensor>("imu_accel", SensorType::Accelerometer, "chassis");
    accel->record_vector(0.5, {0.0, -9.81, 0.0}, "m/s^2");
    auto acc_latest = accel->latest_reading();
    REQUIRE(acc_latest.has_value());
    CHECK(approx_equal(acc_latest->scalar_value, 9.81, 0.01));
    CHECK(acc_latest->vector_value.y == -9.81);
}

// =============================================================================
// DEV-10: Device Diagnostics (Continuity, Floating Ports, Center of Mass)
// =============================================================================

TEST_CASE("DEV-10: Diagnostics for Mass, Center of Mass, and Connectivity") {
    DeviceAssembly dev("multi_part_system");

    // Part 1 at origin: mass = 2 kg
    DevicePart p1;
    p1.part_id = "base";
    p1.local_position = {0.0, 0.0, 0.0};
    p1.primitive.type = GeometricPrimitive::Type::Box;
    p1.primitive.dimensions = {1.0, 1.0, 1.0}; // vol = 1 m^3
    p1.density = 2.0; // mass = 2.0 kg
    dev.add_part(p1);

    // Part 2 at (10, 0, 0): mass = 2 kg
    DevicePart p2;
    p2.part_id = "satellite";
    p2.local_position = {10.0, 0.0, 0.0};
    p2.primitive.type = GeometricPrimitive::Type::Box;
    p2.primitive.dimensions = {1.0, 1.0, 1.0};
    p2.density = 2.0; // mass = 2.0 kg
    dev.add_part(p2);

    auto report = dev.run_diagnostics();
    CHECK(approx_equal(report.total_mass, 4.0));
    // Center of mass should be midway: (5.0, 0.0, 0.0)
    CHECK(approx_equal(report.center_of_mass.x, 5.0));
    CHECK(approx_equal(report.center_of_mass.y, 0.0));
    CHECK(approx_equal(report.center_of_mass.z, 0.0));
}

// =============================================================================
// DEV-11: Component Variants & Parameter Overrides
// =============================================================================

TEST_CASE("DEV-11: Variants and Parameter Sweep Configuration") {
    DeviceAssembly dev("resistor_network");

    DeviceVariant v1;
    v1.variant_id = "high_power";
    v1.label = "50W Ceramic Shunt";
    v1.parameter_overrides["R_val"] = 0.05;
    v1.parameter_overrides["P_max"] = 50.0;
    dev.add_variant(v1);

    DeviceVariant v2;
    v2.variant_id = "precision";
    v2.label = "0.1% Precision Shunt";
    v2.parameter_overrides["R_val"] = 0.05;
    v2.parameter_overrides["P_max"] = 5.0;
    dev.add_variant(v2);

    CHECK(dev.variants().size() == 2u);
    CHECK(dev.variants()[0].parameter_overrides.at("P_max") == 50.0);
    CHECK(dev.variants()[1].parameter_overrides.at("P_max") == 5.0);
}

// =============================================================================
// DEV-12: Geometric Fidelity and Solver Mesh Limitation Auditor
// =============================================================================

TEST_CASE("DEV-12: Mesh Limitation and Small Feature Auditor") {
    DeviceAssembly dev("pcb_assembly");

    // Trace with width 0.1 mm (100 um)
    ConductorPath fine_trace;
    fine_trace.trace_id = "trace_lvds";
    fine_trace.net_name = "LVDS_P";
    fine_trace.width = 100e-6;   // 100 um
    fine_trace.thickness = 35e-6; // 35 um
    fine_trace.waypoints = {{0,0,0}, {0.01, 0, 0}};
    dev.add_conductor_path(fine_trace);

    // Audit with coarse solver mesh resolution of 1 mm (1e-3 m)
    auto issues = dev.audit_geometric_fidelity(1e-3);
    // Since 35 um < 1 mm, it should flag a limitation warning
    CHECK(!issues.empty());
    CHECK(issues[0].feature_size < 1e-3);
}
