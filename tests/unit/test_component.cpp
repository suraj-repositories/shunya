#include "tests/test_framework.hpp"
#include "shunya/core/domain/component.hpp"
#include "shunya/core/domain/component_runtime.hpp"

#include <cmath>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("Component contract, ports, parameters and metadata (CMP-01)") {
    auto id = base::Uuid::generate_random();
    ComponentDefinition comp(id, "Resistor", "Ideal Resistor", base::Version(1, 2, 0));
    comp.set_description("Two-terminal electrical passive resistor");
    comp.set_author("Shunya Core Team");
    comp.set_license("Apache-2.0");
    comp.add_tag("passive");
    comp.add_tag("electronics");

    REQUIRE(comp.id() == id);
    REQUIRE(comp.type_name() == "Resistor");
    REQUIRE(comp.display_name() == "Ideal Resistor");
    REQUIRE(comp.version() == base::Version(1, 2, 0));
    REQUIRE(comp.has_tag("passive"));
    REQUIRE(comp.has_tag("electronics"));
    REQUIRE_FALSE(comp.has_tag("sensor"));

    // Add Electrical Ports (CMP-01)
    ComponentPort p1;
    p1.schema.name = "p";
    p1.schema.domain = PortDomain::Electrical;
    p1.schema.direction = PortDirection::Bidirectional;
    p1.encapsulation = EncapsulationLevel::Public;
    REQUIRE(comp.add_port(p1).is_ok());

    ComponentPort p2;
    p2.schema.name = "n";
    p2.schema.domain = PortDomain::Electrical;
    p2.schema.direction = PortDirection::Bidirectional;
    p2.encapsulation = EncapsulationLevel::Public;
    REQUIRE(comp.add_port(p2).is_ok());

    // Duplicate port rejection
    REQUIRE(comp.add_port(p1).is_err());
    REQUIRE(comp.ports().size() == 2);
    REQUIRE(comp.has_port("p"));
    REQUIRE(comp.has_port("n"));

    // Add Resistance Parameter (CMP-01)
    ParameterSchema r_param;
    r_param.name = "R";
    r_param.display_name = "Resistance";
    r_param.type = ParameterType::Double;
    r_param.default_value = 1000.0;
    r_param.unit = "Ohm";
    r_param.min_value = 0.0;
    REQUIRE(comp.add_parameter(r_param).is_ok());

    // Duplicate parameter rejection
    REQUIRE(comp.add_parameter(r_param).is_err());
    REQUIRE(comp.has_parameter("R"));
    REQUIRE(comp.get_parameter("R")->default_value.as_double() == 1000.0);
}

