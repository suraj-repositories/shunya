#include "tests/test_framework.hpp"
#include "shunya/core/domain/workspace_ui.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// UI-01: Workspace Shell & Docking Layout
// =============================================================================

TEST_CASE("UI-01: Workspace Shell, Presets, and Command Palette") {
    CHECK(workspace_preset_to_string(WorkspacePreset::Modeling) == "Modeling");
    CHECK(workspace_preset_to_string(WorkspacePreset::Schematic) == "Schematic");
    CHECK(workspace_preset_to_string(WorkspacePreset::Simulation) == "Simulation");
    CHECK(workspace_preset_to_string(WorkspacePreset::Results) == "Results");
    CHECK(workspace_preset_to_string(WorkspacePreset::Scripting) == "Scripting");

    CHECK(workspace_preset_from_string("Modeling").value() == WorkspacePreset::Modeling);
    CHECK(workspace_preset_from_string("Schematic").value() == WorkspacePreset::Schematic);
    CHECK_FALSE(workspace_preset_from_string("InvalidPreset").has_value());

    WorkspaceShell shell(WorkspacePreset::Modeling);
    CHECK(shell.current_preset() == WorkspacePreset::Modeling);
    CHECK(shell.panels().size() == 4u);

    // Apply schematic preset
    shell.apply_preset(WorkspacePreset::Schematic);
    CHECK(shell.current_preset() == WorkspacePreset::Schematic);
    auto p = shell.get_panel("schematic_editor");
    REQUIRE(p.has_value());
    CHECK(p->area == DockArea::Center);
    CHECK(p->is_visible);

    // Layout serialization round-trip
    std::string json = shell.serialize_layout_json();
    CHECK(json.find("\"preset\": \"Schematic\"") != std::string::npos);

    WorkspaceShell restored_shell(WorkspacePreset::Modeling);
    auto restore_res = restored_shell.restore_layout_json(json);
    CHECK(restore_res.is_ok());
    CHECK(restored_shell.current_preset() == WorkspacePreset::Schematic);

    // Command palette action execution
    bool action_executed = false;
    shell.command_palette().register_action(CommandPaletteAction{
        .action_id = "test.action",
        .category = "Test",
        .title = "Execute Test Action",
        .shortcut = "Ctrl+T",
        .is_enabled = true,
        .callback = [&action_executed]() { action_executed = true; }
    });

    auto search_results = shell.command_palette().search("Execute Test");
    REQUIRE(search_results.size() == 1u);
    CHECK(search_results[0].action_id == "test.action");

    CHECK(shell.command_palette().execute_action("test.action"));
    CHECK(action_executed);
}

// =============================================================================
// UI-02: Project Browser Model
// =============================================================================

TEST_CASE("UI-02: Project Browser Hierarchy and Search") {
    ProjectBrowserModel browser;

    ProjectTreeNode root{
        .id = "root",
        .name = "RLC_Circuit_Project",
        .type = ProjectTreeItemType::ProjectRoot,
        .tags = {"electronic", "prototype"},
        .is_expanded = true,
        .is_selected = false,
        .children = {
            ProjectTreeNode{
                .id = "comp_r1",
                .name = "Resistor_R1",
                .type = ProjectTreeItemType::Component,
                .tags = {"passive"},
                .is_expanded = false,
                .is_selected = false,
                .children = {}
            },
            ProjectTreeNode{
                .id = "comp_c1",
                .name = "Capacitor_C1",
                .type = ProjectTreeItemType::Component,
                .tags = {"passive"},
                .is_expanded = false,
                .is_selected = false,
                .children = {}
            },
            ProjectTreeNode{
                .id = "run_01",
                .name = "Transient_Run_01",
                .type = ProjectTreeItemType::RunResult,
                .tags = {"transient"},
                .is_expanded = false,
                .is_selected = false,
                .children = {}
            }
        }
    };

    browser.set_root(std::move(root));

    // Search
    auto res_r = browser.search("Resistor");
    REQUIRE(res_r.size() == 1u);
    CHECK(res_r[0]->id == "comp_r1");

    auto res_all = browser.search("C");
    CHECK(res_all.size() >= 2u); // Circuit, Capacitor

    // Selection
    CHECK(browser.select_node("comp_c1"));
    auto sel_id = browser.selected_node_id();
    REQUIRE(sel_id.has_value());
    CHECK(sel_id.value() == "comp_c1");
}

