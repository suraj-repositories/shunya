#include "tests/test_framework.hpp"
#include "shunya/core/domain/workspace_ui.hpp"

#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// PH4-12.01.001: VS Code Menu Bar Aesthetic & Desktop Styling
// =============================================================================

TEST_CASE("PH4-12.01.001: VS Code Menu Bar Aesthetic & Theme Tokens") {
    VSCodeMenuStyleConfig config;

    // Dark theme metrics
    CHECK(config.dark_bg_color == "#1E1E1E");
    CHECK(config.dark_border_color == "#333333");
    CHECK(config.dark_hover_color == "#2A2D2E");
    CHECK(config.dark_active_color == "#37373D");

    // Light theme metrics
    CHECK(config.light_bg_color == "#F3F3F3");
    CHECK(config.light_border_color == "#D4D4D4");
    CHECK(config.light_hover_color == "#E8E8E8");
    CHECK(config.light_active_color == "#DADADA");

    // Menu category support
    CHECK(menu_category_to_string(MenuCategory::Workspaces) == "Workspaces");
    auto parsed = menu_category_from_string("Workspaces");
    REQUIRE(parsed.has_value());
    CHECK(parsed.value() == MenuCategory::Workspaces);
}

// =============================================================================
// PH4-12.01.002: Menu Bar Layout Geometry (30px Height, 8px Padding, Typography)
// =============================================================================

TEST_CASE("PH4-12.01.002: Exact Menu Bar Layout Dimensions & Typography") {
    VSCodeMenuStyleConfig config;

    CHECK(config.bar_height == 30.0);
    CHECK(config.bar_padding_horizontal == 8.0);
    CHECK(config.item_height == 28.0);
    CHECK(config.item_padding_horizontal == 8.0);
    CHECK(config.item_icon_width == 20.0);
}

// =============================================================================
// PH4-12.01.003: Floating Dropdown Positioning & Screen Clamping
// =============================================================================

TEST_CASE("PH4-12.01.003: Floating Dropdown Positioning Directly Below Titles") {
    VSCodeMenuStyleConfig config;

    // Standard dropdown opening below menu title
    auto pos1 = compute_dropdown_position(10.0, 48.0, 220.0, 1920.0, config.bar_height);
    CHECK(pos1.x == 10.0);
    CHECK(pos1.y == 30.0);
    CHECK(pos1.width == 220.0);
    CHECK_FALSE(pos1.clamped);
    CHECK_FALSE(pos1.flipped_horizontal);

    // Screen boundary clamping when opening near right edge
    auto pos2 = compute_dropdown_position(1800.0, 50.0, 220.0, 1920.0, config.bar_height);
    CHECK(pos2.y == 30.0);
    CHECK(pos2.x + pos2.width <= 1920.0);
    CHECK(pos2.clamped);
    CHECK(pos2.flipped_horizontal);
    CHECK(pos2.x == 1700.0);
}

// =============================================================================
// PH4-12.01.004: Nested Submenus & Intelligent Screen Boundary Flip
// =============================================================================

TEST_CASE("PH4-12.01.004: Nested Submenus & Screen Boundary Flip") {
    // Normal submenu opening to the right
    auto sub1 = compute_submenu_position(100.0, 30.0, 240.0, 56.0, 220.0, 180.0, 1920.0, 1080.0);
    CHECK(sub1.x == 340.0); // 100 + 240
    CHECK(sub1.y == 86.0);  // 30 + 56
    CHECK_FALSE(sub1.flipped_left);
    CHECK_FALSE(sub1.clamped_vertical);

    // Submenu near right screen edge flips to open on the left
    auto sub2 = compute_submenu_position(1750.0, 30.0, 200.0, 56.0, 220.0, 180.0, 1920.0, 1080.0);
    // 1750 + 200 + 220 = 2170 > 1920 -> flips to 1750 - 220 = 1530
    CHECK(sub2.flipped_left);
    CHECK(sub2.x == 1530.0);

    // Submenu near bottom screen edge clamps vertically
    auto sub3 = compute_submenu_position(100.0, 950.0, 240.0, 56.0, 220.0, 200.0, 1920.0, 1080.0);
    // 950 + 56 + 200 = 1206 > 1080 -> clamps to 1080 - 200 = 880
    CHECK(sub3.clamped_vertical);
    CHECK(sub3.y == 880.0);
}

// =============================================================================
// PH4-12.01.005: Keyboard Navigation & Alt-Key Mnemonics
// =============================================================================