TEST_CASE("Hierarchical composition and internal wiring (CMP-02)") {
    ComponentDefinition low_pass("LowPassFilter", "Passive RC Low-Pass Filter");

    // Boundary ports of the composite filter
    ComponentPort in_port;
    in_port.schema.name = "vin";
    in_port.schema.domain = PortDomain::Electrical;
    in_port.encapsulation = EncapsulationLevel::Public;
    REQUIRE(low_pass.add_port(in_port).is_ok());

    ComponentPort out_port;
    out_port.schema.name = "vout";
    out_port.schema.domain = PortDomain::Electrical;
    out_port.encapsulation = EncapsulationLevel::Public;
    REQUIRE(low_pass.add_port(out_port).is_ok());

    ComponentPort gnd_port;
    gnd_port.schema.name = "gnd";
    gnd_port.schema.domain = PortDomain::Electrical;
    gnd_port.encapsulation = EncapsulationLevel::Public;
    REQUIRE(low_pass.add_port(gnd_port).is_ok());

    // Add child nodes: Resistor R1 and Capacitor C1 (CMP-02)
    ChildComponentNode r1;
    r1.instance_name = "R1";
    r1.definition_id = base::Uuid::generate_random();
    r1.type_name = "Resistor";
    r1.parameter_overrides["R"] = 1000.0; // 1k Ohm
    REQUIRE(low_pass.add_child(r1).is_ok());

    // Duplicate child rejection
    REQUIRE(low_pass.add_child(r1).is_err());

    ChildComponentNode c1;
    c1.instance_name = "C1";
    c1.definition_id = base::Uuid::generate_random();
    c1.type_name = "Capacitor";
    c1.parameter_overrides["C"] = 1.0e-6; // 1 uF
    REQUIRE(low_pass.add_child(c1).is_ok());

    REQUIRE(low_pass.is_composite());
    REQUIRE(low_pass.children().size() == 2);
    REQUIRE(low_pass.has_child("R1"));
    REQUIRE(low_pass.has_child("C1"));

    // Wire internal topology:
    // vin -> R1.p
    InternalConnection c_in;
    c_in.from_child = ""; // parent boundary
    c_in.from_port = "vin";
    c_in.to_child = "R1";
    c_in.to_port = "p";
    REQUIRE(low_pass.add_connection(c_in).is_ok());

    // R1.n -> C1.p
    InternalConnection c_mid;
    c_mid.from_child = "R1";
    c_mid.from_port = "n";
    c_mid.to_child = "C1";
    c_mid.to_port = "p";
    REQUIRE(low_pass.add_connection(c_mid).is_ok());

    // C1.p -> vout
    InternalConnection c_out;
    c_out.from_child = "C1";
    c_out.from_port = "p";
    c_out.to_child = ""; // parent boundary
    c_out.to_port = "vout";
    REQUIRE(low_pass.add_connection(c_out).is_ok());

    // C1.n -> gnd
    InternalConnection c_gnd;
    c_gnd.from_child = "C1";
    c_gnd.from_port = "n";
    c_gnd.to_child = ""; // parent boundary
    c_gnd.to_port = "gnd";
    REQUIRE(low_pass.add_connection(c_gnd).is_ok());

    REQUIRE(low_pass.internal_connections().size() == 4);

    // Validation of well-formed composite component (CMP-05)
    auto rep = low_pass.validate();
    REQUIRE_FALSE(rep.has_errors());
}

TEST_CASE("Encapsulation levels and public port exposure (CMP-03)") {
    ComponentDefinition comp("RegulatedSupply", "5V Regulated Linear Supply");

    // Public external interface
    ComponentPort p_in;
    p_in.schema.name = "vin_raw";
    p_in.encapsulation = EncapsulationLevel::Public;
    REQUIRE(comp.add_port(p_in).is_ok());

    ComponentPort p_out;
    p_out.schema.name = "vout_reg";
    p_out.encapsulation = EncapsulationLevel::Public;
    REQUIRE(comp.add_port(p_out).is_ok());

    // Protected calibration / test port
    ComponentPort p_calib;
    p_calib.schema.name = "v_trim_test";
    p_calib.encapsulation = EncapsulationLevel::Protected;
    REQUIRE(comp.add_port(p_calib).is_ok());

    // Internal private sensing node (hidden topology)
    ComponentPort p_internal_feedback;
    p_internal_feedback.schema.name = "int_fb_node";
    p_internal_feedback.encapsulation = EncapsulationLevel::InternalPrivate;
    p_internal_feedback.is_inspectable = false;
    REQUIRE(comp.add_port(p_internal_feedback).is_ok());

    REQUIRE(comp.ports().size() == 4);

    // Public ports query returns only Public interfaces
    auto pub_ports = comp.public_ports();
    REQUIRE(pub_ports.size() == 2);
    REQUIRE((pub_ports[0].schema.name == "vin_raw" || pub_ports[0].schema.name == "vout_reg"));
    REQUIRE((pub_ports[1].schema.name == "vin_raw" || pub_ports[1].schema.name == "vout_reg"));

    // Check encapsulation string round-trip
    REQUIRE(encapsulation_level_to_string(EncapsulationLevel::Public) == "Public");
    REQUIRE(encapsulation_level_to_string(EncapsulationLevel::Protected) == "Protected");
    REQUIRE(encapsulation_level_to_string(EncapsulationLevel::InternalPrivate) == "InternalPrivate");
    REQUIRE(encapsulation_level_from_string("Protected").value() == EncapsulationLevel::Protected);
}

