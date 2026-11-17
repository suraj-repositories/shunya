#include "tests/test_framework.hpp"

#include "shunya/cad2d/block_manager.hpp"
#include "shunya/cad2d/clipboard_manager.hpp"
#include "shunya/cad2d/context_menu_model.hpp"
#include "shunya/cad2d/group_manager.hpp"
#include "shunya/cad2d/layer_manager.hpp"
#include "shunya/cad2d/schematic_erc_engine.hpp"
#include "shunya/cad2d/schematic_netlabel_bus.hpp"
#include "shunya/cad2d/schematic_sim_overlay.hpp"
#include "shunya/cad2d/schematic_pin_tool.hpp"
#include "shunya/cad2d/schematic_symbol_engine.hpp"
#include "shunya/cad2d/schematic_wire_router.hpp"
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

TEST_CASE("PH4-04.04.001: Integrated electronic schematic symbol drafting with strict mechanical isolation") {
    SketchDocument2D doc;
    LayerManager lmgr;
    SchematicDraftingEngine sch_engine(shunya::pcb::SymbolStandard::IEEE_US);

    // Ensure schematic layers exist in layer manager
    sch_engine.ensure_schematic_layers(lmgr);
    REQUIRE(lmgr.find_layer(SchematicDraftingEngine::SCHEMATIC_LAYER_SYMBOLS) != nullptr);
    REQUIRE(lmgr.find_layer(SchematicDraftingEngine::SCHEMATIC_LAYER_PINS) != nullptr);
    REQUIRE(lmgr.find_layer(SchematicDraftingEngine::SCHEMATIC_LAYER_WIRES) != nullptr);

    // 1. Create a Mechanical Solid Profile in the document (4 boundary lines forming a closed box 0,0 to 100,50)
    const auto m1 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 0.0}, Point2D{100.0, 0.0}), false, "0");
    const auto m2 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{100.0, 0.0}, Point2D{100.0, 50.0}), false, "0");
    const auto m3 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{100.0, 50.0}, Point2D{0.0, 50.0}), false, "0");
    const auto m4 = doc.add_entity(std::make_shared<LineSegment2D>(Point2D{0.0, 50.0}, Point2D{0.0, 0.0}), false, "0");

    REQUIRE(doc.solid_entities().size() == 4);
    auto initial_profiles = doc.extract_solid_profile_loops();
    REQUIRE(initial_profiles.size() == 1); // 1 closed mechanical loop

    // 2. Switch Mode to Schematic Capture
    sch_engine.set_mode(DraftingDomain::Schematic);
    REQUIRE(sch_engine.current_mode() == DraftingDomain::Schematic);

    // 3. Place Resistor "R" at (20, 25)
    std::string r1_id = sch_engine.place_symbol(doc, "R", Point2D{20.0, 25.0}, 0, "R1", "10k");
    REQUIRE(!r1_id.empty());
    REQUIRE(sch_engine.instance_count() == 1);

    const auto* r1_inst = sch_engine.find_instance(r1_id);
    REQUIRE(r1_inst != nullptr);
    REQUIRE(r1_inst->reference_designator == "R1");
    REQUIRE(r1_inst->value == "10k");
    REQUIRE(r1_inst->pins.size() == 2);

    // 4. Place Capacitor "C" at (60, 25) rotated 90 deg
    std::string c1_id = sch_engine.place_symbol(doc, "C", Point2D{60.0, 25.0}, 90, "C1", "100nF");
    REQUIRE(!c1_id.empty());
    REQUIRE(sch_engine.instance_count() == 2);

    // Pin queries
    const auto pins = sch_engine.all_pins();
    REQUIRE(pins.size() == 4); // 2 on R1 + 2 on C1
    const auto* found_pin = sch_engine.find_pin(pins[0].pin_id);
    REQUIRE(found_pin != nullptr);
    REQUIRE(sch_engine.find_instance_by_pin(pins[0].pin_id) != nullptr);

    // 5. CRUCIAL ARCHITECTURAL INVARIANT: Strict Mechanical Isolation
    // Despite having added schematic symbols, the solid entities count must still be exactly 4!
    REQUIRE(doc.solid_entities().size() == 4);

    // Pure mechanical profile loops must still be exactly 1, completely unaffected by R1 and C1!
    auto mechanical_profiles = sch_engine.extract_pure_mechanical_profiles(doc);
    REQUIRE(mechanical_profiles.size() == 1);
    REQUIRE(sch_engine.validate_mechanical_isolation(doc));

    // Entity filtering
    auto mech_entities = sch_engine.filter_mechanical_entities(doc);
    REQUIRE(mech_entities.size() == 4);
    REQUIRE(mech_entities[0] == m1);
    REQUIRE(mech_entities[1] == m2);
    REQUIRE(mech_entities[2] == m3);
    REQUIRE(mech_entities[3] == m4);

    auto sch_entities = sch_engine.filter_schematic_entities(doc);
    REQUIRE(!sch_entities.empty());

    // 6. Clean removal of schematic component
    REQUIRE(sch_engine.remove_symbol(doc, c1_id));
    REQUIRE(sch_engine.instance_count() == 1);
    REQUIRE(sch_engine.find_instance(c1_id) == nullptr);

    // Mechanical profile remains 100% intact and valid
    REQUIRE(sch_engine.validate_mechanical_isolation(doc));
    auto final_profiles = sch_engine.extract_pure_mechanical_profiles(doc);
    REQUIRE(final_profiles.size() == 1);
}

