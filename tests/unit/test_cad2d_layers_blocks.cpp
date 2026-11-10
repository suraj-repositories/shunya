#include "tests/test_framework.hpp"

#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/layer_manager.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

using namespace shunya::cad2d;
using shunya::core::base::math::PI;

namespace {

bool is_approx(double a, double b, double eps = 1e-7) {
    return std::abs(a - b) <= eps;
}

} // namespace

TEST_CASE("LayerManager: hierarchical visibility, locking inheritance, and entity reassignment") {
    LayerManager mgr;
    REQUIRE(mgr.find_layer("0") != nullptr);
    REQUIRE(mgr.layer_count() == 1);

    REQUIRE(mgr.add_layer("Mechanical", "0", {"#61afef", 0.35, LinePattern::Solid}));
    REQUIRE(mgr.add_layer("Fasteners", "Mechanical", {"#e5c07b", 0.25, LinePattern::Dashed}));
    REQUIRE(mgr.add_layer("Bolts", "Fasteners", {"#98c379", 0.18, LinePattern::Center}));

    // Cycle protection
    REQUIRE(!mgr.set_parent("Mechanical", "Bolts"));
    REQUIRE(!mgr.set_parent("Bolts", "Bolts"));

    // Visibility cascade
    REQUIRE(mgr.is_effectively_visible("Bolts"));
    REQUIRE(mgr.set_visible("Mechanical", false));
    REQUIRE(!mgr.is_effectively_visible("Mechanical"));
    REQUIRE(!mgr.is_effectively_visible("Fasteners"));
    REQUIRE(!mgr.is_effectively_visible("Bolts"));
    REQUIRE(mgr.is_effectively_visible("0"));

    REQUIRE(mgr.set_visible("Mechanical", true));
    REQUIRE(mgr.is_effectively_visible("Bolts"));

    // Lock cascade
    REQUIRE(mgr.set_locked("Fasteners", true));
    REQUIRE(!mgr.is_effectively_locked("Mechanical"));
    REQUIRE(mgr.is_effectively_locked("Fasteners"));
    REQUIRE(mgr.is_effectively_locked("Bolts"));

    SketchDocument2D doc;
    const auto id1 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{0, 0}, Point2D{10, 0}), false, "Bolts");
    const auto id2 = doc.add_entity(std::make_shared<Circle2D>(Point2D{5, 5}, 2.0), false, "Mechanical");

    const SketchEntity* e1 = doc.find_entity(id1);
    const SketchEntity* e2 = doc.find_entity(id2);
    REQUIRE(e1 != nullptr);
    REQUIRE(e2 != nullptr);
    REQUIRE(!mgr.can_modify_entity(*e1)); // Bolts inherits lock from Fasteners
    REQUIRE(mgr.can_modify_entity(*e2));  // Mechanical is visible and unlocked

    // Entity reassignment
    REQUIRE(mgr.reassign_entities(doc, "Bolts", "Mechanical") == 1);
    e1 = doc.find_entity(id1);
    REQUIRE(e1 != nullptr);
    REQUIRE(e1->layer == "Mechanical");
    REQUIRE(mgr.can_modify_entity(*e1));

    // Removing layer falls back entities or parent
    REQUIRE(!mgr.remove_layer("0"));
    REQUIRE(mgr.remove_layer("Bolts"));
    REQUIRE(mgr.find_layer("Bolts") == nullptr);
}

