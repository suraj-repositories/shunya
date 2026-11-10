#include "tests/test_framework.hpp"

#include "shunya/pcb/erc_engine.hpp"
#include "shunya/pcb/schematic_sheet.hpp"
#include "shunya/pcb/wire_router_2d.hpp"

#include <cmath>
#include <random>

using namespace shunya::pcb;

TEST_CASE("SchematicDocument: global CLK net across sheets, 3-level hierarchy ports, and DATA[0..7] bus expansion") {
    auto bus = expand_bus_signal("DATA[0..7]");
    REQUIRE(bus.size() == 8);
    REQUIRE(bus.front() == "DATA[0]");
    REQUIRE(bus.back() == "DATA[7]");

    SchematicDocument doc("Hier3Level");

    // Root sheet (Level 1): U1 drives global CLK, U1 pin2 feeds Sub2 port "DIN", wire labeled "DATA[0..7]"
    SchematicSheet root{"root", "Top Sheet", {}, {}, {}, {}, {}, {}};
    root.components.push_back({"U1", "MCU", {10, 10}, 5, 5, {
        {"U1", "1", "CLK_OUT", PinElectricalType::Output, {20, 10}, false},
        {"U1", "2", "TX", PinElectricalType::Output, {20, 15}, false},
        {"U1", "3", "D_BUS", PinElectricalType::Bidirectional, {20, 20}, false}
    }});
    root.wires.push_back({{20, 10}, {30, 10}, ""});
    root.labels.push_back({"CLK", true, {30, 10}});

    root.wires.push_back({{20, 15}, {40, 15}, ""});
    root.subsheets.push_back({"BLK2", "sheet2", {{"DIN", SheetPortDirection::Input, {40, 15}}}});

    root.wires.push_back({{20, 20}, {35, 20}, ""});
    root.labels.push_back({"DATA[0..7]", true, {35, 20}});
    REQUIRE(doc.add_sheet(std::move(root)).is_ok());

    // Sheet 2 (Level 2): global CLK receiver + hierarchical pass-through DIN -> DOUT to Sheet 3
    SchematicSheet s2{"sheet2", "SubSheet L2", {}, {}, {}, {}, {}, {}};
    s2.components.push_back({"U2", "FF", {10, 10}, 4, 4, {
        {"U2", "1", "CLK", PinElectricalType::Input, {5, 10}, false},
        {"U2", "2", "D_BUS", PinElectricalType::Input, {5, 20}, false}
    }});
    s2.wires.push_back({{5, 10}, {2, 10}, ""});
    s2.labels.push_back({"CLK", true, {2, 10}});
    s2.wires.push_back({{5, 20}, {2, 20}, ""});
    s2.labels.push_back({"DATA[0..7]", true, {2, 20}});

    s2.ports.push_back({"DIN", SheetPortDirection::Input, {10, 30}});
    s2.wires.push_back({{10, 30}, {30, 30}, ""});
    s2.subsheets.push_back({"BLK3", "sheet3", {{"LEAF_IN", SheetPortDirection::Input, {30, 30}}}});
    REQUIRE(doc.add_sheet(std::move(s2)).is_ok());

    // Sheet 3 (Level 3): leaf receives U1.2 through 3-level port chain
    SchematicSheet s3{"sheet3", "LeafSheet L3", {}, {}, {}, {}, {}, {}};
    s3.ports.push_back({"LEAF_IN", SheetPortDirection::Input, {10, 10}});
    s3.components.push_back({"R1", "10k", {20, 10}, 2, 2, {
        {"R1", "1", "A", PinElectricalType::Input, {15, 10}, false}
    }});
    s3.wires.push_back({{10, 10}, {15, 10}, ""});
    REQUIRE(doc.add_sheet(std::move(s3)).is_ok());

    const auto netlist = doc.compile_netlist();

    // Global CLK net connects U1.1 and BLK2/U2.1
    const auto* clk = netlist.find_net("CLK");
    REQUIRE(clk != nullptr);
    REQUIRE(clk->pins.size() == 2);

    // 3-level hierarchical signal bridges U1.2 (root) -> BLK2 -> BLK3/R1.1
    bool found_root_u1 = false;
    bool found_leaf_r1 = false;
    for (const auto& net : netlist.nets) {
        bool has_u1_2 = false;
        bool has_r1_1 = false;
        for (const auto& p : net.pins) {
            if (p.component_ref == "U1" && p.pin_number == "2") has_u1_2 = true;
            if (p.component_ref == "R1" && p.pin_number == "1") has_r1_1 = true;
        }
        if (has_u1_2 && has_r1_1) {
            found_root_u1 = true;
            found_leaf_r1 = true;
        }
    }
    REQUIRE(found_root_u1 && found_leaf_r1);

    // Expanded bus DATA[0]..DATA[7] connects U1.3 and U2.2 across all 8 bit nets
    for (int i = 0; i < 8; ++i) {
        const auto* bit_net = netlist.find_net("DATA[" + std::to_string(i) + "]");
        REQUIRE(bit_net != nullptr);
        REQUIRE(bit_net->pins.size() == 2);
    }
}

