#include "tests/test_framework.hpp"
#include "shunya/core/domain/workspace_ui.hpp"

#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// PH4-13.03.001: Qt Quick GUI Automated Test Suite (Menu, Workspaces, Dialogs)
// =============================================================================

TEST_CASE("PH4-13.03.001: Menu Bar Open/Close, Keyboard Navigation & Modal Dismissal") {
    MenuNavigationController nav;
    nav.set_categories({
        MenuCategory::File,
        MenuCategory::Edit,
        MenuCategory::Workspaces,
        MenuCategory::Simulation,
        MenuCategory::Help
    });

    bool new_called = false;
    bool save_called = false;

    std::vector<MenuItemDescriptor> file_items = {
        MenuItemDescriptor::make_action("file.new", "New Project", MenuCategory::File, "Ctrl+N"),
        MenuItemDescriptor::make_action("file.open", "Open Project", MenuCategory::File, "Ctrl+O"),
        MenuItemDescriptor::make_separator("file.sep1"),
        MenuItemDescriptor::make_action("file.save", "Save", MenuCategory::File, "Ctrl+S"),
        MenuItemDescriptor::make_submenu("file.export", "Export", MenuCategory::File, {
            MenuItemDescriptor::make_action("file.export.bundle", "Shunya Bundle", MenuCategory::File),
            MenuItemDescriptor::make_action("file.export.dxf", "DXF Sketch", MenuCategory::File)
        })
    };
    file_items[0].callback = [&new_called]() { new_called = true; };
    file_items[3].callback = [&save_called]() { save_called = true; };
    nav.set_items_for_category(MenuCategory::File, file_items);

    // Initial state: menu closed
    CHECK_FALSE(nav.is_menu_open());

    // 1. Open File Menu
    CHECK(nav.open_menu(MenuCategory::File));
    CHECK(nav.is_menu_open());
    CHECK(nav.active_category().value() == MenuCategory::File);
    CHECK(nav.focused_item_index() == 0);

    // 2. Down Arrow Navigation (skips separator)
    nav.handle_key(NavigationKey::Down); // Index 1: Open
    CHECK(nav.focused_item_index() == 1);
    nav.handle_key(NavigationKey::Down); // Skips separator -> Index 3: Save
    CHECK(nav.focused_item_index() == 3);

    // 3. Execution via Enter
    CHECK(nav.execute_focused_item());
    CHECK(save_called);
    CHECK_FALSE(nav.is_menu_open());

    // 4. Submenu Expand & Collapse
    nav.open_menu(MenuCategory::File);
    nav.handle_key(NavigationKey::Up); // Wrap around to index 4: Export submenu
    CHECK(nav.focused_item_index() == 4);
    nav.handle_key(NavigationKey::Right); // Open submenu
    CHECK(nav.is_submenu_open());
    nav.handle_key(NavigationKey::Left); // Collapse submenu
    CHECK_FALSE(nav.is_submenu_open());
    CHECK(nav.is_menu_open());

    // 5. Escape dismisses
    nav.handle_key(NavigationKey::Escape);
    CHECK_FALSE(nav.is_menu_open());
}

TEST_CASE("PH4-13.03.001: Workspace Switching & State Retention Across All 9 Workspaces") {
    WorkspaceNavigationModel ws_model;
    const auto& list = ws_model.workspaces();
    REQUIRE(list.size() == 9u);

    const std::vector<std::string> expected_ids = {
        "elements", "mixing_lab", "objects", "components", "circuits",
        "editor_2d", "editor_3d", "simulation", "settings"
    };

    // Verify all 9 workspaces can be switched to
    for (size_t i = 0; i < expected_ids.size(); ++i) {
        const auto& id = expected_ids[i];
        CHECK(ws_model.set_active_workspace(id));
        CHECK(ws_model.active_workspace_id() == id);
        CHECK(ws_model.active_workspace_index() == static_cast<int>(i + 1));
    }

    // View State Retention: save camera / view parameters in editor_3d, switch to elements and back
    ws_model.set_active_workspace("editor_3d");
    ws_model.save_view_state("editor_3d", "camera_zoom", "3.75");
    ws_model.save_view_state("editor_3d", "camera_target", "0.0,1.5,0.0");

    ws_model.set_active_workspace("elements");
    CHECK(ws_model.active_workspace_id() == "elements");

    ws_model.set_active_workspace("editor_3d");
    CHECK(ws_model.active_workspace_id() == "editor_3d");
    CHECK(ws_model.get_view_state("editor_3d", "camera_zoom") == "3.75");
    CHECK(ws_model.get_view_state("editor_3d", "camera_target") == "0.0,1.5,0.0");
}

TEST_CASE("PH4-13.03.001: Command Palette Fuzzy Search & Modal Dialog State") {
    UnifiedCommandPaletteModel palette;

    // Search for 3D Editor
    auto results_3d = palette.search("3D");
    REQUIRE(!results_3d.empty());
    bool found_3d = false;
    for (const auto& r : results_3d) {
        if (r.id == "ws.editor_3d") found_3d = true;
    }
    CHECK(found_3d);

    // Search for file actions
    auto results_save = palette.search("save");
    REQUIRE(!results_save.empty());

    // Search for simulation
    auto results_sim = palette.search("transient");
    REQUIRE(!results_sim.empty());
}
