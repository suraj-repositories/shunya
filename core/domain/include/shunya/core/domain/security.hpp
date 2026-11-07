#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"

#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <filesystem>

namespace shunya::core::domain::security {

// =============================================================================
// SEC-01: Threat Modeling & Security Risk Taxonomy
// =============================================================================

enum class ThreatCategory {
    ExecutionEscalation,
    ResourceExhaustion,
    PathTraversal,
    UntrustedNativeCode,
    DataExfiltration,
    ProjectCorruption
};

enum class RiskSeverity {
    Low,
    Medium,
    High,
    Critical
};

struct SecurityThreat {
    std::string threat_id;
    ThreatCategory category;
    RiskSeverity severity;
    std::string description;
    std::string mitigation;
};

class ThreatModelRegistry {
public:
    [[nodiscard]] static std::vector<SecurityThreat> get_platform_threat_model();
    [[nodiscard]] static bool is_action_permitted(ThreatCategory threat, bool sandbox_active, bool signed_extension) noexcept;
};

// =============================================================================
// SEC-02: Security Context & Resource Budgets
// =============================================================================

struct ResourceBudget {
    uint64_t max_cpu_time_ms = 100;
    size_t max_memory_bytes = 2 * 1024 * 1024; // 2 MB
    size_t max_file_write_bytes = 10 * 1024 * 1024; // 10 MB
    uint32_t max_concurrent_threads = 1;
};

class SecuritySandboxMonitor {
public:
    explicit SecuritySandboxMonitor(ResourceBudget budget = {}) : budget_(budget) {}

    void record_memory_alloc(size_t bytes);
    void record_memory_free(size_t bytes) noexcept;
    void record_cpu_time(uint64_t ms) noexcept;

    [[nodiscard]] bool is_budget_exceeded() const noexcept;
    [[nodiscard]] std::string violation_reason() const noexcept;

    [[nodiscard]] size_t current_memory() const noexcept { return current_memory_; }
    [[nodiscard]] uint64_t total_cpu_time() const noexcept { return total_cpu_time_; }
    [[nodiscard]] const ResourceBudget& budget() const noexcept { return budget_; }

    void reset() noexcept;

private:
    ResourceBudget budget_;
    size_t current_memory_ = 0;
    uint64_t total_cpu_time_ = 0;
    bool exceeded_ = false;
    std::string violation_reason_;
};

// =============================================================================
// SEC-03: Native Extension Signing & Trust Verification
// =============================================================================

enum class ExtensionTrustLevel {
    Untrusted,
    CommunitySigned,
    EnterpriseVerified,
    CorePlatform
};

struct ExtensionManifest {
    std::string extension_id;
    std::string name;
    std::string version;
    std::string publisher;
    std::string public_key;
    std::string signature;
    ExtensionTrustLevel trust_level = ExtensionTrustLevel::Untrusted;
};

class ExtensionSignatureVerifier {
public:
    /// Validates extension manifest and checks cryptographic signature against trusted publishers
    [[nodiscard]] static base::Result<ExtensionTrustLevel> verify_extension(
        const ExtensionManifest& manifest, const std::string& binary_content);

    static void register_trusted_publisher(const std::string& publisher, const std::string& public_key);
    static void clear_trusted_publishers() noexcept;

private:
    static std::unordered_map<std::string, std::string> trusted_publishers_;
};

// =============================================================================
// SEC-04: File Import Validator & Path Traversal Sanitizer
// =============================================================================

class PathSanitizer {
public:
    /// Returns true if the relative path contains traversal sequences (e.g. "../", "..\\", absolute drive letters)
    [[nodiscard]] static bool contains_traversal(std::string_view relative_path) noexcept;

    /// Safely resolves a target relative path within sandbox_root. Returns error if escaping sandbox root.
    [[nodiscard]] static base::Result<std::filesystem::path> resolve_safe_path(
        const std::filesystem::path& sandbox_root, std::string_view user_relative_path);

