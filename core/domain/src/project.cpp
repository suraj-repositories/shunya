#include "shunya/core/domain/project.hpp"

#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <regex>

namespace shunya::core::domain {

namespace {

std::string get_current_iso_time() {
    auto now = std::chrono::system_clock::now();
    auto time_t_val = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    localtime_s(&tm_buf, &time_t_val);
#else
    localtime_r(&time_t_val, &tm_buf);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

std::string extract_json_string_value(std::string_view json, std::string_view key) {
    std::string pattern = "\"" + std::string(key) + "\"\\s*:\\s*\"([^\"]*)\"";
    std::regex re(pattern);
    std::cmatch match;
    const char* start = json.data();
    if (std::regex_search(start, start + json.size(), match, re)) {
        return match[1].str();
    }
    return "";
}

} // namespace

std::optional<FidelityTier> fidelity_tier_from_string(std::string_view s) noexcept {
    if (s == "L0_Visual" || s == "L0") return FidelityTier::L0_Visual;
    if (s == "L1_Functional" || s == "L1") return FidelityTier::L1_Functional;
    if (s == "L2_EngineeringApproximation" || s == "L2") return FidelityTier::L2_EngineeringApproximation;
    if (s == "L3_HighFidelity" || s == "L3") return FidelityTier::L3_HighFidelity;
    if (s == "L4_ResearchCertified" || s == "L4") return FidelityTier::L4_ResearchCertified;
    return std::nullopt;
}

std::string ProjectManifest::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schema_version\": \"" << escape_json(schema_version.as_string()) << "\",\n";
    ss << "  \"id\": \"" << id.to_string() << "\",\n";
    ss << "  \"name\": \"" << escape_json(name) << "\",\n";
    ss << "  \"description\": \"" << escape_json(description) << "\",\n";
    ss << "  \"author\": \"" << escape_json(author) << "\",\n";
    ss << "  \"created_at\": \"" << escape_json(created_at) << "\",\n";
    ss << "  \"updated_at\": \"" << escape_json(updated_at) << "\",\n";
    ss << "  \"target_fidelity\": \"" << fidelity_tier_to_string(target_fidelity) << "\",\n";

