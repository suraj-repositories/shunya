#include "tests/test_framework.hpp"
#include "shunya/core/domain/library.hpp"

#include <string>
#include <vector>

using namespace shunya::core::domain::library;

// =============================================================================
// LIB-01 & LIB-12: Package Manifest & Provenance
// =============================================================================

TEST_CASE("LIB-01 & LIB-12: Package Manifest Structure and Provenance") {
    PackageManifest manifest;
    manifest.package_id = "pkg_diode_schottky";
    manifest.name = "SchottkyDiodeModel";
    manifest.version = SemVer{2, 1, 0, ""};
    manifest.creator = "Dr. Alice Smith";
    manifest.license = "Apache-2.0";
    manifest.description = "Calibrated thermal-electrical Schottky diode model";
    manifest.tags = {"semiconductor", "diode", "spice"};
    manifest.compatible_domains = {"electronics", "thermal"};
    manifest.validation_status = ValidationStatus::CertifiedL4;

    CHECK(manifest.package_id == "pkg_diode_schottky");
    CHECK(manifest.name == "SchottkyDiodeModel");
    CHECK(manifest.version.to_string() == "2.1.0");
    CHECK(manifest.validation_status == ValidationStatus::CertifiedL4);
    CHECK(manifest.compatible_domains.size() == 2u);
}

// =============================================================================
// LIB-07: Semantic Versioning (SemVer) & Immutability
// =============================================================================

TEST_CASE("LIB-07: SemVer Parsing, Ordering, and Constraints") {
    auto v1_res = SemVer::parse("1.2.3");
    REQUIRE(v1_res.is_ok());
    CHECK(v1_res.value().major == 1);
    CHECK(v1_res.value().minor == 2);
    CHECK(v1_res.value().patch == 3);

    auto v2_res = SemVer::parse("2.0.0-alpha.1");
    REQUIRE(v2_res.is_ok());
    CHECK(v2_res.value().major == 2);
    CHECK(v2_res.value().prerelease == "alpha.1");

    // Ordering
    CHECK(v1_res.value() < v2_res.value());
    CHECK(v2_res.value() > v1_res.value());

    // Constraints
    SemVer v124{1, 2, 4, ""};
    CHECK(v124.satisfies("^1.2.0"));
    CHECK(v124.satisfies("~1.2.0"));
    CHECK(v124.satisfies(">=1.0.0"));
    CHECK_FALSE(v124.satisfies("^2.0.0"));
}

// =============================================================================
// LIB-02, LIB-03, LIB-05, LIB-06: Scopes & Moderation
// =============================================================================

TEST_CASE("LIB-02, LIB-03, LIB-05, LIB-06: Library Scopes, Moderation and Immutability") {
    ComponentRepository repo("PublicCommunityRepo", LibraryScope::PublicRepository);
    CHECK(repo.scope() == LibraryScope::PublicRepository);

    PackageManifest pkg;
    pkg.package_id = "pkg_filter";
    pkg.name = "BesselFilter";
    pkg.version = SemVer{1, 0, 0, ""};
    pkg.creator = "Community Contributor";
    pkg.license = "MIT";
    pkg.description = "4th order Bessel low-pass filter";
    pkg.compatible_domains = {"electronics"};

    auto pub_res = repo.publish_package(pkg);
    REQUIRE(pub_res.is_ok());

    // In public repo, package is unmoderated initially, hence not returned by default search
    CHECK(repo.search("Bessel").empty());

    // Moderate and approve
    repo.moderate_package("pkg_filter", true);
    auto matches = repo.search("Bessel");
    CHECK(matches.size() == 1u);
    CHECK(matches[0].name == "BesselFilter");

    // Immutability check: republishing same version must fail
    auto repub_res = repo.publish_package(pkg);
    CHECK(repub_res.is_err());
}

// =============================================================================
// LIB-04: Templates (Starter, Device, Experiment, Subsystem)
// =============================================================================

TEST_CASE("LIB-04: Standard Engineering Templates") {
    auto templates = TemplateRegistry::get_standard_templates();
    CHECK(templates.size() >= 4u);

    auto tpl_starter = TemplateRegistry::find_template("tpl_starter_project");
    REQUIRE(tpl_starter.has_value());
    CHECK(tpl_starter->category == TemplateCategory::StarterProject);
    CHECK(!tpl_starter->included_files.empty());

    auto tpl_motor = TemplateRegistry::find_template("tpl_subsys_motor");
    REQUIRE(tpl_motor.has_value());
    CHECK(tpl_motor->category == TemplateCategory::SubsystemTemplate);
}

