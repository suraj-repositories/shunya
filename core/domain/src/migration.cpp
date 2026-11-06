#include "shunya/core/domain/migration.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <regex>

namespace shunya::core::domain {

ProjectMigrator::ProjectMigrator() {
    register_step(std::make_shared<MigrationStep_0_0_1_to_0_1_0>());
}

void ProjectMigrator::register_step(std::shared_ptr<IMigrationStep> step) {
    if (step) {
        steps_.push_back(std::move(step));
    }
}

base::Result<base::Version> ProjectMigrator::extract_schema_version(std::string_view json_str) {
    static const std::regex ver_regex(R"re("schema_version"\s*:\s*"([^"]*)")re");
    std::string s(json_str);
    std::smatch match;
    if (std::regex_search(s, match, ver_regex)) {
        return base::Version::parse(match[1].str());
    }

    // Default legacy schema version if missing
    return base::Version(0, 0, 1, "alpha");
}

bool ProjectMigrator::can_migrate(const base::Version& from, const base::Version& to) const {
    return create_plan(from, to).is_valid;
}

MigrationPlan ProjectMigrator::create_plan(const base::Version& from, const base::Version& to) const {
    MigrationPlan plan;
    plan.from_version = from;
    plan.to_version = to;

    if (from == to) {
        plan.is_valid = true;
        return plan;
    }

    base::Version curr = from;
    std::vector<std::string> descriptions;

    // Greedy path finding across sequential migration steps
    while (curr != to) {
        bool found_next = false;
        for (const auto& step : steps_) {
            if (step->source_version() == curr) {
                descriptions.push_back(std::string(step->description()));
                curr = step->target_version();
                found_next = true;
                break;
            }
        }
        if (!found_next) {
            plan.is_valid = false;
            return plan;
        }
    }

    plan.step_descriptions = std::move(descriptions);
    plan.is_valid = true;
    return plan;
}

base::Result<std::string> ProjectMigrator::preview_migration(
    std::string_view json_content,
    const base::Version& target_version
) const {
    auto current_ver_res = extract_schema_version(json_content);
    if (current_ver_res.is_err()) {
        return current_ver_res.error();
    }

    base::Version current_ver = current_ver_res.value();
    if (current_ver == target_version) {
        return std::string(json_content);
    }

    auto plan = create_plan(current_ver, target_version);
    if (!plan.is_valid) {
        return base::Result<std::string>::err({
            .code = 2301,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "No migration path available from " + current_ver.as_string() + " to " + target_version.as_string(),
            .details = ""
        });
    }

    std::string working_json(json_content);
    base::Version curr = current_ver;

    while (curr != target_version) {
        std::shared_ptr<IMigrationStep> next_step = nullptr;
        for (const auto& step : steps_) {
            if (step->source_version() == curr) {
                next_step = step;
                break;
            }
        }

        if (!next_step) {
            return base::Result<std::string>::err({
                .code = 2301,
                .domain = base::ErrorDomain::System,
                .severity = base::Severity::Error,
                .message = "Migration chain broken at " + curr.as_string(),
                .details = ""
            });
        }

        auto step_res = next_step->migrate(working_json);
        if (step_res.is_err()) {
            return step_res;
        }

        working_json = step_res.value();
        curr = next_step->target_version();
    }

    return working_json;
}

base::Result<void> ProjectMigrator::migrate_file_with_backup(
    const std::string& file_path,
    const base::Version& target_version,
    bool keep_backup_on_success
) {
    if (!std::filesystem::exists(file_path)) {
        return base::Result<void>::err({
            .code = 2302,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "File to migrate does not exist: " + file_path,
            .details = ""
        });
    }

    // Read original file
    std::ifstream in(file_path, std::ios::in);
    if (!in.is_open()) {
        return base::Result<void>::err({
            .code = 2304,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Failed to open file for migration: " + file_path,
            .details = ""
        });
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    in.close();

    std::string original_content = ss.str();

    // Create backup file
    std::string backup_path = file_path + ".bak";
    std::error_code ec;
    std::filesystem::copy_file(file_path, backup_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        return base::Result<void>::err({
            .code = 2305,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Failed to create migration backup file: " + ec.message(),
            .details = ""
        });
    }

    // Preview / perform migration in-memory
    auto migrated_res = preview_migration(original_content, target_version);
    if (migrated_res.is_err()) {
        return migrated_res.error();
    }

    // Atomic write migrated content
    std::string tmp_path = file_path + ".migrated.tmp";
    std::ofstream out(tmp_path, std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return base::Result<void>::err({
            .code = 2306,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Failed to create temporary migration file: " + tmp_path,
            .details = ""
        });
    }
    out << migrated_res.value();
    out.flush();
    out.close();

#if defined(_WIN32)
    if (std::filesystem::exists(file_path)) {
        std::filesystem::remove(file_path, ec);
    }
#endif
    std::filesystem::rename(tmp_path, file_path, ec);
    if (ec) {
        // Rollback from backup if rename failed
        [[maybe_unused]] auto rb_res = rollback_from_backup(file_path);
        return base::Result<void>::err({
            .code = 2307,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Atomic replacement failed during migration: " + ec.message(),
            .details = ""
        });
    }

    if (!keep_backup_on_success) {
        std::filesystem::remove(backup_path, ec);
    }

    return base::Result<void>::ok();
}

base::Result<void> ProjectMigrator::rollback_from_backup(const std::string& file_path) {
    std::string backup_path = file_path + ".bak";
    if (!std::filesystem::exists(backup_path)) {
        return base::Result<void>::err({
            .code = 2303,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Backup file not found for rollback: " + backup_path,
            .details = ""
        });
    }

    std::error_code ec;
#if defined(_WIN32)
    if (std::filesystem::exists(file_path)) {
        std::filesystem::remove(file_path, ec);
    }
#endif
    std::filesystem::copy_file(backup_path, file_path, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        return base::Result<void>::err({
            .code = 2308,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Failed to restore file from backup: " + ec.message(),
            .details = ""
        });
    }

    return base::Result<void>::ok();
}

base::Result<std::string> MigrationStep_0_0_1_to_0_1_0::migrate(std::string_view source_json) {
    std::string json(source_json);

    // Inject or update schema_version
    static const std::regex schema_re(R"re("schema_version"\s*:\s*"[^"]*")re");
    if (std::regex_search(json, schema_re)) {
        json = std::regex_replace(json, schema_re, "\"schema_version\": \"0.1.0-dev\"");
    } else {
        auto open_brace = json.find('{');
        if (open_brace != std::string::npos) {
            json.insert(open_brace + 1, "\n  \"schema_version\": \"0.1.0-dev\",");
        }
    }

    // Inject target_fidelity if missing
    static const std::regex fid_re(R"re("target_fidelity"\s*:)re");
    if (!std::regex_search(json, fid_re)) {
        auto schema_pos = json.find("\"schema_version\": \"0.1.0-dev\",");
        if (schema_pos != std::string::npos) {
            json.insert(schema_pos + 31, "\n  \"target_fidelity\": \"L2_EngineeringApproximation\",");
        }
    }

    return json;
}

} // namespace shunya::core::domain
