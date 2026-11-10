#include "tests/test_framework.hpp"

#include "shunya/pcb/board_outline.hpp"
#include "shunya/pcb/board_stackup.hpp"

#include <cmath>

using namespace shunya::pcb;

TEST_CASE("BoardStackupManager & ImpedanceCalculator: IPC-2141 50-ohm microstrip on FR-4 and 32-layer support") {
    auto s2 = BoardStackupManager::standard_2layer_fr4();
    REQUIRE(s2.copper_layer_count() == 2);
    REQUIRE(std::abs(s2.total_thickness_mm() - 1.60) < 1e-6);

    // Solve 50-ohm microstrip width on 1.53 mm FR-4 core (er=4.4, 1oz copper) -> ~2.92 mm
    auto w50 = ImpedanceCalculator::solve_microstrip_width_mm(50.0, 0.035, 1.53, 4.4);
    REQUIRE(w50.is_ok());
    auto z_check = s2.compute_top_microstrip_z0(w50.value());
    REQUIRE(z_check.is_ok());
    REQUIRE(100.0 * std::abs(z_check.value() - 50.0) / 50.0 < 1.0);

    // 4-layer stackup (0.2mm prepreg, er=4.4 -> ~0.35mm trace gives ~50 ohms)
    auto s4 = BoardStackupManager::standard_4layer_fr4();
    REQUIRE(s4.copper_layer_count() == 4);
    auto w4 = ImpedanceCalculator::solve_microstrip_width_mm(50.0, 0.035, 0.20, 4.4);
    REQUIRE(w4.is_ok());
    REQUIRE(w4.value() > 0.25 && w4.value() < 0.45);

    // Stripline Z_0 check (b=0.40 mm, w=0.18 mm, t=0.035 mm, er=4.4 -> ~48-52 ohms)
    auto z_strip = ImpedanceCalculator::stripline_z0_ipc2141({0.18, 0.035, 0.40, 4.4});
    REQUIRE(z_strip.is_ok());
    REQUIRE(z_strip.value() > 40.0 && z_strip.value() < 60.0);

    // Enforce 32 copper layer limit
    BoardStackupManager big;
    for (int i = 0; i < 32; ++i) {
        REQUIRE(big.add_layer({"L" + std::to_string(i), StackupLayerKind::Signal, 0.035}).is_ok());
    }
    REQUIRE(big.add_layer({"L33", StackupLayerKind::Signal, 0.035}).is_err());
}

TEST_CASE("BoardOutline & KeepoutManager: cutouts, circular mounting holes, and edge clearance violations") {
    auto board = BoardOutline::make_rectangle(100.0, 80.0);
    // Internal milled slot [40..60, 30..50]
    board.add_cutout_polygon({{40.0, 30.0}, {60.0, 30.0}, {60.0, 50.0}, {40.0, 50.0}});
    // Circular M3 mounting hole at (5, 5) with 3.2mm drill + 0.5mm margin
    board.add_mounting_hole({{5.0, 5.0}, 3.2, 0.5});
    // RF antenna keepout zone [80..95, 60..75]
    board.add_keepout_zone({"ANTENNA_KEEPOUT", {{80, 60}, {95, 60}, {95, 75}, {80, 75}}, true, true, true});

    // Valid interior position
    REQUIRE(board.contains_copper_region({20.0, 20.0}));
    REQUIRE(board.is_component_placement_allowed({20.0, 20.0}, 2.0, 0.5));
    REQUIRE(board.is_trace_point_allowed({20.0, 20.0}, 0.15, 0.3));

    // Inside milled slot
    REQUIRE(!board.contains_copper_region({50.0, 40.0}));
    REQUIRE(!board.is_component_placement_allowed({50.0, 40.0}, 1.0, 0.5));

    // Too close to edge (x=0.3mm violates 0.5mm edge clearance for trace half_width 0.25mm)
    REQUIRE(!board.is_trace_point_allowed({0.3, 40.0}, 0.25, 0.3));

    // Inside mounting hole drill and keepout zone
    REQUIRE(!board.contains_copper_region({5.0, 5.0}));
    REQUIRE(!board.is_component_placement_allowed({88.0, 68.0}, 1.0, 0.5));
    REQUIRE(!board.is_trace_point_allowed({88.0, 68.0}, 0.1, 0.3));
}
