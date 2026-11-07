#include "tests/test_framework.hpp"
#include "shunya/core/domain/workspace_ui.hpp"

#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// UI-01-001-01: Layout Node Tree & Docking Operations
// =============================================================================

TEST_CASE("UI-01-001-01: DockNode Hierarchy and Operations") {
    // 1. Leaf creation
    auto leaf1 = DockNode::make_leaf("leaf_1", "viewport_3d", "3D Viewport");
    REQUIRE(leaf1 != nullptr);
    CHECK(leaf1->type == DockNodeType::Leaf);
    CHECK(leaf1->node_id == "leaf_1");
    CHECK(leaf1->panel_id == "viewport_3d");
    CHECK(leaf1->title == "3D Viewport");
    CHECK(leaf1->is_visible);
    CHECK_FALSE(leaf1->is_floating);

    // 2. Split creation
    auto leaf2 = DockNode::make_leaf("leaf_2", "property_inspector", "Properties");
    auto split = DockNode::make_split("split_main", DockNodeType::SplitHorizontal, 0.70, leaf1, leaf2);
    REQUIRE(split != nullptr);
    CHECK(split->type == DockNodeType::SplitHorizontal);
    CHECK(split->split_ratio == 0.70);
    CHECK(split->first_child == leaf1);
    CHECK(split->second_child == leaf2);

    // 3. Tabbed creation
    auto leaf3 = DockNode::make_leaf("leaf_3", "schematic_editor", "Schematic");
    auto tabbed = DockNode::make_tabbed("tab_group", {leaf1, leaf3}, 0);
    REQUIRE(tabbed != nullptr);
    CHECK(tabbed->type == DockNodeType::Tabbed);
    CHECK(tabbed->tabs.size() == 2u);
    CHECK(tabbed->active_tab_index == 0u);

    // 4. Recursive search
    auto found1 = split->find_node("leaf_2");
    REQUIRE(found1 != nullptr);
    CHECK(found1->panel_id == "property_inspector");

    auto found_panel = split->find_panel("viewport_3d");
    REQUIRE(found_panel != nullptr);
    CHECK(found_panel->node_id == "leaf_1");

    // 5. Leaf collection
    std::vector<std::shared_ptr<DockNode>> leaves;
    split->collect_leaves(leaves);
    CHECK(leaves.size() == 2u);
}

TEST_CASE("UI-01-001-01: DockingManager Snap Zones and Mutators") {
    DockingManager mgr;
    auto root_leaf = DockNode::make_leaf("center_leaf", "viewport_3d", "3D Viewport");
    mgr.set_root(root_leaf);

    // Dock West (Left)
    bool dock_w = mgr.dock_panel("project_browser", "Project Browser", "center_leaf", DockSnapZone::West);
    CHECK(dock_w);
    REQUIRE(mgr.root() != nullptr);
    CHECK(mgr.root()->type == DockNodeType::SplitHorizontal);
    CHECK(mgr.find_panel("project_browser") != nullptr);
    CHECK(mgr.find_panel("viewport_3d") != nullptr);

    // Dock South (Bottom)
    bool dock_s = mgr.dock_panel("simulation_toolbar", "Simulation Bar", "center_leaf", DockSnapZone::South);
    CHECK(dock_s);
    CHECK(mgr.find_panel("simulation_toolbar") != nullptr);

    // Dock CenterTab
    bool dock_tab = mgr.dock_panel("schematic_editor", "Schematic Canvas", "center_leaf", DockSnapZone::CenterTab);
    CHECK(dock_tab);
    auto tab_node = mgr.find_panel("schematic_editor");
    REQUIRE(tab_node != nullptr);

    // Verify leaf inventory
    auto all_leaves = mgr.get_all_leaves();
    CHECK(all_leaves.size() == 4u);

    // Mutate split ratio
    auto split_root_id = mgr.root()->node_id;
    CHECK(mgr.set_split_ratio(split_root_id, 0.25));
    CHECK(mgr.root()->split_ratio == 0.25);

    // Clamp out-of-range split ratio
    CHECK(mgr.set_split_ratio(split_root_id, 1.5));
    CHECK(mgr.root()->split_ratio == 0.95);

    // Mutate visibility and floating
    CHECK(mgr.toggle_panel_visibility("project_browser", false));
    CHECK_FALSE(mgr.find_panel("project_browser")->is_visible);
    CHECK(mgr.toggle_panel_visibility("project_browser", true));
    CHECK(mgr.find_panel("project_browser")->is_visible);

    CHECK(mgr.set_floating("viewport_3d", true));
    CHECK(mgr.find_panel("viewport_3d")->is_floating);

    // Remove node
    CHECK(mgr.remove_node("node_schematic_editor"));
    CHECK(mgr.find_panel("schematic_editor") == nullptr);
    CHECK(mgr.get_all_leaves().size() == 3u);
}