TEST_CASE("Component variants across fidelity tiers L0 to L4 (CMP-04)") {
    ComponentDefinition diode("Diode", "Semiconductor PN Diode");

    // Default variant is L1
    REQUIRE(diode.active_variant().fidelity == FidelityTier::L1_Functional);

    // Variant 1: L0 Visual / Form-factor
    ComponentVariant v_l0;
    v_l0.variant_id = "v_l0_visual";
    v_l0.display_name = "DO-41 Package 3D Model";
    v_l0.fidelity = FidelityTier::L0_Visual;
    v_l0.implementation_type = "Mesh3D";
    REQUIRE(diode.add_variant(v_l0).is_ok());

    // Variant 2: L1 Ideal Switch
    ComponentVariant v_l1;
    v_l1.variant_id = "v_l1_ideal";
    v_l1.display_name = "Piecewise Linear Ideal Diode";
    v_l1.fidelity = FidelityTier::L1_Functional;
    v_l1.implementation_type = "Analytical";
    v_l1.model_expression = "i = (v > 0.7) ? (v - 0.7)/0.01 : 0.0";
    REQUIRE(diode.add_variant(v_l1).is_ok());

    // Variant 3: L2 Shockley Exponential
    ComponentVariant v_l2;
    v_l2.variant_id = "v_l2_shockley";
    v_l2.display_name = "Shockley Diode Equation";
    v_l2.fidelity = FidelityTier::L2_EngineeringApproximation;
    v_l2.implementation_type = "Analytical";
    v_l2.model_expression = "i = Is * (exp(q*v/(n*k*T)) - 1)";
    REQUIRE(diode.add_variant(v_l2).is_ok());

    // Variant 4: L3 Electro-Thermal SPICE Model
    ComponentVariant v_l3;
    v_l3.variant_id = "v_l3_electrothermal";
    v_l3.display_name = "SPICE Level 1 Diode with Self-Heating";
    v_l3.fidelity = FidelityTier::L3_HighFidelity;
    v_l3.implementation_type = "MNA_Subcircuit";
    v_l3.supported_solvers = {"Transient", "AC_Frequency", "Thermal_Transient"};
    REQUIRE(diode.add_variant(v_l3).is_ok());

    // Duplicate variant id rejection
    REQUIRE(diode.add_variant(v_l2).is_err());
    REQUIRE(diode.variants().size() == 5); // default + 4 added

    // Switch active variant without altering external port contract
    REQUIRE(diode.set_active_variant("v_l3_electrothermal"));
    REQUIRE(diode.active_variant().variant_id == "v_l3_electrothermal");
    REQUIRE(diode.active_variant().fidelity == FidelityTier::L3_HighFidelity);
    REQUIRE(diode.active_variant().supported_solvers.size() == 3);

    // Nonexistent variant switch fails
    REQUIRE_FALSE(diode.set_active_variant("v_nonexistent"));

    // Fidelity tier strings
    REQUIRE(fidelity_tier_to_string(FidelityTier::L0_Visual) == "L0_Visual");
    REQUIRE(fidelity_tier_to_string(FidelityTier::L4_ResearchCertified) == "L4_ResearchCertified");
    REQUIRE(fidelity_tier_from_string("L2_EngineeringApproximation").value() == FidelityTier::L2_EngineeringApproximation);
}

TEST_CASE("Component validation for schema, ports, and topology errors (CMP-05)") {
    // 1. Invalid component: empty name and nil ID
    ComponentDefinition invalid_comp;
    invalid_comp.set_id(base::Uuid{}); // nil
    invalid_comp.set_type_name("");    // empty
    auto rep1 = invalid_comp.validate();
    REQUIRE(rep1.has_error("CMP_NIL_ID"));
    REQUIRE(rep1.has_error("CMP_EMPTY_TYPE_NAME"));

    // 2. Unbound child in connection
    ComponentDefinition comp2("Subsystem", "Subsystem With Broken Link");
    InternalConnection bad_conn;
    bad_conn.from_child = "NonexistentMotor";
    bad_conn.from_port = "shaft";
    bad_conn.to_child = "";
    bad_conn.to_port = "out_torque";
    // Adding connection to nonexistent child fails immediately
    REQUIRE(comp2.add_connection(bad_conn).is_err());

    // 3. Self loop on port
    ComponentPort sp;
    sp.schema.name = "loopback";
    REQUIRE(comp2.add_port(sp).is_ok());
    InternalConnection self_loop;
    self_loop.from_child = "";
    self_loop.from_port = "loopback";
    self_loop.to_child = "";
    self_loop.to_port = "loopback";
    REQUIRE(comp2.add_connection(self_loop).is_err());
}

