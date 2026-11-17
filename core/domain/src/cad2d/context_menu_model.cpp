#include "shunya/cad2d/context_menu_model.hpp"

#include <cmath>

namespace shunya::cad2d {

ContextMenuContext ContextMenuModel2D::determine_context(
    const SketchDocument2D& doc,
    const GroupManager& group_mgr,
    const SelectionGizmo2D& selection_gizmo,
    Point2D click_pos
) const {
    if (selection_gizmo.selection_count() > 1) {
        return ContextMenuContext::MultiEntity;
    }

    std::string target_id;
    if (selection_gizmo.selection_count() == 1) {
        target_id = selection_gizmo.selected_ids().front();
    } else {
        target_id = selection_gizmo.hit_test_entity(doc, click_pos);
    }

    if (target_id.empty()) {
        return ContextMenuContext::EmptyCanvas;
    }

    if (!group_mgr.find_parent_group_of_entity(target_id).empty()) {
        return ContextMenuContext::Group;
    }

    return ContextMenuContext::SingleEntity;
}

std::vector<MenuItem2D> ContextMenuModel2D::build_menu(
    const SketchDocument2D& doc,
    const LayerManager& layer_mgr,
    const GroupManager& group_mgr,
    const SelectionGizmo2D& selection_gizmo,
    Point2D click_pos,
    bool has_clipboard_data
) const {
    ContextMenuContext ctx = determine_context(doc, group_mgr, selection_gizmo, click_pos);
    std::vector<MenuItem2D> menu;

    if (ctx == ContextMenuContext::EmptyCanvas) {
        menu.push_back(MenuItem2D{"action_paste", "Paste", "Ctrl+V", "paste", has_clipboard_data, std::nullopt, false, {}});
        menu.push_back(MenuItem2D{"action_select_all", "Select All", "Ctrl+A", "select-all", true, std::nullopt, false, {}});
        menu.push_back(MenuItem2D::separator());
        menu.push_back(MenuItem2D{"action_grid_settings", "Grid Settings...", "", "grid", true, std::nullopt, false, {}});
        menu.push_back(MenuItem2D{"action_zoom_extents", "Zoom Extents", "Z", "zoom", true, std::nullopt, false, {}});
        menu.push_back(MenuItem2D::separator());

        MenuItem2D add_prim{"action_add_primitive", "Add Primitive", "", "plus", true, std::nullopt, false, {}};
        add_prim.subitems.push_back(MenuItem2D{"action_add_line", "Line", "L", "line", true, std::nullopt, false, {}});
        add_prim.subitems.push_back(MenuItem2D{"action_add_circle", "Circle", "C", "circle", true, std::nullopt, false, {}});
        add_prim.subitems.push_back(MenuItem2D{"action_add_rectangle", "Rectangle", "R", "rect", true, std::nullopt, false, {}});
        add_prim.subitems.push_back(MenuItem2D{"action_add_arc", "Arc (3-Point)", "A", "arc", true, std::nullopt, false, {}});
        menu.push_back(std::move(add_prim));
        return menu;
    }

    // Determine target entity if single
    std::string single_id;
    const SketchEntity* single_ent = nullptr;
    if (ctx == ContextMenuContext::SingleEntity || ctx == ContextMenuContext::Group) {
        if (selection_gizmo.selection_count() == 1) {
            single_id = selection_gizmo.selected_ids().front();
        } else {
            single_id = selection_gizmo.hit_test_entity(doc, click_pos);
        }
        single_ent = doc.find_entity(single_id);
    }

    if (ctx == ContextMenuContext::Group) {
        menu.push_back(MenuItem2D{"action_ungroup", "Ungroup", "Ctrl+Shift+G", "ungroup", true, std::nullopt, false, {}});
        menu.push_back(MenuItem2D::separator());
    } else if (ctx == ContextMenuContext::MultiEntity) {
        menu.push_back(MenuItem2D{"action_group", "Group", "Ctrl+G", "group", true, std::nullopt, false, {}});

        MenuItem2D align_menu{"action_align", "Align", "", "align", true, std::nullopt, false, {}};
        align_menu.subitems.push_back(MenuItem2D{"action_align_left", "Align Left", "", "", true, std::nullopt, false, {}});
        align_menu.subitems.push_back(MenuItem2D{"action_align_center", "Align Center", "", "", true, std::nullopt, false, {}});
        align_menu.subitems.push_back(MenuItem2D{"action_align_right", "Align Right", "", "", true, std::nullopt, false, {}});
        align_menu.subitems.push_back(MenuItem2D{"action_align_top", "Align Top", "", "", true, std::nullopt, false, {}});
        align_menu.subitems.push_back(MenuItem2D{"action_align_middle", "Align Middle", "", "", true, std::nullopt, false, {}});
        align_menu.subitems.push_back(MenuItem2D{"action_align_bottom", "Align Bottom", "", "", true, std::nullopt, false, {}});
        menu.push_back(std::move(align_menu));

        MenuItem2D dist_menu{"action_distribute", "Distribute", "", "distribute", true, std::nullopt, false, {}};
        dist_menu.subitems.push_back(MenuItem2D{"action_distribute_h", "Distribute Horizontally", "", "", true, std::nullopt, false, {}});
        dist_menu.subitems.push_back(MenuItem2D{"action_distribute_v", "Distribute Vertically", "", "", true, std::nullopt, false, {}});
        menu.push_back(std::move(dist_menu));
        menu.push_back(MenuItem2D::separator());
    }

    if (single_ent) {
        menu.push_back(MenuItem2D{"action_properties", "Entity Properties...", "", "info", true, std::nullopt, false, {}});
        menu.push_back(MenuItem2D::separator());
    }

    // Transformations submenu
    MenuItem2D xf_menu{"action_transform", "Transform", "", "transform", true, std::nullopt, false, {}};
    xf_menu.subitems.push_back(MenuItem2D{"action_rotate_90_cw", "Rotate 90° CW", "", "rotate-right", true, std::nullopt, false, {}});
    xf_menu.subitems.push_back(MenuItem2D{"action_rotate_90_ccw", "Rotate 90° CCW", "", "rotate-left", true, std::nullopt, false, {}});
    xf_menu.subitems.push_back(MenuItem2D{"action_flip_h", "Flip Horizontally", "", "flip-h", true, std::nullopt, false, {}});
    xf_menu.subitems.push_back(MenuItem2D{"action_flip_v", "Flip Vertically", "", "flip-v", true, std::nullopt, false, {}});
    menu.push_back(std::move(xf_menu));

    // Layer Assignment Submenu
    MenuItem2D layer_sub{"action_assign_layer", "Assign to Layer", "", "layers", true, std::nullopt, false, {}};
    for (const auto* l : layer_mgr.ordered_layers()) {
        bool is_chk = single_ent ? (single_ent->layer == l->name) : false;
        layer_sub.subitems.push_back(MenuItem2D{
            "action_assign_layer:" + l->name,
            l->name,
            "",
            "",
            true,
            is_chk,
            false,
            {}
        });
    }
    menu.push_back(std::move(layer_sub));

    // Conversion Submenu
    MenuItem2D conv_menu{"action_conversion", "Conversion", "", "convert", true, std::nullopt, false, {}};
    if (ctx == ContextMenuContext::SingleEntity && single_ent) {
        conv_menu.subitems.push_back(MenuItem2D{
            "action_toggle_construction",
            "Construction Geometry",
            "",
            "construction",
            true,
            single_ent->is_construction,
            false,
            {}
        });
    } else {
        conv_menu.subitems.push_back(MenuItem2D{"action_convert_construction_all", "Convert to Construction", "", "", true, std::nullopt, false, {}});
        conv_menu.subitems.push_back(MenuItem2D{"action_convert_solid_all", "Convert to Solid", "", "", true, std::nullopt, false, {}});
    }
    menu.push_back(std::move(conv_menu));

    menu.push_back(MenuItem2D::separator());
    menu.push_back(MenuItem2D{"action_cut", "Cut", "Ctrl+X", "cut", true, std::nullopt, false, {}});
    menu.push_back(MenuItem2D{"action_copy", "Copy", "Ctrl+C", "copy", true, std::nullopt, false, {}});
    menu.push_back(MenuItem2D{"action_delete", "Delete", "Del", "trash", true, std::nullopt, false, {}});

    return menu;
}

bool ContextMenuModel2D::execute_action(
    std::string_view action_id,
    SketchDocument2D& doc,
    LayerManager& layer_mgr,
    GroupManager& group_mgr,
    SelectionGizmo2D& selection_gizmo,
    SketchCommandHistory2D* history
) {
    (void)layer_mgr;
    const auto& sel = selection_gizmo.selected_ids();

    if (action_id == "action_select_all") {
        std::vector<std::string> all_ids;
        for (const auto& e : doc.entities()) {
            if (e.visible) all_ids.push_back(e.id);
        }
        selection_gizmo.set_selection(all_ids);
        selection_gizmo.update_bounds(doc);
        return true;
    }

    if (action_id == "action_group") {
        if (sel.size() < 2) return false;
        if (history) {
            std::string gid = history->execute_group(doc, group_mgr, sel, "Group");
            return !gid.empty();
        } else {
            std::string gid = group_mgr.create_group(doc, sel);
            return !gid.empty();
        }
    }

    if (action_id == "action_ungroup") {
        for (const auto& id : sel) {
            std::string p_gid = group_mgr.find_parent_group_of_entity(id);
            if (!p_gid.empty()) {
                return group_mgr.ungroup(doc, p_gid);
            }
        }
        return false;
    }

    if (action_id == "action_delete") {
        if (sel.empty()) return false;
        if (history) {
            bool ok = history->execute_delete_entities(doc, sel);
            if (ok) selection_gizmo.clear_selection();
            return ok;
        } else {
            for (const auto& id : sel) doc.remove_entity(id);
            selection_gizmo.clear_selection();
            return true;
        }
    }

    if (action_id == "action_toggle_construction") {
        if (sel.empty()) return false;
        for (const auto& id : sel) {
            if (const auto* e = doc.find_entity(id)) {
                bool next_val = !e->is_construction;
                if (history) {
                    history->execute_set_construction(doc, {id}, next_val);
                } else {
                    doc.set_construction(id, next_val);
                }
            }
        }
        return true;
    }

    if (action_id == "action_convert_construction_all") {
        if (sel.empty()) return false;
        if (history) {
            return history->execute_set_construction(doc, sel, true);
        } else {
            for (const auto& id : sel) doc.set_construction(id, true);
            return true;
        }
    }

    if (action_id == "action_convert_solid_all") {
        if (sel.empty()) return false;
        if (history) {
            return history->execute_set_construction(doc, sel, false);
        } else {
            for (const auto& id : sel) doc.set_construction(id, false);
            return true;
        }
    }

    if (action_id == "action_rotate_90_cw" || action_id == "action_rotate_90_ccw") {
        if (sel.empty()) return false;
        Point2D pivot = selection_gizmo.pivot_point();
        double d_theta = (action_id == "action_rotate_90_cw") ? (-3.14159265358979323846 * 0.5) : (3.14159265358979323846 * 0.5);
        auto t_to = Transform2D::translation(-pivot.x, -pivot.y);
        auto rot = Transform2D::rotation(d_theta);
        auto t_back = Transform2D::translation(pivot.x, pivot.y);
        Transform2D xf = t_back * rot * t_to;

        if (history) {
            return history->execute_transform(doc, sel, xf);
        } else {
            return selection_gizmo.apply_transform(doc, xf);
        }
    }

    if (action_id.rfind("action_assign_layer:", 0) == 0) {
        std::string layer_name = std::string(action_id.substr(20));
        if (sel.empty()) return false;
        if (history) {
            return history->execute_reassign_layer(doc, sel, layer_name);
        } else {
            for (const auto& id : sel) {
                for (auto& ent : doc.mutable_entities()) {
                    if (ent.id == id) {
                        ent.layer = layer_name;
                        break;
                    }
                }
            }
            return true;
        }
    }

    return false;
}

} // namespace shunya::cad2d