// =============================================================================
// LIB-08 & LIB-09: Dependency Resolution & Cycle Detection
// =============================================================================

TEST_CASE("LIB-08 & LIB-09: Dependency Graph Resolution and Cycle Detection") {
    std::unordered_map<std::string, std::vector<PackageManifest>> repo;

    // Package B v1.0.0 and v1.2.0
    PackageManifest pkg_b1;
    pkg_b1.name = "LibB";
    pkg_b1.version = SemVer{1, 0, 0, ""};
    PackageManifest pkg_b2;
    pkg_b2.name = "LibB";
    pkg_b2.version = SemVer{1, 2, 0, ""};
    repo["LibB"] = {pkg_b1, pkg_b2};

    // Root package A depends on LibB ^1.0.0
    PackageManifest root_a;
    root_a.name = "RootA";
    root_a.version = SemVer{1, 0, 0, ""};
    root_a.dependencies.push_back(DependencyRequirement{"LibB", "^1.0.0"});

    auto res = DependencyResolver::resolve_dependencies(root_a, repo);
    CHECK(res.success);
    REQUIRE(res.resolved_packages.size() == 1u);
    CHECK(res.resolved_packages[0].package_name == "LibB");
    SemVer expected_v{1, 2, 0, ""};
    CHECK(res.resolved_packages[0].resolved_version == expected_v); // resolved highest compatible

    // Test Circular Dependency Detection: X -> Y -> X
    PackageManifest pkg_x;
    pkg_x.name = "PkgX";
    pkg_x.dependencies.push_back(DependencyRequirement{"PkgY", ">=1.0.0"});

    PackageManifest pkg_y;
    pkg_y.name = "PkgY";
    pkg_y.dependencies.push_back(DependencyRequirement{"PkgX", ">=1.0.0"});

    std::unordered_map<std::string, std::vector<PackageManifest>> cycle_repo;
    cycle_repo["PkgX"] = {pkg_x};
    cycle_repo["PkgY"] = {pkg_y};

    auto cycle_res = DependencyResolver::resolve_dependencies(pkg_x, cycle_repo);
    CHECK_FALSE(cycle_res.success);
    CHECK(!cycle_res.circular_dependencies.empty());
}

// =============================================================================
// LIB-10 & LIB-11: Forking, Search, and Catalog
// =============================================================================

TEST_CASE("LIB-10 & LIB-11: Forking and Search Filtering") {
    ComponentRepository repo("PersonalRepo", LibraryScope::UserPersonal);

    PackageManifest base_pkg;
    base_pkg.package_id = "pkg_motor_base";
    base_pkg.name = "BrushedMotor";
    base_pkg.version = SemVer{1, 0, 0, ""};
    base_pkg.creator = "Bob";
    base_pkg.license = "MIT";
    base_pkg.description = "Base brushed motor model";
    base_pkg.tags = {"actuator", "motor", "electromechanical"};
    base_pkg.compatible_domains = {"electronics", "mechanics"};
    auto pub_res = repo.publish_package(base_pkg);
    CHECK(pub_res.is_ok());

    // Fork package
    auto fork_res = repo.fork_package("pkg_motor_base", "HighTorqueMotor");
    REQUIRE(fork_res.is_ok());
    CHECK(fork_res.value().name == "HighTorqueMotor");
    CHECK(fork_res.value().version.prerelease == "fork");

    // Search by tag
    auto tag_results = repo.search("", "motor");
    CHECK(tag_results.size() == 1u);
    CHECK(tag_results[0].name == "BrushedMotor");
}

// =============================================================================
// LIB-13: Safe Install Policy
// =============================================================================

TEST_CASE("LIB-13: Safe Install Policy Enforcement") {
    PackageManifest p;
    p.name = "SafeComponent";
    CHECK(ComponentRepository::verify_safe_install(p));
}

// =============================================================================
// LIB-14: Package Validator
// =============================================================================

TEST_CASE("LIB-14: Package Validation Suite") {
    PackageManifest valid_pkg;
    valid_pkg.package_id = "pkg_valid";
    valid_pkg.name = "ValidComponent";
    valid_pkg.creator = "Alice";
    valid_pkg.license = "BSD-3-Clause";
    valid_pkg.compatible_domains = {"thermal"};

    auto issues = PackageValidator::validate_manifest(valid_pkg);
    CHECK(issues.empty());

    PackageManifest invalid_pkg; // missing id, name, creator, domains
    auto bad_issues = PackageValidator::validate_manifest(invalid_pkg);
    CHECK(bad_issues.size() >= 4u);
}
