#include "tests/test_framework.hpp"
#include "shunya/core/domain/material_dictionary.hpp"
#include "shunya/core/domain/periodic_table.hpp"
#include "shunya/core/domain/dataset_release.hpp"

#include <cmath>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("Data dictionary schema, canonical names, and alias resolution (MATDB-01)") {
    DataDictionary dict(base::Version(2, 0, 0));
    REQUIRE(dict.version() == base::Version(2, 0, 0));
    REQUIRE(dict.field_count() >= 10);

    // Canonical names
    REQUIRE(dict.has_field("atomic_number"));
    REQUIRE(dict.has_field("density"));
    REQUIRE(dict.has_field("thermal_conductivity"));

    // Aliases
    REQUIRE(dict.has_field("Z"));
    REQUIRE(dict.has_field("rho"));
    REQUIRE(dict.has_field("k"));
    REQUIRE(dict.has_field("E"));

    const auto* z_def = dict.get_field("Z");
    REQUIRE(z_def != nullptr);
    REQUIRE(z_def->canonical_name == "atomic_number");
    REQUIRE(z_def->data_type == FieldDataType::Numeric);
    REQUIRE(z_def->is_required == true);

    const auto* e_def = dict.get_field("E");
    REQUIRE(e_def != nullptr);
    REQUIRE(e_def->canonical_name == "youngs_modulus");
    REQUIRE(e_def->canonical_unit == "Pa");
    REQUIRE(e_def->applicability == FieldApplicability::SolidOnly);

    // Custom field registration
    FieldDefinition custom_field;
    custom_field.canonical_name = "piezoelectric_d33";
    custom_field.aliases = {"d33"};
    custom_field.data_type = FieldDataType::Quantity;
    custom_field.canonical_unit = "C/N";
    custom_field.applicability = FieldApplicability::SolidOnly;
    custom_field.description = "Longitudinal piezoelectric strain coefficient";
    auto reg_res = dict.register_field(custom_field);
    REQUIRE(reg_res.is_ok());

    REQUIRE(dict.has_field("d33"));
    REQUIRE(dict.get_field("d33")->canonical_name == "piezoelectric_d33");

    // Dictionary JSON roundtrip
    std::string json = dict.to_json();
    auto dict_rt = DataDictionary::from_json(json);
    REQUIRE(dict_rt.is_ok());
    REQUIRE(dict_rt.value().has_field("piezoelectric_d33"));
}

TEST_CASE("Field provenance and licence redistribution verification (MATDB-03)") {
    FieldProvenance nist_prov;
    nist_prov.source_name = "NIST Chemistry WebBook";
    nist_prov.provider_id = "nist-srd-69";
    nist_prov.citation_or_doi = "https://doi.org/10.18434/T4D303";
    nist_prov.year = 2024;
    nist_prov.licence = LicenceType::PublicDomain;
    nist_prov.can_redistribute = true;
    nist_prov.confidence_score = 0.99;

    REQUIRE(nist_prov.can_redistribute == true);
    REQUIRE(licence_type_to_string(nist_prov.licence) == "PublicDomain");

    FieldProvenance proprietary_prov;
    proprietary_prov.source_name = "Commercial Materials Inc.";
    proprietary_prov.provider_id = "cmi-spec-2025";
    proprietary_prov.licence = LicenceType::ProprietaryCommercial;
    proprietary_prov.can_redistribute = false;
    proprietary_prov.confidence_score = 0.85;

    REQUIRE(proprietary_prov.can_redistribute == false);
    REQUIRE(licence_type_to_string(proprietary_prov.licence) == "ProprietaryCommercial");

    auto lic_enum = licence_type_from_string("CC_BY_4_0");
    REQUIRE(lic_enum.is_ok());
    REQUIRE(lic_enum.value() == LicenceType::CC_BY_4_0);
}