TEST_CASE("ComponentBuilder fluent assembly and JSON roundtrip (CMP-06)") {
    auto comp_res = ComponentBuilder("DC_Motor", "Permanent Magnet DC Motor")
        .set_author("Mechatronics Lab")
        .set_version(base::Version(2, 0, 0))
        .set_description("Armature resistance, inductance, back-EMF and rotor inertia")
        .add_tag("actuator")
        .add_tag("electromechanical")
        .add_electrical_port("pos", PortDirection::Bidirectional)
        .add_electrical_port("neg", PortDirection::Bidirectional)
        .add_mechanical_port("shaft", true, PortDirection::Output)
        .add_thermal_port("thermal_casing", PortDirection::Bidirectional)
        .add_numeric_param("R_a", 1.5, "Ohm", 0.01, 100.0)
        .add_numeric_param("L_a", 0.002, "H", 0.0, 1.0)
        .add_numeric_param("K_t", 0.05, "N*m/A", 0.001, 10.0)
        .add_numeric_param("J_m", 1.0e-4, "kg*m^2", 1e-7, 1.0)
        .build();

    REQUIRE(comp_res.is_ok());
    const auto& motor = comp_res.value();
    REQUIRE(motor.type_name() == "DC_Motor");
    REQUIRE(motor.ports().size() == 4);
    REQUIRE(motor.parameters().size() == 4);
    REQUIRE(motor.has_port("shaft"));
    REQUIRE(motor.get_port("shaft")->schema.domain == PortDomain::MechanicalRotational);

    // Serialization Roundtrip (CMP-01, CMP-06)
    std::string json = motor.to_json();
    auto restored_res = ComponentDefinition::from_json(json);
    REQUIRE(restored_res.is_ok());
    const auto& restored = restored_res.value();
    REQUIRE(restored.type_name() == "DC_Motor");
    REQUIRE(restored.ports().size() == 4);
    REQUIRE(restored.has_port("pos"));
    REQUIRE(restored.has_port("shaft"));
}

TEST_CASE("Component runtime, lifecycle stepping, and state serialization (CMP-07)") {
    auto def = std::make_shared<ComponentDefinition>("Integrator", "Numerical Integrator");
    ParameterSchema gain;
    gain.name = "gain";
    gain.default_value = 2.0;
    REQUIRE(def->add_parameter(gain).is_ok());

    ComponentRuntimeInstance instance(def, "int_01");
    REQUIRE(instance.instance_name() == "int_01");
    REQUIRE(instance.lifecycle_state() == RuntimeLifecycleState::Uninitialized);
    REQUIRE(instance.get_parameter("gain")->as_double() == 2.0);

    // Provide custom state-space integration behavioral model
    // dx/dt = gain * u; y = x
    instance.set_custom_step_evaluator([](double dt, double /*sim_time*/,
                                          const std::unordered_map<std::string, double>& inputs,
                                          std::unordered_map<std::string, double>& state_vars,
                                          std::unordered_map<std::string, double>& outputs) -> base::Result<void> {
        double u = inputs.contains("u") ? inputs.at("u") : 0.0;
        double gain = 2.0;
        double x = state_vars.contains("x") ? state_vars.at("x") : 0.0;
        x += dt * gain * u;
        state_vars["x"] = x;
        outputs["y"] = x;
        return base::Result<void>();
    });

    REQUIRE(instance.initialize().is_ok());
    REQUIRE(instance.lifecycle_state() == RuntimeLifecycleState::Ready);
    REQUIRE(instance.simulated_time() == 0.0);

    // Set input u = 5.0 and step 3 times with dt = 0.1s
    instance.set_input_value("u", 5.0);
    // dx/dt = 2.0 * 5.0 = 10.0; each step adds 10 * 0.1 = 1.0
    REQUIRE(instance.step(0.1).is_ok());
    REQUIRE(instance.step_count() == 1);
    REQUIRE(std::abs(instance.get_output_value("y") - 1.0) < 1e-6);

    REQUIRE(instance.step(0.1).is_ok());
    REQUIRE(instance.step(0.1).is_ok());
    REQUIRE(instance.step_count() == 3);
    REQUIRE(std::abs(instance.simulated_time() - 0.3) < 1e-6);
    REQUIRE(std::abs(instance.get_state_variable("x") - 3.0) < 1e-6);
    REQUIRE(std::abs(instance.get_output_value("y") - 3.0) < 1e-6);

    // State Serialization Roundtrip (CMP-07)
    std::string state_json = instance.serialize_state_json();
    ComponentRuntimeInstance instance2(def, "int_01_restored");
    REQUIRE(instance2.restore_state_json(state_json).is_ok());
    REQUIRE(std::abs(instance2.simulated_time() - 0.3) < 1e-6);
    REQUIRE(instance2.step_count() == 3);
    REQUIRE(std::abs(instance2.get_state_variable("x") - 3.0) < 1e-6);

    // Reset instance
    REQUIRE(instance.reset().is_ok());
    REQUIRE(instance.simulated_time() == 0.0);
    REQUIRE(instance.step_count() == 0);
    REQUIRE(instance.get_state_variable("x") == 0.0);
}

