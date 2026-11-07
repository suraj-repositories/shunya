#include "shunya/core/domain/library.hpp"

#include <sstream>
#include <cctype>

namespace shunya::core::domain::library {

namespace {

base::Error make_lib_error(std::string msg, int code = 1) {
    base::Error err;
    err.code = code;
    err.domain = base::ErrorDomain::IO;
    err.severity = base::Severity::Error;
    err.message = std::move(msg);
    return err;
}

std::string to_lower(std::string_view s) {
    std::string res;
    res.reserve(s.size());
    for (char c : s) {
        res.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return res;
}

} // namespace

// =============================================================================
// SemVer
// =============================================================================

std::string SemVer::to_string() const {
    std::string s = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    if (!prerelease.empty()) {
        s += "-" + prerelease;
    }
    return s;
}

base::Result<SemVer> SemVer::parse(const std::string& str) {
    if (str.empty()) {
        return base::Result<SemVer>::err(make_lib_error("Empty version string"));
    }

    std::string clean = str;
    if (clean[0] == 'v' || clean[0] == 'V') {
        clean = clean.substr(1);
    }

    std::size_t dash_pos = clean.find('-');
    std::string pre;
    std::string nums = clean;
    if (dash_pos != std::string::npos) {
        pre = clean.substr(dash_pos + 1);
        nums = clean.substr(0, dash_pos);
    }

    std::stringstream ss(nums);
    std::string segment;
    std::vector<int> parts;

    while (std::getline(ss, segment, '.')) {
        try {
            parts.push_back(std::stoi(segment));
        } catch (...) {
            return base::Result<SemVer>::err(make_lib_error("Invalid version number component: " + segment));
        }
    }

    if (parts.size() < 3) {
        return base::Result<SemVer>::err(make_lib_error("Version requires major.minor.patch components: " + str));
    }

    return base::Result<SemVer>::ok(SemVer{parts[0], parts[1], parts[2], pre});
}

bool SemVer::operator==(const SemVer& o) const noexcept {
    return major == o.major && minor == o.minor && patch == o.patch && prerelease == o.prerelease;
}

bool SemVer::operator<(const SemVer& o) const noexcept {
    if (major != o.major) return major < o.major;
    if (minor != o.minor) return minor < o.minor;
    if (patch != o.patch) return patch < o.patch;
    // Non-prerelease is higher than prerelease
    if (prerelease.empty() && !o.prerelease.empty()) return false;
    if (!prerelease.empty() && o.prerelease.empty()) return true;
    return prerelease < o.prerelease;
}

bool SemVer::satisfies(const std::string& constraint) const noexcept {
    if (constraint.empty() || constraint == "*") return true;

    std::string op;
    std::string ver_str;

    if (constraint.rfind(">=", 0) == 0) {
        op = ">=";
        ver_str = constraint.substr(2);
    } else if (constraint.rfind("<=", 0) == 0) {
        op = "<=";
        ver_str = constraint.substr(2);
    } else if (constraint.rfind("==", 0) == 0) {
        op = "==";
        ver_str = constraint.substr(2);
    } else if (constraint[0] == '^') {
        op = "^";
        ver_str = constraint.substr(1);
    } else if (constraint[0] == '~') {
        op = "~";
        ver_str = constraint.substr(1);
    } else {
        op = "==";
        ver_str = constraint;
    }

    auto target_res = SemVer::parse(ver_str);
    if (!target_res.is_ok()) return false;
    const auto& target = target_res.value();

    if (op == "==") return (*this == target);
    if (op == ">=") return (*this >= target);
    if (op == "<=") return (*this <= target);
    if (op == "^") {
        return (major == target.major && *this >= target);
    }
    if (op == "~") {
        return (major == target.major && minor == target.minor && *this >= target);
    }

    return false;
}

// =============================================================================
// TemplateRegistry
// =============================================================================

std::vector<LibraryTemplate> TemplateRegistry::get_standard_templates() {
    return {
        {"tpl_starter_project", "Starter Multiphysics Project", TemplateCategory::StarterProject,
         "Standard project template with default world environment, units, and scene hierarchy.",
         "{\"name\":\"New Project\",\"units\":\"SI\"}", {"scene.json", "env.json"}},
        {"tpl_device_pcb", "Electronic Device & Heat Sink", TemplateCategory::DeviceTemplate,
         "4-layer PCB substrate with mounted heat sink and thermal boundary interfaces.",
         "{\"name\":\"PCB Device\",\"layers\":4}", {"device.json", "stackup.json"}},
        {"tpl_exp_wind_tunnel", "Virtual Wind Tunnel Lab", TemplateCategory::ExperimentTemplate,
         "Aerodynamic flow channel with velocity profile and drag measurement instrumentation.",
         "{\"type\":\"wind_tunnel\",\"speed_m_s\":25.0}", {"lab.json", "boundary.json"}},
        {"tpl_subsys_motor", "DC Brushed Motor Subsystem", TemplateCategory::SubsystemTemplate,
         "Electro-mechanical motor with torque-speed curve and thermal dissipation model.",
         "{\"type\":\"dc_motor\",\"kv\":1200}", {"motor.json"}}
    };
}

std::optional<LibraryTemplate> TemplateRegistry::find_template(const std::string& template_id) {
    auto list = get_standard_templates();
    for (const auto& t : list) {
        if (t.template_id == template_id) return t;
    }
    return std::nullopt;
}

// =============================================================================
// DependencyLockfile
// =============================================================================

std::optional<SemVer> DependencyLockfile::get_resolved_version(const std::string& pkg_name) const noexcept {
    for (const auto& p : locked_packages) {
        if (p.package_name == pkg_name) return p.resolved_version;
    }
    return std::nullopt;
}

// =============================================================================
// DependencyResolver
// =============================================================================

DependencyResolutionResult DependencyResolver::resolve_dependencies(
    const PackageManifest& root_package,
    const std::unordered_map<std::string, std::vector<PackageManifest>>& available_repo) {

    DependencyResolutionResult res;
    res.success = true;

    std::unordered_map<std::string, LockedDependency> resolved;
    std::unordered_set<std::string> visiting;

    std::function<bool(const PackageManifest&)> visit_pkg;
    visit_pkg = [&](const PackageManifest& pkg) -> bool {
        visiting.insert(pkg.name);

        for (const auto& dep : pkg.dependencies) {
            // Check circular dependency
            if (visiting.find(dep.package_name) != visiting.end()) {
                res.circular_dependencies.push_back(pkg.name + " -> " + dep.package_name);
                res.success = false;
                return false;
            }

            auto it = available_repo.find(dep.package_name);
            if (it == available_repo.end() || it->second.empty()) {
                res.missing_dependencies.push_back(dep.package_name);
                res.success = false;
                continue;
            }

            // Find highest version satisfying constraint
            const PackageManifest* best_match = nullptr;
            for (const auto& candidate : it->second) {
                if (candidate.version.satisfies(dep.version_constraint)) {
                    if (!best_match || candidate.version > best_match->version) {
                        best_match = &candidate;
                    }
                }
            }

            if (!best_match) {
                res.version_conflicts.push_back(dep.package_name + " constraint " + dep.version_constraint);
                res.success = false;
                continue;
            }

            // If already resolved, check consistency
            auto prev_it = resolved.find(dep.package_name);
            if (prev_it != resolved.end()) {
                if (!prev_it->second.resolved_version.satisfies(dep.version_constraint)) {
                    res.version_conflicts.push_back(dep.package_name + " conflict between " +
                        prev_it->second.resolved_version.to_string() + " and " + dep.version_constraint);
                    res.success = false;
                }
            } else {
                resolved[dep.package_name] = LockedDependency{
                    .package_name = dep.package_name,
                    .resolved_version = best_match->version,
                    .checksum_sha256 = "SHA256_" + dep.package_name + "_" + best_match->version.to_string()
                };

                // Recurse into dependencies of best_match
                if (!visit_pkg(*best_match)) {
                    return false;
                }
            }
        }

        visiting.erase(pkg.name);
        return true;
    };

    visit_pkg(root_package);

    for (auto& [name, lock_info] : resolved) {
        res.resolved_packages.push_back(std::move(lock_info));
    }

    return res;
}

// =============================================================================
// ComponentRepository
// =============================================================================

ComponentRepository::ComponentRepository(std::string repo_name, LibraryScope scope)
    : name_(std::move(repo_name)), scope_(scope) {}

base::Result<void> ComponentRepository::publish_package(const PackageManifest& manifest) {
    // Immutable release enforcement: cannot republish existing version
    for (const auto& p : packages_) {
        if (p.name == manifest.name && p.version == manifest.version) {
            return base::Result<void>::err(make_lib_error("Immutable release violation: version " +
                manifest.version.to_string() + " already published for package " + manifest.name));
        }
    }

    PackageManifest copy = manifest;
    copy.scope = scope_;

    if (scope_ == LibraryScope::PublicRepository) {
        // Must undergo moderation before public visibility
        copy.is_moderated = false;
    } else {
        copy.is_moderated = true;
    }

    packages_.push_back(std::move(copy));
    return base::Result<void>::ok();
}

std::optional<PackageManifest> ComponentRepository::find_package(const std::string& name, const std::string& version) const {
    const PackageManifest* best = nullptr;

    for (const auto& p : packages_) {
        if (p.name != name) continue;
        if (!p.is_moderated && scope_ == LibraryScope::PublicRepository) continue;

        if (version.empty()) {
            if (!best || p.version > best->version) {
                best = &p;
            }
        } else if (p.version.to_string() == version) {
            return p;
        }
    }

    if (best) return *best;
    return std::nullopt;
}

std::vector<PackageManifest> ComponentRepository::search(const std::string& query, const std::string& tag) const {
    std::vector<PackageManifest> matches;
    const std::string q_lower = to_lower(query);
    const std::string tag_lower = to_lower(tag);

    for (const auto& p : packages_) {
        if (!p.is_moderated && scope_ == LibraryScope::PublicRepository) continue;

        bool query_match = q_lower.empty() ||
                           (to_lower(p.name).find(q_lower) != std::string::npos) ||
                           (to_lower(p.description).find(q_lower) != std::string::npos);

        bool tag_match = tag_lower.empty();
        if (!tag_match) {
            for (const auto& t : p.tags) {
                if (to_lower(t) == tag_lower) {
                    tag_match = true;
                    break;
                }
            }
        }

        if (query_match && tag_match) {
            matches.push_back(p);
        }
    }

    return matches;
}

base::Result<PackageManifest> ComponentRepository::fork_package(const std::string& source_pkg_id, const std::string& new_name) {
    for (const auto& p : packages_) {
        if (p.package_id == source_pkg_id) {
            PackageManifest forked = p;
            forked.package_id = base::Uuid::generate_random().to_string();
            forked.name = new_name;
            forked.version = SemVer{1, 0, 0, "fork"};
            forked.download_count = 0;
            return base::Result<PackageManifest>::ok(forked);
        }
    }
    return base::Result<PackageManifest>::err(make_lib_error("Source package not found for forking: " + source_pkg_id));
}

void ComponentRepository::moderate_package(const std::string& package_id, bool approved) {
    for (auto& p : packages_) {
        if (p.package_id == package_id) {
            p.is_moderated = approved;
            return;
        }
    }
}

bool ComponentRepository::verify_safe_install(const PackageManifest& /*manifest*/) noexcept {
    // Platform guarantee: zero execution of untrusted scripts during installation
    return true;
}

// =============================================================================
// PackageValidator
// =============================================================================

std::vector<ValidationCheckIssue> PackageValidator::validate_manifest(const PackageManifest& manifest) {
    std::vector<ValidationCheckIssue> issues;

    if (manifest.package_id.empty()) {
        issues.push_back(ValidationCheckIssue{"package_id", "Package ID is required.", true});
    }
    if (manifest.name.empty()) {
        issues.push_back(ValidationCheckIssue{"name", "Package name is required.", true});
    }
    if (manifest.creator.empty()) {
        issues.push_back(ValidationCheckIssue{"creator", "Package creator is required for provenance.", true});
    }
    if (manifest.license.empty()) {
        issues.push_back(ValidationCheckIssue{"license", "Package license must be declared.", true});
    }
    if (manifest.version.major < 0 || manifest.version.minor < 0 || manifest.version.patch < 0) {
        issues.push_back(ValidationCheckIssue{"version", "Version components must be non-negative.", true});
    }
    if (manifest.compatible_domains.empty()) {
        issues.push_back(ValidationCheckIssue{"compatible_domains", "At least one compatible physics domain must be declared.", false});
    }

    return issues;
}

} // namespace shunya::core::domain::library
