#include "tests/test_framework.hpp"

#include "shunya/cad2d/drawing_tools.hpp"
#include "shunya/cad2d/schematic_wire_router.hpp"

#include <cmath>
#include <vector>

using namespace shunya::cad2d;

// =============================================================================
// PH4-13.03.003: 2D Sketch Canvas Mouse Interactions & Wire Routing
// =============================================================================

TEST_CASE("PH4-13.03.003: Interactive Line Tool Mouse Down, Move, Up & Commit") {
    LineTool tool;
    CHECK(tool.tool_type() == DrawingToolType::Line);
    CHECK(tool.state() == ToolState::Idle);

    // 1. Mouse down at start point (0, 0)
    bool finished = tool.on_pointer_down({0.0, 0.0});
    CHECK_FALSE(finished);
    CHECK(tool.state() == ToolState::Active);

    // 2. Mouse move to (10, 5) -> live preview curve should update
    tool.on_pointer_move({10.0, 5.0});
    auto preview = tool.preview_curves();
    REQUIRE(preview.size() == 1);
    CHECK(std::abs(preview[0]->total_length() - std::sqrt(125.0)) < 1e-6);

    // 3. Mouse down (second click) at (10, 5) -> commits line
    finished = tool.on_pointer_down({10.0, 5.0});
    CHECK(finished);
    CHECK(tool.state() == ToolState::Finished);

    auto committed = tool.commit_curves();
    REQUIRE(committed.size() == 1);
    CHECK(std::abs(committed[0]->total_length() - std::sqrt(125.0)) < 1e-6);

    // After commit, tool returns to Idle
    CHECK(tool.state() == ToolState::Idle);
}

TEST_CASE("PH4-13.03.003: Interactive Rectangle Tool Mouse Interactions") {
    RectangleTool tool;
    CHECK(tool.tool_type() == DrawingToolType::Rectangle);
    CHECK(tool.state() == ToolState::Idle);

    // Mouse down at corner (2, 3)
    tool.on_pointer_down({2.0, 3.0});
    CHECK(tool.state() == ToolState::Active);

    // Drag to opposite corner (8, 7) -> width = 6, height = 4
    tool.on_pointer_move({8.0, 7.0});
    auto preview = tool.preview_curves();
    REQUIRE(!preview.size() == 0);

    // Click to finish
    bool finished = tool.on_pointer_down({8.0, 7.0});
    CHECK(finished);
    CHECK(tool.state() == ToolState::Finished);

    auto curves = tool.commit_curves();
    REQUIRE(curves.size() == 1); // 1 closed polyline containing 4 vertices
    double total_perimeter = curves[0]->total_length();
    // Perimeter = 2 * (6 + 4) = 20
    CHECK(std::abs(total_perimeter - 20.0) < 1e-6);
}

TEST_CASE("PH4-13.03.003: Schematic Wire Routing & 90-Degree Orthogonal Snapping") {
    // 1. Direct Straight Route
    auto direct_pts = SchematicWireRouter::calculate_route(
        {0.0, 0.0}, {10.0, 0.0}, WireRouteStyle::Direct
    );
    REQUIRE(direct_pts.size() == 2);
    CHECK(direct_pts[0].x == 0.0);
    CHECK(direct_pts[1].x == 10.0);

    // 2. Orthogonal HV Route: Horizontal then Vertical
    auto hv_pts = SchematicWireRouter::calculate_route(
        {0.0, 0.0}, {15.0, 25.0}, WireRouteStyle::Orthogonal_HV
    );
    REQUIRE(hv_pts.size() == 3);
    CHECK(hv_pts[0].x == 0.0);
    CHECK(hv_pts[0].y == 0.0);
    CHECK(hv_pts[1].x == 15.0); // Knee point
    CHECK(hv_pts[1].y == 0.0);
    CHECK(hv_pts[2].x == 15.0);
    CHECK(hv_pts[2].y == 25.0);

    // 3. Orthogonal VH Route: Vertical then Horizontal
    auto vh_pts = SchematicWireRouter::calculate_route(
        {0.0, 0.0}, {15.0, 25.0}, WireRouteStyle::Orthogonal_VH
    );
    REQUIRE(vh_pts.size() == 3);
    CHECK(vh_pts[1].x == 0.0);  // Knee point
    CHECK(vh_pts[1].y == 25.0);
}
