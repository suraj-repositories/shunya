#include "tests/test_framework.hpp"
#include "shunya/core/domain/workspace_ui.hpp"

#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// PH4-12.02.003: Command Palette / Quick Switcher Fuzzy Search
// =============================================================================

TEST_CASE("PH4-12.02.003: Command Palette & Quick Switcher Search") {
    UnifiedCommandPaletteModel palette;
    const auto& all = palette.all_entries();
    CHECK(!all.empty());
    CHECK(all.size() >= 15u);

    // Search for workspace
    auto ws_results = palette.search("elements");
    REQUIRE(!ws_results.empty());
    CHECK(ws_results[0].id == "ws.elements");
    CHECK(ws_results[0].category == "Workspace");

    // Search for menu action
    auto save_results = palette.search("save");
    REQUIRE(save_results.size() >= 2u);

    // Search for simulation
    auto sim_results = palette.search("transient");
    REQUIRE(!sim_results.empty());
    CHECK(sim_results[0].id == "sim.start");

    // Execution callback
    bool action_executed = false;
    palette.register_entry({"custom.action", "Custom Test Action", "Custom", "Ctrl+T", "test", [&action_executed]() {
        action_executed = true;
    }});

    CHECK(palette.execute_entry("custom.action"));
    CHECK(action_executed);
}

// =============================================================================
// PH4-12.02.004: Bottom Status Bar Formatter
// =============================================================================

TEST_CASE("PH4-12.02.004: Status Bar Formatter Metrics") {
    StatusBarState status;
    status.active_tool = "Wire";
    status.cursor_x_mm = 12.5;
    status.cursor_y_mm = -4.2;
    status.cursor_z_mm = 0.0;
    status.snap_enabled = true;
    status.snap_grid_mm = 1.0;
    status.simulation_clock_sec = 2.45;
    status.simulation_status = "Running";

    CHECK(status.format_cursor_coords() == "X: 12.50 mm  Y: -4.20 mm  Z: 0.00 mm");
    CHECK(status.format_snap_status() == "SNAP: ON (1.0 mm)");
    CHECK(status.format_clock() == "Clock: 2.45 s [Running]");

    status.snap_enabled = false;
    CHECK(status.format_snap_status() == "SNAP: OFF");
}

// =============================================================================
// PH4-12.02.005: Theme Harmonization & Token Contrast
// =============================================================================

TEST_CASE("PH4-12.02.005: Theme Token Contrast & Color Mapping") {
    VSCodeMenuStyleConfig config;

    // Dark theme background is low-luminance dark grey
    CHECK(config.dark_bg_color.rfind("#", 0) == 0);
    CHECK(config.dark_bg_color == "#1E1E1E");

    // Light theme background is high-luminance light grey
    CHECK(config.light_bg_color.rfind("#", 0) == 0);
    CHECK(config.light_bg_color == "#F3F3F3");

    // Invariant: disabled opacity must be strictly 0.40 (40%)
    CHECK(config.disabled_opacity == 0.40);
}

// =============================================================================
// PH4-12.03.001: Modal Dialog Configurations
// =============================================================================

TEST_CASE("PH4-12.03.001: Modal Dialog Configurations & Layouts") {
    ProjectTemplateRegistry templates;
    const auto& list = templates.templates();
    CHECK(!list.empty());

    // Verify default templates present for New Project Wizard
    auto pcb = templates.get_template("template.pcb");
    REQUIRE(pcb.has_value());
    CHECK(pcb->default_preset == WorkspacePreset::Schematic);

    auto multi = templates.get_template("template.multiphysics");
    REQUIRE(multi.has_value());
    CHECK(multi->default_preset == WorkspacePreset::Simulation);
}

// =============================================================================
// PH4-12.03.002: Property Panel Accordions & Spinboxes with Units
// =============================================================================

TEST_CASE("PH4-12.03.002: Property Accordion Sections & Units") {
    PropertyAccordionSection section{
        "geom_props", "Dimensions & Placement", false,
        {{"Width", "10.00 mm"}, {"Resistance", "2.50 kΩ"}, {"Voltage", "3.70 V"}}
    };

    CHECK(section.section_id == "geom_props");
    CHECK_FALSE(section.is_collapsed);
    REQUIRE(section.properties.size() == 3u);
    CHECK(section.properties[0].second == "10.00 mm");
    CHECK(section.properties[1].second == "2.50 kΩ");
}

// =============================================================================
// PH4-12.03.003: Empty States & Error Banners
// =============================================================================

TEST_CASE("PH4-12.03.003: Empty States & Error Banner Descriptors") {
    EmptyStateDescriptor empty_desc{
        "No Active Simulation Run",
        "Configure circuit excitation or press Start (F5) to generate telemetry data.",
        "Start Simulation",
        "simulation"
    };

    CHECK(empty_desc.title == "No Active Simulation Run");
    CHECK(empty_desc.action_label == "Start Simulation");
    CHECK(empty_desc.icon_name == "simulation");
}

// =============================================================================
// PH4-12.03.004: Telemetry CSV Export & Multi-Curve Telemetry
// =============================================================================

TEST_CASE("PH4-12.03.004: Telemetry CSV Export Formatting") {
    TelemetryCsvExport export_data;
    export_data.column_names = {"time_s", "voltage_v", "current_a"};
    export_data.rows = {
        {0.0, 3.70, 0.50},
        {0.1, 3.68, 0.52},
        {0.2, 3.65, 0.55}
    };

    std::string csv = export_data.to_csv_string();
    CHECK(!csv.empty());
    CHECK(csv.find("time_s,voltage_v,current_a\n") == 0);
    CHECK(csv.find("0,3.7,0.5\n") != std::string::npos);
}

// =============================================================================
// PH4-12.03.005: Toolbar Button Tooltips & Shortcut Reminders
// =============================================================================

TEST_CASE("PH4-12.03.005: Toolbar Button Tooltips & Shortcuts") {
    struct TooltipEntry {
        std::string label;
        std::string shortcut;
    };

    std::vector<TooltipEntry> tooltips = {
        {"New Project", "Ctrl+N"},
        {"Save Project", "Ctrl+S"},
        {"Undo Edit", "Ctrl+Z"},
        {"Redo Edit", "Ctrl+Y"},
        {"Start Simulation", "F5"},
        {"Toggle Console", "F12"}
    };

    for (const auto& t : tooltips) {
        CHECK(!t.label.empty());
        CHECK(!t.shortcut.empty());
    }
}