TEST_CASE("Unit normalisation with audit trail and original raw value preservation (MATDB-04)") {
    DataDictionary dict;
    FieldProvenance prov{"CRC Handbook", "crc-104", "", 2023, LicenceType::AcademicOnly, false, 0.01, 0.95};

    // 1. Density: g/cm^3 -> kg/m^3
    auto norm_rho = dict.audit_and_normalize("density", 8.96, "g/cm^3", prov);
    REQUIRE(norm_rho.is_ok());
    const auto& av_rho = norm_rho.value();
    REQUIRE(av_rho.raw_value == "8.960000");
    REQUIRE(av_rho.raw_unit == "g/cm^3");
    REQUIRE(std::abs(av_rho.normalized_value - 8960.0) < 1e-4);
    REQUIRE(av_rho.canonical_unit == "kg/m^3");
    REQUIRE(std::abs(av_rho.conversion_factor - 1000.0) < 1e-4);
    REQUIRE(av_rho.provenance == prov);

    // 2. Temperature: °C -> K
    auto norm_temp = dict.audit_and_normalize("melting_point", 1084.62, "degC", prov);
    REQUIRE(norm_temp.is_ok());
    const auto& av_temp = norm_temp.value();
    REQUIRE(std::abs(av_temp.normalized_value - 1357.77) < 1e-3);
    REQUIRE(av_temp.canonical_unit == "K");

    // 3. Modulus: GPa -> Pa
    auto norm_e = dict.audit_and_normalize("youngs_modulus", 117.0, "GPa", prov);
    REQUIRE(norm_e.is_ok());
    REQUIRE(std::abs(norm_e.value().normalized_value - 117.0e9) < 1.0);
    REQUIRE(norm_e.value().canonical_unit == "Pa");

    // Range validation on audited value
    auto rep_valid = dict.validate_audited_value(av_rho);
    REQUIRE(!rep_valid.has_errors());

    AuditedValue bad_av = av_rho;
    bad_av.normalized_value = -5.0; // Negative density!
    auto rep_invalid = dict.validate_audited_value(bad_av);
    REQUIRE(rep_invalid.has_errors());
}

TEST_CASE("Dataset record import adapter with provider metadata preservation (MATDB-02)") {
    PeriodicTableDatabase pt_db;
    FieldProvenance iupac_prov{"IUPAC Commission", "iupac-ciaaw", "", 2021, LicenceType::PublicDomain, true, 0.001, 1.0};

    std::vector<AuditedValue> titanium_fields = {
        {"density", "4.506", "g/cm^3", 4506.0, "kg/m^3", 1000.0, iupac_prov, "2026-11-07T00:00:00Z"},
        {"melting_point", "1668", "degC", 1941.15, "K", 1.0, iupac_prov, "2026-11-07T00:00:00Z"},
        {"atomic_mass", "47.867", "u", 47.867, "u", 1.0, iupac_prov, "2026-11-07T00:00:00Z"},
        {"thermal_conductivity", "21.9", "W/(m*K)", 21.9, "W/(m*K)", 1.0, iupac_prov, "2026-11-07T00:00:00Z"}
    };

    auto imp_res = pt_db.import_dataset_record("iupac-dataset", "IUPAC", 22, "Ti", "Titanium", titanium_fields);
    REQUIRE(imp_res.is_ok());

    REQUIRE(pt_db.has_element(22));
    REQUIRE(pt_db.has_element("Ti"));

    const auto* ti = pt_db.get_element("Ti");
    REQUIRE(ti != nullptr);
    REQUIRE(ti->atomic_number == 22);
    REQUIRE(ti->name == "Titanium");
    REQUIRE(ti->density_at_stp.has_value());
    REQUIRE(std::abs(*ti->density_at_stp - 4506.0) < 1e-3);
    REQUIRE(ti->melting_point_k.has_value());
    REQUIRE(std::abs(*ti->melting_point_k - 1941.15) < 1e-3);
    REQUIRE(ti->audited_fields.size() == 4);
}

TEST_CASE("Periodic table consistency and isotope relationship validation (MATDB-05)") {
    PeriodicTableDatabase db;
    db.seed_representative_elements();

    auto report = db.validate_periodic_consistency();
    REQUIRE(!report.has_errors());

    // Introduce consistency anomalies
    PeriodicElement bad_elem;
    bad_elem.atomic_number = 150; // Invalid Z > 118!
    bad_elem.symbol = "Xx";
    bad_elem.name = "Anomalous";
    bad_elem.period = 9; // Invalid period > 7!
    bad_elem.group = 25; // Invalid group > 18!
    bad_elem.block = 'x'; // Invalid block!
    bad_elem.standard_atomic_weight = -10.0; // Negative atomic weight!
    bad_elem.melting_point_k = 2000.0;
    bad_elem.boiling_point_k = 1000.0; // Inverted phase temperatures (boiling < melting)!
    bad_elem.isotopes = {
        {5, 5.0, 50.0, true, std::nullopt, ""}, // Isotope A < Z (5 < 150)!
        {160, 160.0, 30.0, true, std::nullopt, ""} // Stable sum = 80% != 100%!
    };

    REQUIRE(db.add_element(bad_elem).is_err()); // rejected at addition because Z=150

    bad_elem.atomic_number = 110;
    REQUIRE(db.add_element(bad_elem).is_ok());

    auto rep_anomalies = db.validate_periodic_consistency();
    REQUIRE(rep_anomalies.has_errors());
    REQUIRE(rep_anomalies.has_error("Period out of range [1, 7]: 9") || rep_anomalies.error_count() > 0);
}