TEST_CASE("PH4-04.04.002: Orthogonal and 45-degree wire routing with automated junction dot synthesis") {
    // 1. Orthogonal route calculation tests
    // HV route from (0, 0) to (10, 20) -> knee at (10, 0)
    auto route_hv = SchematicWireRouter::calculate_route(Point2D{0.0, 0.0}, Point2D{10.0, 20.0}, WireRouteStyle::Orthogonal_HV);
    REQUIRE(route_hv.size() == 3);
    REQUIRE(is_approx(route_hv[0].x, 0.0) && is_approx(route_hv[0].y, 0.0));
    REQUIRE(is_approx(route_hv[1].x, 10.0) && is_approx(route_hv[1].y, 0.0));
    REQUIRE(is_approx(route_hv[2].x, 10.0) && is_approx(route_hv[2].y, 20.0));

    // VH route from (0, 0) to (10, 20) -> knee at (0, 20)
    auto route_vh = SchematicWireRouter::calculate_route(Point2D{0.0, 0.0}, Point2D{10.0, 20.0}, WireRouteStyle::Orthogonal_VH);
    REQUIRE(route_vh.size() == 3);
    REQUIRE(is_approx(route_vh[1].x, 0.0) && is_approx(route_vh[1].y, 20.0));

    // 45-degree octilinear route from (0, 0) to (10, 30):
    // min(dx=10, dy=30) = 10 -> diagonal to (10, 10), then straight to (10, 30)
    auto route_45 = SchematicWireRouter::calculate_route(Point2D{0.0, 0.0}, Point2D{10.0, 30.0}, WireRouteStyle::Octilinear_45);
    REQUIRE(route_45.size() == 3);
    REQUIRE(is_approx(route_45[1].x, 10.0));
    REQUIRE(is_approx(route_45[1].y, 10.0));
    REQUIRE(is_approx(route_45[2].x, 10.0));
    REQUIRE(is_approx(route_45[2].y, 30.0));

    // 2. Wire network and automated 3-way T-junction synthesis
    SketchDocument2D doc;
    SchematicWireRouter router;

    // Horizontal Wire W1: (0, 10) to (50, 10) on Net "VCC"
    std::string w1_id = router.add_wire(doc, {Point2D{0.0, 10.0}, Point2D{50.0, 10.0}}, "VCC");
    REQUIRE(!w1_id.empty());
    REQUIRE(router.wire_count() == 1);
    REQUIRE(router.junction_count() == 0); // No junction yet (single straight line)

    // Vertical Wire W2: (25, 0) to (25, 10) -> touches W1 in its interior!
    std::string w2_id = router.add_wire(doc, {Point2D{25.0, 0.0}, Point2D{25.0, 10.0}});
    REQUIRE(!w2_id.empty());
    REQUIRE(router.wire_count() == 2);

    // Automatic junction dot created at (25, 10)
    REQUIRE(router.junction_count() == 1);
    const auto& junc = router.junction_dots().front();
    REQUIRE(is_approx(junc.position.x, 25.0, 1e-4));
    REQUIRE(is_approx(junc.position.y, 10.0, 1e-4));
    REQUIRE(junc.connected_wire_ids.size() == 2);
    REQUIRE(is_approx(junc.radius_mm, 0.8));

    // Net name propagation: W2 adopted "VCC" from W1
    const auto* w2_wire = router.find_wire(w2_id);
    REQUIRE(w2_wire != nullptr);
    REQUIRE(w2_wire->net_name == "VCC");

    // 3. Removing W2 removes junction dot
    REQUIRE(router.remove_wire(doc, w2_id));
    REQUIRE(router.wire_count() == 1);
    REQUIRE(router.junction_count() == 0); // orphan dot removed
}

