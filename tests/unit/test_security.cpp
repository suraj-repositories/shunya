#include "tests/test_framework.hpp"
#include "shunya/core/domain/security.hpp"

#include <string>
#include <filesystem>

using namespace shunya::core::domain::security;

// =============================================================================
// SEC-01: Threat Modeling & Risk Categorization
// =============================================================================

TEST_CASE("SEC-01: Threat Model Registry and Action Permits") {
    auto threats = ThreatModelRegistry::get_platform_threat_model();
    CHECK(!threats.empty());
    CHECK(threats.size() >= 5u);

    // Verify key categories exist
    bool has_traversal = false;
    bool has_escalation = false;
    for (const auto& t : threats) {
        if (t.category == ThreatCategory::PathTraversal) has_traversal = true;
        if (t.category == ThreatCategory::ExecutionEscalation) has_escalation = true;
    }
    CHECK(has_traversal);
    CHECK(has_escalation);

    // Unsigned extension executing native code must be blocked
    CHECK_FALSE(ThreatModelRegistry::is_action_permitted(ThreatCategory::UntrustedNativeCode, true, false));
    // Signed extension is permitted
    CHECK(ThreatModelRegistry::is_action_permitted(ThreatCategory::UntrustedNativeCode, true, true));
}

// =============================================================================
// SEC-02: Security Sandbox Resource Budgets
// =============================================================================

TEST_CASE("SEC-02: Sandbox Memory and CPU Resource Budgets") {
    ResourceBudget budget;
    budget.max_memory_bytes = 1000; // 1000 bytes
    budget.max_cpu_time_ms = 50;    // 50 ms

    SecuritySandboxMonitor monitor(budget);
    CHECK_FALSE(monitor.is_budget_exceeded());

    monitor.record_memory_alloc(500);
    CHECK(monitor.current_memory() == 500u);
    CHECK_FALSE(monitor.is_budget_exceeded());

    // Exceed memory limit
    monitor.record_memory_alloc(600); // 1100 > 1000
    CHECK(monitor.is_budget_exceeded());
    CHECK(!monitor.violation_reason().empty());

    monitor.reset();
    CHECK_FALSE(monitor.is_budget_exceeded());

    // Exceed CPU time
    monitor.record_cpu_time(60);
    CHECK(monitor.is_budget_exceeded());
}

// =============================================================================
// SEC-03: Native Extension Signing & Trust Verification
// =============================================================================

TEST_CASE("SEC-03: Extension Cryptographic Trust Verification") {
    ExtensionSignatureVerifier::clear_trusted_publishers();
    ExtensionSignatureVerifier::register_trusted_publisher("Shunya Official Core", "PUBKEY_OFFICIAL_123");
    ExtensionSignatureVerifier::register_trusted_publisher("Acme Engineering", "PUBKEY_ACME_456");

    ExtensionManifest core_ext;
    core_ext.extension_id = "fem_solver_pro";
    core_ext.publisher = "Shunya Official Core";
    core_ext.public_key = "PUBKEY_OFFICIAL_123";
    core_ext.signature = "SIG_VALID_HEX";

    auto res1 = ExtensionSignatureVerifier::verify_extension(core_ext, "BINARY_DATA");
    REQUIRE(res1.is_ok());
    CHECK(res1.value() == ExtensionTrustLevel::CorePlatform);

    // Untrusted publisher
    ExtensionManifest unknown_ext;
    unknown_ext.extension_id = "malicious_tool";
    unknown_ext.publisher = "Unknown Hacker";
    unknown_ext.public_key = "PUBKEY_UNKNOWN";
    unknown_ext.signature = "SIG_UNKNOWN";

    auto res2 = ExtensionSignatureVerifier::verify_extension(unknown_ext, "BINARY_DATA");
    REQUIRE(res2.is_ok());
    CHECK(res2.value() == ExtensionTrustLevel::Untrusted);

    // Tampered public key
    ExtensionManifest tampered_ext = core_ext;
    tampered_ext.public_key = "PUBKEY_WRONG_FAKE";
    auto res3 = ExtensionSignatureVerifier::verify_extension(tampered_ext, "BINARY_DATA");
    CHECK(res3.is_err());
}

