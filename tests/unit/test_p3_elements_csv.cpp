#include "tests/test_framework.hpp"

#include "shunya/matdb/element_database.hpp"

#include <cmath>
#include <filesystem>

using namespace shunya::matdb;

TEST_CASE("ELEMDB: RFC 4180 CSV parser handles multiline quoted fields and escaped quotes") {
    const std::string csv =
        "Name,Symbol,Atomic_Number,Space_Group_Name,Notes\n"
        "Helium,He,2,\"Fm_\n3m\",\"Quoted \"\"text\"\"\"\n"
        "Lithium,Li,3,\"Im_\n3m\",Plain\n";
    auto rows = ElementDatabase::parse_rfc4180_csv(csv);
    REQUIRE(rows.size() == 3);
    REQUIRE(rows[1].size() == 5);
    CHECK(rows[1][0] == "Helium");
    CHECK(rows[1][3] == "Fm_\n3m");
    CHECK(rows[1][4] == "Quoted \"text\"");
    CHECK(rows[2][3] == "Im_\n3m");
}

TEST_CASE("ELEMDB: Load full elements.csv (118 elements, 82 columns) with normalization & provenance") {
    ElementDatabase db;
    auto res = db.load_default();
    REQUIRE(res.is_ok());
    CHECK(db.count() == 118u);
    CHECK(db.csv_columns().size() == 82u);

    // Hydrogen: Adiabatic_Index normalized from Excel-mangled "07-May" -> 1.4
    const auto* h = db.find_by_symbol("H");
    REQUIRE(h != nullptr);
    CHECK(h->atomic_number == 1);
    CHECK(h->name == "Hydrogen");
    REQUIRE(h->adiabatic_index.has_value());
    CHECK(std::abs(*h->adiabatic_index - 1.4) < 1e-6);
    CHECK(h->provenance_of("Adiabatic_Index") == PropertyProvenance::CsvNormalized);
    CHECK(h->raw_csv_fields.at("Adiabatic_Index") == "07-May");

    // Helium: Adiabatic_Index "05-Mar" -> 5/3, Space_Group_Name multiline "Fm_\n3m" -> "Fm-3m"
    const auto* he = db.find_by_z(2);
    REQUIRE(he != nullptr);
    REQUIRE(he->adiabatic_index.has_value());
    CHECK(std::abs(*he->adiabatic_index - (5.0 / 3.0)) < 1e-6);
    CHECK(he->space_group_name == "Fm-3m");
    CHECK(he->provenance_of("Space_Group_Name") == PropertyProvenance::CsvNormalized);

    // Lithium & Copper key engineering properties from CSV
    const auto* li = db.find_by_symbol("Li");
    REQUIRE(li != nullptr);
    REQUIRE(li->density.has_value());
    CHECK(std::abs(*li->density - 535.0) < 1e-6);
    REQUIRE(li->melting_point.has_value());
    CHECK(std::abs(*li->melting_point - 453.69) < 1e-6);
    REQUIRE(li->autoignition_point.has_value());
    CHECK(std::abs(*li->autoignition_point - 452.15) < 1e-6);

    const auto* cu = db.find_by_symbol("Cu");
    REQUIRE(cu != nullptr);
    REQUIRE(cu->density.has_value());
    CHECK(std::abs(*cu->density - 8920.0) < 1e-6);
    REQUIRE(cu->thermal_conductivity.has_value());
    CHECK(std::abs(*cu->thermal_conductivity - 400.0) < 1e-6);
    REQUIRE(cu->effective_conductivity().has_value());
    CHECK(std::abs(*cu->effective_conductivity() - 5.9e7) < 1e3);

    // Z=117 (Uus/Ts): Missing Atomic_Weight in CSV gets IUPAC fallback provenance, modern symbol alias resolves
    const auto* ts = db.find_by_symbol("Ts");
    REQUIRE(ts != nullptr);
    CHECK(ts->atomic_number == 117);
    CHECK(ts->symbol == "Uus");
    CHECK(ts->iupac_symbol == "Ts");
    CHECK(ts->provenance_of("Atomic_Weight") == PropertyProvenance::NistReferenceFallback);
    CHECK(ts->atomic_weight.has_value());

    // Missing optional property must remain MissingUnsupported (never fabricated)
    CHECK(cu->provenance_of("Critical_Pressure") == PropertyProvenance::MissingUnsupported);
    CHECK_FALSE(cu->critical_pressure.has_value());
}

TEST_CASE("ELEMDB: Search, filter, sort, and PeriodicTableDatabase population") {
    const auto& db = ElementDatabase::global();
    REQUIRE(db.count() == 118u);

    // Filter conductors in Period 4 sorted by thermal conductivity descending
    ElementQuery q;
    q.period_filter = 4;
    q.electrical_type_filter = "Conductor";
    q.sort_by = ElementSortField::ThermalConductivity;
    q.descending = true;
    auto res = db.query(q);
    REQUIRE(!res.empty());
    CHECK(res.front()->symbol == "Cu"); // Copper (400 W/mK) is highest in period 4

    // Search by alternate name ("Natrium" -> Sodium) or CAS number
    ElementQuery q_alt;
    q_alt.search_text = "Natrium";
    auto na_res = db.query(q_alt);
    REQUIRE(na_res.size() == 1u);
    CHECK(na_res[0]->symbol == "Na");

    // Populate PeriodicTableDatabase
    shunya::core::domain::PeriodicTableDatabase pdb;
    db.populate_periodic_database(pdb);
    CHECK(pdb.element_count() == 118u);
    const auto* fe = pdb.get_element("Fe");
    REQUIRE(fe != nullptr);
    CHECK(fe->density_at_stp.has_value());
    CHECK(std::abs(*fe->density_at_stp - 7874.0) < 1e-6);
}

TEST_CASE("ELEMDB: Rejects invalid or malformed CSV inputs cleanly") {
    ElementDatabase bad;
    CHECK(bad.load_from_csv_text("").is_err());
    CHECK(bad.load_from_csv_text("Foo,Bar\n1,2\n").is_err());
    CHECK(bad.load_from_csv_text("Name,Symbol,Atomic_Number\nBad,X,999\n").is_err());
    CHECK(bad.load_from_csv("nonexistent_elements_file_12345.csv").is_err());
}
