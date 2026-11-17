#include "tests/test_framework.hpp"

#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/clipboard_manager.hpp"
#include "shunya/cad2d/context_menu_model.hpp"
#include "shunya/cad2d/group_manager.hpp"
#include "shunya/cad2d/layer_manager.hpp"
#include "shunya/cad2d/selection_gizmo.hpp"
#include "shunya/cad2d/sketch_commands.hpp"
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

TEST_CASE("PH4-04.03.003: Group and Ungroup operations preserving relative child transforms and layer assignments") {
    SketchDocument2D doc;
    // Entity 1 on layer "Mechanical"
    const auto id1 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{10.0, 0.0}), false, "Mechanical");
    // Entity 2 on layer "Electrical"
    const auto id2 = doc.add_entity(std::make_shared<Circle2D>(Point2D{10.0, 10.0}, 5.0), false, "Electrical");
    // Entity 3 on layer "Annotation"
    const auto id3 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{20.0, 20.0}, Point2D{30.0, 20.0}), false, "Annotation");

    GroupManager gmgr;

    // 1. Create Subgroup G1 with id1 and id2
    const std::string g1_id = gmgr.create_group(doc, {id1, id2}, {}, "Subgroup_G1", "SubLayer");
    REQUIRE(!g1_id.empty());
    REQUIRE(gmgr.group_count() == 1);
    REQUIRE(gmgr.find_parent_group_of_entity(id1) == g1_id);
    REQUIRE(gmgr.find_parent_group_of_entity(id2) == g1_id);

    // 2. Create Parent Group G2 containing Subgroup G1 and id3
    const std::string g2_id = gmgr.create_group(doc, {id3}, {g1_id}, "Parent_G2");
    REQUIRE(!g2_id.empty());
    REQUIRE(gmgr.group_count() == 2);
    REQUIRE(gmgr.find_parent_group_of_group(g1_id) == g2_id);
    REQUIRE(gmgr.find_parent_group_of_entity(id3) == g2_id);

    // Bounding box of G2 includes all three entities: [0, 0] to [30, 20] (and circle radius extends to [15, 15])
    const BBox2D g2_bbox = gmgr.compute_group_bbox(doc, g2_id);
    REQUIRE(is_approx(g2_bbox.min.x, 0.0));
    REQUIRE(is_approx(g2_bbox.max.x, 30.0));

    // 3. Transform Group G2 by (+15, +25)
    const Transform2D move_xf = Transform2D::translation(15.0, 25.0);
    REQUIRE(gmgr.transform_group(doc, g2_id, move_xf));

    // Verify all child positions moved coherently
    const auto* e1 = doc.find_entity(id1);
    const auto* e2 = doc.find_entity(id2);
    const auto* e3 = doc.find_entity(id3);
    REQUIRE(e1 != nullptr && e2 != nullptr && e3 != nullptr);

    // e1 originally (0,0)->(10,0) now (15,25)->(25,25)
    const auto& seg1 = static_cast<const LineSegment2D&>(*e1->curve);
    REQUIRE(is_approx(seg1.p0.x, 15.0));
    REQUIRE(is_approx(seg1.p0.y, 25.0));
    REQUIRE(is_approx(seg1.p1.x, 25.0));
    REQUIRE(is_approx(seg1.p1.y, 25.0));
    // Verify layers preserved during transform!
    REQUIRE(e1->layer == "Mechanical");
    REQUIRE(e2->layer == "Electrical");
    REQUIRE(e3->layer == "Annotation");

    // e2 center originally (10, 10) now (25, 35)
    const auto& circ2 = static_cast<const Circle2D&>(*e2->curve);
    REQUIRE(is_approx(circ2.center.x, 25.0));
    REQUIRE(is_approx(circ2.center.y, 35.0));

    // 4. Ungroup top level G2
    REQUIRE(gmgr.ungroup(doc, g2_id));
    REQUIRE(gmgr.group_count() == 1); // G1 remains
    REQUIRE(gmgr.find_group(g1_id) != nullptr);
    REQUIRE(gmgr.find_parent_group_of_group(g1_id).empty()); // G1 has no parent now
    REQUIRE(gmgr.find_parent_group_of_entity(id3).empty());  // id3 is a free entity now

    // 5. Ungroup recursive on G1
    REQUIRE(gmgr.ungroup_recursive(doc, g1_id));
    REQUIRE(gmgr.group_count() == 0);
    REQUIRE(gmgr.find_parent_group_of_entity(id1).empty());
    REQUIRE(gmgr.find_parent_group_of_entity(id2).empty());

    // Final check: all entities in doc maintain their layers and transformed positions
    REQUIRE(e1->layer == "Mechanical");
    REQUIRE(e2->layer == "Electrical");
    REQUIRE(e3->layer == "Annotation");
    REQUIRE(is_approx(seg1.p0.x, 15.0));
    REQUIRE(is_approx(circ2.center.x, 25.0));
}