TEST_CASE("PH4-04.04.003: Schematic pin placement tool defining electrical terminals, names, numbers, and types") {
    SketchDocument2D doc;
    SchematicPinPlacementTool pin_tool;
    REQUIRE(pin_tool.pin_count() == 0);

    // 1. Place Input Pin (Pin 1, "DATA_IN", Input, Right orientation) at (0, 10)
    std::string p1_id = pin_tool.place_pin(
        doc,
        Point2D{0.0, 10.0},
        "1",
        "DATA_IN",
        shunya::pcb::PinElectricalType::Input,
        PinOrientation::Right
    );
    REQUIRE(!p1_id.empty());
    REQUIRE(pin_tool.pin_count() == 1);

    const auto* p1 = pin_tool.find_pin(p1_id);
    REQUIRE(p1 != nullptr);
    REQUIRE(p1->pin_number == "1");
    REQUIRE(p1->pin_name == "DATA_IN");
    REQUIRE(p1->electrical_type == shunya::pcb::PinElectricalType::Input);
    REQUIRE(p1->orientation == PinOrientation::Right);

    // Terminal anchor is offset +5.08 along X for Right orientation
    Point2D a1 = pin_tool.terminal_anchor(*p1);
    REQUIRE(is_approx(a1.x, 5.08));
    REQUIRE(is_approx(a1.y, 10.0));

    // 2. Place Output Pin (Pin 2, "DATA_OUT", Output, Left orientation) at (50, 10)
    std::string p2_id = pin_tool.place_pin(
        doc,
        Point2D{50.0, 10.0},
        "2",
        "DATA_OUT",
        shunya::pcb::PinElectricalType::Output,
        PinOrientation::Left
    );
    REQUIRE(!p2_id.empty());
    const auto* p2 = pin_tool.find_pin(p2_id);
    Point2D a2 = pin_tool.terminal_anchor(*p2);
    REQUIRE(is_approx(a2.x, 50.0 - 5.08));
    REQUIRE(is_approx(a2.y, 10.0));

    // 3. Auto-incrementing Pin Numbers (Pin 3 and 4 Power pins)
    pin_tool.set_next_pin_number(3);
    pin_tool.set_default_electrical_type(shunya::pcb::PinElectricalType::PowerInput);
    pin_tool.set_default_orientation(PinOrientation::Up);

    std::string p3_id = pin_tool.place_pin(doc, Point2D{25.0, 0.0});
    std::string p4_id = pin_tool.place_pin(doc, Point2D{25.0, 50.0});
    REQUIRE(pin_tool.find_pin(p3_id)->pin_number == "3");
    REQUIRE(pin_tool.find_pin(p4_id)->pin_number == "4");
    REQUIRE(pin_tool.pin_count() == 4);

    // 4. Hit testing
    REQUIRE(pin_tool.hit_test_pin(Point2D{0.0, 10.0}) == p1_id);
    REQUIRE(pin_tool.hit_test_pin(a1) == p1_id);
    REQUIRE(pin_tool.hit_test_pin(Point2D{999.0, 999.0}).empty());

    // 5. Export to standard symbol pin definitions
    auto exported = pin_tool.export_symbol_pin_defs();
    REQUIRE(exported.size() == 4);
    REQUIRE(exported[0].number == "1");
    REQUIRE(exported[0].name == "DATA_IN");
    REQUIRE(exported[0].electrical_type == shunya::pcb::PinElectricalType::Input);

    // 6. Mechanical isolation check: pins in doc are construction/schematic
    REQUIRE(doc.solid_entities().empty());

    // 7. Removal cleanup
    REQUIRE(pin_tool.remove_pin(doc, p1_id));
    REQUIRE(pin_tool.pin_count() == 3);
    REQUIRE(pin_tool.find_pin(p1_id) == nullptr);
}