// =============================================================================
// UI-01-001-02: Workspace Presets Serialization & DPI Restoration
// =============================================================================

TEST_CASE("UI-01-001-02: Presets Layout Generation and JSON Round-Trip") {
    DockingManager mgr;

    // Test all standard presets
    const WorkspacePreset presets[] = {
        WorkspacePreset::Modeling,
        WorkspacePreset::Schematic,
        WorkspacePreset::Simulation,
        WorkspacePreset::Results,
        WorkspacePreset::Scripting
    };

    for (auto preset : presets) {
        mgr.apply_preset(preset);
        REQUIRE(mgr.root() != nullptr);
        auto leaves = mgr.get_all_leaves();
        CHECK(leaves.size() >= 2u);

        // Serialize layout
        std::string json = mgr.serialize_layout_json();
        CHECK(json.find("\"schema_version\": 1") != std::string::npos);
        CHECK(json.find("\"split_ratio\":") != std::string::npos);

        // Restore into new manager
        DockingManager restored_mgr;
        auto res = restored_mgr.restore_layout_json(json);
        REQUIRE(res.is_ok());

        auto restored_leaves = restored_mgr.get_all_leaves();
        CHECK(restored_leaves.size() == leaves.size());

        // Verify leaf panel IDs match
        for (const auto& original_leaf : leaves) {
            auto found = restored_mgr.find_panel(original_leaf->panel_id);
            REQUIRE(found != nullptr);
            CHECK(found->title == original_leaf->title);
            CHECK(found->is_visible == original_leaf->is_visible);
        }

        // Verify root split ratio preserved accurately
        if (mgr.root()->type == DockNodeType::SplitHorizontal || mgr.root()->type == DockNodeType::SplitVertical) {
            CHECK(std::abs(restored_mgr.root()->split_ratio - mgr.root()->split_ratio) < 1e-4);
        }
    }
}

// =============================================================================
// UI-01-001-03: Hotkey-Driven Panel Toggle & Multi-Monitor Management
// =============================================================================

TEST_CASE("UI-01-001-03: DockActionManager Hotkey Bindings & Panel Toggles") {
    DockActionManager action_mgr;
    CHECK(action_mgr.shortcuts().size() >= 5u);

    // Verify default bindings
    auto f9 = action_mgr.find_by_key("F9");
    REQUIRE(f9.has_value());
    CHECK(f9->action_id == "panel.project_browser");
    CHECK(f9->target_panel_id == "project_browser");

    auto f10 = action_mgr.find_by_key("F10");
    REQUIRE(f10.has_value());
    CHECK(f10->action_id == "panel.simulation_toolbar");

    auto f12 = action_mgr.find_by_key("F12");
    REQUIRE(f12.has_value());
    CHECK(f12->action_id == "panel.property_inspector");

    auto ctrl_b = action_mgr.find_by_key("Ctrl+B");
    REQUIRE(ctrl_b.has_value());
    CHECK(ctrl_b->action_id == "view.toggle_sidebars");

    // Execute shortcut toggle against DockingManager
    DockingManager dock_mgr;
    dock_mgr.apply_preset(WorkspacePreset::Modeling);

    auto browser = dock_mgr.find_panel("project_browser");
    REQUIRE(browser != nullptr);
    CHECK(browser->is_visible);

    // Trigger F9 -> should hide project_browser
    bool executed = action_mgr.execute_shortcut("F9", dock_mgr);
    CHECK(executed);
    CHECK_FALSE(browser->is_visible);

    // Trigger F9 again -> should show project_browser
    executed = action_mgr.execute_shortcut("F9", dock_mgr);
    CHECK(executed);
    CHECK(browser->is_visible);

    // Trigger Ctrl+B (toggle sidebars: project_browser & property_inspector)
    auto inspector = dock_mgr.find_panel("property_inspector");
    REQUIRE(inspector != nullptr);
    CHECK(inspector->is_visible);

    executed = action_mgr.execute_shortcut("Ctrl+B", dock_mgr);
    CHECK(executed);
    CHECK_FALSE(browser->is_visible);
    CHECK_FALSE(inspector->is_visible);

    executed = action_mgr.execute_shortcut("Ctrl+B", dock_mgr);
    CHECK(executed);
    CHECK(browser->is_visible);
    CHECK(inspector->is_visible);

    // Non-existent key returns false
    CHECK_FALSE(action_mgr.execute_shortcut("F1", dock_mgr));
}
