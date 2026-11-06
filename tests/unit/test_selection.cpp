#include "tests/test_framework.hpp"
#include "shunya/core/domain/selection.hpp"
#include "shunya/core/domain/scene_graph.hpp"

#include <numbers>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("SelectionSet modes Replace, Add, Remove, Toggle") {
    SelectionSet sel("Active Selection");
    auto id1 = base::Uuid::generate_random();
    auto id2 = base::Uuid::generate_random();
    auto id3 = base::Uuid::generate_random();

    // Replace
    sel.select(id1, SelectionMode::Replace);
    CHECK(sel.size() == 1);
    CHECK(sel.contains(id1));
    CHECK(sel.primary().value() == id1);

    // Add
    sel.select(id2, SelectionMode::Add);
    CHECK(sel.size() == 2);
    CHECK(sel.contains(id1));
    CHECK(sel.contains(id2));
    CHECK(sel.primary().value() == id2);

    // Toggle existing (removes id1)
    sel.select(id1, SelectionMode::Toggle);
    CHECK(sel.size() == 1);
    CHECK(!sel.contains(id1));
    CHECK(sel.contains(id2));

    // Toggle non-existing (adds id3)
    sel.select(id3, SelectionMode::Toggle);
    CHECK(sel.size() == 2);
    CHECK(sel.contains(id3));
    CHECK(sel.primary().value() == id3);

    // Remove id3
    sel.select(id3, SelectionMode::Remove);
    CHECK(sel.size() == 1);
    CHECK(!sel.contains(id3));
    CHECK(sel.primary().value() == id2);
}

TEST_CASE("SelectionSet predicate filtering") {
    SelectionSet sel;
    auto id1 = base::Uuid::generate_random();
    auto id2 = base::Uuid::generate_random();
    auto id3 = base::Uuid::generate_random();

    sel.select({id1, id2, id3}, SelectionMode::Replace);
    CHECK(sel.size() == 3);

    auto filtered = sel.filter([&](const base::Uuid& id) {
        return id == id2;
    });

    CHECK(filtered.size() == 1);
    CHECK(filtered.contains(id2));
    CHECK(!filtered.contains(id1));
}

TEST_CASE("SelectionManager callbacks and named sets") {
    SelectionManager mgr;
    int callback_fired = 0;
    mgr.set_on_selection_changed([&](const SelectionSet& s) {
        ++callback_fired;
        [[maybe_unused]] auto sz = s.size();
    });

    auto idA = base::Uuid::generate_random();
    auto idB = base::Uuid::generate_random();

    mgr.select(idA, SelectionMode::Replace);
    CHECK(callback_fired == 1);
    mgr.select(idB, SelectionMode::Add);
    CHECK(callback_fired == 2);

    // Save selection set
    mgr.save_selection_set("Sensors_Group");
    CHECK(mgr.has_selection_set("Sensors_Group"));
    CHECK(!mgr.has_selection_set("Nonexistent"));

    auto saved = mgr.get_selection_set("Sensors_Group");
    REQUIRE(saved.has_value());
    CHECK(saved.value().size() == 2);
    CHECK(saved.value().contains(idA));
    CHECK(saved.value().contains(idB));

    auto list = mgr.list_selection_sets();
    CHECK(list.size() == 1);
    CHECK(list[0] == "Sensors_Group");

    mgr.remove_selection_set("Sensors_Group");
    CHECK(!mgr.has_selection_set("Sensors_Group"));
}

TEST_CASE("SelectionManager snapping utilities") {
    SelectionManager mgr;
    mgr.snap_settings().grid_snap_enabled = true;
    mgr.snap_settings().grid_size = 0.5;

    base::math::Vec3 raw_pos{1.23, 4.89, -0.67};
    base::math::Vec3 snapped_pos = mgr.snap_position(raw_pos);

    CHECK(base::math::approx_equal(snapped_pos.x, 1.0));
    CHECK(base::math::approx_equal(snapped_pos.y, 5.0));
    CHECK(base::math::approx_equal(snapped_pos.z, -0.5));

    // Rotation snapping (e.g. 15 degrees)
    mgr.snap_settings().rotation_snap_enabled = true;
    mgr.snap_settings().rotation_step_degrees = 15.0;

    double deg2rad = std::numbers::pi / 180.0;
    double raw_angle = 32.0 * deg2rad; // should snap to 30 deg
    double snapped_angle = mgr.snap_rotation_angle(raw_angle);
    double expected_angle = 30.0 * deg2rad;

    CHECK(base::math::approx_equal(snapped_angle, expected_angle));
}

TEST_CASE("SelectionManager duplication and deletion operations") {
    SceneGraph scene;
    auto ent1 = scene.create_entity("Sensor Plate").value();
    ent1->add_tag("hardware");
    ent1->set_attachment("type", "piezoelectric");
    ent1->local_transform().position = base::math::Vec3{5.0, 0.0, 0.0};

    SelectionManager mgr;
    mgr.select(ent1->id(), SelectionMode::Replace);

    // Duplicate selection
    auto dup_res = mgr.duplicate_selection(scene, base::math::Vec3{0.0, 2.0, 0.0});
    REQUIRE(dup_res.is_ok());
    auto new_ids = dup_res.value();
    REQUIRE(new_ids.size() == 1);

    CHECK(scene.entity_count() == 2);
    auto cloned_ent = scene.get_entity(new_ids[0]);
    REQUIRE(cloned_ent != nullptr);
    CHECK(cloned_ent->name() == "Sensor Plate (Copy)");
    CHECK(cloned_ent->has_tag("hardware"));
    CHECK(cloned_ent->get_attachment("type").value() == "piezoelectric");
    CHECK(base::math::approx_equal(cloned_ent->local_transform().position.x, 5.0));
    CHECK(base::math::approx_equal(cloned_ent->local_transform().position.y, 2.0));

    // Active selection should now point to cloned item
    CHECK(mgr.current_selection().size() == 1);
    CHECK(mgr.current_selection().contains(new_ids[0]));

    // Delete duplicated item
    auto del_res = mgr.delete_selection(scene);
    REQUIRE(del_res.is_ok());
    CHECK(del_res.value() == 1);
    CHECK(scene.entity_count() == 1);
    CHECK(mgr.current_selection().empty());
}
