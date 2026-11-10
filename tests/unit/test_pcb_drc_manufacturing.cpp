#include "tests/test_framework.hpp"

#include "shunya/pcb/drc_engine.hpp"
#include "shunya/pcb/excellon_exporter.hpp"
#include "shunya/pcb/gerber_exporter.hpp"
#include "shunya/pcb/pcb_3d_exporter.hpp"

#include <cmath>

using namespace shunya::pcb;

TEST_CASE("DrcEngine: 100% detection across 20 intentional clearance, trace width, drill, and annular ring errors") {
    DrcRules rules{0.15, 0.15, 0.20, 0.125};
    std::vector<PcbTrackSegment> tracks;
    // 10 pairs of tracks intentionally spaced 0.08 mm apart (violating 0.15 mm) -> 10 clearance violations
    for (int i = 0; i < 10; ++i) {
        const double y = 10.0 + i * 3.0;
        tracks.push_back({"N_A" + std::to_string(i), 0, 0.20, {5.0, y}, {30.0, y}, false});
        tracks.push_back({"N_B" + std::to_string(i), 0, 0.20, {5.0, y + 0.28}, {30.0, y + 0.28}, false});
    }
    // 6 thin tracks below 0.15 mm minimum width
    for (int i = 0; i < 6; ++i) {
        tracks.push_back({"THIN" + std::to_string(i), 0, 0.09, {5.0, 50.0 + i * 2.0}, {25.0, 50.0 + i * 2.0}, false});
    }

    // 4 via violations (2 drill < 0.20mm + 2 annular ring < 0.125mm via manual add)
    ViaManager vm(4);
    for (int i = 0; i < 4; ++i) {
        REQUIRE(vm.add_via({"V" + std::to_string(i), {50.0 + i * 2.0, 10.0},
                            (i < 2) ? 0.15 : 0.25,
                            (i < 2) ? 0.45 : 0.42,
                            0, 3, ViaType::ThroughHole}, 0.05).is_ok());
    }

    const auto rep = DrcEngine::check_board(rules, tracks, {}, vm, {});
    REQUIRE(!rep.passed());
    REQUIRE(rep.violations.size() == 20);
}

TEST_CASE("GerberExporter & ExcellonExporter: RS-274X rev 2023.08 and METRIC TZ drill syntax conformance") {
    auto board = BoardOutline::make_rectangle(100.0, 80.0);
    board.add_mounting_hole({{5.0, 5.0}, 3.2, 0.5});

    std::vector<PcbTrackSegment> tracks = {
        {"VCC", 0, 0.25, {10.0, 10.0}, {50.0, 10.0}, false}
    };
    std::vector<DrcPad> pads = {
        {"U1", "1", "VCC", 0, {10.0, 10.0}, 1.55, 0.48}
    };
    std::vector<PcbVia> vias = {
        {"VCC", {50.0, 10.0}, 0.30, 0.60, 0, 3, ViaType::ThroughHole}
    };

    const auto gerber = GerberExporter::export_copper_layer("F.Cu", 0, tracks, pads, vias);
    REQUIRE(gerber.find("%FSLAX36Y36*%") != std::string::npos);
    REQUIRE(gerber.find("%MOMM*%") != std::string::npos);
    REQUIRE(gerber.find("%ADD10C,") != std::string::npos);
    REQUIRE(gerber.find("D01*") != std::string::npos);
    REQUIRE(gerber.find("D03*") != std::string::npos);
    REQUIRE(gerber.find("M02*") != std::string::npos);

    const auto edge_gbr = GerberExporter::export_board_outline(board);
    REQUIRE(edge_gbr.find("%TF.FileFunction,Profile,NP*%") != std::string::npos);

    const auto drl = ExcellonExporter::export_drill_file(vias, board.mounting_holes());
    REQUIRE(drl.find("M48") != std::string::npos);
    REQUIRE(drl.find("METRIC,TZ") != std::string::npos);
    REQUIRE(drl.find("T1C0.300") != std::string::npos);
    REQUIRE(drl.find("M30") != std::string::npos);
}

TEST_CASE("Pcb3DExporter: watertight 3D assembly OBJ & STEP matching stackup thickness and volume") {
    auto board = BoardOutline::make_rectangle(100.0, 80.0);
    auto stackup = BoardStackupManager::standard_4layer_fr4();
    std::vector<PcbComponent3DBody> bodies = {
        {"U1", {50.0, 40.0}, 10.0, 10.0, 1.40, true} // volume = 140 mm^3
    };

    const auto asm3d = Pcb3DExporter::assemble_3d_model(board, stackup, {}, {}, bodies);
    const double expected_z_max = stackup.total_thickness_mm() + 1.40;
    REQUIRE(std::abs(asm3d.bbox_min_mm.z - 0.0) < 1e-6);
    REQUIRE(std::abs(asm3d.bbox_max_mm.z - expected_z_max) < 1e-6);
    // Substrate volume = 100*80*total_thickness + U1 volume (140 mm^3)
    const double expected_vol = 100.0 * 80.0 * stackup.total_thickness_mm() + 140.0;
    REQUIRE(std::abs(asm3d.mesh_volume_mm3 - expected_vol) < 1e-3);

    const auto obj = Pcb3DExporter::export_obj(asm3d);
    const auto step = Pcb3DExporter::export_step_ap214("TestBoard", asm3d);
    REQUIRE(obj.find("v ") != std::string::npos);
    REQUIRE(step.find("ISO-10303-21;") != std::string::npos);
    REQUIRE(step.find("END-ISO-10303-21;") != std::string::npos);
}
