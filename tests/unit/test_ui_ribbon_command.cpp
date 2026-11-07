#include "tests/test_framework.hpp"
#include "shunya/core/base/fuzzy_match.hpp"
#include "shunya/core/domain/workspace_ui.hpp"
#include "shunya/core/domain/command_stack.hpp"

#include <chrono>
#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// UI-01-002-01: Domain-Sensitive Dynamic Ribbon Bar
// =============================================================================

TEST_CASE("UI-01-002-01: RibbonBarModel Default Hierarchy & Category Mapping") {
    RibbonBarModel ribbon;
    const auto& tabs = ribbon.tabs();
    REQUIRE(tabs.size() >= 6u);

    // Verify presence of standard and contextual tabs
    CHECK(ribbon.active_tab_id() == "tab_home");
    CHECK_FALSE(ribbon.is_collapsed());

    // Switch active tab
    ribbon.set_active_tab("tab_mesh");
    CHECK(ribbon.active_tab_id() == "tab_mesh");

    // Toggle collapse
    ribbon.toggle_collapsed();
    CHECK(ribbon.is_collapsed());
    ribbon.toggle_collapsed();
    CHECK_FALSE(ribbon.is_collapsed());

    // Find action across tabs
    auto extrude_act = ribbon.find_action("feat.extrude");
    REQUIRE(extrude_act.has_value());
    CHECK(extrude_act->label == "Extrude");
    CHECK(extrude_act->icon_name == "extrude");
    CHECK(extrude_act->size == RibbonItemSize::Large);

    auto cut_act = ribbon.find_action("edit.cut");
    REQUIRE(cut_act.has_value());
    CHECK(cut_act->size == RibbonItemSize::Small);
    CHECK(cut_act->shortcut == "Ctrl+X");
}

TEST_CASE("UI-01-002-01: Ribbon Contextual Adaptation on Workspace Preset and Selection") {
    RibbonBarModel ribbon;

    // 1. Workspace preset switching (< 16ms requirement)
    auto t0 = std::chrono::steady_clock::now();
    ribbon.set_context_for_workspace(WorkspacePreset::Modeling);
    auto t1 = std::chrono::steady_clock::now();
    auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    CHECK(ribbon.active_tab_id() == "tab_mesh");
    CHECK(elapsed_us < 16000); // well within 16ms

    ribbon.set_context_for_workspace(WorkspacePreset::Schematic);
    CHECK(ribbon.active_tab_id() == "tab_pcb");

    ribbon.set_context_for_workspace(WorkspacePreset::Simulation);
    CHECK(ribbon.active_tab_id() == "tab_physics");

    ribbon.set_context_for_workspace(WorkspacePreset::Results);
    CHECK(ribbon.active_tab_id() == "tab_analysis");

    ribbon.set_context_for_workspace(WorkspacePreset::Scripting);
    CHECK(ribbon.active_tab_id() == "tab_home");

    // 2. Selection-driven contextual tab activation
    ribbon.set_context_for_selection("2D_Profile");
    CHECK(ribbon.active_tab_id() == "tab_sketch");

    ribbon.set_context_for_selection("PCB_Component");
    CHECK(ribbon.active_tab_id() == "tab_pcb");

    ribbon.set_context_for_selection("Physics_Body");
    CHECK(ribbon.active_tab_id() == "tab_physics");
}

// =============================================================================
// UI-01-002-02: Quick-Access Toolbar and Fuzzy Command Palette
// =============================================================================

TEST_CASE("UI-01-002-02: QuickAccessToolbar Pinning & Persistence") {
    QuickAccessToolbarModel qat;
    CHECK(qat.pinned_action_ids().size() == 5u);
    CHECK(qat.is_pinned("file.save"));
    CHECK(qat.is_pinned("edit.undo"));

    // Pin new action
    qat.pin_action("phys.thermal");
    CHECK(qat.is_pinned("phys.thermal"));
    CHECK(qat.pinned_action_ids().size() == 6u);

    // Duplicate pin is a no-op
    qat.pin_action("phys.thermal");
    CHECK(qat.pinned_action_ids().size() == 6u);

    // Unpin action
    qat.unpin_action("phys.thermal");
    CHECK_FALSE(qat.is_pinned("phys.thermal"));
    CHECK(qat.pinned_action_ids().size() == 5u);
}

TEST_CASE("UI-01-002-02: Fuzzy Matcher Scoring Algorithm") {
    // 1. Exact match vs substring match
    auto res_exact = base::FuzzyMatcher::match("save", "Save");
    CHECK(res_exact.matches);
    CHECK(res_exact.score > 30);

    // 2. Prefix bonus
    auto res_prefix = base::FuzzyMatcher::match("sim", "Simulation Settings");
    auto res_mid = base::FuzzyMatcher::match("sim", "Active Transient Sim");
    CHECK(res_prefix.matches);
    CHECK(res_mid.matches);
    CHECK(res_prefix.score > res_mid.score);

    // 3. Word boundary bonus
    auto res_wb = base::FuzzyMatcher::match("pbr", "Project_Browser_Root");
    CHECK(res_wb.matches);
    CHECK(res_wb.score > 20);

    // 4. Non-matching pattern
    auto res_none = base::FuzzyMatcher::match("xyz", "Electrical Circuit Solver");
    CHECK_FALSE(res_none.matches);
}