TEST_CASE("PH4-04.04.004: Netlabel callouts and hierarchical bus connectors for clean schematic organization") {
    // 1. Bus Specification Parsing
    auto parsed_bus = SchematicNetlabelBusManager::parse_bus_spec("DATA[0..7]");
    REQUIRE(parsed_bus.has_value());
    REQUIRE(parsed_bus->prefix == "DATA");
    REQUIRE(parsed_bus->member_nets.size() == 8);
    REQUIRE(parsed_bus->member_nets.front() == "DATA0");
    REQUIRE(parsed_bus->member_nets.back() == "DATA7");
    REQUIRE(parsed_bus->contains_net("DATA3"));
    REQUIRE(!parsed_bus->contains_net("DATA8"));

    SketchDocument2D doc;
    SchematicNetlabelBusManager mgr;

    // 2. Place Hierarchical Bus in document
    std::vector<Point2D> bus_path = {Point2D{0.0, 50.0}, Point2D{100.0, 50.0}};
    std::string bus_id = mgr.place_bus(doc, bus_path, "DATA[0..7]");
    REQUIRE(!bus_id.empty());
    REQUIRE(mgr.bus_count() == 1);

    // 3. Place Bus Taps
    // Valid tap: breakout DATA3 at contact (40, 50)
    std::string tap1_id = mgr.place_bus_tap(doc, bus_id, Point2D{40.0, 50.0}, "DATA3", 5.0, true);
    REQUIRE(!tap1_id.empty());
    REQUIRE(mgr.bus_tap_count() == 1);
    const auto* tap1 = mgr.find_bus_tap(tap1_id);
    REQUIRE(tap1 != nullptr);
    REQUIRE(tap1->member_net == "DATA3");
    REQUIRE(is_approx(tap1->bus_contact_pos.x, 40.0));
    // 45-deg tap end: 40 + 5*0.7071 = 43.5355, 50 + 5*0.7071 = 53.5355
    REQUIRE(is_approx(tap1->tap_end_pos.x, 40.0 + 5.0 * 0.70710678, 1e-4));

    // Invalid tap: net not on bus is rejected
    REQUIRE(mgr.place_bus_tap(doc, bus_id, Point2D{60.0, 50.0}, "CLOCK").empty());

    // 4. Place Netlabel Callouts
    std::string l1_id = mgr.place_netlabel(doc, Point2D{10.0, 20.0}, "VCC", NetLabelType::Global);
    std::string l2_id = mgr.place_netlabel(doc, Point2D{80.0, 20.0}, "VCC", NetLabelType::Global);
    std::string l3_id = mgr.place_netlabel(doc, Point2D{43.5355, 53.5355}, "DATA3", NetLabelType::Local);
    REQUIRE(mgr.netlabel_count() == 3);

    // 5. Query Wireless Unified Net Connections
    auto vcc_pts = mgr.find_all_positions_for_net("VCC");
    REQUIRE(vcc_pts.size() == 2);
    REQUIRE(is_approx(vcc_pts[0].x, 10.0));
    REQUIRE(is_approx(vcc_pts[1].x, 80.0));

    auto data3_pts = mgr.find_all_positions_for_net("DATA3");
    REQUIRE(data3_pts.size() == 2); // 1 netlabel + 1 bus tap

    // 6. Bus removal cascades to attached taps
    REQUIRE(mgr.remove_bus(doc, bus_id));
    REQUIRE(mgr.bus_count() == 0);
    REQUIRE(mgr.bus_tap_count() == 0); // taps cleaned up
    REQUIRE(mgr.netlabel_count() == 3); // netlabels remain
}

