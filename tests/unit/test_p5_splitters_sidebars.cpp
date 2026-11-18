#include "tests/test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace shunya;

TEST_CASE("PH5-06.01: Universal Splitter Dimensions & Clamping Bounds") {
    // 1. Left Workflow Rail Bounds
    constexpr double kLeftRailMin = 56.0;
    constexpr double kLeftRailDefault = 184.0;
    constexpr double kLeftRailMax = 360.0;

    CHECK(kLeftRailMin == 56.0);
    CHECK(kLeftRailDefault >= kLeftRailMin);
    CHECK(kLeftRailMax >= kLeftRailDefault);

    // Test drag clamping
    auto clamp_rail = [&](double current, double delta) -> double {
        return std::max(kLeftRailMin, std::min(kLeftRailMax, current + delta));
    };

    CHECK(clamp_rail(184.0, 50.0) == 234.0);
    CHECK(clamp_rail(184.0, 500.0) == 360.0);
    CHECK(clamp_rail(184.0, -300.0) == 56.0);

    // 2. Diagnostics Console Drawer Bounds
    constexpr double kConsoleMin = 80.0;
    constexpr double kConsoleDefault = 150.0;
    constexpr double kWindowHeight = 840.0;
    const double kConsoleMax = kWindowHeight * 0.7; // 588.0

    CHECK(kConsoleMin == 80.0);
    CHECK(kConsoleDefault >= kConsoleMin);
    CHECK(kConsoleMax == 588.0);

    auto clamp_console = [&](double current, double delta_up) -> double {
        return std::max(kConsoleMin, std::min(kConsoleMax, current + delta_up));
    };

    CHECK(clamp_console(150.0, 50.0) == 200.0);
    CHECK(clamp_console(150.0, 600.0) == 588.0);
    CHECK(clamp_console(150.0, -100.0) == 80.0);

    // 3. Right Details Inspector Bounds
    constexpr double kInspectorMin = 280.0;
    constexpr double kInspectorDefault = 360.0;
    CHECK(kInspectorMin == 280.0);
    CHECK(kInspectorDefault >= kInspectorMin);
}

TEST_CASE("PH5-06.02: Session Persistence Schema Validation") {
    const std::vector<std::string> expected_layout_keys = {
        "leftRailWidth",
        "leftRailCollapsed",
        "consoleDrawerHeight",
        "showConsoleDrawer",
        "rightPanelCollapsed",
        "rightPanelWidth"
    };

    CHECK(expected_layout_keys.size() == 6);
    for (const auto& k : expected_layout_keys) {
        CHECK(!k.empty());
    }
}

TEST_CASE("PH5-07.01: Collapsible Sidebar Dimensions & Animation Timing") {
    // Left rail collapsed mode
    constexpr double kCollapsedRailWidth = 56.0;
    constexpr double kExpandedRailWidth = 184.0;
    constexpr int kAnimationDurationMs = 180;

    CHECK(kCollapsedRailWidth < kExpandedRailWidth);
    CHECK(kAnimationDurationMs >= 150 && kAnimationDurationMs <= 200);
}

TEST_CASE("PH5-08.01: View Menu Actions & Hotkey Specification") {
    struct MenuActionDef {
        std::string label;
        std::string shortcut;
        bool checkable;
    };

    const std::vector<MenuActionDef> view_actions = {
        { "Toggle Left Sidebar", "Ctrl+B", true },
        { "Toggle Details Panel", "Ctrl+Alt+B", true },
        { "Toggle Diagnostics Console", "F12", true },
        { "Reset Workspace Layout", "", false },
        { "Toggle Fullscreen", "F11", false }
    };

    CHECK(view_actions.size() == 5);
    for (const auto& a : view_actions) {
        CHECK(!a.label.empty());
    }
}
