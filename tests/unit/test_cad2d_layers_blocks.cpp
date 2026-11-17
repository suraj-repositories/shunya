#include "tests/test_framework.hpp"

#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/layer_manager.hpp"
#include "shunya/cad2d/selection_gizmo.hpp"
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

TEST_CASE("PH4-04.03.001: Hierarchical 2D layer system with opacity, color overrides, line weight, and hierarchy management") {
    LayerManager mgr;
    REQUIRE(mgr.active_layer() == "0");

    // Add layers with hierarchy: 0 -> Electrical -> Wiring -> SignalLines
    REQUIRE(mgr.add_layer("Electrical", "0", {"#ff5555", 0.5, LinePattern::Solid, 0.8}));
    REQUIRE(mgr.add_layer("Wiring", "Electrical", {"#50fa7b", 0.35, LinePattern::Dashed, 0.9}));
    REQUIRE(mgr.add_layer("SignalLines", "Wiring", {"#8be9fd", 0.2, LinePattern::Solid, 0.5}));

    // Check children and ancestors
    const auto children = mgr.get_children("Electrical");
    REQUIRE(children.size() == 1);
    REQUIRE(children[0] == "Wiring");

    const auto ancestors = mgr.get_ancestors("SignalLines");
    REQUIRE(ancestors.size() == 3);
    REQUIRE(ancestors[0] == "Wiring");
    REQUIRE(ancestors[1] == "Electrical");
    REQUIRE(ancestors[2] == "0");

    // Effective opacity calculation (0.8 * 0.9 * 0.5 = 0.36)
    REQUIRE(is_approx(mgr.effective_opacity("Electrical"), 0.8, 1e-5));
    REQUIRE(is_approx(mgr.effective_opacity("Wiring"), 0.72, 1e-5));
    REQUIRE(is_approx(mgr.effective_opacity("SignalLines"), 0.36, 1e-5));

    // Color overrides
    REQUIRE(mgr.effective_color("SignalLines") == "#8be9fd");
    REQUIRE(mgr.set_color_override("SignalLines", "#ffffff"));
    REQUIRE(mgr.effective_color("SignalLines") == "#ffffff");
    REQUIRE(mgr.set_color_override("SignalLines", std::nullopt));
    REQUIRE(mgr.effective_color("SignalLines") == "#8be9fd");

    // Line weight overrides
    REQUIRE(is_approx(mgr.effective_line_weight("SignalLines"), 0.2, 1e-5));
    REQUIRE(mgr.set_line_weight_override("SignalLines", 0.75));
    REQUIRE(is_approx(mgr.effective_line_weight("SignalLines"), 0.75, 1e-5));
    REQUIRE(mgr.set_line_weight_override("SignalLines", std::nullopt));
    REQUIRE(is_approx(mgr.effective_line_weight("SignalLines"), 0.2, 1e-5));

    // Active layer switching
    REQUIRE(mgr.set_active_layer("Wiring"));
    REQUIRE(mgr.active_layer() == "Wiring");
    REQUIRE(!mgr.set_active_layer("NonExistentLayer"));

    // Layer reordering
    REQUIRE(mgr.reorder_layer("SignalLines", 1));
    const auto ordered = mgr.ordered_layers();
    REQUIRE(ordered.size() == 4);
    REQUIRE(ordered[1]->name == "SignalLines");

    // Serialization & Deserialization
    const std::string serialized = mgr.to_json();
    REQUIRE(!serialized.empty());

    LayerManager mgr2;
    REQUIRE(mgr2.from_json(serialized));
    REQUIRE(mgr2.layer_count() == 4);
    REQUIRE(mgr2.active_layer() == "Wiring");
    REQUIRE(is_approx(mgr2.effective_opacity("SignalLines"), 0.36, 1e-5));
    REQUIRE(mgr2.find_layer("SignalLines") != nullptr);
}