TEST_CASE("Component packaging, lockfile dependencies and CRC32 integrity (CMP-08)") {
    ComponentPackage pkg("org.shunya.analog_devices", "Standard Analog Devices Library", base::Version(1, 4, 0));
    pkg.set_author("Shunya Component Authors");
    pkg.set_description("Reference diodes, transistors, and operational amplifiers");

    // Add component definitions
    ComponentDefinition opamp("OpAmp", "Ideal Operational Amplifier");
    ComponentDefinition bjt("NPN_Transistor", "NPN Bipolar Junction Transistor");
    pkg.add_component(std::move(opamp));
    pkg.add_component(std::move(bjt));
    REQUIRE(pkg.components().size() == 2);
    REQUIRE(pkg.get_component("OpAmp") != nullptr);

    // Dependency lock entries (CMP-08)
    DependencyLockEntry dep1{
        .package_name = "org.shunya.core_units",
        .required_version = base::Version(2, 0, 0),
        .content_hash_sha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    };
    pkg.add_dependency_lock(dep1);
    REQUIRE(pkg.dependency_locks().size() == 1);

    // Asset entry (SVG symbol)
    AssetEntry sym_asset{
        .relative_path = "symbols/opamp.svg",
        .mime_type = "image/svg+xml",
        .content_sha256 = "c2a96931551a89c2bb01c750b284e365ad4beeb74c2e5b61a3512e0307bb5dbd",
        .text_payload = "<svg><polygon points='0,0 20,10 0,20'/></svg>"
    };
    pkg.add_asset(sym_asset);
    REQUIRE(pkg.assets().size() == 1);

    // Validation & CRC32 calculation
    auto rep = pkg.validate_package();
    REQUIRE_FALSE(rep.has_errors());

    uint32_t crc1 = pkg.compute_package_crc32();
    uint32_t crc2 = pkg.compute_package_crc32();
    REQUIRE(crc1 != 0);
    REQUIRE(crc1 == crc2);

    // Manifest serialization and deserialization roundtrip
    std::string manifest_json = pkg.serialize_manifest_json();
    auto rt_res = ComponentPackage::deserialize_manifest_json(manifest_json);
    REQUIRE(rt_res.is_ok());
    const auto& restored = rt_res.value();
    REQUIRE(restored.package_id() == "org.shunya.analog_devices");
    REQUIRE(restored.name() == "Standard Analog Devices Library");
    REQUIRE(restored.version() == base::Version(1, 4, 0));
    REQUIRE(restored.dependency_locks().size() == 1);
    REQUIRE(restored.dependency_locks()[0].package_name == "org.shunya.core_units");
}