TEST_CASE("Conflicting sources review queue and resolution workflow (MATDB-06)") {
    PeriodicTableDatabase db;
    db.seed_representative_elements();

    FieldProvenance nist{"NIST", "srd-69", "", 2024, LicenceType::PublicDomain, true, 0.05, 0.98};
    FieldProvenance crc{"CRC Handbook", "crc-104", "", 2023, LicenceType::AcademicOnly, false, 0.1, 0.92};

    AuditedValue v_nist{"thermal_conductivity", "401.0", "W/(m*K)", 401.0, "W/(m*K)", 1.0, nist, "2026-11-07"};
    AuditedValue v_crc{"thermal_conductivity", "398.0", "W/(m*K)", 398.0, "W/(m*K)", 1.0, crc, "2026-11-07"};

    auto reg_conflict = db.register_conflict("Cu", "thermal_conductivity", {v_nist, v_crc});
    REQUIRE(reg_conflict.is_ok());
    auto conflict_id = reg_conflict.value();

    REQUIRE(db.conflict_count() == 1);
    auto pending = db.get_pending_conflicts();
    REQUIRE(pending.size() == 1);
    REQUIRE(pending[0].element_symbol == "Cu");
    REQUIRE(pending[0].property_name == "thermal_conductivity");
    REQUIRE(pending[0].candidate_values.size() == 2);
    REQUIRE(pending[0].resolution_state == ConflictResolutionState::PendingReview);

    // Resolve conflict by accepting NIST source
    auto resolve_res = db.resolve_conflict(
        conflict_id,
        ConflictResolutionState::AcceptedSource,
        v_nist,
        "NIST SRD measurement selected due to higher confidence and public domain license"
    );
    REQUIRE(resolve_res.is_ok());

    REQUIRE(db.get_pending_conflicts().empty());
    const auto* rec = db.get_conflict(conflict_id);
    REQUIRE(rec != nullptr);
    REQUIRE(rec->resolution_state == ConflictResolutionState::AcceptedSource);
    REQUIRE(rec->resolved_value.has_value());
    REQUIRE(rec->resolved_value->normalized_value == 401.0);

    // Element audited field updated
    const auto* cu = db.get_element("Cu");
    REQUIRE(cu != nullptr);
    REQUIRE(cu->audited_fields.find("thermal_conductivity") != cu->audited_fields.end());
    REQUIRE(cu->audited_fields.at("thermal_conductivity").normalized_value == 401.0);
}

TEST_CASE("Dataset releases and immutable version snapshots (MATDB-07)") {
    PeriodicTableDatabase db;
    db.seed_representative_elements();

    base::Version v1{1, 0, 0};
    auto release = DatasetRelease::create(v1, "IUPAC-2026-R1", db, "Initial official element dataset release");

    REQUIRE(release.version() == v1);
    REQUIRE(release.release_tag() == "IUPAC-2026-R1");
    REQUIRE(release.is_locked() == true);
    REQUIRE(!release.content_hash().empty());
    REQUIRE(release.verify_integrity() == true);

    // Unpack database from snapshot
    auto unpack_res = release.unpack_database();
    REQUIRE(unpack_res.is_ok());
    auto unpacked_db = unpack_res.value();
    REQUIRE(unpacked_db.element_count() == db.element_count());
    REQUIRE(unpacked_db.has_element("Cu"));
    REQUIRE(unpacked_db.has_element("U"));

    // Release JSON serialization roundtrip
    std::string rel_json = release.to_json();
    auto rel_rt = DatasetRelease::from_json(rel_json);
    REQUIRE(rel_rt.is_ok());
    REQUIRE(rel_rt.value().release_tag() == "IUPAC-2026-R1");
    REQUIRE(rel_rt.value().verify_integrity() == true);
}

TEST_CASE("Update diff reports across dataset versions (MATDB-08)") {
    PeriodicTableDatabase old_db;
    old_db.seed_representative_elements();

    PeriodicTableDatabase new_db = old_db;

    // Modify a property in new_db
    PeriodicElement cu_mod = *new_db.get_element("Cu");
    cu_mod.standard_atomic_weight = 63.550; // updated measurement
    cu_mod.density_at_stp = 8965.0;         // density refined
    new_db.add_element(cu_mod);

    // Add a new element
    PeriodicElement silver;
    silver.atomic_number = 47;
    silver.symbol = "Ag";
    silver.name = "Silver";
    silver.period = 5;
    silver.group = 11;
    silver.block = 'd';
    silver.standard_atomic_weight = 107.8682;
    new_db.add_element(silver);

    auto diff = compute_dataset_diff(old_db, new_db, base::Version(1, 0, 0), base::Version(1, 1, 0));
    REQUIRE(diff.has_changes());
    REQUIRE(diff.added_elements.size() == 1);
    REQUIRE(diff.added_elements[0] == "Ag");
    REQUIRE(diff.modified_properties.size() >= 2);

    REQUIRE(diff.impacted_domains.count("Materials") > 0);
    REQUIRE(diff.impacted_domains.count("Mechanical") > 0);

    std::string summary = diff.generate_summary();
    REQUIRE(!summary.empty());
}