TEST_CASE("BlockManager: 50+ instances update dynamically when block definition changes") {
    BlockManager bmgr;

    BlockDefinition bolt;
    bolt.name = "M6_Bolt";
    bolt.items.push_back({std::make_shared<Circle2D>(Point2D{0.0, 0.0}, 3.0), "Fasteners", false});
    bolt.items.push_back({std::make_shared<LineSegment2D>(Point2D{-3.0, 0.0}, Point2D{3.0, 0.0}), "Fasteners", false});
    REQUIRE(bmgr.create_block(bolt));

    std::vector<std::string> inst_ids;
    for (int i = 0; i < 55; ++i) {
        const double px = static_cast<double>(i % 11) * 15.0;
        const double py = static_cast<double>(i / 11) * 20.0;
        const std::string id = bmgr.place_instance("M6_Bolt", Point2D{px, py}, 0.0, 1.0, 1.0, "0");
        REQUIRE(!id.empty());
        inst_ids.push_back(id);
    }
    REQUIRE(bmgr.instance_count() == 55);

    // Check initial bounding box on last instance (#54 at x=10*15=150, y=4*20=80)
    const BBox2D bbox_before = bmgr.compute_instance_bbox(inst_ids.back());
    REQUIRE(is_approx(bbox_before.width(), 6.0, 1e-6));
    REQUIRE(is_approx(bbox_before.center().x, 150.0, 1e-6));
    REQUIRE(is_approx(bbox_before.center().y, 80.0, 1e-6));

    // Modify block definition radius 3.0 -> 5.5 and add vertical slot line
    BlockDefinition bolt_v2 = bolt;
    bolt_v2.items[0].curve = std::make_shared<Circle2D>(Point2D{0.0, 0.0}, 5.5);
    bolt_v2.items[1].curve = std::make_shared<LineSegment2D>(Point2D{-5.5, 0.0}, Point2D{5.5, 0.0});
    bolt_v2.items.push_back({std::make_shared<LineSegment2D>(Point2D{0.0, -5.5}, Point2D{0.0, 5.5}), "Fasteners", false});
    REQUIRE(bmgr.update_block(bolt_v2));

    // All 55 instances must reflect updated entity count and radius 5.5 immediately
    for (const auto& id : inst_ids) {
        const auto resolved = bmgr.resolve_instance_entities(id);
        REQUIRE(resolved.size() == 3);
        REQUIRE(resolved[0].curve->type() == CurveType2D::Circle);
        const auto& c = static_cast<const Circle2D&>(*resolved[0].curve);
        REQUIRE(is_approx(c.radius, 5.5, 1e-9));
    }
}

TEST_CASE("BlockManager: nested transforms, grouping, and explode into SketchDocument2D") {
    BlockManager bmgr;

    BlockDefinition pin;
    pin.name = "Pin";
    pin.items.push_back({std::make_shared<Circle2D>(Point2D{0.0, 0.0}, 2.0), "0", false});
    REQUIRE(bmgr.create_block(pin));

    BlockDefinition bracket;
    bracket.name = "Bracket";
    bracket.items.push_back({std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{10.0, 0.0}), "0", false});
    bracket.nested_refs.push_back({"", "Pin", Point2D{10.0, 0.0}, 0.0, 1.0, 1.0, "Pins"});
    REQUIRE(bmgr.create_block(bracket));

    // Place Bracket rotated 90 degrees at (5, 5), scaled 2x
    const std::string inst_id = bmgr.place_instance("Bracket", Point2D{5.0, 5.0}, PI * 0.5, 2.0, 2.0, "Frame");
    const auto resolved = bmgr.resolve_instance_entities(inst_id);
    REQUIRE(resolved.size() == 2);

    // Line (0,0)->(10,0) scaled 2x rotated 90deg + (5,5) => (5,5)->(5,25)
    REQUIRE(resolved[0].curve->type() == CurveType2D::LineSegment);
    const auto& seg = static_cast<const LineSegment2D&>(*resolved[0].curve);
    REQUIRE(is_approx(seg.p0.x, 5.0) && is_approx(seg.p0.y, 5.0));
    REQUIRE(is_approx(seg.p1.x, 5.0) && is_approx(seg.p1.y, 25.0));

    // Nested Pin center should be at (5, 25), radius 4.0, layer "Pins"
    REQUIRE(resolved[1].curve->type() == CurveType2D::Circle);
    const auto& circ = static_cast<const Circle2D&>(*resolved[1].curve);
    REQUIRE(is_approx(circ.center.x, 5.0) && is_approx(circ.center.y, 25.0));
    REQUIRE(is_approx(circ.radius, 4.0));
    REQUIRE(resolved[1].layer == "Pins");

    // Explode into SketchDocument2D
    SketchDocument2D doc;
    const auto exploded_ids = bmgr.explode_instance(inst_id, doc);
    REQUIRE(exploded_ids.size() == 2);
    REQUIRE(bmgr.instance_count() == 0);
    REQUIRE(doc.entities().size() == 2);

    // Regroup doc entities back into a block
    const std::string grouped_inst = bmgr.group_entities(doc, exploded_ids, "ExplodedGroup", Point2D{5.0, 5.0});
    REQUIRE(!grouped_inst.empty());
    REQUIRE(doc.entities().empty());
    REQUIRE(bmgr.find_block("ExplodedGroup") != nullptr);
    REQUIRE(bmgr.resolve_instance_entities(grouped_inst).size() == 2);
}
