#include "tests/test_framework.hpp"
#include "shunya/eng/assembly_model.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/core/domain/material_assignment.hpp"

#include <cmath>

using namespace shunya;

TEST_CASE("PH5-28.01: Assembly Part Instantiation, Independent Transforms & Precision Snaps") {
    eng::PhysicalObjectManager obj_mgr;
    core::domain::MaterialRegistry mat_reg;

    core::domain::Material mat_brass(core::base::Uuid::generate_random(), "Brass",
                                     core::domain::MaterialPhysicalState::Solid, 8500.0);
    const auto brass_id = mat_brass.id();
    REQUIRE(mat_reg.register_material(std::move(mat_brass)).is_ok());

    core::domain::Material mat_glass(core::base::Uuid::generate_random(), "Glass",
                                     core::domain::MaterialPhysicalState::Solid, 2500.0);
    const auto glass_id = mat_glass.id();
    REQUIRE(mat_reg.register_material(std::move(mat_glass)).is_ok());

    // Create 2 library objects
    eng::PhysicalObject socket;
    socket.name = "Lamp_Socket";
    socket.geometry = eng::ObjectGeometrySpec::make_cylinder(0.020, 0.050);
    eng::ObjectRegion r_sock;
    r_sock.name = "socket_body";
    r_sock.material_id = brass_id;
    r_sock.volume_fraction = 1.0;
    socket.regions.push_back(r_sock);
    auto socket_id = socket.id;
    REQUIRE(obj_mgr.add_object(socket, mat_reg).is_ok());

    eng::PhysicalObject bulb;
    bulb.name = "A19_Bulb";
    bulb.geometry = eng::ObjectGeometrySpec::make_cylinder(0.030, 0.110);
    eng::ObjectRegion r_bulb;
    r_bulb.name = "bulb_envelope";
    r_bulb.material_id = glass_id;
    r_bulb.volume_fraction = 1.0;
    bulb.regions.push_back(r_bulb);
    auto bulb_id = bulb.id;
    REQUIRE(obj_mgr.add_object(bulb, mat_reg).is_ok());

    eng::AssemblyModel assy;
    assy.name = "Desk_Lamp";

    // 1. Instantiate 2 separate bulb parts from the same object
    auto& bulb_inst1 = assy.add_part_instance(bulb_id, "Bulb_Left", { -0.10, 0.20, 0.0 });
    auto& bulb_inst2 = assy.add_part_instance(bulb_id, "Bulb_Right", { 0.10, 0.20, 0.0 });

    CHECK(bulb_inst1.instance_id != bulb_inst2.instance_id);
    CHECK(bulb_inst1.object_id == bulb_inst2.object_id);

    // Verify independent transform matrices
    auto m1 = bulb_inst1.compute_transform_matrix();
    auto m2 = bulb_inst2.compute_transform_matrix();
    // Translation column in row-major Matrix4x4 is m[3], m[7], m[11]
    CHECK(std::abs(m1.m[3] - (-0.10)) < 1e-6);
    CHECK(std::abs(m2.m[3] - (0.10)) < 1e-6);

    // 2. Instantiate socket
    auto& socket_inst = assy.add_part_instance(socket_id, "Base_Socket", { 0.0, 0.0, 0.0 });

    // 3. Precision Concentric Snap
    bool snap_c = assy.snap_concentric(socket_inst.instance_id, bulb_inst1.instance_id);
    CHECK(snap_c);
    auto* b1_mut = assy.find_part(bulb_inst1.instance_id);
    CHECK(std::abs(b1_mut->position.x - socket_inst.position.x) < 1e-6);
    CHECK(std::abs(b1_mut->position.z - socket_inst.position.z) < 1e-6);

    // 4. Precision Face-to-Face Snap
    bool snap_f = assy.snap_face_to_face(socket_inst.instance_id, bulb_inst1.instance_id, 0.050);
    CHECK(snap_f);
    CHECK(std::abs(b1_mut->position.y - 0.050) < 1e-6);
}