TEST_CASE("Missing-data reports by property and domain requirement (MATDB-09)") {
    PeriodicTableDatabase db;
    db.seed_representative_elements();

    const auto* cu = db.get_element("Cu");
    REQUIRE(cu != nullptr);

    auto reqs = standard_domain_requirements();
    auto rep_cu = evaluate_missing_data(*cu, reqs);

    // Copper has atomic_mass, density, thermal_conductivity, specific_heat, melting_point, electrical_conductivity
    REQUIRE(rep_cu.overall_completeness_pct > 50.0);
    REQUIRE(rep_cu.is_fully_satisfied("Thermal") == true);
    REQUIRE(rep_cu.is_fully_satisfied("Nuclear") == true);

    std::string rep_text = rep_cu.generate_report_text();
    REQUIRE(!rep_text.empty());

    // Element with missing mechanical data
    const auto* he = db.get_element("He");
    REQUIRE(he != nullptr);
    auto rep_he = evaluate_missing_data(*he, reqs);
    REQUIRE(!rep_he.is_fully_satisfied("Structural/Mechanical"));
    REQUIRE(!rep_he.missing_properties_by_domain["Structural/Mechanical"].empty());
}

TEST_CASE("Regression fixtures for representative elements and extreme-condition curves (MATDB-10)") {
    PeriodicTableDatabase db;
    db.seed_representative_elements();

    REQUIRE(db.element_count() == 8);

    // 1. Hydrogen (Z=1, diatomic gas, s-block)
    const auto* h = db.get_element("H");
    REQUIRE(h != nullptr);
    REQUIRE(h->atomic_number == 1);
    REQUIRE(h->block == 's');
    REQUIRE(h->has_isotope(1));
    REQUIRE(h->has_isotope(2));
    REQUIRE(h->has_isotope(3));

    // 2. Helium (Z=2, noble gas, low boiling point)
    const auto* he = db.get_element(2);
    REQUIRE(he != nullptr);
    REQUIRE(he->symbol == "He");
    REQUIRE(he->boiling_point_k.has_value());
    REQUIRE(std::abs(*he->boiling_point_k - 4.22) < 0.1);

    // 3. Carbon (Z=6, polyatomic non-metal)
    const auto* c = db.get_element("C");
    REQUIRE(c != nullptr);
    REQUIRE(c->atomic_number == 6);
    REQUIRE(c->block == 'p');
    REQUIRE(c->isotopes.size() == 3);

    // 4. Silicon (Z=14, metalloid semiconductor)
    const auto* si = db.get_element("Si");
    REQUIRE(si != nullptr);
    REQUIRE(si->electrical_conductivity.has_value());

    // 5. Iron (Z=26, ferromagnetic transition metal)
    const auto* fe = db.get_element("Fe");
    REQUIRE(fe != nullptr);
    REQUIRE(fe->density_at_stp.has_value());
    REQUIRE(*fe->density_at_stp > 7000.0);

    // 6. Copper (Z=29, electrical conductor + cryogenic thermal curve)
    const auto* cu = db.get_element("Cu");
    REQUIRE(cu != nullptr);
    REQUIRE(cu->curves.find("thermal_conductivity_vs_T") != cu->curves.end());
    const auto& k_curve = cu->curves.at("thermal_conductivity_vs_T");
    // Verify cryogenic peak at 10 K (k ~ 16,000 W/(m*K))
    auto k_10 = k_curve.evaluate(10.0);
    REQUIRE(k_10.is_ok());
    REQUIRE(std::abs(k_10.value() - 16000.0) < 1.0);
    // Verify room temperature (300 K, k ~ 401 W/(m*K))
    auto k_300 = k_curve.evaluate(300.0);
    REQUIRE(k_300.is_ok());
    REQUIRE(std::abs(k_300.value() - 401.0) < 1.0);

    // 7. Gold (Z=79, noble metal)
    const auto* au = db.get_element("Au");
    REQUIRE(au != nullptr);
    REQUIRE(au->atomic_number == 79);
    REQUIRE(au->density_at_stp.has_value());
    REQUIRE(std::abs(*au->density_at_stp - 19300.0) < 1.0);

    // 8. Uranium (Z=92, actinide, f-block radioactive)
    const auto* u = db.get_element("U");
    REQUIRE(u != nullptr);
    REQUIRE(u->atomic_number == 92);
    REQUIRE(u->block == 'f');
    REQUIRE(u->has_isotope(235));
    REQUIRE(u->has_isotope(238));
    const auto* u235 = u->get_isotope(235);
    REQUIRE(u235 != nullptr);
    REQUIRE(u235->is_stable == false);
    REQUIRE(u235->half_life_seconds.has_value());
}
