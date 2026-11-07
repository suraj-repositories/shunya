#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"

#include <vector>
#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>
#include <algorithm>

namespace shunya::core::domain::library {

// =============================================================================
// LIB-07: Semantic Versioning (SemVer)
// =============================================================================

struct SemVer {
    int major = 1;
    int minor = 0;
    int patch = 0;
    std::string prerelease;

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] static base::Result<SemVer> parse(const std::string& str);

    [[nodiscard]] bool operator==(const SemVer& o) const noexcept;
    [[nodiscard]] bool operator<(const SemVer& o) const noexcept;
    [[nodiscard]] bool operator<=(const SemVer& o) const noexcept { return (*this < o) || (*this == o); }
    [[nodiscard]] bool operator>(const SemVer& o) const noexcept { return !(*this <= o); }
    [[nodiscard]] bool operator>=(const SemVer& o) const noexcept { return !(*this < o); }

    /// Checks if this version satisfies a simple constraint (e.g. "^1.2.0", ">=1.0.0", "==2.1.0")
    [[nodiscard]] bool satisfies(const std::string& constraint) const noexcept;
};

// =============================================================================
// LIB-01 & LIB-12: Package Manifest & Provenance
// =============================================================================

enum class ValidationStatus {
    Unverified,
    Benchmarked,
    CertifiedL4
};

enum class LibraryScope {
    ProjectLocal,
    UserPersonal,
    Organization,
    PublicRepository
};

struct DependencyRequirement {
    std::string package_name;
    std::string version_constraint; // e.g. "^1.0.0"
};

struct PackageManifest {
    std::string package_id;
    std::string name;
    SemVer version{1, 0, 0, ""};
    std::string creator;
    std::string license = "MIT";
    std::string description;
    std::vector<std::string> tags;
    std::vector<std::string> compatible_domains; // "electronics", "thermal", "mechanics"
    std::vector<DependencyRequirement> dependencies;
    LibraryScope scope = LibraryScope::ProjectLocal;
    ValidationStatus validation_status = ValidationStatus::Unverified;
    bool is_moderated = false;
    double rating = 5.0; // 1.0 to 5.0
    uint32_t download_count = 0;
};

// =============================================================================
// LIB-04: Templates (Starter Project, Device, Experiment, Subsystem)
// =============================================================================

enum class TemplateCategory {
    StarterProject,
    DeviceTemplate,
    ExperimentTemplate,
    SubsystemTemplate
};

struct LibraryTemplate {
    std::string template_id;
    std::string name;
    TemplateCategory category = TemplateCategory::StarterProject;
    std::string description;
    std::string default_manifest_json;
    std::vector<std::string> included_files;
};

class TemplateRegistry {
public:
    [[nodiscard]] static std::vector<LibraryTemplate> get_standard_templates();
    [[nodiscard]] static std::optional<LibraryTemplate> find_template(const std::string& template_id);
};

// =============================================================================
// LIB-08 & LIB-09: Dependency Graph, Lockfiles & Resolution
// =============================================================================

struct LockedDependency {
    std::string package_name;
    SemVer resolved_version{1, 0, 0, ""};
    std::string checksum_sha256;
};

struct DependencyLockfile {
    std::string lockfile_version = "1.0";
    std::vector<LockedDependency> locked_packages;

    [[nodiscard]] std::optional<SemVer> get_resolved_version(const std::string& pkg_name) const noexcept;
};

struct DependencyResolutionResult {
    bool success = true;
    std::vector<LockedDependency> resolved_packages;
    std::vector<std::string> missing_dependencies;
    std::vector<std::string> circular_dependencies;
    std::vector<std::string> version_conflicts;
};

class DependencyResolver {
public:
    /// Resolves dependency graph across an available repository
    [[nodiscard]] static DependencyResolutionResult resolve_dependencies(
        const PackageManifest& root_package,
        const std::unordered_map<std::string, std::vector<PackageManifest>>& available_repo);
};

// =============================================================================
// LIB-11: Component Catalog & Repository Manager
// =============================================================================

class ComponentRepository {
public:
    ComponentRepository(std::string repo_name, LibraryScope scope);

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] LibraryScope scope() const noexcept { return scope_; }

    [[nodiscard]] base::Result<void> publish_package(const PackageManifest& manifest);
    [[nodiscard]] std::optional<PackageManifest> find_package(const std::string& name, const std::string& version = "") const;

    [[nodiscard]] std::vector<PackageManifest> search(const std::string& query, const std::string& tag = "") const;
    [[nodiscard]] const std::vector<PackageManifest>& all_packages() const noexcept { return packages_; }

    [[nodiscard]] base::Result<PackageManifest> fork_package(const std::string& source_pkg_id, const std::string& new_name);

    // LIB-06: Moderation for public repository
    void moderate_package(const std::string& package_id, bool approved);

    // LIB-13: Safe install verification (checks no automatic post-install scripts)
    [[nodiscard]] static bool verify_safe_install(const PackageManifest& manifest) noexcept;

private:
    std::string name_;
    LibraryScope scope_;
    std::vector<PackageManifest> packages_;
};

// =============================================================================
// LIB-14: Package Validator & Quality Suite
// =============================================================================

struct ValidationCheckIssue {
    std::string field;
    std::string message;
    bool is_fatal = true;
};

class PackageValidator {
public:
    [[nodiscard]] static std::vector<ValidationCheckIssue> validate_manifest(const PackageManifest& manifest);
};

} // namespace shunya::core::domain::library