TEST_CASE("OrthogonalWireRouter: A* grid routing around 10 IC obstacles with zero diagonals and T-junction dots") {
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dx(20.0, 90.0);
    std::uniform_real_distribution<double> dy(15.0, 70.0);

    std::vector<SchematicComponent> obstacles;
    for (int i = 0; i < 10; ++i) {
        const double cx = std::round(dx(rng) / 2.54) * 2.54;
        const double cy = std::round(dy(rng) / 2.54) * 2.54;
        obstacles.push_back({"IC" + std::to_string(i), "IC", {cx, cy}, 3.81, 3.81, {}});
    }

    auto res = OrthogonalWireRouter::route({2.54, 2.54}, {114.3, 81.28}, obstacles, 2.54, 1.27);
    REQUIRE(res.is_ok());
    const auto& segs = res.value().segments;
    REQUIRE(!segs.empty());

    // Zero diagonal segments allowed
    for (const auto& s : segs) {
        const bool horiz = std::abs(s.start_mm.y - s.end_mm.y) < 1e-6;
        const bool vert = std::abs(s.start_mm.x - s.end_mm.x) < 1e-6;
        REQUIRE(horiz || vert);
    }

    // T-junction detection
    std::vector<WireSegment2D> tee = {
        {{0.0, 10.0}, {40.0, 10.0}, "SIG"},
        {{20.0, 10.0}, {20.0, 30.0}, "SIG"}
    };
    auto dots = OrthogonalWireRouter::detect_junctions(tee);
    REQUIRE(dots.size() == 1);
    REQUIRE(std::abs(dots[0].x - 20.0) < 1e-6 && std::abs(dots[0].y - 10.0) < 1e-6);
}

TEST_CASE("ErcEngine: detects Output-Output conflict, shorted power rails, and floating CMOS Inputs") {
    SchematicDocument doc("ErcFaulty");
    SchematicSheet s{"root", "Faulty", {}, {}, {}, {}, {}, {}};

    // U1 Output shorted to U2 Output; U3 PowerOutput (5V) shorted to U4 PowerOutput (3.3V); U5 floating Input
    s.components.push_back({"U1", "GateA", {10, 10}, 4, 4, {
        {"U1", "1", "Y", PinElectricalType::Output, {20, 10}, false}
    }});
    s.components.push_back({"U2", "GateB", {40, 10}, 4, 4, {
        {"U2", "1", "Y", PinElectricalType::Output, {30, 10}, false}
    }});
    s.wires.push_back({{20, 10}, {30, 10}, "SHORT_OUT"});

    s.components.push_back({"U3", "Reg5V", {10, 30}, 4, 4, {
        {"U3", "1", "VOUT", PinElectricalType::PowerOutput, {20, 30}, false}
    }});
    s.components.push_back({"U4", "Reg3V3", {40, 30}, 4, 4, {
        {"U4", "1", "VOUT", PinElectricalType::PowerOutput, {30, 30}, false}
    }});
    s.wires.push_back({{20, 30}, {30, 30}, "PWR_SHORT"});

    s.components.push_back({"U5", "CMOS_INV", {10, 50}, 4, 4, {
        {"U5", "1", "IN_FLOAT", PinElectricalType::Input, {5, 50}, false}
    }});

    REQUIRE(doc.add_sheet(std::move(s)).is_ok());

    const auto report = ErcEngine::run_checks(doc);
    REQUIRE(!report.passed());
    REQUIRE(report.error_count() == 2);
    REQUIRE(report.warning_count() >= 1);
}
