#include "tests/test_framework.hpp"

#include "shunya/pcb/copper_pour.hpp"
#include "shunya/pcb/trace_router.hpp"
#include "shunya/pcb/via_manager.hpp"

#include <chrono>

using namespace shunya::pcb;

TEST_CASE("InteractiveTraceRouter & PushShoveEngine: 45-deg octilinear constraint and 10-trace bus shove") {
    auto segs = InteractiveTraceRouter::route_octilinear_pair("CLK", 0, 0.20, {10.0, 10.0}, {25.0, 18.0});
    REQUIRE(segs.size() == 2);
    for (const auto& s : segs) {
        REQUIRE(InteractiveTraceRouter::is_45_deg_aligned(s));
    }

    // 10 parallel bus traces spaced tightly at 0.35 mm center-to-center (width=0.20 mm -> 0.15 mm clearance)
    std::vector<PcbTrackSegment> bus;
    for (int i = 0; i < 10; ++i) {
        const double y = 10.0 + static_cast<double>(i) * 0.36;
        bus.push_back({"BUS" + std::to_string(i), 0, 0.20, {5.0, y}, {50.0, y}, false});
    }

    // Route an intruder right beside BUS0 at y = 10.05 mm (violating 0.15 mm clearance)
    PcbTrackSegment intruder{"INTRUDER", 0, 0.25, {5.0, 10.05}, {50.0, 10.05}, false};
    auto pushed = PushShoveEngine::shove_parallel_tracks(intruder, bus, {0.15, 32});
    REQUIRE(pushed.size() == 10);

    // Every single pushed bus trace maintains >= 0.15 mm clearance against intruder AND adjacent neighbors
    for (size_t i = 0; i < pushed.size(); ++i) {
        REQUIRE(InteractiveTraceRouter::edge_clearance_mm(intruder, pushed[i]) >= 0.15 - 1e-4);
        for (size_t j = i + 1; j < pushed.size(); ++j) {
            REQUIRE(InteractiveTraceRouter::edge_clearance_mm(pushed[i], pushed[j]) >= 0.15 - 1e-4);
        }
    }
}

TEST_CASE("ViaManager: ThroughHole/Blind/Buried rules and multi-layer Top-to-Bottom net continuity") {
    ViaManager vm(4);
    REQUIRE(vm.add_via({"NET_A", {20.0, 20.0}, 0.30, 0.60, 0, 3, ViaType::ThroughHole}).is_ok());
    REQUIRE(vm.add_via({"NET_A", {40.0, 20.0}, 0.30, 0.60, 0, 3, ViaType::ThroughHole}).is_ok());
    REQUIRE(vm.add_via({"NET_B", {10.0, 10.0}, 0.20, 0.50, 0, 1, ViaType::Blind}).is_ok());
    REQUIRE(vm.add_via({"NET_C", {15.0, 15.0}, 0.20, 0.50, 1, 2, ViaType::Buried}).is_ok());

    // Reject blind via that does not start/end on outer layer; reject tiny annular ring
    REQUIRE(vm.add_via({"BAD", {5.0, 5.0}, 0.20, 0.50, 1, 2, ViaType::Blind}).is_err());
    REQUIRE(vm.add_via({"BAD", {5.0, 5.0}, 0.40, 0.45, 0, 3, ViaType::ThroughHole}).is_err());

    // Multi-layer route Top(0) -> Via1(20,20) -> Bottom(3) -> Via2(40,20) -> Top(0)
    std::vector<PcbTrackSegment> tracks = {
        {"NET_A", 0, 0.25, {10.0, 20.0}, {20.0, 20.0}, false},
        {"NET_A", 3, 0.25, {20.0, 20.0}, {40.0, 20.0}, false},
        {"NET_A", 0, 0.25, {40.0, 20.0}, {50.0, 20.0}, false}
    };
    REQUIRE(vm.verify_net_continuity("NET_A", tracks));

    // Break bottom segment -> discontinuity detected
    tracks[1].end_mm = {30.0, 20.0};
    REQUIRE(!vm.verify_net_continuity("NET_A", tracks));
}

TEST_CASE("CopperPourEngine: ground plane flood fill < 100ms, 4 thermal relief spokes, and zero acid traps") {
    auto board = BoardOutline::make_rectangle(100.0, 80.0);
    std::vector<PcbTrackSegment> tracks;
    for (int i = 0; i < 40; ++i) {
        tracks.push_back({"SIG" + std::to_string(i), 0, 0.20,
                          {5.0 + i * 2.0, 10.0}, {5.0 + i * 2.0, 70.0}, false});
    }
    std::vector<PourPadObstacle> pads = {
        {"GND", {15.0, 5.0}, 0.80},
        {"GND", {35.0, 5.0}, 0.80},
        {"SIG0", {5.0, 10.0}, 0.60}
    };
    std::vector<PcbVia> vias = {
        {"SIG1", {7.0, 20.0}, 0.30, 0.60, 0, 3, ViaType::ThroughHole}
    };

    const auto t0 = std::chrono::steady_clock::now();
    auto res = CopperPourEngine::generate_pour({"GND", 0, 0.20, 0.30, 0.50}, board, tracks, pads, vias);
    const auto t1 = std::chrono::steady_clock::now();
    const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    REQUIRE(ms < 100.0);
    REQUIRE(res.thermal_relief_pads.size() == 2);
    REQUIRE(res.thermal_relief_pads[0].spokes.size() == 4);
    REQUIRE(res.clearance_voids_mm.size() >= 42);
    REQUIRE(!res.has_acute_acid_traps);
    REQUIRE(res.net_copper_area_mm2 > 5000.0);
}