TEST_CASE("PH4-12.01.005: Keyboard Navigation & Alt Mnemonics") {
    // Alt-key mnemonics
    CHECK(resolve_alt_mnemonic("Alt+F").value() == MenuCategory::File);
    CHECK(resolve_alt_mnemonic("Alt+f").value() == MenuCategory::File);
    CHECK(resolve_alt_mnemonic("Alt+E").value() == MenuCategory::Edit);
    CHECK(resolve_alt_mnemonic("Alt+W").value() == MenuCategory::Workspaces);
    CHECK(resolve_alt_mnemonic("Alt+S").value() == MenuCategory::Simulation);
    CHECK(resolve_alt_mnemonic("Alt+H").value() == MenuCategory::Help);
    CHECK_FALSE(resolve_alt_mnemonic("Alt+Z").has_value());

    // Navigation controller
    MenuNavigationController nav;
    nav.set_categories({MenuCategory::File, MenuCategory::Edit, MenuCategory::Workspaces, MenuCategory::Help});

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

    CHECK_FALSE(nav.is_menu_open());

    // Open File menu via mnemonic
    CHECK(nav.open_menu(MenuCategory::File));
    CHECK(nav.is_menu_open());
    CHECK(nav.active_category().value() == MenuCategory::File);
    CHECK(nav.focused_item_index() == 0);

    // Down arrow navigates items and skips separator
    nav.handle_key(NavigationKey::Down); // Index 1: Open
    CHECK(nav.focused_item_index() == 1);
    nav.handle_key(NavigationKey::Down); // Skips sep1 (index 2) -> Index 3: Save
    CHECK(nav.focused_item_index() == 3);

    // Enter executes Save action
    CHECK(nav.execute_focused_item());
    CHECK(save_called);
    CHECK_FALSE(nav.is_menu_open()); // Closed after execution

    // Reopen and test submenus
    nav.open_menu(MenuCategory::File);
    nav.handle_key(NavigationKey::Up); // Wrap around to index 4: Export
    CHECK(nav.focused_item_index() == 4);
    CHECK_FALSE(nav.is_submenu_open());

    // Right arrow on submenu opens it
    nav.handle_key(NavigationKey::Right);
    CHECK(nav.is_submenu_open());
    CHECK(nav.focused_submenu_item_index() == 0);

    // Left arrow inside submenu closes it without closing parent
    nav.handle_key(NavigationKey::Left);
    CHECK_FALSE(nav.is_submenu_open());
    CHECK(nav.is_menu_open());

    // Escape closes menu
    nav.handle_key(NavigationKey::Escape);
    CHECK_FALSE(nav.is_menu_open());
}

// =============================================================================
// PH4-12.01.006: Monospace Keyboard Shortcut Badges
// =============================================================================

TEST_CASE("PH4-12.01.006: Keyboard Shortcut Badges Formatting") {
    MenuItemDescriptor item = MenuItemDescriptor::make_action("file.save_as", "Save As...", MenuCategory::File, "Ctrl+Shift+S");

    CHECK(item.shortcut == "Ctrl+Shift+S");
    CHECK(item.shortcut.find("Ctrl") != std::string::npos);
    CHECK(item.shortcut.find("Shift") != std::string::npos);
    CHECK(item.shortcut.find("S") != std::string::npos);

    VSCodeMenuStyleConfig config;
    CHECK(config.item_shortcut_spacing >= 16.0);
}

// =============================================================================
// PH4-12.01.007: Menu Separators, Disabled Opacity (40%), and Checkable Ticks
// =============================================================================

TEST_CASE("PH4-12.01.007: Menu Separators, Disabled State & Checkable Items") {
    VSCodeMenuStyleConfig config;

    // 40% disabled opacity invariant
    CHECK(config.disabled_opacity == 0.40);
    CHECK(config.separator_height == 9.0);
    CHECK(config.separator_line_height == 1.0);

    // Checkable menu item
    MenuItemDescriptor check_item = MenuItemDescriptor::make_action("view.console", "Diagnostics Console", MenuCategory::View, "F12");
    check_item.is_checkable = true;
    check_item.is_checked = false;

    CHECK(check_item.is_checkable);
    CHECK_FALSE(check_item.is_checked);

    check_item.is_checked = true;
    CHECK(check_item.is_checked);

    // Disabled item
    MenuItemDescriptor disabled_item = MenuItemDescriptor::make_action("edit.redo", "Redo", MenuCategory::Edit, "Ctrl+Y");
    disabled_item.is_enabled = false;
    CHECK_FALSE(disabled_item.is_enabled);
}

// =============================================================================
// PH4-12.01.008: Click-Outside-To-Close & Modal Z-Index Layering
// =============================================================================

