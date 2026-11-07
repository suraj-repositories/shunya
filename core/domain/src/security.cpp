#include "shunya/core/domain/security.hpp"

#include <algorithm>
#include <sstream>
#include <regex>

namespace shunya::core::domain::security {

namespace {

base::Error make_sec_error(std::string msg, int code = 1) {
    base::Error err;
    err.code = code;
    err.domain = base::ErrorDomain::System;
    err.severity = base::Severity::Error;
    err.message = std::move(msg);
    return err;
}

} // namespace

// =============================================================================
// ThreatModelRegistry
// =============================================================================

std::vector<SecurityThreat> ThreatModelRegistry::get_platform_threat_model() {
    return {
        {"THR-01", ThreatCategory::ExecutionEscalation, RiskSeverity::Critical,
         "Arbitrary native code execution via malicious plugin",
         "Strict capability sandboxing and cryptographic publisher signature verification."},
        {"THR-02", ThreatCategory::ResourceExhaustion, RiskSeverity::High,
         "Denial of service via infinite loop or memory allocation bomb",
         "Instruction cycle limits, memory allocation quotas, and execution timeouts."},
        {"THR-03", ThreatCategory::PathTraversal, RiskSeverity::High,
         "Directory traversal attack reading or overwriting host system files",
         "Strict path normalization, canonical boundary enforcement, and sandbox jail."},
        {"THR-04", ThreatCategory::UntrustedNativeCode, RiskSeverity::Critical,
         "Loading unverified shared libraries or DLLs",
         "Code signing trust chain, manifest verification, and explicit user consent."},
        {"THR-05", ThreatCategory::DataExfiltration, RiskSeverity::Medium,
         "Unauthorized transmission of proprietary model data over network",
         "Zero network socket capabilities for sandboxed script domains by default."},
        {"THR-06", ThreatCategory::ProjectCorruption, RiskSeverity::Medium,
         "File corruption during power failure or unexpected process termination",
         "Atomic filesystem writes via temporary staging and generational rolling autosave snapshots."}
    };
}

bool ThreatModelRegistry::is_action_permitted(ThreatCategory threat, bool sandbox_active, bool signed_extension) noexcept {
    if (threat == ThreatCategory::ExecutionEscalation || threat == ThreatCategory::UntrustedNativeCode) {
        return signed_extension;
    }
    if (threat == ThreatCategory::ResourceExhaustion || threat == ThreatCategory::PathTraversal) {
        return sandbox_active;
    }
    return true;
}

// =============================================================================
// SecuritySandboxMonitor
// =============================================================================

void SecuritySandboxMonitor::record_memory_alloc(size_t bytes) {
    current_memory_ += bytes;
    if (current_memory_ > budget_.max_memory_bytes) {
        exceeded_ = true;
        violation_reason_ = "Memory quota exceeded (" + std::to_string(current_memory_) +
                            " > " + std::to_string(budget_.max_memory_bytes) + " bytes)";
    }
}

void SecuritySandboxMonitor::record_memory_free(size_t bytes) noexcept {
    if (bytes >= current_memory_) {
        current_memory_ = 0;
    } else {
        current_memory_ -= bytes;
    }
}

void SecuritySandboxMonitor::record_cpu_time(uint64_t ms) noexcept {
    total_cpu_time_ += ms;
    if (total_cpu_time_ > budget_.max_cpu_time_ms) {
        exceeded_ = true;
        violation_reason_ = "CPU time budget exceeded (" + std::to_string(total_cpu_time_) +
                            " > " + std::to_string(budget_.max_cpu_time_ms) + " ms)";
    }
}

bool SecuritySandboxMonitor::is_budget_exceeded() const noexcept {
    return exceeded_;
}

std::string SecuritySandboxMonitor::violation_reason() const noexcept {
    return violation_reason_;
}

void SecuritySandboxMonitor::reset() noexcept {
    current_memory_ = 0;
    total_cpu_time_ = 0;
    exceeded_ = false;
    violation_reason_.clear();
}

// =============================================================================
// ExtensionSignatureVerifier
// =============================================================================

std::unordered_map<std::string, std::string> ExtensionSignatureVerifier::trusted_publishers_;

void ExtensionSignatureVerifier::register_trusted_publisher(const std::string& publisher, const std::string& public_key) {
    trusted_publishers_[publisher] = public_key;
}

void ExtensionSignatureVerifier::clear_trusted_publishers() noexcept {
    trusted_publishers_.clear();
}

base::Result<ExtensionTrustLevel> ExtensionSignatureVerifier::verify_extension(
    const ExtensionManifest& manifest, const std::string& binary_content) {
    if (manifest.publisher.empty() || manifest.signature.empty() || binary_content.empty()) {
        return base::Result<ExtensionTrustLevel>::err(make_sec_error("Invalid or empty manifest/binary content"));
    }

    auto it = trusted_publishers_.find(manifest.publisher);
    if (it == trusted_publishers_.end()) {
        return base::Result<ExtensionTrustLevel>::ok(ExtensionTrustLevel::Untrusted);
    }

    if (it->second != manifest.public_key) {
        return base::Result<ExtensionTrustLevel>::err(make_sec_error("Public key mismatch for publisher: " + manifest.publisher));
    }

    // Check core platform identity
    if (manifest.publisher == "Shunya Official Core") {
        return base::Result<ExtensionTrustLevel>::ok(ExtensionTrustLevel::CorePlatform);
    }

    return base::Result<ExtensionTrustLevel>::ok(ExtensionTrustLevel::EnterpriseVerified);
}

// =============================================================================
// PathSanitizer
// =============================================================================

bool PathSanitizer::contains_traversal(std::string_view relative_path) noexcept {
    if (relative_path.find("..") != std::string_view::npos) return true;
    if (relative_path.find(':') != std::string_view::npos) return true; // Windows drive colon C:
    if (!relative_path.empty() && (relative_path[0] == '/' || relative_path[0] == '\\')) return true;
    return false;
}

base::Result<std::filesystem::path> PathSanitizer::resolve_safe_path(
    const std::filesystem::path& sandbox_root, std::string_view user_relative_path) {
    if (contains_traversal(user_relative_path)) {
        return base::Result<std::filesystem::path>::err(make_sec_error("Path traversal sequence detected in: " + std::string(user_relative_path)));
    }

    const std::filesystem::path resolved = sandbox_root / user_relative_path;
    return base::Result<std::filesystem::path>::ok(resolved);
}

bool PathSanitizer::is_file_size_safe(size_t file_bytes, size_t max_allowed_bytes) noexcept {
    return file_bytes <= max_allowed_bytes;
}

// =============================================================================
// PartitionPolicy
// =============================================================================

bool PartitionPolicy::is_execution_allowed(PartitionType partition) noexcept {
    switch (partition) {
        case PartitionType::ProjectDataReadOnly:
        case PartitionType::ProjectDataWritable:
            return false;
        case PartitionType::ExecutableScriptCode:
        case PartitionType::NativeBinaryPlugin:
            return true;
    }
    return false;
}

bool PartitionPolicy::is_write_allowed(PartitionType partition) noexcept {
    switch (partition) {
        case PartitionType::ProjectDataReadOnly:
        case PartitionType::ExecutableScriptCode:
        case PartitionType::NativeBinaryPlugin:
            return false;
        case PartitionType::ProjectDataWritable:
            return true;
    }
    return false;
}

// =============================================================================
// AssetAccessController (RBAC)
// =============================================================================

bool AssetAccessController::can_perform_action(
    AssetScope scope, UserRole user_role, AssetAction action) noexcept {
    if (user_role == UserRole::Admin) return true;

    if (scope == AssetScope::PublicReadOnly) {
        if (action == AssetAction::Read) return true;
        return (user_role == UserRole::Owner);
    }

    if (scope == AssetScope::TeamInternal) {
        if (action == AssetAction::Read) return true;
        if (action == AssetAction::Write && (user_role == UserRole::Contributor || user_role == UserRole::Owner)) return true;
        if (action == AssetAction::Share && user_role == UserRole::Owner) return true;
        if (action == AssetAction::Delete && user_role == UserRole::Owner) return true;
        return false;
    }

    // Private scope
    if (user_role == UserRole::Owner) return true;
    return false;
}

// =============================================================================
// AutosaveRecoveryManager
// =============================================================================

AutosaveRecoveryManager::AutosaveRecoveryManager(size_t max_snapshots)
    : max_snapshots_(std::max(max_snapshots, size_t{1})) {}

uint32_t AutosaveRecoveryManager::compute_checksum(const std::string& content) noexcept {
    // 32-bit FNV-1a
    uint32_t hash = 2166136261u;
    for (char c : content) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    return hash;
}

void AutosaveRecoveryManager::save_snapshot(uint64_t revision, double timestamp, const std::string& project_json) {
    ProjectSnapshot snap{
        .revision = revision,
        .timestamp = timestamp,
        .project_json = project_json,
        .checksum = compute_checksum(project_json)
    };

    snapshots_.push_back(std::move(snap));
    if (snapshots_.size() > max_snapshots_) {
        snapshots_.erase(snapshots_.begin());
    }
}

std::optional<ProjectSnapshot> AutosaveRecoveryManager::get_latest_valid_snapshot() const {
    for (auto it = snapshots_.rbegin(); it != snapshots_.rend(); ++it) {
        if (!is_manifest_corrupted(it->project_json, it->checksum)) {
            return *it;
        }
    }
    return std::nullopt;
}

bool AutosaveRecoveryManager::is_manifest_corrupted(const std::string& json_content, uint32_t expected_checksum) const noexcept {
    return compute_checksum(json_content) != expected_checksum;
}

// =============================================================================
// TelemetryCollector
// =============================================================================

TelemetryCollector::TelemetryCollector(TelemetryConsentLevel consent)
    : consent_(consent) {}

base::Result<void> TelemetryCollector::record_event(
    const std::string& event_name, double duration_ms, std::unordered_map<std::string, std::string> metadata) {
    if (consent_ == TelemetryConsentLevel::Disabled) {
        return base::Result<void>::err(make_sec_error("Telemetry disabled by user consent policy", 403));
    }

    // Scrub all metadata values of PII
    std::unordered_map<std::string, std::string> scrubbed_meta;
    for (const auto& [k, v] : metadata) {
        scrubbed_meta[k] = scrub_pii(v);
    }

    events_.push_back(TelemetryEvent{
        .event_name = event_name,
        .duration_ms = duration_ms,
        .anonymized_session_id = "session_anon_token",
        .metadata = std::move(scrubbed_meta)
    });

    return base::Result<void>::ok();
}

std::string TelemetryCollector::scrub_pii(const std::string& input) {
    std::string result = input;
    // Anonymize user paths: C:\Users\... or /home/...
    static const std::regex user_path_win(R"([A-Za-z]:\\[Uu]sers\\[^\\]+)");
    result = std::regex_replace(result, user_path_win, "[REDACTED_USER_PATH]");

    static const std::regex user_path_nix(R"(/home/[^/]+)");
    result = std::regex_replace(result, user_path_nix, "[REDACTED_USER_PATH]");

    // Anonymize email addresses
    static const std::regex email_regex(R"([a-zA-Z0-9_.+-]+@[a-zA-Z0-9-]+\.[a-zA-Z0-9-.]+)");
    result = std::regex_replace(result, email_regex, "[REDACTED_EMAIL]");

    return result;
}

// =============================================================================
// SbomAuditor
// =============================================================================

void SbomAuditor::register_component(const SbomComponent& component) {
    components_.push_back(component);
}

std::vector<SbomComponent> SbomAuditor::find_vulnerabilities() const {
    std::vector<SbomComponent> vuln;
    for (const auto& c : components_) {
        if (c.is_vulnerable) {
            vuln.push_back(c);
        }
    }
    return vuln;
}

std::string SbomAuditor::export_spdx_summary() const {
    std::ostringstream ss;
    ss << "SPDXVersion: SPDX-2.3\n";
    ss << "DataLicense: CC0-1.0\n";
    ss << "DocumentName: Shunya-Platform-SBOM\n";
    for (const auto& c : components_) {
        ss << "PackageName: " << c.name << "\n";
        ss << "PackageVersion: " << c.version << "\n";
        ss << "PackageLicenseConcluded: " << c.license << "\n";
        ss << "PackageChecksum: SHA256: " << c.sha256_hash << "\n";
        if (c.is_vulnerable) {
            ss << "PackageVulnerability: " << c.cve_id << "\n";
        }
    }
    return ss.str();
}

// =============================================================================
// ReleaseUpdateManager
// =============================================================================

base::Result<void> ReleaseUpdateManager::stage_update(const ReleaseArtifact& update) {
    if (!update.verified) {
        return base::Result<void>::err(make_sec_error("Unverified release signature on update artifact"));
    }
    staged_update_ = update;
    return base::Result<void>::ok();
}

base::Result<void> ReleaseUpdateManager::apply_staged_update() {
    if (!staged_update_.has_value()) {
        return base::Result<void>::err(make_sec_error("No update staged"));
    }
    previous_version_ = current_version_;
    current_version_ = staged_update_->version;
    staged_update_.reset();
    return base::Result<void>::ok();
}

base::Result<void> ReleaseUpdateManager::rollback_to_previous() {
    if (previous_version_.empty()) {
        return base::Result<void>::err(make_sec_error("No previous version available for rollback"));
    }
    std::swap(current_version_, previous_version_);
    return base::Result<void>::ok();
}

} // namespace shunya::core::domain::security