// =============================================================================
// UI-03: Viewport Controller (2D/3D)
// =============================================================================

TEST_CASE("UI-03: Viewport Controller and Camera Controls") {
    ViewportController vp;
    CHECK(vp.camera().mode == CameraMode::Perspective);
    CHECK(vp.gizmo() == GizmoMode::Translate);
    CHECK(vp.grid_visible());

    vp.set_camera_mode(CameraMode::Orthographic);
    CHECK(vp.camera().mode == CameraMode::Orthographic);

    vp.set_gizmo(GizmoMode::Rotate);
    CHECK(vp.gizmo() == GizmoMode::Rotate);

    vp.set_grid_spacing(0.5);
    CHECK(std::abs(vp.grid_spacing() - 0.5) < 1e-9);

    // Diagnostics overlay
    CHECK(vp.diagnostics().overlay_visible);
    CHECK(vp.diagnostics().frames_per_second >= 30.0);
}

// =============================================================================
// UI-04: Schematic Editor Model
// =============================================================================

TEST_CASE("UI-04: Schematic Editor Model and Electrical Diagnostics") {
    SchematicEditorModel schematic;

    // Add symbols
    schematic.add_symbol(SchematicSymbol{
        .symbol_id = "sym_r1",
        .name = "R1",
        .pos_x = 100.0,
        .pos_y = 150.0,
        .width = 60.0,
        .height = 30.0,
        .pins = {
            SchematicPin{.name = "p1", .offset_x = 0.0, .offset_y = 15.0},
            SchematicPin{.name = "p2", .offset_x = 60.0, .offset_y = 15.0}
        }
    });

    // Valid wire
    schematic.add_wire(SchematicWire{
        .wire_id = "w1",
        .net_name = "NET_IN",
        .waypoints = {{50.0, 150.0}, {100.0, 150.0}}
    });

    auto issues_clean = schematic.run_electrical_diagnostics();
    CHECK(issues_clean.empty());

    // Add dangling wire (only 1 point)
    schematic.add_wire(SchematicWire{
        .wire_id = "w_dangling",
        .net_name = "NET_HANG",
        .waypoints = {{200.0, 200.0}}
    });

    auto issues_dangling = schematic.run_electrical_diagnostics();
    REQUIRE(issues_dangling.size() == 1u);
    CHECK(issues_dangling[0].is_critical);
    CHECK(issues_dangling[0].message.find("Dangling wire") != std::string::npos);
}

// =============================================================================
// UI-05: Graph / Flow Editor Model
// =============================================================================

TEST_CASE("UI-05: Graph / Flow Editor Model and Socket Connections") {
    FlowEditorModel flow;

    flow.add_block(FlowBlock{
        .block_id = "block_source",
        .title = "Sine Generator",
        .pos_x = 50.0,
        .pos_y = 100.0,
        .sockets = {FlowSocket{.name = "signal_out", .data_type = "signal", .is_input = false}}
    });

    flow.add_block(FlowBlock{
        .block_id = "block_filter",
        .title = "Low Pass Filter",
        .pos_x = 250.0,
        .pos_y = 100.0,
        .sockets = {
            FlowSocket{.name = "signal_in", .data_type = "signal", .is_input = true},
            FlowSocket{.name = "filtered_out", .data_type = "signal", .is_input = false}
        }
    });

    // Valid connection: output -> input
    auto ok_conn = flow.connect("block_source", "signal_out", "block_filter", "signal_in");
    CHECK(ok_conn.is_ok());
    CHECK(flow.connections().size() == 1u);

    // Invalid: connect to non-existent socket
    auto bad_conn = flow.connect("block_source", "nonexistent", "block_filter", "signal_in");
    CHECK(bad_conn.is_err());

    // Highlight
    flow.set_highlighted_block("block_filter");
    CHECK(flow.active_block() == "block_filter");
}