    /// Checks file size quota to defend against decompression bombs
    [[nodiscard]] static bool is_file_size_safe(size_t file_bytes, size_t max_allowed_bytes) noexcept;
};

// =============================================================================
// SEC-05: Separation of Project Data and Executable Code
// =============================================================================

enum class PartitionType {
    ProjectDataReadOnly,
    ProjectDataWritable,
    ExecutableScriptCode,
    NativeBinaryPlugin
};

class PartitionPolicy {
public:
    [[nodiscard]] static bool is_execution_allowed(PartitionType partition) noexcept;
    [[nodiscard]] static bool is_write_allowed(PartitionType partition) noexcept;
};

// =============================================================================
// SEC-06: Multi-Tier Asset Permissions & Access Control (RBAC)
// =============================================================================

enum class AssetScope {
    Private,
    TeamInternal,
    PublicReadOnly
};

enum class UserRole {
    Viewer,
    Contributor,
    Owner,
    Admin
};

enum class AssetAction {
    Read,
    Write,
    Delete,
    Share,
    Execute
};

class AssetAccessController {
public:
    [[nodiscard]] static bool can_perform_action(
        AssetScope scope, UserRole user_role, AssetAction action) noexcept;
};

// =============================================================================
// SEC-07: Crash Recovery, Autosave & Corrupted Project Repair
// =============================================================================

struct ProjectSnapshot {
    uint64_t revision = 0;
    double timestamp = 0.0;
    std::string project_json;
    uint32_t checksum = 0;
};

class AutosaveRecoveryManager {
public:
    explicit AutosaveRecoveryManager(size_t max_snapshots = 3);

    void save_snapshot(uint64_t revision, double timestamp, const std::string& project_json);
    [[nodiscard]] const std::vector<ProjectSnapshot>& snapshots() const noexcept { return snapshots_; }

    [[nodiscard]] std::optional<ProjectSnapshot> get_latest_valid_snapshot() const;
    [[nodiscard]] bool is_manifest_corrupted(const std::string& json_content, uint32_t expected_checksum) const noexcept;

    [[nodiscard]] static uint32_t compute_checksum(const std::string& content) noexcept;

private:
    size_t max_snapshots_;
    std::vector<ProjectSnapshot> snapshots_;
};

// =============================================================================
// SEC-08: Privacy-Safe Opt-In Telemetry & Data Inventory
// =============================================================================

enum class TelemetryConsentLevel {
    Disabled,
    AnonymousDiagnosticsOnly,
    FullTelemetry
};

struct TelemetryEvent {
    std::string event_name;
    double duration_ms = 0.0;
    std::string anonymized_session_id;
    std::unordered_map<std::string, std::string> metadata;
};

class TelemetryCollector {
public:
    explicit TelemetryCollector(TelemetryConsentLevel consent = TelemetryConsentLevel::Disabled);

    void set_consent(TelemetryConsentLevel consent) noexcept { consent_ = consent; }
    [[nodiscard]] TelemetryConsentLevel consent() const noexcept { return consent_; }

    [[nodiscard]] base::Result<void> record_event(const std::string& event_name, double duration_ms, std::unordered_map<std::string, std::string> metadata);

    [[nodiscard]] const std::vector<TelemetryEvent>& events() const noexcept { return events_; }
    void clear() noexcept { events_.clear(); }

    /// Scrubs PII from string (user home paths, emails, hostnames)
    [[nodiscard]] static std::string scrub_pii(const std::string& input);

private:
    TelemetryConsentLevel consent_;
    std::vector<TelemetryEvent> events_;
};

// =============================================================================
// SEC-09: Software Bill of Materials (SBOM) & Vulnerability Scanner
// =============================================================================

struct SbomComponent {
    std::string name;
    std::string version;
    std::string license;
    std::string sha256_hash;
    bool is_vulnerable = false;
    std::string cve_id;
};

class SbomAuditor {
public:
    void register_component(const SbomComponent& component);
    [[nodiscard]] const std::vector<SbomComponent>& components() const noexcept { return components_; }

    [[nodiscard]] std::vector<SbomComponent> find_vulnerabilities() const;
    [[nodiscard]] std::string export_spdx_summary() const;

private:
    std::vector<SbomComponent> components_;
};

// =============================================================================
// SEC-10: Release Signing, Update Channels & Safe Rollback
// =============================================================================

enum class UpdateChannel {
    Stable,
    Beta,
    Nightly
};

struct ReleaseArtifact {
    std::string version;
    UpdateChannel channel = UpdateChannel::Stable;
    std::string release_hash;
    std::string signature;
    bool verified = false;
};

class ReleaseUpdateManager {
public:
    void set_channel(UpdateChannel channel) noexcept { channel_ = channel; }
    [[nodiscard]] UpdateChannel channel() const noexcept { return channel_; }

    [[nodiscard]] base::Result<void> stage_update(const ReleaseArtifact& update);
    [[nodiscard]] base::Result<void> apply_staged_update();
    [[nodiscard]] base::Result<void> rollback_to_previous();

    [[nodiscard]] const std::string& current_version() const noexcept { return current_version_; }
    [[nodiscard]] const std::string& rollback_version() const noexcept { return previous_version_; }

private:
    UpdateChannel channel_ = UpdateChannel::Stable;
    std::string current_version_ = "1.0.0";
    std::string previous_version_ = "0.9.0";
    std::optional<ReleaseArtifact> staged_update_;
};

} // namespace shunya::core::domain::security