TEST_CASE("PH4-04.04.005: Real-time Electrical Rules Check (ERC) highlighting floating pins, open circuits, and net conflicts") {
    SketchDocument2D doc;
    SchematicDraftingEngine sch_engine(shunya::pcb::SymbolStandard::IEEE_US);
    SchematicWireRouter wire_router(&sch_engine);
    SchematicNetlabelBusManager netlabel_mgr;
    SchematicPinPlacementTool pin_tool;
    SchematicErcEngine2D erc_engine;

    // 1. Place an Input pin without any connection -> Must flag FloatingPin Error
    std::string pin_in_id = pin_tool.place_pin(
        doc,
        Point2D{0.0, 0.0},
        "1",
        "CLK_IN",
        shunya::pcb::PinElectricalType::Input,
        PinOrientation::Right
    );
    Point2D in_anchor = pin_tool.terminal_anchor(*pin_tool.find_pin(pin_in_id));

    auto res1 = erc_engine.analyze(doc, sch_engine, wire_router, netlabel_mgr, &pin_tool);
    REQUIRE(!res1.passed());
    REQUIRE(res1.error_count() == 1);
    REQUIRE(res1.badges[0].rule == ErcRuleKind::FloatingPin);
    REQUIRE(res1.badges[0].severity == shunya::pcb::ErcSeverity::Error);

    // 2. Connect wire W1 to the input pin -> Resolves floating pin, but now wire has only 1 pin (Open Circuit warning)
    std::string w1_id = wire_router.add_wire(doc, {in_anchor, Point2D{50.0, 0.0}}, "CLK");
    auto res2 = erc_engine.analyze(doc, sch_engine, wire_router, netlabel_mgr, &pin_tool);
    REQUIRE(res2.error_count() == 0); // No errors now!
    REQUIRE(res2.warning_count() == 1); // Open circuit warning
    REQUIRE(res2.badges[0].rule == ErcRuleKind::OpenCircuit);

    // 3. Place Output pin at (50, 0) -> Net now has Driver (Output) and Receiver (Input) -> ERC passes completely!
    std::string pin_out_id = pin_tool.place_pin(
        doc,
        Point2D{50.0 + 5.08, 0.0},
        "2",
        "CLK_OUT",
        shunya::pcb::PinElectricalType::Output,
        PinOrientation::Left
    );
    auto res3 = erc_engine.analyze(doc, sch_engine, wire_router, netlabel_mgr, &pin_tool);
    REQUIRE(res3.passed());
    REQUIRE(res3.badges.empty());

    // 4. Test Driver Contention (Output vs Output on same net)
    std::string pin_out2_id = pin_tool.place_pin(
        doc,
        Point2D{50.0 + 5.08, 10.0},
        "3",
        "CLK_OUT2",
        shunya::pcb::PinElectricalType::Output,
        PinOrientation::Left
    );
    Point2D out2_anchor = pin_tool.terminal_anchor(*pin_tool.find_pin(pin_out2_id));
    wire_router.add_wire(doc, {Point2D{50.0, 0.0}, out2_anchor}, "CLK");

    auto res4 = erc_engine.analyze(doc, sch_engine, wire_router, netlabel_mgr, &pin_tool);
    REQUIRE(!res4.passed());
    REQUIRE(res4.error_count() >= 1);
    bool has_driver_conflict = false;
    for (const auto& b : res4.badges) {
        if (b.rule == ErcRuleKind::DriverConflict) has_driver_conflict = true;
    }
    REQUIRE(has_driver_conflict);

    // 5. Test Conflicting Net Labels (VCC and GND on same wire)
    netlabel_mgr.place_netlabel(doc, in_anchor, "VCC");
    netlabel_mgr.place_netlabel(doc, Point2D{50.0, 0.0}, "GND");
    auto res5 = erc_engine.analyze(doc, sch_engine, wire_router, netlabel_mgr, &pin_tool);
    bool has_net_conflict = false;
    for (const auto& b : res5.badges) {
        if (b.rule == ErcRuleKind::NetNameConflict) has_net_conflict = true;
    }
    REQUIRE(has_net_conflict);

    // 6. Visual Badges update and hit-testing
    size_t doc_count_before = doc.entities().size();
    auto updated = erc_engine.update_badges(doc, sch_engine, wire_router, netlabel_mgr, &pin_tool);
    REQUIRE(doc.entities().size() == doc_count_before + updated.badges.size());
    REQUIRE(erc_engine.active_badges().size() == updated.badges.size());

    const auto* badge_found = erc_engine.find_badge_at(in_anchor, 2.0);
    REQUIRE(badge_found != nullptr);

    // Clear badges
    erc_engine.clear_badges(doc);
    REQUIRE(erc_engine.active_badges().empty());
    REQUIRE(doc.entities().size() == doc_count_before);
}