TEST_CASE("PH4-04.03.002: Multi-selection marquee, lasso, shift-click, and transform gizmo") {
    SketchDocument2D doc;
    // Box 1 at [10, 10] to [20, 20]
    const auto id1 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{10.0, 10.0}, Point2D{20.0, 20.0}));
    // Box 2 at [30, 10] to [40, 20]
    const auto id2 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{30.0, 10.0}, Point2D{40.0, 20.0}));
    // Circle 3 at center [60, 15], radius 5
    const auto id3 = doc.add_entity(std::make_shared<Circle2D>(Point2D{60.0, 15.0}, 5.0));

    SelectionGizmo2D gizmo;

    // 1. Marquee Window selection (only item 1 strictly inside [5, 5] to [25, 25])
    auto window_sel = gizmo.select_marquee(doc, Point2D{5.0, 5.0}, Point2D{25.0, 25.0}, MarqueeMode::Window);
    REQUIRE(window_sel.size() == 1);
    REQUIRE(window_sel[0] == id1);
    REQUIRE(gizmo.is_selected(id1));
    REQUIRE(!gizmo.is_selected(id2));

    // 2. Marquee Crossing selection ([15, 5] to [35, 25] intersects both id1 and id2)
    auto crossing_sel = gizmo.select_marquee(doc, Point2D{15.0, 5.0}, Point2D{35.0, 25.0}, MarqueeMode::Crossing);
    REQUIRE(crossing_sel.size() == 2);
    REQUIRE(gizmo.is_selected(id1));
    REQUIRE(gizmo.is_selected(id2));
    REQUIRE(!gizmo.is_selected(id3));

    // 3. Shift-click toggle selection: toggle id3 into selection, toggle id1 out
    gizmo.apply_click(doc, Point2D{60.0, 15.0}, SelectionModifier::ShiftToggle);
    REQUIRE(gizmo.is_selected(id3));
    gizmo.apply_click(doc, Point2D{15.0, 15.0}, SelectionModifier::ShiftToggle);
    REQUIRE(!gizmo.is_selected(id1));
    REQUIRE(gizmo.selection_count() == 2); // id2 and id3

    // 4. Lasso Polygon Selection around id1
    std::vector<Point2D> lasso = {
        Point2D{5.0, 5.0}, Point2D{25.0, 5.0}, Point2D{25.0, 25.0}, Point2D{5.0, 25.0}
    };
    auto lasso_sel = gizmo.select_lasso(doc, lasso, MarqueeMode::Window, SelectionModifier::Replace);
    REQUIRE(lasso_sel.size() == 1);
    REQUIRE(lasso_sel[0] == id1);
    REQUIRE(gizmo.selection_count() == 1);

    // 5. Transform Gizmo Hit Testing
    gizmo.set_selection({id1, id2});
    gizmo.update_bounds(doc);
    const BBox2D b = gizmo.bounds();
    REQUIRE(is_approx(b.min.x, 10.0));
    REQUIRE(is_approx(b.max.x, 40.0));
    REQUIRE(is_approx(b.min.y, 10.0));
    REQUIRE(is_approx(b.max.y, 20.0));

    // Hit test Move body (center)
    const Point2D center = gizmo.pivot_point();
    REQUIRE(gizmo.hit_test_gizmo(center) == GizmoHandle::MoveBody);

    // Hit test Rotate handle (above top edge)
    const Point2D rot_knob = gizmo.rotate_handle_pos();
    REQUIRE(gizmo.hit_test_gizmo(rot_knob) == GizmoHandle::RotateHandle);

    // Hit test Scale Top-Right handle
    const Point2D tr_handle = gizmo.handle_pos(GizmoHandle::ScaleTopRight);
    REQUIRE(gizmo.hit_test_gizmo(tr_handle) == GizmoHandle::ScaleTopRight);

    // 6. Interactive Translation
    REQUIRE(gizmo.begin_transform(GizmoHandle::MoveBody, center));
    REQUIRE(gizmo.is_transforming());
    Transform2D move_xf = gizmo.update_transform(Point2D{center.x + 10.0, center.y + 5.0});
    REQUIRE(is_approx(move_xf.tx, 10.0));
    REQUIRE(is_approx(move_xf.ty, 5.0));
    REQUIRE(gizmo.apply_transform(doc, move_xf));
    gizmo.end_transform();

    // Verify entities shifted by +10 in X, +5 in Y
    const auto* e1_after = doc.find_entity(id1);
    REQUIRE(e1_after != nullptr);
    const auto& seg1 = static_cast<const LineSegment2D&>(*e1_after->curve);
    REQUIRE(is_approx(seg1.p0.x, 20.0));
    REQUIRE(is_approx(seg1.p0.y, 15.0));
    REQUIRE(is_approx(seg1.p1.x, 30.0));
    REQUIRE(is_approx(seg1.p1.y, 25.0));

    // 7. Interactive Scale 2x from TopRight handle
    gizmo.update_bounds(doc);
    const Point2D tr_new = gizmo.handle_pos(GizmoHandle::ScaleTopRight);
    REQUIRE(gizmo.begin_transform(GizmoHandle::ScaleTopRight, tr_new));
    Transform2D scale_xf = gizmo.update_transform(Point2D{tr_new.x + 30.0, tr_new.y + 10.0});
    REQUIRE(gizmo.apply_transform(doc, scale_xf));
    gizmo.end_transform();

    const BBox2D scaled_b = gizmo.bounds();
    REQUIRE(is_approx(scaled_b.width(), 60.0, 1e-4));
    REQUIRE(is_approx(scaled_b.height(), 20.0, 1e-4));
}


