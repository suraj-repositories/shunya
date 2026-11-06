#include "tests/test_framework.hpp"
#include "shunya/core/domain/scene_graph.hpp"

#include <vector>
#include <string>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("SceneGraph entity creation and hierarchy") {
    SceneGraph scene;

    auto root_res = scene.create_entity("Root Assembly");
    REQUIRE(root_res.is_ok());
    auto root = root_res.value();
    CHECK(root->name() == "Root Assembly");
    CHECK(!root->parent_id().has_value());
    CHECK(root->children_ids().empty());

    auto child1_res = scene.create_entity("Subsystem Chassis", root->id());
    REQUIRE(child1_res.is_ok());
    auto child1 = child1_res.value();
    REQUIRE(child1->parent_id().has_value());
    CHECK(child1->parent_id().value() == root->id());
    CHECK(root->children_ids().size() == 1);
    CHECK(root->children_ids()[0] == child1->id());

    auto child2_res = scene.create_entity("Subsystem Motor", root->id());
    REQUIRE(child2_res.is_ok());
    CHECK(root->children_ids().size() == 2);

    auto grandchild_res = scene.create_entity("Rotor Core", child2_res.value()->id());
    REQUIRE(grandchild_res.is_ok());
    CHECK(child2_res.value()->children_ids().size() == 1);

    auto roots = scene.root_entities();
    CHECK(roots.size() == 1);
    CHECK(roots[0]->id() == root->id());
    CHECK(scene.entity_count() == 4);
}

TEST_CASE("SceneGraph reparenting and cycle detection") {
    SceneGraph scene;
    auto nodeA = scene.create_entity("Node A").value();
    auto nodeB = scene.create_entity("Node B", nodeA->id()).value();
    auto nodeC = scene.create_entity("Node C", nodeB->id()).value();

    // Prevent parenting to self
    auto self_parent_res = scene.set_parent(nodeA->id(), nodeA->id());
    CHECK(self_parent_res.is_err());
    CHECK(self_parent_res.error().code == 2102);

    // Prevent circular hierarchy (Node A -> Node C where Node C is descendant of Node A)
    auto cycle_res = scene.set_parent(nodeA->id(), nodeC->id());
    CHECK(cycle_res.is_err());
    CHECK(cycle_res.error().code == 2103);

    // Valid reparenting: move Node C to become child of Node A directly
    auto valid_reparent = scene.set_parent(nodeC->id(), nodeA->id());
    REQUIRE(valid_reparent.is_ok());
    CHECK(nodeC->parent_id().value() == nodeA->id());
    CHECK(nodeB->children_ids().empty());
    CHECK(nodeA->children_ids().size() == 2);

    // Promote Node C to root
    auto to_root = scene.set_parent(nodeC->id(), std::nullopt);
    REQUIRE(to_root.is_ok());
    CHECK(!nodeC->parent_id().has_value());
    CHECK(scene.root_entities().size() == 2);
}

TEST_CASE("SceneGraph entity removal") {
    SceneGraph scene;
    auto root = scene.create_entity("Car Body").value();
    auto wheel_fl = scene.create_entity("Wheel FL", root->id()).value();
    auto tire_fl = scene.create_entity("Tire FL", wheel_fl->id()).value();
    auto wheel_fr = scene.create_entity("Wheel FR", root->id()).value();

    CHECK(scene.entity_count() == 4);

    // Recursive removal of wheel_fl should also remove tire_fl
    auto rm_res = scene.remove_entity(wheel_fl->id(), true);
    REQUIRE(rm_res.is_ok());
    CHECK(!scene.has_entity(wheel_fl->id()));
    CHECK(!scene.has_entity(tire_fl->id()));
    CHECK(scene.has_entity(wheel_fr->id()));
    CHECK(scene.entity_count() == 2);
    CHECK(root->children_ids().size() == 1);
    CHECK(root->children_ids()[0] == wheel_fr->id());
}

TEST_CASE("SceneGraph spatial transform propagation") {
    SceneGraph scene;
    auto parent = scene.create_entity("Arm Base").value();
    parent->local_transform().position = base::math::Vec3{10.0, 0.0, 0.0};

    auto child = scene.create_entity("Arm Link", parent->id()).value();
    child->local_transform().position = base::math::Vec3{0.0, 5.0, 0.0};

    auto world_mat = scene.compute_world_matrix(child->id());
    auto local_origin = base::math::Vec3{0.0, 0.0, 0.0};
    auto world_pos = world_mat.transform_point(local_origin);

    CHECK(base::math::approx_equal(world_pos.x, 10.0));
    CHECK(base::math::approx_equal(world_pos.y, 5.0));
    CHECK(base::math::approx_equal(world_pos.z, 0.0));
}