TEST_CASE("PH4-04.04.006: Real-time simulation overlay rendering live node voltages, branch currents, and power") {
    // 1. SI Prefix Formatting Checks
    REQUIRE(SchematicSimOverlay::format_voltage(5.0) == "5.00 V");
    REQUIRE(SchematicSimOverlay::format_voltage(0.0125) == "12.50 mV");
    REQUIRE(SchematicSimOverlay::format_voltage(0.0) == "0.00 V");
    REQUIRE(SchematicSimOverlay::format_current(0.015) == "15.00 mA");
    REQUIRE(SchematicSimOverlay::format_current(2.5) == "2.50 A");
    REQUIRE(SchematicSimOverlay::format_power(0.075) == "75.00 mW");
    REQUIRE(SchematicSimOverlay::format_power(1.5) == "1.50 W");

    // Color map check
    REQUIRE(SchematicSimOverlay::voltage_to_color(5.0) == "#ffb86c");  // Orange
    REQUIRE(SchematicSimOverlay::voltage_to_color(0.0) == "#8be9fd");  // Cyan
    REQUIRE(SchematicSimOverlay::voltage_to_color(-5.0) == "#bd93f9"); // Purple

    // 2. Setup Schematic Scene
    SketchDocument2D doc;
    SchematicDraftingEngine sch_engine(shunya::pcb::SymbolStandard::IEEE_US);
    SchematicWireRouter wire_router(&sch_engine);

    // Place Resistor R1 at (20, 30)
    std::string r1_id = sch_engine.place_symbol(doc, "R", Point2D{20.0, 30.0}, 0, "R1", "1k");
    REQUIRE(!r1_id.empty());

    // Connect Wire W_VCC at (0, 30) to (15, 30)
    std::string w_vcc = wire_router.add_wire(doc, {Point2D{0.0, 30.0}, Point2D{15.0, 30.0}}, "VCC");
    // Connect Wire W_GND at (25, 30) to (50, 30)
    std::string w_gnd = wire_router.add_wire(doc, {Point2D{25.0, 30.0}, Point2D{50.0, 30.0}}, "GND");
    REQUIRE(!w_vcc.empty());
    REQUIRE(!w_gnd.empty());

    // 3. Mock DC Operating Point Simulation Result
    shunya::core::domain::DCOperatingPointResult sim_res;
    sim_res.converged = true;
    sim_res.node_voltages["VCC"] = 5.0;
    sim_res.node_voltages["GND"] = 0.0;
    sim_res.branch_currents["R1"] = 0.005; // 5 mA
    sim_res.component_power["R1"] = 0.025;  // 25 mW

    // 4. Render Simulation Overlay
    SchematicSimOverlay overlay;
    size_t doc_count_before = doc.entities().size();
    overlay.render_overlay(doc, sim_res, sch_engine, wire_router);

    // Verify voltage badges rendered
    REQUIRE(overlay.voltage_badges().size() == 2);
    REQUIRE(overlay.voltage_badges()[0].node_name == "GND" || overlay.voltage_badges()[0].node_name == "VCC");
    REQUIRE(overlay.current_probes().size() == 1);
    REQUIRE(overlay.current_probes()[0].formatted_text == "5.00 mA");
    REQUIRE(overlay.power_badges().size() == 1);
    REQUIRE(overlay.power_badges()[0].formatted_text == "25.00 mW");

    // 5. Interactive Probing
    // Probe along VCC wire at (5, 30)
    auto probe_vcc = overlay.probe_at(Point2D{5.0, 30.0}, sim_res, sch_engine, wire_router);
    REQUIRE(probe_vcc.has_data);
    REQUIRE(probe_vcc.target_label == "Net: VCC");
    REQUIRE(is_approx(probe_vcc.voltage_v, 5.0));

    // Probe at component R1 at (20, 30)
    auto probe_r1 = overlay.probe_at(Point2D{20.0, 30.0}, sim_res, sch_engine, wire_router);
    REQUIRE(probe_r1.has_data);
    REQUIRE(probe_r1.target_label == "R1");
    REQUIRE(is_approx(probe_r1.current_a, 0.005));
    REQUIRE(is_approx(probe_r1.power_w, 0.025));

    // 6. Clear Overlay
    overlay.clear_overlay(doc);
    REQUIRE(overlay.voltage_badges().empty());
    REQUIRE(overlay.current_probes().empty());
    REQUIRE(overlay.power_badges().empty());
    REQUIRE(doc.entities().size() == doc_count_before);
}