TEST_CASE("PH4-12.01.008: Modal Z-Index Layering (z=10000) & Outside Click Dismissal") {
    VSCodeMenuStyleConfig config;

    // Must layer above OpenGL / Quick3D viewports (z >= 10000)
    CHECK(config.popup_z_index >= 10000);

    MenuNavigationController nav;
    nav.set_categories({MenuCategory::File});
    nav.set_items_for_category(MenuCategory::File, {
        MenuItemDescriptor::make_action("file.new", "New", MenuCategory::File)
    });

    nav.open_menu(MenuCategory::File);
    CHECK(nav.is_menu_open());

    // Click outside dismissal
    nav.close_menu();
    CHECK_FALSE(nav.is_menu_open());
    CHECK(nav.focused_item() == nullptr);
}

// =============================================================================
// PH4-12.02.001: Workspace Switcher Bar Active Highlights & Alignment
// =============================================================================

TEST_CASE("PH4-12.02.001: Workspace Switcher Bar Alignment & Indicators") {
    WorkspaceNavigationModel ws_model;
    const auto& list = ws_model.workspaces();
    REQUIRE(list.size() == 9u);

    // Default active is Elements
    CHECK(ws_model.active_workspace_id() == "elements");
    CHECK(ws_model.active_workspace_index() == 1);

    // Each workspace has exact identifier, label, and matching SVG icon
    CHECK(list[0].id == "elements");
    CHECK(list[0].label == "Elements");
    CHECK(list[0].icon == "elements");

    CHECK(list[1].id == "mixing_lab");
    CHECK(list[1].label == "Mixing Lab");
    CHECK(list[1].icon == "mixing");

    CHECK(list[2].id == "objects");
    CHECK(list[2].label == "Objects");
    CHECK(list[2].icon == "objects");

    CHECK(list[3].id == "components");
    CHECK(list[3].label == "Components");
    CHECK(list[3].icon == "components");

    CHECK(list[4].id == "circuits");
    CHECK(list[4].label == "Circuits");
    CHECK(list[4].icon == "circuits");

    CHECK(list[5].id == "editor_2d");
    CHECK(list[5].label == "2D CAD");
    CHECK(list[5].icon == "editor2d");

    CHECK(list[6].id == "editor_3d");
    CHECK(list[6].label == "3D CAD");
    CHECK(list[6].icon == "editor3d");

    CHECK(list[7].id == "simulation");
    CHECK(list[7].label == "Simulation");
    CHECK(list[7].icon == "simulation");

    CHECK(list[8].id == "settings");
    CHECK(list[8].label == "Settings");
    CHECK(list[8].icon == "settings");
}

// =============================================================================
// PH4-12.02.002: 9 Primary Workspaces Accessibility & View State Preservation
// =============================================================================

TEST_CASE("PH4-12.02.002: 9 Primary Workspaces Accessibility & State Preservation") {
    WorkspaceNavigationModel ws_model;

    const std::vector<std::string> expected_ids = {
        "elements", "mixing_lab", "objects", "components", "circuits",
        "editor_2d", "editor_3d", "simulation", "settings"
    };

    for (size_t i = 0; i < expected_ids.size(); ++i) {
        const auto& id = expected_ids[i];
        CHECK(ws_model.set_active_workspace(id));
        CHECK(ws_model.active_workspace_id() == id);
        CHECK(ws_model.active_workspace_index() == static_cast<int>(i + 1));
        const auto* item = ws_model.find_workspace(id);
        REQUIRE(item != nullptr);
        CHECK(item->index == static_cast<int>(i + 1));
    }

    // Invalid workspace rejection
    CHECK_FALSE(ws_model.set_active_workspace("invalid_unknown_ws"));
    CHECK(ws_model.find_workspace("invalid_unknown_ws") == nullptr);

    // View state preservation across switches
    ws_model.save_view_state("editor_2d", "zoom_level", "1.75");
    ws_model.save_view_state("editor_2d", "pan_x", "120.5");
    ws_model.save_view_state("editor_3d", "camera_pitch", "-22.5");
    ws_model.save_view_state("circuits", "selected_net", "NET_VCC");
    ws_model.save_view_state("simulation", "playback_time", "3.45");

    CHECK(ws_model.has_view_state("editor_2d"));
    CHECK(ws_model.get_view_state("editor_2d", "zoom_level") == "1.75");
    CHECK(ws_model.get_view_state("editor_2d", "pan_x") == "120.5");
    CHECK(ws_model.get_view_state("editor_3d", "camera_pitch") == "-22.5");
    CHECK(ws_model.get_view_state("circuits", "selected_net") == "NET_VCC");
    CHECK(ws_model.get_view_state("simulation", "playback_time") == "3.45");

    // Switching back and forth preserves states
    ws_model.set_active_workspace("editor_3d");
    CHECK(ws_model.get_view_state("editor_2d", "zoom_level") == "1.75");

    ws_model.set_active_workspace("simulation");
    CHECK(ws_model.get_view_state("editor_3d", "camera_pitch") == "-22.5");
}