TEST_CASE("PH4-04.03.004: Full clipboard operations Copy, Cut, Paste, PasteAt, Duplicate with deep object cloning") {
    SketchDocument2D doc;
    const auto id1 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{20.0, 0.0}), false, "Mechanical");
    const auto id2 = doc.add_entity(std::make_shared<Circle2D>(Point2D{10.0, 10.0}, 5.0), true, "Electrical");

    ClipboardManager2D clip;
    REQUIRE(!clip.has_data());
    REQUIRE(clip.item_count() == 0);

    // 1. Copy
    REQUIRE(clip.copy(doc, {id1, id2}) == 2);
    REQUIRE(clip.has_data());
    REQUIRE(clip.item_count() == 2);
    REQUIRE(is_approx(clip.clipboard_bbox().center().x, 10.0));

    // Deep clone check: modifying original in doc does NOT mutate clipboard
    doc.mutable_entities()[0].curve = std::make_shared<LineSegment2D>(Point2D{100.0, 100.0}, Point2D{200.0, 100.0});
    const auto& clip_seg = static_cast<const LineSegment2D&>(*clip.items()[0].curve);
    REQUIRE(is_approx(clip_seg.p0.x, 0.0)); // still 0.0, untouched!

    // 2. Paste with offset (+10, +10)
    auto pasted1 = clip.paste(doc, Point2D{10.0, 10.0});
    REQUIRE(pasted1.size() == 2);
    REQUIRE(clip.paste_streak() == 1);

    const auto* p1_ent0 = doc.find_entity(pasted1[0]);
    const auto* p1_ent1 = doc.find_entity(pasted1[1]);
    REQUIRE(p1_ent0 != nullptr && p1_ent1 != nullptr);
    REQUIRE(p1_ent0->layer == "Mechanical");
    REQUIRE(p1_ent1->layer == "Electrical");
    REQUIRE(p1_ent1->is_construction); // construction flag preserved

    const auto& p1_seg = static_cast<const LineSegment2D&>(*p1_ent0->curve);
    REQUIRE(is_approx(p1_seg.p0.x, 10.0));
    REQUIRE(is_approx(p1_seg.p0.y, 10.0));

    // 3. Second paste advances streak to (+20, +20)
    auto pasted2 = clip.paste(doc, Point2D{10.0, 10.0});
    REQUIRE(pasted2.size() == 2);
    REQUIRE(clip.paste_streak() == 2);
    const auto* p2_ent0 = doc.find_entity(pasted2[0]);
    const auto& p2_seg = static_cast<const LineSegment2D&>(*p2_ent0->curve);
    REQUIRE(is_approx(p2_seg.p0.x, 20.0));
    REQUIRE(is_approx(p2_seg.p0.y, 20.0));

    // 4. PasteAt specific center (e.g. at (100, 200))
    auto pasted_at = clip.paste_at(doc, Point2D{100.0, 200.0});
    REQUIRE(pasted_at.size() == 2);
    BBox2D pat_bbox;
    for (const auto& id : pasted_at) {
        pat_bbox.expand(doc.find_entity(id)->curve->bounding_box());
    }
    REQUIRE(is_approx(pat_bbox.center().x, 100.0, 1e-4));
    REQUIRE(is_approx(pat_bbox.center().y, 200.0, 1e-4));

    // 5. In-place Duplicate
    size_t count_before = doc.entities().size();
    auto duplicated = clip.duplicate(doc, {id2}, Point2D{50.0, 50.0});
    REQUIRE(duplicated.size() == 1);
    REQUIRE(doc.entities().size() == count_before + 1);
    const auto* dup_ent = doc.find_entity(duplicated[0]);
    REQUIRE(dup_ent != nullptr);
    REQUIRE(dup_ent->layer == "Electrical");
    const auto& dup_circ = static_cast<const Circle2D&>(*dup_ent->curve);
    REQUIRE(is_approx(dup_circ.center.x, 60.0)); // 10 + 50 = 60
    REQUIRE(is_approx(dup_circ.center.y, 60.0)); // 10 + 50 = 60

    // 6. Cut
    const auto cut_ids = clip.cut(doc, {duplicated[0]});
    REQUIRE(cut_ids == 1);
    REQUIRE(doc.find_entity(duplicated[0]) == nullptr); // removed from doc
    REQUIRE(clip.item_count() == 1);                    // present in clipboard
}

