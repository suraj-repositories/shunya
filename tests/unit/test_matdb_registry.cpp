#include "tests/test_framework.hpp"

#include "shunya/matdb/elements_118_catalog.hpp"
#include "shunya/matdb/isotope_repository.hpp"
#include "shunya/matdb/periodic_table_extended.hpp"

#include <array>
#include <cmath>

using namespace shunya::matdb;
using shunya::core::domain::PeriodicTableDatabase;

TEST_CASE("Elements118Catalog: completeness audit, IUPAC 2021 atomic weights, SQLite DDL, and JSON export") {
    auto report = Elements118Catalog::audit_completeness();
    REQUIRE(report.is_valid());
    REQUIRE(Elements118Catalog::all_elements().size() == 118);

    const auto* h = Elements118Catalog::find_by_z(1);
    REQUIRE(h != nullptr);
    REQUIRE(h->symbol == "H");
    REQUIRE(std::abs(h->atomic_weight_amu - 1.008) < 1e-4);

    const auto* fe = Elements118Catalog::find_by_symbol("Fe");
    REQUIRE(fe != nullptr);
    REQUIRE(fe->atomic_number == 26);
    REQUIRE(fe->period == 4);
    REQUIRE(fe->group == 8);
    REQUIRE(fe->block == 'd');
    REQUIRE(std::abs(fe->atomic_weight_amu - 55.845) < 1e-3);

    const auto* og = Elements118Catalog::find_by_z(118);
    REQUIRE(og != nullptr);
    REQUIRE(og->symbol == "Og");
    REQUIRE(og->name == "Oganesson");
    REQUIRE(og->period == 7);
    REQUIRE(og->group == 18);

    PeriodicTableDatabase db;
    Elements118Catalog::populate_database(db);
    REQUIRE(db.element_count() == 118);
    REQUIRE(db.get_element("Au") != nullptr);
    REQUIRE(db.get_element("Au")->atomic_number == 79);

    auto ddl = Elements118Catalog::generate_sqlite_elements_v2_ddl();
    REQUIRE(ddl.find("CREATE TABLE IF NOT EXISTS elements_v2") != std::string::npos);

    auto json = Elements118Catalog::to_json();
    REQUIRE(json.find("\"symbol\":\"Fe\"") != std::string::npos);
    REQUIRE(json.find("\"symbol\":\"Og\"") != std::string::npos);
}

TEST_CASE("IsotopeRepository: natural terrestrial isotope abundances sum to 100% +/- 0.01% and unit conversions") {
    auto report = IsotopeRepository::audit_abundances();
    REQUIRE(report.is_valid());

    // Multi-isotope naturally occurring elements explicitly checked
    const std::array<int, 20> natural_z = {
        1, 3, 5, 6, 7, 8, 12, 14, 16, 17,
        26, 29, 30, 35, 47, 50, 56, 78, 82, 92
    };
    for (int z : natural_z) {
        double sum = IsotopeRepository::natural_abundance_sum(z);
        REQUIRE(std::abs(sum - 100.0) <= 0.01);
    }

    // Radioactive isotopes & half-life unit conversion
    const auto* c14 = IsotopeRepository::find_isotope(6, 14);
    REQUIRE(c14 != nullptr);
    REQUIRE(!c14->is_stable);
    REQUIRE(c14->half_life_seconds.has_value());
    REQUIRE(std::abs(*c14->half_life_seconds - half_life_to_seconds(5730.0, TimeUnit::Years)) < 1e-3);
    REQUIRE(!c14->decay_modes.empty());
    REQUIRE(c14->decay_modes[0] == DecayMode::BetaMinus);
    REQUIRE(c14->decay_daughters[0] == "N-14");

    const auto* u235 = IsotopeRepository::find_isotope(92, 235);
    REQUIRE(u235 != nullptr);
    REQUIRE(std::abs(u235->abundance_pct - 0.7204) < 1e-6);

    REQUIRE(std::abs(half_life_to_seconds(1.0, TimeUnit::Days) - 86400.0) < 1e-9);
    REQUIRE(std::abs(half_life_to_seconds(2.0, TimeUnit::Hours) - 7200.0) < 1e-9);
    REQUIRE(std::abs(half_life_to_seconds(500.0, TimeUnit::Milliseconds) - 0.5) < 1e-9);

    PeriodicTableDatabase db;
    Elements118Catalog::populate_database(db);
    IsotopeRepository::populate_isotopes(db);
    const auto* c_elem = db.get_element(6);
    REQUIRE(c_elem != nullptr);
    REQUIRE(c_elem->has_isotope(12));
    REQUIRE(c_elem->has_isotope(13));
    REQUIRE(c_elem->has_isotope(14));

    auto ddl = IsotopeRepository::generate_sqlite_isotopes_ddl();
    REQUIRE(ddl.find("CREATE TABLE IF NOT EXISTS isotopes") != std::string::npos);
    REQUIRE(!IsotopeRepository::to_json().empty());
}