TEST_CASE("PH5-28.02: Kinematic Joints, Wire Routing & Desk Lamp E2E Assembly") {
    eng::PhysicalObjectManager obj_mgr;
    core::domain::MaterialRegistry mat_reg;

    core::domain::Material mat_steel(core::base::Uuid::generate_random(), "Steel",
                                     core::domain::MaterialPhysicalState::Solid, 7850.0);
    const auto steel_id = mat_steel.id();
    REQUIRE(mat_reg.register_material(std::move(mat_steel)).is_ok());

    core::domain::Material mat_glass(core::base::Uuid::generate_random(), "Glass",
                                     core::domain::MaterialPhysicalState::Solid, 2500.0);
    const auto glass_id = mat_glass.id();
    REQUIRE(mat_reg.register_material(std::move(mat_glass)).is_ok());

    // 1. Desk Lamp Parts: Base, Articulated Arm, Socket, Bulb
    eng::PhysicalObject base_obj;
    base_obj.name = "Lamp_Base";
    base_obj.geometry = eng::ObjectGeometrySpec::make_box(0.15, 0.02, 0.15);
    eng::ObjectRegion r_base;
    r_base.name = "base_body";
    r_base.material_id = steel_id;
    r_base.volume_fraction = 1.0;
    base_obj.regions.push_back(r_base);
    auto base_id = base_obj.id;
    REQUIRE(obj_mgr.add_object(base_obj, mat_reg).is_ok());

    eng::PhysicalObject arm_obj;
    arm_obj.name = "Lamp_Arm";
    arm_obj.geometry = eng::ObjectGeometrySpec::make_cylinder(0.008, 0.35);
    eng::ObjectRegion r_arm;
    r_arm.name = "arm_body";
    r_arm.material_id = steel_id;
    r_arm.volume_fraction = 1.0;
    arm_obj.regions.push_back(r_arm);
    auto arm_id = arm_obj.id;
    REQUIRE(obj_mgr.add_object(arm_obj, mat_reg).is_ok());

    eng::PhysicalObject socket_obj;
    socket_obj.name = "E26_Socket";
    socket_obj.geometry = eng::ObjectGeometrySpec::make_cylinder(0.020, 0.045);
    eng::ObjectRegion r_sock;
    r_sock.name = "sock_body";
    r_sock.material_id = steel_id;
    r_sock.volume_fraction = 1.0;
    socket_obj.regions.push_back(r_sock);
    auto socket_id = socket_obj.id;
    REQUIRE(obj_mgr.add_object(socket_obj, mat_reg).is_ok());

    eng::PhysicalObject bulb_obj;
    bulb_obj.name = "A19_Bulb";
    bulb_obj.geometry = eng::ObjectGeometrySpec::make_cylinder(0.030, 0.110);
    eng::ObjectRegion r_bulb;
    r_bulb.name = "bulb_body";
    r_bulb.material_id = glass_id;
    r_bulb.volume_fraction = 1.0;
    bulb_obj.regions.push_back(r_bulb);
    auto bulb_id = bulb_obj.id;
    REQUIRE(obj_mgr.add_object(bulb_obj, mat_reg).is_ok());

    // 2. Assemble in 3D scene
    eng::AssemblyModel lamp_assembly;
    lamp_assembly.name = "Articulated_Desk_Lamp_Assembly";

    auto& p_base = lamp_assembly.add_part_instance(base_id, "Base", {0, 0, 0});
    auto& p_arm = lamp_assembly.add_part_instance(arm_id, "Arm", {0, 0.02, 0});
    auto& p_sock = lamp_assembly.add_part_instance(socket_id, "Socket", {0, 0.37, 0});
    auto& p_bulb = lamp_assembly.add_part_instance(bulb_id, "Bulb", {0, 0.42, 0});

    CHECK(lamp_assembly.parts.size() == 4u);

    // 3. Kinematic Joints:
    // Joint 1: Revolute joint between Base and Arm
    auto& j_arm = lamp_assembly.add_joint("Hinge_Base_Arm", eng::KinematicJointKind::Revolute,
                                         p_base.instance_id, p_arm.instance_id, {1, 0, 0});
    j_arm.lower_limit = -45.0;
    j_arm.upper_limit = 90.0;
    CHECK(j_arm.kind == eng::KinematicJointKind::Revolute);

    // Joint 2: Threaded Screw Mating between Socket and Bulb (pitch = 2mm/rev)
    auto& j_screw = lamp_assembly.add_joint("Screw_Socket_Bulb", eng::KinematicJointKind::ThreadedScrew,
                                           p_sock.instance_id, p_bulb.instance_id, {0, 1, 0});
    j_screw.screw_pitch_m_per_rev = 0.002;
    CHECK(j_screw.kind == eng::KinematicJointKind::ThreadedScrew);

    // 4. Electrical Wire Routing:
    // Route 2 wires (AWG 22) connecting Base Switch to Bulb terminals
    auto& w_hot = lamp_assembly.add_wire("Live_AC_Wire", p_base.instance_id, "Switch_Out",
                                         p_bulb.instance_id, "Base_Contact", 22, 0.50);
    auto& w_neutral = lamp_assembly.add_wire("Neutral_AC_Wire", p_base.instance_id, "Mains_N",
                                             p_bulb.instance_id, "Screw_Shell", 22, 0.50);

    CHECK(w_hot.resistance_ohm > 0.0);
    CHECK(w_neutral.resistance_ohm > 0.0);

    // 5. Thermal Contact Interface between Socket and Bulb
    auto& t_contact = lamp_assembly.add_thermal_contact("Thread_Thermal_Contact",
                                                        p_sock.instance_id, p_bulb.instance_id,
                                                        0.0003, 4500.0);
    CHECK(t_contact.thermal_resistance_k_w() > 0.0);

    // 6. Pre-Flight Validation
    auto preflight = lamp_assembly.run_preflight_checks(obj_mgr);
    CHECK(preflight.passed);
    CHECK(preflight.has_closed_electrical_loop);

    // 7. Export to Co-Simulation Scene
    std::string cosim_payload = lamp_assembly.export_to_cosim_scene(obj_mgr);
    CHECK_FALSE(cosim_payload.empty());
    CHECK(cosim_payload.find("Desk_Lamp_Assembly") != std::string::npos);
    CHECK(cosim_payload.find("CoSimulationReady") != std::string::npos);
}