TEST_CASE("PH4-04.03.005: Transactional Undo/Redo command stack connecting 2D editing actions") {
    SketchDocument2D doc;
    GroupManager gmgr;
    SketchCommandHistory2D history;

    REQUIRE(!history.can_undo());
    REQUIRE(!history.can_redo());

    // 1. Add Entity Command
    const std::string id1 = history.execute_add_entity(
        doc,
        std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{50.0, 0.0}),
        false,
        "Mechanical"
    );
    REQUIRE(!id1.empty());
    REQUIRE(doc.entities().size() == 1);
    REQUIRE(history.can_undo());
    REQUIRE(history.undo_name() == "Add 2D Entity");

    // Undo Add
    REQUIRE(history.undo().is_ok());
    REQUIRE(doc.entities().empty());
    REQUIRE(history.can_redo());
    REQUIRE(history.redo_name() == "Add 2D Entity");

    // Redo Add
    REQUIRE(history.redo().is_ok());
    REQUIRE(doc.entities().size() == 1);
    REQUIRE(doc.find_entity(id1) != nullptr);

    // 2. Transform Command (+20, +30)
    const Transform2D move_xf = Transform2D::translation(20.0, 30.0);
    REQUIRE(history.execute_transform(doc, {id1}, move_xf));
    const auto* e1 = doc.find_entity(id1);
    const auto& seg1 = static_cast<const LineSegment2D&>(*e1->curve);
    REQUIRE(is_approx(seg1.p0.x, 20.0));
    REQUIRE(is_approx(seg1.p0.y, 30.0));

    // Undo Transform
    REQUIRE(history.undo().is_ok());
    const auto* e1_undone = doc.find_entity(id1);
    const auto& seg1_undone = static_cast<const LineSegment2D&>(*e1_undone->curve);
    REQUIRE(is_approx(seg1_undone.p0.x, 0.0));
    REQUIRE(is_approx(seg1_undone.p0.y, 0.0));

    // Redo Transform
    REQUIRE(history.redo().is_ok());
    const auto* e1_redone = doc.find_entity(id1);
    const auto& seg1_redone = static_cast<const LineSegment2D&>(*e1_redone->curve);
    REQUIRE(is_approx(seg1_redone.p0.x, 20.0));
    REQUIRE(is_approx(seg1_redone.p0.y, 30.0));

    // 3. Layer and Construction Property Modification
    REQUIRE(history.execute_reassign_layer(doc, {id1}, "Electrical"));
    REQUIRE(doc.find_entity(id1)->layer == "Electrical");
    REQUIRE(history.execute_set_construction(doc, {id1}, true));
    REQUIRE(doc.find_entity(id1)->is_construction);

    REQUIRE(history.undo().is_ok()); // undo construction
    REQUIRE(!doc.find_entity(id1)->is_construction);
    REQUIRE(history.undo().is_ok()); // undo layer
    REQUIRE(doc.find_entity(id1)->layer == "Mechanical");

    // 4. Group Command
    const std::string id2 = history.execute_add_entity(
        doc,
        std::make_shared<Circle2D>(Point2D{10.0, 10.0}, 4.0),
        false,
        "Mechanical"
    );
    const std::string grp_id = history.execute_group(doc, gmgr, {id1, id2}, "TestGroup");
    REQUIRE(!grp_id.empty());
    REQUIRE(gmgr.group_count() == 1);

    REQUIRE(history.undo().is_ok()); // undo group
    REQUIRE(gmgr.group_count() == 0);
    REQUIRE(history.redo().is_ok()); // redo group
    REQUIRE(gmgr.group_count() == 1);

    // 5. Transaction Grouping (Atomic multi-action batch)
    REQUIRE(history.begin_transaction("Batch Add & Delete").is_ok());
    REQUIRE(history.is_in_transaction());
    const std::string id3 = history.execute_add_entity(doc, std::make_shared<LineSegment2D>(Point2D{1, 1}, Point2D{2, 2}));
    REQUIRE(history.execute_delete_entities(doc, {id3}));
    REQUIRE(history.commit_transaction().is_ok());
    REQUIRE(!history.is_in_transaction());

    // Single undo reverts the entire transaction batch
    REQUIRE(history.undo().is_ok());
    // And redo restores it
    REQUIRE(history.redo().is_ok());
}