// =============================================================================
// UI-06: Component Editor Model
// =============================================================================

TEST_CASE("UI-06: Component Editor Model and Parameter Authoring") {
    ComponentEditorModel comp;
    comp.set_component_name("CustomOperationalAmplifier");
    CHECK(comp.component_name() == "CustomOperationalAmplifier");

    comp.add_port(ComponentEditorPort{
        .name = "in_plus",
        .domain = "Electrical",
        .direction = "Input",
        .unit_symbol = "V"
    });
    comp.add_port(ComponentEditorPort{
        .name = "out",
        .domain = "Electrical",
        .direction = "Output",
        .unit_symbol = "V"
    });
    CHECK(comp.ports().size() == 2u);

    comp.add_parameter(ComponentEditorParameter{
        .name = "open_loop_gain",
        .default_value = 100000.0,
        .unit_symbol = "V/V",
        .min_value = 1.0,
        .max_value = 1e7
    });
    CHECK(comp.parameters().size() == 1u);

    comp.set_documentation_markdown("# Operational Amplifier Model\nAuthoritative behavioral model.");
    CHECK(comp.documentation_markdown().find("Operational Amplifier") != std::string::npos);
}

// =============================================================================
// UI-07: Property Inspector Model
// =============================================================================

TEST_CASE("UI-07: Property Inspector Model and Reset-to-Default") {
    PropertyInspectorModel inspector;

    inspector.add_property(PropertyItem{
        .name = "resistance",
        .category = "Electrical",
        .value = "100.0",
        .default_value = "100.0",
        .unit_symbol = "Ohm",
        .is_read_only = false,
        .validation_error = ""
    });

    inspector.add_property(PropertyItem{
        .name = "element_uuid",
        .category = "Metadata",
        .value = "abc-1234",
        .default_value = "abc-1234",
        .unit_symbol = "",
        .is_read_only = true,
        .validation_error = ""
    });

    // Modify editable property
    auto set_ok = inspector.set_value("resistance", "220.0");
    CHECK(set_ok.is_ok());
    CHECK(inspector.properties()[0].value == "220.0");

    // Reset to default
    inspector.reset_to_default("resistance");
    CHECK(inspector.properties()[0].value == "100.0");

    // Modify read-only property -> reject
    auto set_ro = inspector.set_value("element_uuid", "new-uuid");
    CHECK(set_ro.is_err());
}

// =============================================================================
// UI-08: Simulation Toolbar Controller
// =============================================================================

TEST_CASE("UI-08: Simulation Toolbar State Machine and Telemetry") {
    SimulationToolbarController toolbar;
    toolbar.set_state(SimulationState::Stopped);
    CHECK(toolbar.can_run());
    CHECK_FALSE(toolbar.can_pause());
    CHECK_FALSE(toolbar.can_stop());

    toolbar.set_state(SimulationState::Running);
    CHECK_FALSE(toolbar.can_run());
    CHECK(toolbar.can_pause());
    CHECK(toolbar.can_stop());

    toolbar.set_state(SimulationState::Paused);
    CHECK(toolbar.can_run());
    CHECK(toolbar.can_step());
    CHECK(toolbar.can_stop());

    // Telemetry updates
    toolbar.update_telemetry(1.234, 1234u, 1.05);
    CHECK(std::abs(toolbar.sim_time() - 1.234) < 1e-9);
    CHECK(toolbar.step_count() == 1234u);
    CHECK(std::abs(toolbar.current_rtf() - 1.05) < 1e-9);
}

// =============================================================================
// UI-09: Results Workspace Controller
// =============================================================================

