#include "tests/test_framework.hpp"
#include "shunya/core/domain/workspace_ui.hpp"
#include "shunya/core/domain/project.hpp"

#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// UI-01-003: Desktop Menu Bar & File Management Architecture Tests
// =============================================================================

TEST_CASE("UI-01-003-01: Menu Bar Structure, Categories & Accelerator Shortcuts") {
    ApplicationMenuBarModel menu_bar;

    CHECK(menu_category_to_string(MenuCategory::File) == "File");
    CHECK(menu_category_to_string(MenuCategory::Edit) == "Edit");
    CHECK(menu_category_to_string(MenuCategory::View) == "View");
    CHECK(menu_category_to_string(MenuCategory::Simulation) == "Simulation");
    CHECK(menu_category_to_string(MenuCategory::Help) == "Help");

    CHECK(menu_category_from_string("File").value() == MenuCategory::File);
    CHECK(menu_category_from_string("Simulation").value() == MenuCategory::Simulation);
    CHECK(!menu_category_from_string("NonExistent").has_value());

    const auto& file_items = menu_bar.items_for_category(MenuCategory::File);
    CHECK(!file_items.empty());

    // Verify critical File actions exist with appropriate shortcuts
    const auto* new_item = menu_bar.find_item("file.new");
    CHECK(new_item != nullptr);
    CHECK(new_item->title == "New Project...");
    CHECK(new_item->shortcut == "Ctrl+N");
    CHECK(new_item->is_enabled);

    const auto* open_item = menu_bar.find_item("file.open");
    CHECK(open_item != nullptr);
    CHECK(open_item->shortcut == "Ctrl+O");

    const auto* save_item = menu_bar.find_item("file.save");
    CHECK(save_item != nullptr);
    CHECK(save_item->shortcut == "Ctrl+S");

    const auto* exit_item = menu_bar.find_item("file.exit");
    CHECK(exit_item != nullptr);
    CHECK(exit_item->shortcut == "Ctrl+Q");

    // Verify Simulation menu shortcuts
    const auto* run_item = menu_bar.find_item("sim.run");
    CHECK(run_item != nullptr);
    CHECK(run_item->shortcut == "F5");

    const auto* step_item = menu_bar.find_item("sim.step");
    CHECK(step_item != nullptr);
    CHECK(step_item->shortcut == "F10");
}

TEST_CASE("UI-01-003-02: Recent Projects Manager & Project Templates Registry") {
    RecentProjectsManager recent_mgr(5);
    CHECK(recent_mgr.empty());
    CHECK(recent_mgr.size() == 0);

    recent_mgr.add_recent_project("C:/projects/rig1.shunya", "Electro-Thermal Rig", "2026-11-01T10:00:00Z");
    recent_mgr.add_recent_project("C:/projects/circuit_bjt.shunya", "BJT Amplifier", "2026-11-02T12:00:00Z");
    recent_mgr.add_recent_project("C:/projects/heat_sink.shunya", "Extruded Heat Sink", "2026-11-03T15:00:00Z");

    CHECK(recent_mgr.size() == 3);
    // Most recent is at front
    CHECK(recent_mgr.recent_projects().front().title == "Extruded Heat Sink");
    CHECK(recent_mgr.recent_projects().front().path == "C:/projects/heat_sink.shunya");

    // Re-adding promotes to front without duplicates
    recent_mgr.add_recent_project("C:/projects/rig1.shunya", "Electro-Thermal Rig Updated", "2026-11-04T18:00:00Z");
    CHECK(recent_mgr.size() == 3);
    CHECK(recent_mgr.recent_projects().front().path == "C:/projects/rig1.shunya");
    CHECK(recent_mgr.recent_projects().front().title == "Electro-Thermal Rig Updated");

    // Pinning prevents removal
    recent_mgr.pin_project("C:/projects/circuit_bjt.shunya", true);
    for (int i = 0; i < 10; ++i) {
        recent_mgr.add_recent_project("C:/projects/temp_" + std::to_string(i) + ".shunya", "Temp Project");
    }
    // Max entries 5, but pinned project should remain or limit enforced
    bool pinned_found = false;
    for (const auto& e : recent_mgr.recent_projects()) {
        if (e.path == "C:/projects/circuit_bjt.shunya") pinned_found = true;
    }
    CHECK(pinned_found);

    // Serialization & Restoration roundtrip
    std::string json = recent_mgr.serialize_json();
    CHECK(!json.empty());
    CHECK(json.find("circuit_bjt.shunya") != std::string::npos);

    RecentProjectsManager restored_mgr;
    auto res = restored_mgr.restore_json(json);
    CHECK(res.is_ok());
    CHECK(restored_mgr.size() == recent_mgr.size());

    // Project Templates
    ProjectTemplateRegistry templates;
    const auto& all_templates = templates.templates();
    CHECK(all_templates.size() >= 5);

    auto multi = templates.get_template("template.multiphysics");
    CHECK(multi.has_value());
    CHECK(multi->name == "Coupled Multiphysics Rig");
    CHECK(multi->default_preset == WorkspacePreset::Simulation);

    auto pcb = templates.get_template("template.pcb");
    CHECK(pcb.has_value());
    CHECK(pcb->default_preset == WorkspacePreset::Schematic);

    auto blank = templates.get_template("template.blank");
    CHECK(blank.has_value());
    CHECK(blank->category == "General");
}

TEST_CASE("UI-01-003-03: File Import/Export Actions & Project Dirty State Guard") {
    ApplicationMenuBarModel menu_bar;

    // Verify Import submenu items
    const auto* import_dxf = menu_bar.find_item("file.import.dxf");
    CHECK(import_dxf != nullptr);
    CHECK(import_dxf->title == "AutoCAD DXF (*.dxf)");

    const auto* import_step = menu_bar.find_item("file.import.step");
    CHECK(import_step != nullptr);

    // Verify Export submenu items
    const auto* export_gerber = menu_bar.find_item("file.export.gerber");
    CHECK(export_gerber != nullptr);
    CHECK(export_gerber->title == "Gerber RS-274X Package (*.zip)");

    const auto* export_report = menu_bar.find_item("file.export.report");
    CHECK(export_report != nullptr);

    // Action Execution Callback
    bool executed = false;
    auto* save_item = menu_bar.find_item("file.save");
    REQUIRE(save_item != nullptr);
    save_item->callback = [&executed]() { executed = true; };

    CHECK(menu_bar.execute_item("file.save"));
    CHECK(executed);

    // Project Dirty State Guard Verification
    auto prj_res = Project::create_new("TestGuardProject", "Architect");
    REQUIRE(prj_res.is_ok());
    auto prj = prj_res.value();
    // A newly created unsaved project is dirty until saved
    CHECK(prj->is_dirty());

    // Clearing dirty flag (or saving) makes it clean
    prj->clear_dirty();
    CHECK(!prj->is_dirty());

    // Modifying manifest marks it dirty again
    prj->manifest().description = "Updated project settings";
    CHECK(prj->is_dirty());

    // Clearing dirty flag restores clean state
    prj->clear_dirty();
    CHECK(!prj->is_dirty());
}