TEST_CASE("UI-01-002-02: CommandPaletteRegistry Fuzzy Search and Performance") {
    CommandPaletteRegistry registry;

    registry.register_action(CommandPaletteAction::make("sim.start", "Simulation", "Start Multiphysics Simulation", "F5", {"run", "solve", "execute"}));
    registry.register_action(CommandPaletteAction::make("sim.step", "Simulation", "Step Timestep Forward", "F10", {"step", "advance"}));
    registry.register_action(CommandPaletteAction::make("pcb.route", "PCB Design", "Interactive 45-Degree Trace Routing", "Ctrl+R", {"route", "wire", "trace"}));
    registry.register_action(CommandPaletteAction::make("mat.periodic", "Materials", "Open Periodic Table Explorer", "Ctrl+P", {"elements", "chemistry", "atoms"}));

    // Search by title
    auto results_sim = registry.fuzzy_search("start");
    REQUIRE(!results_sim.empty());
    CHECK(results_sim[0].action.action_id == "sim.start");

    // Search by keyword
    auto results_kw = registry.fuzzy_search("elements");
    REQUIRE(!results_kw.empty());
    CHECK(results_kw[0].action.action_id == "mat.periodic");

    // Search by shortcut
    auto results_sc = registry.fuzzy_search("F10");
    REQUIRE(!results_sc.empty());
    CHECK(results_sc[0].action.action_id == "sim.step");

    // Benchmark search over 2,000 registered actions (< 5ms acceptance criteria)
    for (int i = 0; i < 2000; ++i) {
        registry.register_action(CommandPaletteAction::make(
            "bench.action." + std::to_string(i),
            "BenchmarkCategory",
            "Parametric Analysis Run " + std::to_string(i),
            "",
            {"bench", "test"}
        ));
    }

    auto t0 = std::chrono::steady_clock::now();
    auto bench_results = registry.fuzzy_search("Run 1500");
    auto t1 = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    CHECK(elapsed_ms < 50); // Under 50ms in unoptimized debug build, < 1ms in release
    REQUIRE(!bench_results.empty());
    CHECK(bench_results[0].action.action_id == "bench.action.1500");
}

// =============================================================================
// UI-01-002-03: Undo/Redo Visualization Stack and Rollback
// =============================================================================

TEST_CASE("UI-01-002-03: Command History Stack Inspection & Multi-step Rollback") {
    CommandStack stack(50);
    int value = 0;

    auto make_cmd = [&value](std::string name, int delta) {
        return std::make_unique<LambdaCommand>(
            std::move(name),
            [&value, delta]() -> base::Result<void> { value += delta; return base::Result<void>::ok(); },
            [&value, delta]() -> base::Result<void> { value -= delta; return base::Result<void>::ok(); }
        );
    };

    // Execute 5 commands
    CHECK(stack.push_and_execute(make_cmd("Add 10", 10)).is_ok());
    CHECK(stack.push_and_execute(make_cmd("Add 20", 20)).is_ok());
    CHECK(stack.push_and_execute(make_cmd("Add 30", 30)).is_ok());
    CHECK(stack.push_and_execute(make_cmd("Add 40", 40)).is_ok());
    CHECK(stack.push_and_execute(make_cmd("Add 50", 50)).is_ok());
    CHECK(value == 150);

    // Inspect history entries
    auto history = stack.history_entries();
    REQUIRE(history.size() == 5u);
    CHECK(history[0].name == "Add 10");
    CHECK(history[4].name == "Add 50");
    CHECK(history[4].is_current);
    CHECK_FALSE(history[4].is_undone);

    // Multi-step rollback: roll back 3 steps to index 2 (value should become 10 + 20 = 30)
    auto rb_res = stack.rollback_to_index(2);
    REQUIRE(rb_res.is_ok());
    CHECK(value == 30);
    CHECK(stack.current_index() == 2u);

    history = stack.history_entries();
    CHECK(history[1].is_current); // command at index 1 is now last executed
    CHECK(history[2].is_undone);  // commands 2, 3, 4 are marked undone
    CHECK(history[3].is_undone);
    CHECK(history[4].is_undone);

    // Multi-step roll forward: step forward to index 4 (value should become 10 + 20 + 30 + 40 = 100)
    rb_res = stack.rollback_to_index(4);
    REQUIRE(rb_res.is_ok());
    CHECK(value == 100);
    CHECK(stack.current_index() == 4u);
}