    // Dependencies array
    ss << "  \"dependencies\": [\n";
    for (std::size_t i = 0; i < dependencies.size(); ++i) {
        const auto& dep = dependencies[i];
        ss << "    {\"name\": \"" << escape_json(dep.name)
           << "\", \"version\": \"" << escape_json(dep.version)
           << "\", \"source_uri\": \"" << escape_json(dep.source_uri) << "\"}";
        if (i + 1 < dependencies.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";

    // Metadata object
    ss << "  \"metadata\": {\n";
    std::size_t meta_idx = 0;
    for (const auto& [k, v] : metadata) {
        ss << "    \"" << escape_json(k) << "\": \"" << escape_json(v) << "\"";
        if (++meta_idx < metadata.size()) ss << ",";
        ss << "\n";
    }
    ss << "  }\n";
    ss << "}\n";

    return ss.str();
}

base::Result<ProjectManifest> ProjectManifest::from_json(std::string_view json_str) {
    if (json_str.empty()) {
        return base::Result<ProjectManifest>::err({
            .code = 2001,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Cannot parse empty project JSON",
            .details = ""
        });
    }

    std::string id_str = extract_json_string_value(json_str, "id");
    auto uuid_opt = base::Uuid::from_string(id_str);
    if (!uuid_opt.has_value()) {
        return base::Result<ProjectManifest>::err({
            .code = 2002,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Project manifest missing or invalid UUID: " + id_str,
            .details = ""
        });
    }

    ProjectManifest pm;
    pm.id = uuid_opt.value();
    pm.name = extract_json_string_value(json_str, "name");
    pm.description = extract_json_string_value(json_str, "description");
    pm.author = extract_json_string_value(json_str, "author");
    pm.created_at = extract_json_string_value(json_str, "created_at");
    pm.updated_at = extract_json_string_value(json_str, "updated_at");

    std::string ver_str = extract_json_string_value(json_str, "schema_version");
    if (!ver_str.empty()) {
        auto ver_res = base::Version::parse(ver_str);
        if (ver_res.is_ok()) {
            pm.schema_version = ver_res.value();
        }
    }

    std::string fid_str = extract_json_string_value(json_str, "target_fidelity");
    auto fid_opt = fidelity_tier_from_string(fid_str);
    if (fid_opt.has_value()) {
        pm.target_fidelity = fid_opt.value();
    }

    // Dependencies extraction
    static const std::regex dep_regex(R"re(\{\s*"name"\s*:\s*"([^"]*)"\s*,\s*"version"\s*:\s*"([^"]*)"\s*,\s*"source_uri"\s*:\s*"([^"]*)"\s*\})re");
    std::string json_s(json_str);
    auto words_begin = std::sregex_iterator(json_s.begin(), json_s.end(), dep_regex);
    auto words_end = std::sregex_iterator();
    for (std::sregex_iterator it = words_begin; it != words_end; ++it) {
        std::smatch match = *it;
        pm.dependencies.push_back({match[1].str(), match[2].str(), match[3].str()});
    }

    // Metadata extraction
    auto meta_pos = json_s.find("\"metadata\"");
    if (meta_pos != std::string::npos) {
        auto brace_open = json_s.find('{', meta_pos);
        auto brace_close = json_s.find('}', brace_open);
        if (brace_open != std::string::npos && brace_close != std::string::npos) {
            std::string meta_block = json_s.substr(brace_open, brace_close - brace_open + 1);
            static const std::regex kv_regex(R"re("([^"]+)"\s*:\s*"([^"]*)")re");
            auto kv_begin = std::sregex_iterator(meta_block.begin(), meta_block.end(), kv_regex);
            for (std::sregex_iterator it = kv_begin; it != words_end; ++it) {
                std::smatch m = *it;
                pm.metadata[m[1].str()] = m[2].str();
            }
        }
    }

    return pm;
}

Project::Project(ProjectManifest manifest, std::string file_path)
    : manifest_(std::move(manifest)), file_path_(std::move(file_path)) {}

base::Result<std::shared_ptr<Project>> Project::create_new(std::string name, std::string author) {
    if (name.empty()) {
        return base::Result<std::shared_ptr<Project>>::err({
            .code = 2010,
            .domain = base::ErrorDomain::System,
            .severity = base::Severity::Error,
            .message = "Project name cannot be empty",
            .details = ""
        });
    }

    ProjectManifest manifest;
    manifest.id = base::Uuid::generate_random();
    manifest.name = std::move(name);
    manifest.author = std::move(author);
    manifest.created_at = get_current_iso_time();
    manifest.updated_at = manifest.created_at;
    manifest.target_fidelity = FidelityTier::L2_EngineeringApproximation;

    auto project = std::make_shared<Project>(std::move(manifest));
    project->mark_dirty();
    return project;
}

base::Result<std::shared_ptr<Project>> Project::open_from_file(const std::string& file_path) {
    if (!std::filesystem::exists(file_path)) {
        return base::Result<std::shared_ptr<Project>>::err({
            .code = 2011,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Project file does not exist: " + file_path,
            .details = ""
        });
    }

    std::ifstream file(file_path, std::ios::in);
    if (!file.is_open()) {
        return base::Result<std::shared_ptr<Project>>::err({
            .code = 2012,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Failed to open project file for reading: " + file_path,
            .details = ""
        });
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    file.close();

    auto parse_res = ProjectManifest::from_json(ss.str());
    if (parse_res.is_err()) {
        return parse_res.error();
    }

    auto project = std::make_shared<Project>(parse_res.value(), file_path);
    project->clear_dirty();
    return project;
}

base::Result<void> Project::save_to_file(const std::string& file_path, bool atomic) {
    if (is_read_only_) {
        return base::Result<void>::err({
            .code = 2013,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Cannot save project: file is open in read-only mode",
            .details = ""
        });
    }

    std::string target_path = file_path.empty() ? file_path_ : file_path;
    if (target_path.empty()) {
        return base::Result<void>::err({
            .code = 2014,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "No target file path specified for saving project",
            .details = ""
        });
    }

    manifest_.updated_at = get_current_iso_time();
    std::string json_data = manifest_.to_json();

    if (atomic) {
        std::string tmp_path = target_path + ".tmp";
        std::ofstream tmp_file(tmp_path, std::ios::out | std::ios::trunc);
        if (!tmp_file.is_open()) {
            return base::Result<void>::err({
                .code = 2015,
                .domain = base::ErrorDomain::IO,
                .severity = base::Severity::Error,
                .message = "Failed to write temporary atomic project file: " + tmp_path,
                .details = ""
            });
        }
        tmp_file << json_data;
        tmp_file.flush();
        tmp_file.close();

        std::error_code ec;
#if defined(_WIN32)
        if (std::filesystem::exists(target_path)) {
            std::filesystem::remove(target_path, ec);
        }
#endif
        std::filesystem::rename(tmp_path, target_path, ec);
        if (ec) {
            return base::Result<void>::err({
                .code = 2016,
                .domain = base::ErrorDomain::IO,
                .severity = base::Severity::Error,
                .message = "Atomic rename failed: " + ec.message(),
                .details = ""
            });
        }
    } else {
        std::ofstream file(target_path, std::ios::out | std::ios::trunc);
        if (!file.is_open()) {
            return base::Result<void>::err({
                .code = 2017,
                .domain = base::ErrorDomain::IO,
                .severity = base::Severity::Error,
                .message = "Failed to open project file for writing: " + target_path,
                .details = ""
            });
        }
        file << json_data;
        file.flush();
        file.close();
    }

    file_path_ = target_path;
    clear_dirty();
    return base::Result<void>::ok();
}

} // namespace shunya::core::domain