TEST_CASE("PeriodicTableExtended: electronegativity scales, noble gas vdW radii, and periodic trends") {
    auto report = PeriodicTableExtended::audit_consistency();
    REQUIRE(report.is_valid());
    REQUIRE(PeriodicTableExtended::all_records().size() == 118);

    const auto* fluorine = PeriodicTableExtended::find_by_z(9);
    REQUIRE(fluorine != nullptr);
    REQUIRE(fluorine->electronegativity.pauling.has_value());
    REQUIRE(std::abs(*fluorine->electronegativity.pauling - 3.98) < 1e-6);
    REQUIRE(fluorine->electronegativity.allen.has_value());
    REQUIRE(fluorine->electronegativity.mulliken_ev.has_value());
    REQUIRE(fluorine->radii_pm.vdw_pm.has_value());
    REQUIRE(fluorine->radii_pm.covalent_single_pm.has_value());

    // Noble gases He..Rn have van der Waals radii populated
    for (int z : {2, 10, 18, 36, 54, 86}) {
        const auto* ng = PeriodicTableExtended::find_by_z(z);
        REQUIRE(ng != nullptr);
        REQUIRE(ng->radii_pm.vdw_pm.has_value());
        REQUIRE(*ng->radii_pm.vdw_pm >= 140.0);
    }

    // Group 1 covalent radius increases monotonically down Li -> Na -> K -> Rb -> Cs
    const std::array<int, 5> alkali = {3, 11, 19, 37, 55};
    for (size_t i = 0; i + 1 < alkali.size(); ++i) {
        const auto* a = PeriodicTableExtended::find_by_z(alkali[i]);
        const auto* b = PeriodicTableExtended::find_by_z(alkali[i + 1]);
        REQUIRE(a->radii_pm.covalent_single_pm.has_value());
        REQUIRE(b->radii_pm.covalent_single_pm.has_value());
        REQUIRE(*a->radii_pm.covalent_single_pm < *b->radii_pm.covalent_single_pm);
    }

    // Period 2 Pauling electronegativity increases monotonically across Li..F
    for (int z = 3; z < 9; ++z) {
        const auto* a = PeriodicTableExtended::find_by_z(z);
        const auto* b = PeriodicTableExtended::find_by_z(z + 1);
        REQUIRE(a->electronegativity.pauling.has_value());
        REQUIRE(b->electronegativity.pauling.has_value());
        REQUIRE(*a->electronegativity.pauling < *b->electronegativity.pauling);
    }

    PeriodicTableDatabase db;
    Elements118Catalog::populate_database(db);
    PeriodicTableExtended::populate_electronegativity(db);
    const auto* o_elem = db.get_element(8);
    REQUIRE(o_elem != nullptr);
    REQUIRE(o_elem->electronegativity.has_value());
    REQUIRE(std::abs(*o_elem->electronegativity - 3.44) < 1e-6);

    REQUIRE(!PeriodicTableExtended::to_json().empty());
}
