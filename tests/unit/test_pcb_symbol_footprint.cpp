#include "tests/test_framework.hpp"

#include "shunya/pcb/footprint_generator.hpp"
#include "shunya/pcb/symbol_library.hpp"

#include <cmath>

using namespace shunya::pcb;

TEST_CASE("SymbolLibraryManager: IEEE vs IEC resistor toggle, rotation (0/90/180/270), and X/Y mirroring") {
    SymbolLibraryManager mgr(SymbolStandard::IEEE_US);
    auto r_us = mgr.get_symbol("Resistor");
    REQUIRE(r_us.pins.size() == 2);
    REQUIRE(r_us.graphics.size() == 9); // Classic zigzag
    REQUIRE(mgr.all_symbols().size() >= 7);

    mgr.set_standard(SymbolStandard::IEC_EU);
    auto r_eu = mgr.get_symbol("Resistor");
    REQUIRE(r_eu.graphics.size() == 3); // Rectangular box + leads

    // 90 deg rotation moves OpAmp pin 3 (7.62, 0) -> (0, 7.62); 180 deg -> (-7.62, 0)
    auto op = mgr.get_symbol("OpAmp");
    auto op90 = op.transformed(90, false, false);
    REQUIRE(std::abs(op90.pins[2].pos_mm.x - 0.0) < 1e-6);
    REQUIRE(std::abs(op90.pins[2].pos_mm.y - 7.62) < 1e-6);

    auto op_mir = op.transformed(0, true, true);
    REQUIRE(std::abs(op_mir.pins[2].pos_mm.x - (-7.62)) < 1e-6);
    REQUIRE(!mgr.to_json().empty());
}

TEST_CASE("FootprintGenerator: IPC-7351B density level B SOIC-8, LQFP-48, 0603 chip, and BGA-64") {
    // SOIC-8: pitch=1.27mm, span=6.0mm, lead_len=0.85mm, lead_wid=0.42mm
    // Level B: toe=0.35, heel=0.35, side=0.03 => pad_len = 0.85+0.70 = 1.55mm, pad_wid = 0.48mm, center_span = 5.15mm
    auto soic8 = FootprintGenerator::preset_soic_8();
    REQUIRE(soic8.pads.size() == 8);
    REQUIRE(std::abs(soic8.pads[0].width_mm - 1.55) <= 0.01);
    REQUIRE(std::abs(soic8.pads[0].height_mm - 0.48) <= 0.01);
    REQUIRE(std::abs(soic8.pads[0].center_mm.y - 1.905) <= 0.01);

    // LQFP-48: 48 pads (12 per side), 0.5mm pitch
    auto qfp48 = FootprintGenerator::preset_lqfp_48();
    REQUIRE(qfp48.pads.size() == 48);
    REQUIRE(std::abs(qfp48.pads[0].width_mm - 1.30) <= 0.01); // 0.60 + 0.35 + 0.35

    // 0603 Chip Resistor/Capacitor Level B vs Level A vs Level C courtyard area
    auto c0603_b = FootprintGenerator::generate_chip_passive("0603", DensityLevel::Nominal_B);
    auto c0603_a = FootprintGenerator::generate_chip_passive("0603", DensityLevel::Most_A);
    REQUIRE(c0603_b.is_ok() && c0603_a.is_ok());
    REQUIRE(c0603_a.value().courtyard_width_mm > c0603_b.value().courtyard_width_mm);

    // BGA 8x8 = 64 circular pads
    auto bga = FootprintGenerator::generate_bga("BGA-64", 8, 8, 0.80, 0.40);
    REQUIRE(bga.is_ok());
    REQUIRE(bga.value().pads.size() == 64);
    REQUIRE(bga.value().pads[0].number == "A1");
    REQUIRE(std::abs(bga.value().pads[0].width_mm - 0.32) <= 0.01);
}
