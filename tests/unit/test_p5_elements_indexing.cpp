#include "tests/test_framework.hpp"

#include "shunya/matdb/element_database.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"

#include <cmath>
#include <filesystem>

using namespace shunya::matdb;

TEST_CASE("PH5-01.01: Deterministic Path Resolution & 118 Elements Multi-Tier Ingestion") {
    // Fresh database instance verifies independent multi-tier resolution
    ElementDatabase db;
    auto res = db.load_default();
    REQUIRE(res.is_ok());
    CHECK(db.is_loaded());
    CHECK(db.count() == 118u);
    CHECK(db.csv_columns().size() == 82u);

    // Verify source path was recorded (either filesystem or embedded fallback)
    CHECK(!db.source_path().empty());

    // 1. Hydrogen (Z=1): Default element selected on initial studio load
    const auto* h = db.find_by_z(1);
    REQUIRE(h != nullptr);
    CHECK(h->atomic_number == 1);
    CHECK(h->symbol == "H");
    CHECK(h->name == "Hydrogen");
    CHECK(h->phase == "Gas");
    CHECK(h->block == "s");
    REQUIRE(h->atomic_weight.has_value());
    CHECK(std::abs(*h->atomic_weight - 1.00794) < 1e-3);
    REQUIRE(h->density.has_value());
    CHECK(std::abs(*h->density - 0.0899) < 1e-3);

    // 2. Iron (Z=26): Crucial for Mixing Lab alloy validation (e.g. Fe + C + Cu)
    const auto* fe = db.find_by_symbol("Fe");
    REQUIRE(fe != nullptr);
    CHECK(fe->atomic_number == 26);
    CHECK(fe->symbol == "Fe");
    CHECK(fe->name == "Iron");
    CHECK(fe->phase == "Solid");
    CHECK(fe->block == "d");
    REQUIRE(fe->density.has_value());
    CHECK(std::abs(*fe->density - 7874.0) < 10.0);
    REQUIRE(fe->melting_point.has_value());
    CHECK(std::abs(*fe->melting_point - 1811.0) < 20.0);

    // 3. Copper (Z=29): Benchmark electrical conductor
    const auto* cu = db.find_by_symbol("Cu");
    REQUIRE(cu != nullptr);
    CHECK(cu->atomic_number == 29);
    CHECK(cu->symbol == "Cu");
    CHECK(cu->name == "Copper");
    REQUIRE(cu->density.has_value());
    CHECK(std::abs(*cu->density - 8960.0) < 50.0);
    REQUIRE(cu->electrical_conductivity.has_value());
    CHECK(*cu->electrical_conductivity > 5.0e7);

    // 4. Tungsten (Z=74): Refractory metal for incandescent bulb filament milestone
    const auto* w = db.find_by_symbol("W");
    REQUIRE(w != nullptr);
    CHECK(w->atomic_number == 74);
    CHECK(w->symbol == "W");
    CHECK(w->name == "Tungsten");
    REQUIRE(w->melting_point.has_value());
    CHECK(*w->melting_point > 3600.0); // 3695 K

    // 5. Argon (Z=18): Inert fill gas for bulb milestone
    const auto* ar = db.find_by_symbol("Ar");
    REQUIRE(ar != nullptr);
    CHECK(ar->atomic_number == 18);
    CHECK(ar->symbol == "Ar");
    CHECK(ar->phase == "Gas");

    // 6. Element 118: Final synthetic transactinide element (CSV Uuo / IUPAC Og)
    const auto* og = db.find_by_z(118);
    REQUIRE(og != nullptr);
    CHECK(og->atomic_number == 118);
    CHECK((og->symbol == "Uuo" || og->symbol == "Og"));
    CHECK(og->iupac_symbol == "Og");
}

TEST_CASE("PH5-01.02: Global Database Singleton & Non-Zero Navbar/Inspector State") {
    // Ensure global singleton is loaded
    if (!ElementDatabase::global().is_loaded()) {
        auto res = ElementDatabase::global().load_default();
        REQUIRE(res.is_ok());
    }
    CHECK(ElementDatabase::global().is_loaded());
    CHECK(ElementDatabase::global().count() == 118u);

    // Verify lookup by name, symbol, IUPAC symbol, and Z
    const auto* by_sym = ElementDatabase::global().find_by_symbol("Fe");
    const auto* by_name = ElementDatabase::global().find_by_name("Iron");
    const auto* by_z = ElementDatabase::global().find_by_z(26);
    REQUIRE(by_sym != nullptr);
    REQUIRE(by_name != nullptr);
    REQUIRE(by_z != nullptr);
    CHECK(by_sym == by_name);
    CHECK(by_name == by_z);
}