// =============================================================================
// SEC-04: File Import Validator & Path Traversal Sanitizer
// =============================================================================

TEST_CASE("SEC-04: Path Traversal Defense and Quotas") {
    // 1. Path traversal detection
    CHECK(PathSanitizer::contains_traversal("../../etc/passwd"));
    CHECK(PathSanitizer::contains_traversal("..\\Windows\\System32"));
    CHECK(PathSanitizer::contains_traversal("C:\\secret.key"));
    CHECK(PathSanitizer::contains_traversal("/root/secret"));
    CHECK_FALSE(PathSanitizer::contains_traversal("models/bracket.step"));
    CHECK_FALSE(PathSanitizer::contains_traversal("subfolder/data.json"));

    // 2. Safe path resolution inside sandbox
    std::filesystem::path root = "C:/projects/sandbox";
    auto safe_res = PathSanitizer::resolve_safe_path(root, "assets/texture.png");
    REQUIRE(safe_res.is_ok());
    CHECK(safe_res.value().string().find("texture.png") != std::string::npos);

    auto bad_res = PathSanitizer::resolve_safe_path(root, "../../../escape.txt");
    CHECK(bad_res.is_err());

    // 3. File size quota check (e.g. max 50 MB)
    CHECK(PathSanitizer::is_file_size_safe(10 * 1024 * 1024, 50 * 1024 * 1024));
    CHECK_FALSE(PathSanitizer::is_file_size_safe(100 * 1024 * 1024, 50 * 1024 * 1024));
}

// =============================================================================
// SEC-05: Partition Policy (Data vs Executable)
// =============================================================================

TEST_CASE("SEC-05: Separation of Data and Executable Code") {
    CHECK_FALSE(PartitionPolicy::is_execution_allowed(PartitionType::ProjectDataReadOnly));
    CHECK_FALSE(PartitionPolicy::is_execution_allowed(PartitionType::ProjectDataWritable));
    CHECK(PartitionPolicy::is_execution_allowed(PartitionType::ExecutableScriptCode));
    CHECK(PartitionPolicy::is_execution_allowed(PartitionType::NativeBinaryPlugin));

    CHECK(PartitionPolicy::is_write_allowed(PartitionType::ProjectDataWritable));
    CHECK_FALSE(PartitionPolicy::is_write_allowed(PartitionType::ProjectDataReadOnly));
    CHECK_FALSE(PartitionPolicy::is_write_allowed(PartitionType::ExecutableScriptCode));
}

// =============================================================================
// SEC-06: Multi-Tier Asset Permissions (RBAC)
// =============================================================================

TEST_CASE("SEC-06: Role-Based Asset Access Control") {
    // Public read-only: anyone can read, only owner can write
    CHECK(AssetAccessController::can_perform_action(AssetScope::PublicReadOnly, UserRole::Viewer, AssetAction::Read));
    CHECK_FALSE(AssetAccessController::can_perform_action(AssetScope::PublicReadOnly, UserRole::Viewer, AssetAction::Write));
    CHECK(AssetAccessController::can_perform_action(AssetScope::PublicReadOnly, UserRole::Owner, AssetAction::Write));

    // Team internal: Contributor can write, Viewer cannot write
    CHECK(AssetAccessController::can_perform_action(AssetScope::TeamInternal, UserRole::Contributor, AssetAction::Write));
    CHECK_FALSE(AssetAccessController::can_perform_action(AssetScope::TeamInternal, UserRole::Viewer, AssetAction::Write));

    // Private: only Owner can read/write
    CHECK_FALSE(AssetAccessController::can_perform_action(AssetScope::Private, UserRole::Viewer, AssetAction::Read));
    CHECK(AssetAccessController::can_perform_action(AssetScope::Private, UserRole::Owner, AssetAction::Read));
}

// =============================================================================
// SEC-07: Crash Recovery, Autosave & Manifest Repair
// =============================================================================

