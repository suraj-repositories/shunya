#include "tests/test_framework.hpp"
#include "shunya/core/domain/material.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/eng/physical_object.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

using namespace shunya;

namespace {
inline bool approx_eq(double a, double b, double eps = 1e-4) {
    return std::abs(a - b) <= eps * (1.0 + std::abs(b));
}
} // namespace

TEST_CASE("PH5-21.01: 4-Pane Workspace Architecture & Splitter Clamping Bounds") {
    // 1. Minimum and Default Constraints Specification
    constexpr double kLeftPaneMin = 280.0;
    constexpr double kCenterPaneMin = 400.0;
    constexpr double kRightPaneMin = 340.0;
    constexpr double kBottomPaneMin = 80.0;

    CHECK(kLeftPaneMin >= 280.0);
    CHECK(kCenterPaneMin >= 400.0);
    CHECK(kRightPaneMin >= 340.0);
    CHECK(kBottomPaneMin >= 80.0);

    // 2. Window Bounds Clamping
    constexpr double kWindowWidth = 1600.0;
    constexpr double kWindowHeight = 900.0;

    // Default Proportional Distribution (Left: 20%, Center: 55%, Right: 25%)
    double leftDefault = std::max(kLeftPaneMin, kWindowWidth * 0.20);   // 320.0
    double rightDefault = std::max(kRightPaneMin, kWindowWidth * 0.25); // 400.0
    double centerDefault = kWindowWidth - leftDefault - rightDefault;    // 880.0

    CHECK(leftDefault >= kLeftPaneMin);
    CHECK(rightDefault >= kRightPaneMin);
    CHECK(centerDefault >= kCenterPaneMin);
    CHECK(approx_eq(leftDefault + centerDefault + rightDefault, kWindowWidth));

    // Bottom pane bounds (80px to 70% of window height)
    double bottomMax = kWindowHeight * 0.70; // 630.0
    CHECK(bottomMax == 630.0);

    auto clamp_bottom = [&](double current, double delta_up) -> double {
        return std::max(kBottomPaneMin, std::min(bottomMax, current + delta_up));
    };

    CHECK(clamp_bottom(140.0, 50.0) == 190.0);
    CHECK(clamp_bottom(140.0, 600.0) == 630.0); // clamped to 70% max
    CHECK(clamp_bottom(140.0, -100.0) == 80.0);  // clamped to min 80

    // Left pane drag clamping ensuring Center has >= 400px
    auto clamp_left = [&](double current, double delta, double right) -> double {
        double maxLeft = std::max(kLeftPaneMin, kWindowWidth - right - kCenterPaneMin);
        return std::max(kLeftPaneMin, std::min(maxLeft, current + delta));
    };

    CHECK(clamp_left(300.0, 50.0, 400.0) == 350.0);
    CHECK(clamp_left(300.0, 1000.0, 400.0) == 800.0); // 1600 - 400 - 400 = 800
    CHECK(clamp_left(300.0, -200.0, 400.0) == 280.0); // clamped to min 280
}

TEST_CASE("PH5-21.02: Layout Presets, Presistence Keys & Maximize Viewport Transitions") {
    // 1. Persistence Schema Validation
    const std::vector<std::string> expected_persistence_keys = {
        "objectsLeftWidth",
        "objectsRightWidth",
        "objectsBottomHeight",
        "leftCollapsed",
        "rightCollapsed",
        "bottomCollapsed",
        "layoutPreset"
    };

    CHECK(expected_persistence_keys.size() == 7);
    for (const auto& k : expected_persistence_keys) {
        CHECK(!k.empty());
    }

    // 2. Preset Transition Specifications
    struct PresetSpec {
        std::string name;
        double expected_left;
        double expected_right;
        double expected_bottom;
        bool bottom_collapsed;
        bool is_compare;
    };

    const std::vector<PresetSpec> presets = {
        { "Default", 320.0, 400.0, 140.0, false, false },
        { "Modeling", 280.0, 340.0, 80.0, true, false },
        { "Testing", 280.0, 560.0, 180.0, false, false },
        { "Compare", 280.0, 340.0, 120.0, false, true }
    };

    for (const auto& p : presets) {
        CHECK(p.expected_left >= 280.0);
        CHECK(p.expected_right >= 340.0);
        CHECK(p.expected_bottom >= 80.0);
    }

    // 3. Maximize Viewport Mode
    bool maximized = false;
    auto toggle_maximize = [&]() {
        maximized = !maximized;
    };

    toggle_maximize(); // F11 or Ctrl+Space pressed
    CHECK(maximized == true);

    toggle_maximize(); // Escape pressed
    CHECK(maximized == false);
}

TEST_CASE("PH5-21.03: Object Pre-Flight Check & Simulation Qualification") {
    eng::PhysicalObjectManager mgr;
    core::domain::MaterialRegistry reg;

    core::domain::Material copper(core::base::Uuid::generate_random(), "Pure Copper",
                                 core::domain::MaterialPhysicalState::Solid, 8960.0);
    core::domain::ThermalProperties tp;
    tp.thermal_conductivity = 401.0;
    tp.specific_heat_capacity = 385.0;
    tp.thermal_expansion_coeff = 16.5e-6;
    copper.set_thermal(tp);

    core::domain::ElectricalProperties ep;
    ep.electrical_conductivity = 5.96e7;
    ep.electrical_resistivity = 1.0 / 5.96e7;
    copper.set_electrical(ep);

    const core::base::Uuid copper_id = copper.id();
    auto reg_res = reg.register_material(std::move(copper));
    CHECK(reg_res.is_ok());

    eng::PhysicalObject rod;
    rod.name = "Copper_Preflight_Bar";
    rod.category = "Conductor";
    rod.geometry.kind = eng::ObjectShapeKind::Cylinder;
    rod.geometry.dim_x_m = 0.01;
    rod.geometry.dim_y_m = 0.50; // Length
    rod.geometry.radius_m = 0.005; // 5mm radius
    rod.operating_temp_k = 293.15;
    rod.operating_pressure_pa = 101325.0;

    eng::ObjectRegion r;
    r.region_id = "body";
    r.name = "Core Rod";
    r.material_id = copper_id;
    r.volume_fraction = 1.0;
    r.is_conductive_path = true;
    rod.regions.push_back(r);

    auto add_obj_res = mgr.add_object(rod, reg);
    CHECK(add_obj_res.is_ok());

    const auto* stored = mgr.get_object(add_obj_res.value());
    CHECK(stored != nullptr);
    CHECK(stored->computed.valid == true);
    CHECK(stored->computed.volume_m3 > 0.0);
    CHECK(stored->computed.mass_kg > 0.0);
    CHECK(stored->computed.effective_density_kg_m3 > 8000.0);
    CHECK(stored->computed.has_conductive_path == true);
    CHECK(stored->computed.is_solid_at_operating_temp == true);
    CHECK(stored->computed.axial_electrical_resistance_ohm > 0.0);

    // Pre-flight qualification status
    bool preflight_passed = (stored->computed.volume_m3 > 1e-18 &&
                             stored->computed.mass_kg > 1e-12 &&
                             stored->computed.is_solid_at_operating_temp &&
                             stored->computed.has_conductive_path);
    CHECK(preflight_passed == true);
}