TEST_CASE("PH4-04.03.006: Right-click contextual menus with properties, transformations, layer assignment, and conversions") {
    SketchDocument2D doc;
    LayerManager lmgr;
    REQUIRE(lmgr.add_layer("Mechanical"));
    REQUIRE(lmgr.add_layer("Electrical"));

    GroupManager gmgr;
    SelectionGizmo2D gizmo;
    ContextMenuModel2D menu_model;

    // 1. Right click on Empty Canvas
    auto empty_menu = menu_model.build_menu(doc, lmgr, gmgr, gizmo, Point2D{500.0, 500.0}, false);
    REQUIRE(!empty_menu.empty());
    REQUIRE(empty_menu[0].id == "action_paste");
    REQUIRE(!empty_menu[0].enabled); // no clipboard data
    REQUIRE(empty_menu[1].id == "action_select_all");

    // Check Add Primitive submenu
    auto it_prim = std::find_if(empty_menu.begin(), empty_menu.end(), [](const MenuItem2D& m) {
        return m.id == "action_add_primitive";
    });
    REQUIRE(it_prim != empty_menu.end());
    REQUIRE(it_prim->subitems.size() >= 3);

    // 2. Right click on Single Entity
    const auto id1 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{10.0, 10.0}, Point2D{50.0, 10.0}), false, "Mechanical");
    gizmo.set_selection({id1});

    auto single_menu = menu_model.build_menu(doc, lmgr, gmgr, gizmo, Point2D{30.0, 10.0});
    REQUIRE(!single_menu.empty());
    REQUIRE(single_menu[0].id == "action_properties");

    // Check Layer Assignment submenu contains "Mechanical" and "Electrical"
    auto it_layers = std::find_if(single_menu.begin(), single_menu.end(), [](const MenuItem2D& m) {
        return m.id == "action_assign_layer";
    });
    REQUIRE(it_layers != single_menu.end());
    REQUIRE(it_layers->subitems.size() >= 3); // "0", "Mechanical", "Electrical"

    // Execute layer assignment via action ID
    REQUIRE(menu_model.execute_action("action_assign_layer:Electrical", doc, lmgr, gmgr, gizmo));
    REQUIRE(doc.find_entity(id1)->layer == "Electrical");

    // Execute conversion to construction geometry
    REQUIRE(menu_model.execute_action("action_toggle_construction", doc, lmgr, gmgr, gizmo));
    REQUIRE(doc.find_entity(id1)->is_construction);

    // 3. Right click on Multi-Entity selection
    const auto id2 = doc.add_entity(std::make_shared<Circle2D>(Point2D{30.0, 30.0}, 10.0), false, "Electrical");
    gizmo.set_selection({id1, id2});

    auto multi_menu = menu_model.build_menu(doc, lmgr, gmgr, gizmo, Point2D{30.0, 20.0});
    REQUIRE(!multi_menu.empty());
    REQUIRE(multi_menu[0].id == "action_group");

    // Check Align submenu exists
    auto it_align = std::find_if(multi_menu.begin(), multi_menu.end(), [](const MenuItem2D& m) {
        return m.id == "action_align";
    });
    REQUIRE(it_align != multi_menu.end());
    REQUIRE(it_align->subitems.size() == 6); // left, center, right, top, middle, bottom

    // Execute Group action
    REQUIRE(menu_model.execute_action("action_group", doc, lmgr, gmgr, gizmo));
    REQUIRE(gmgr.group_count() == 1);

    // 4. Right click on Grouped Entity
    gizmo.set_selection({id1});
    auto group_menu = menu_model.build_menu(doc, lmgr, gmgr, gizmo, Point2D{30.0, 10.0});
    REQUIRE(!group_menu.empty());
    REQUIRE(group_menu[0].id == "action_ungroup");

    // Execute Ungroup action
    REQUIRE(menu_model.execute_action("action_ungroup", doc, lmgr, gmgr, gizmo));
    REQUIRE(gmgr.group_count() == 0);
}