TEST_CASE("SceneGraph visibility and active state hierarchy") {
    SceneGraph scene;
    auto parent = scene.create_entity("Parent").value();
    auto child = scene.create_entity("Child", parent->id()).value();

    CHECK(scene.is_globally_visible(child->id()));
    CHECK(scene.is_globally_active(child->id()));

    // Hiding parent hides child globally even if child is visible locally
    parent->set_visible(false);
    CHECK(!scene.is_globally_visible(child->id()));
    CHECK(child->is_visible()); // local state unchanged

    parent->set_visible(true);
    CHECK(scene.is_globally_visible(child->id()));

    // Inactivating parent inactivates child globally
    parent->set_active(false);
    CHECK(!scene.is_globally_active(child->id()));
}

TEST_CASE("SceneGraph tags and domain attachments") {
    SceneGraph scene;
    auto motor = scene.create_entity("PMSM Motor").value();
    motor->add_tag("actuator");
    motor->add_tag("electrical");
    motor->set_attachment("cad_mesh", "models/motor_pmsm_v2.step");
    motor->set_attachment("spice_subcircuit", ".subckt pmsm_3phase 1 2 3");

    auto sensor = scene.create_entity("Hall Sensor").value();
    sensor->add_tag("sensor");
    sensor->add_tag("electrical");
    sensor->set_attachment("spice_subcircuit", ".subckt hall_sensor in out");

    CHECK(motor->has_tag("actuator"));
    CHECK(!motor->has_tag("sensor"));

    auto electrical_entities = scene.find_by_tag("electrical");
    CHECK(electrical_entities.size() == 2);

    auto spice_entities = scene.find_by_domain_attachment("spice_subcircuit");
    CHECK(spice_entities.size() == 2);

    auto cad_entities = scene.find_by_domain_attachment("cad_mesh");
    CHECK(cad_entities.size() == 1);
    CHECK(cad_entities[0]->name() == "PMSM Motor");
    CHECK(cad_entities[0]->get_attachment("cad_mesh").value() == "models/motor_pmsm_v2.step");
}

TEST_CASE("SceneGraph depth-first traversal") {
    SceneGraph scene;
    auto root = scene.create_entity("Root").value();
    auto childA = scene.create_entity("Child A", root->id()).value();
    REQUIRE(scene.create_entity("Grandchild A1", childA->id()).is_ok());
    REQUIRE(scene.create_entity("Child B", root->id()).is_ok());

    std::vector<std::string> visited_order;
    scene.traverse_depth_first([&](const std::shared_ptr<Entity>& entity) {
        visited_order.push_back(entity->name());
    });

    REQUIRE(visited_order.size() == 4);
    CHECK(visited_order[0] == "Root");
    CHECK(visited_order[1] == "Child A");
    CHECK(visited_order[2] == "Grandchild A1");
    CHECK(visited_order[3] == "Child B");
}

TEST_CASE("SceneGraph JSON serialization roundtrip") {
    SceneGraph scene;
    auto root = scene.create_entity("Assembly").value();
    root->add_tag("top_level");
    root->set_attachment("doc", "assembly_spec.pdf");

    auto sub = scene.create_entity("Subassembly", root->id()).value();
    sub->local_transform().position = base::math::Vec3{1.0, 2.0, 3.0};
    sub->set_visible(true);
    sub->set_active(false);

    std::string json = scene.to_json();
    CHECK(!json.empty());
    CHECK(json.find("\"Assembly\"") != std::string::npos);
    CHECK(json.find("\"Subassembly\"") != std::string::npos);

    auto parsed_res = SceneGraph::from_json(json);
    REQUIRE(parsed_res.is_ok());
    auto deserialized = std::move(parsed_res.value());

    CHECK(deserialized->entity_count() == 2);
    auto found_sub = deserialized->find_by_name("Subassembly");
    REQUIRE(found_sub.size() == 1);
    CHECK(!found_sub[0]->is_active());
}