TEST_CASE("UI-09: Results Workspace Active Traces and Comparison") {
    ResultsWorkspaceController results;
    results.add_trace(ActivePlotTrace{
        .signal_name = "voltage_out",
        .color_hex = "#ff5500",
        .is_visible = true,
        .log_scale = false
    });
    results.add_trace(ActivePlotTrace{
        .signal_name = "current_in",
        .color_hex = "#00aaff",
        .is_visible = true,
        .log_scale = true
    });

    CHECK(results.traces().size() == 2u);
    CHECK(results.traces()[1].log_scale);

    CHECK_FALSE(results.split_comparison_enabled());
    results.set_split_comparison_enabled(true);
    CHECK(results.split_comparison_enabled());
}

// =============================================================================
// UI-10: Code Editor Controller
// =============================================================================

TEST_CASE("UI-10: Code Editor Controller and Diagnostic Markers") {
    CodeEditorController editor;
    editor.set_language("lua");
    CHECK(editor.language() == "lua");

    editor.set_text("local r = 100\nlocal v = 5.0\nlocal i = v / r");
    CHECK(editor.text().find("local i = v / r") != std::string::npos);

    editor.add_diagnostic(DiagnosticMarker{
        .line = 3,
        .column = 7,
        .message = "Variable 'i' is defined but not used",
        .is_error = false
    });
    CHECK(editor.diagnostics().size() == 1u);

    // Breakpoint toggling
    CHECK_FALSE(editor.has_breakpoint(2));
    editor.toggle_breakpoint(2);
    CHECK(editor.has_breakpoint(2));
    editor.toggle_breakpoint(2);
    CHECK_FALSE(editor.has_breakpoint(2));
}

// =============================================================================
// UI-11: Library Browser Model
// =============================================================================

TEST_CASE("UI-11: Library Browser Model Filtering and Search") {
    LibraryBrowserModel lib;
    lib.add_package(LibraryPackageCard{
        .package_name = "shunya.circuits.passive",
        .version = base::Version{1, 0, 0, "final"},
        .author = "Shunya Core Team",
        .license = "Apache-2.0",
        .category = "Templates",
        .tags = {"electronics", "resistor", "capacitor"},
        .is_installed = true
    });

    lib.add_package(LibraryPackageCard{
        .package_name = "shunya.thermal.heatsink",
        .version = base::Version{0, 2, 0, "dev"},
        .author = "Thermal Community",
        .license = "MIT",
        .category = "PublicCommunity",
        .tags = {"thermal", "cooling"},
        .is_installed = false
    });

    // Category filter
    auto tmpl = lib.filter_by_category("Templates");
    REQUIRE(tmpl.size() == 1u);
    CHECK(tmpl[0]->package_name == "shunya.circuits.passive");

    // Search
    auto heatsink = lib.search("heatsink");
    REQUIRE(heatsink.size() == 1u);
    CHECK(heatsink[0]->package_name == "shunya.thermal.heatsink");
}

// =============================================================================
// UI-12: Accessibility and Usability Manager
// =============================================================================

TEST_CASE("UI-12: Accessibility and Usability Manager") {
    AccessibilityManager acc;
    CHECK(std::abs(acc.settings().ui_scale_factor - 1.0) < 1e-9);
    CHECK_FALSE(acc.settings().high_contrast_mode);

    // Scale factor
    auto ok_scale = acc.set_ui_scale_factor(1.5);
    CHECK(ok_scale.is_ok());
    CHECK(std::abs(acc.settings().ui_scale_factor - 1.5) < 1e-9);

    auto bad_scale = acc.set_ui_scale_factor(-0.5);
    CHECK(bad_scale.is_err());

    // High contrast
    acc.set_high_contrast(true);
    CHECK(acc.settings().high_contrast_mode);

    // Locale
    acc.set_locale("de_DE");
    CHECK(acc.settings().locale == "de_DE");
}