TEST_CASE("SEC-07: Autosave Rolling Snapshots and Corruption Recovery") {
    AutosaveRecoveryManager mgr(3);

    mgr.save_snapshot(1, 10.0, "{\"manifest_ver\": 1}");
    mgr.save_snapshot(2, 20.0, "{\"manifest_ver\": 2}");
    mgr.save_snapshot(3, 30.0, "{\"manifest_ver\": 3}");

    CHECK(mgr.snapshots().size() == 3u);

    auto latest = mgr.get_latest_valid_snapshot();
    REQUIRE(latest.has_value());
    CHECK(latest->revision == 3u);

    // Test corruption detection
    const uint32_t good_checksum = AutosaveRecoveryManager::compute_checksum("{\"data\": 42}");
    CHECK_FALSE(mgr.is_manifest_corrupted("{\"data\": 42}", good_checksum));
    CHECK(mgr.is_manifest_corrupted("{\"data\": 999_TAMPERED}", good_checksum));
}

// =============================================================================
// SEC-08: Privacy-Safe Opt-In Telemetry & PII Scrubbing
// =============================================================================

TEST_CASE("SEC-08: Telemetry Consent Gating and PII Scrubbing") {
    TelemetryCollector collector(TelemetryConsentLevel::Disabled);

    // When disabled, recording fails or is dropped
    auto r1 = collector.record_event("UI_START", 12.0, {});
    CHECK(r1.is_err());
    CHECK(collector.events().empty());

    // When opt-in enabled
    collector.set_consent(TelemetryConsentLevel::AnonymousDiagnosticsOnly);
    auto r2 = collector.record_event("SIMULATION_STEP", 45.0, {
        {"error_path", "C:\\Users\\shubham\\Desktop\\secret_model.cad"},
        {"contact_email", "engineer@domain.com"}
    });
    REQUIRE(r2.is_ok());
    CHECK(collector.events().size() == 1u);

    // Verify PII was scrubbed
    const auto& meta = collector.events()[0].metadata;
    CHECK(meta.at("error_path").find("shubham") == std::string::npos);
    CHECK(meta.at("error_path").find("[REDACTED_USER_PATH]") != std::string::npos);
    CHECK(meta.at("contact_email") == "[REDACTED_EMAIL]");
}

// =============================================================================
// SEC-09: Software Bill of Materials (SBOM) & Vulnerability Scanner
// =============================================================================

TEST_CASE("SEC-09: SBOM Generation and Vulnerability Auditing") {
    SbomAuditor auditor;
    auditor.register_component(SbomComponent{"Eigen", "3.4.0", "MPL-2.0", "EIGEN_HASH_123", false, ""});
    auditor.register_component(SbomComponent{"Catch2", "3.4.0", "BSL-1.0", "CATCH2_HASH_456", false, ""});
    auditor.register_component(SbomComponent{"VulnerableLib", "1.0.0", "MIT", "VULN_HASH_789", true, "CVE-2026-9999"});

    CHECK(auditor.components().size() == 3u);

    auto vulns = auditor.find_vulnerabilities();
    CHECK(vulns.size() == 1u);
    CHECK(vulns[0].name == "VulnerableLib");
    CHECK(vulns[0].cve_id == "CVE-2026-9999");

    std::string spdx = auditor.export_spdx_summary();
    CHECK(spdx.find("SPDXVersion: SPDX-2.3") != std::string::npos);
    CHECK(spdx.find("PackageName: Eigen") != std::string::npos);
}

// =============================================================================
// SEC-10: Release Signing, Staging & Rollback Strategy
// =============================================================================

TEST_CASE("SEC-10: Release Updates and Instant Rollback") {
    ReleaseUpdateManager updater;
    CHECK(updater.current_version() == "1.0.0");

    ReleaseArtifact bad_update{"1.1.0", UpdateChannel::Stable, "HASH_110", "SIG", false}; // unverified
    auto r_bad = updater.stage_update(bad_update);
    CHECK(r_bad.is_err());

    ReleaseArtifact good_update{"1.1.0", UpdateChannel::Stable, "HASH_110", "SIG_VERIFIED", true};
    auto r_stage = updater.stage_update(good_update);
    REQUIRE(r_stage.is_ok());

    auto r_apply = updater.apply_staged_update();
    REQUIRE(r_apply.is_ok());
    CHECK(updater.current_version() == "1.1.0");
    CHECK(updater.rollback_version() == "1.0.0");

    // Rollback to previous version
    auto r_rollback = updater.rollback_to_previous();
    REQUIRE(r_rollback.is_ok());
    CHECK(updater.current_version() == "1.0.0");
}
