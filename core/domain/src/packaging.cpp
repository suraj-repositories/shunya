#include "shunya/core/domain/packaging.hpp"

#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <iomanip>

namespace shunya::core::domain {

namespace {

uint32_t compute_crc32(std::string_view data) {
    uint32_t crc = 0xFFFFFFFF;
    for (unsigned char b : data) {
        crc ^= b;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

std::string get_iso_now() {
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

} // namespace

std::string PackageManifest::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"package_id\": \"" << package_id.to_string() << "\",\n";
    ss << "  \"project_id\": \"" << project_id.to_string() << "\",\n";
    ss << "  \"project_name\": \"" << escape_json(project_name) << "\",\n";
    ss << "  \"created_at\": \"" << escape_json(created_at) << "\",\n";
    ss << "  \"format_version\": \"" << format_version.as_string() << "\",\n";
    ss << "  \"entries\": [\n";

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        ss << "    {\n";
        ss << "      \"path\": \"" << escape_json(e.relative_path) << "\",\n";
        ss << "      \"type\": \"" << package_entry_type_to_string(e.type) << "\",\n";
        ss << "      \"size\": " << e.size_bytes << ",\n";
        ss << "      \"checksum\": " << e.checksum << "\n";
        ss << "    }";
        if (i + 1 < entries.size()) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

base::Result<PackageManifest> PackageManifest::from_json(std::string_view json_str) {
    if (json_str.empty()) {
        return base::Result<PackageManifest>::err({
            .code = 2501,
            .domain = base::ErrorDomain::IO,
            .severity = base::Severity::Error,
            .message = "Empty package manifest json",
            .details = ""
        });
    }

    std::string s(json_str);
    static const std::regex pkg_id_re(R"re("package_id"\s*:\s*"([0-9a-fA-F-]+)")re");
    static const std::regex prj_id_re(R"re("project_id"\s*:\s*"([0-9a-fA-F-]+)")re");
    static const std::regex name_re(R"re("project_name"\s*:\s*"([^"]*)")re");
    static const std::regex date_re(R"re("created_at"\s*:\s*"([^"]*)")re");

    std::smatch pkg_m, prj_m, name_m, date_m;
    if (!std::regex_search(s, pkg_m, pkg_id_re) || !std::regex_search(s, prj_m, prj_id_re)) {
        return base::Result<PackageManifest>::err({
            .code = 2502,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Package manifest missing required UUIDs",
            .details = ""
        });
    }

    PackageManifest pm;
    pm.package_id = base::Uuid::from_string(pkg_m[1].str()).value();
    pm.project_id = base::Uuid::from_string(prj_m[1].str()).value();
    if (std::regex_search(s, name_m, name_re)) pm.project_name = name_m[1].str();
    if (std::regex_search(s, date_m, date_re)) pm.created_at = date_m[1].str();

    // Parse entries
    static const std::regex entry_re(R"re(\{\s*"path"\s*:\s*"([^"]*)"\s*,\s*"type"\s*:\s*"([^"]*)"\s*,\s*"size"\s*:\s*([0-9]+)\s*,\s*"checksum"\s*:\s*([0-9]+)\s*\})re");
    auto it_begin = std::sregex_iterator(s.begin(), s.end(), entry_re);
    auto it_end = std::sregex_iterator();

    for (auto it = it_begin; it != it_end; ++it) {
        std::smatch m = *it;
        PackageEntry entry;
        entry.relative_path = m[1].str();
        std::string type_str = m[2].str();
        if (type_str == "Manifest") entry.type = PackageEntryType::Manifest;
        else if (type_str == "Scene") entry.type = PackageEntryType::Scene;
        else if (type_str == "Script") entry.type = PackageEntryType::Script;
        else if (type_str == "Result") entry.type = PackageEntryType::Result;
        else entry.type = PackageEntryType::Asset;

        entry.size_bytes = std::stoull(m[3].str());
        entry.checksum = std::stoul(m[4].str());
        pm.entries.push_back(std::move(entry));
    }

    return pm;
}

base::Result<PackageManifest> ProjectPackager::create_package(
    const Project& project,
    const SceneGraph& scene_graph,
    const std::filesystem::path& asset_source_dir,
    const std::filesystem::path& output_package_dir,
    const PackagingOptions& options
) {
    std::error_code ec;
    std::filesystem::create_directories(output_package_dir, ec);
    std::filesystem::create_directories(output_package_dir / "assets", ec);
    std::filesystem::create_directories(output_package_dir / "scripts", ec);

    PackageManifest manifest;
    manifest.package_id = base::Uuid::generate_random();
    manifest.project_id = project.manifest().id;
    manifest.project_name = project.manifest().name;
    manifest.created_at = get_iso_now();

    // 1. Write project.json
    std::string project_json = project.manifest().to_json();
    std::filesystem::path project_path = output_package_dir / "project.json";
    {
        std::ofstream p_out(project_path, std::ios::out | std::ios::binary | std::ios::trunc);
        p_out.write(project_json.data(), project_json.size());
    }
    manifest.entries.push_back({
        .relative_path = "project.json",
        .type = PackageEntryType::Manifest,
        .size_bytes = project_json.size(),
        .checksum = compute_crc32(project_json)
    });

    // 2. Write scene.json
    std::string scene_json = scene_graph.to_json();
    std::filesystem::path scene_path = output_package_dir / "scene.json";
    {
        std::ofstream s_out(scene_path, std::ios::out | std::ios::binary | std::ios::trunc);
        s_out.write(scene_json.data(), scene_json.size());
    }
    manifest.entries.push_back({
        .relative_path = "scene.json",
        .type = PackageEntryType::Scene,
        .size_bytes = scene_json.size(),
        .checksum = compute_crc32(scene_json)
    });

    // 3. Collect asset and script files
    std::unordered_set<std::string> copied_assets;
    scene_graph.traverse_depth_first([&](const std::shared_ptr<Entity>& entity) {
        for (const auto& [key, uri_or_path] : entity->domain_attachments()) {
            if (uri_or_path.empty()) continue;
            if (uri_or_path.starts_with("http://") ||
                uri_or_path.starts_with("https://") ||
                uri_or_path.starts_with("shunya://")) {
                continue;
            }

            std::filesystem::path src = asset_source_dir / uri_or_path;
            if (!std::filesystem::exists(src)) {
                src = uri_or_path;
            }

            if (std::filesystem::exists(src) && !std::filesystem::is_directory(src)) {
                std::string fname = src.filename().string();
                if (copied_assets.insert(fname).second) {
                    bool is_script = (key == "script_file" || fname.ends_with(".py") || fname.ends_with(".lua"));
                    std::string folder = is_script ? "scripts" : "assets";
                    std::filesystem::path dest = output_package_dir / folder / fname;

                    std::filesystem::copy_file(src, dest, std::filesystem::copy_options::overwrite_existing, ec);
                    if (!ec) {
                        std::ifstream in(dest, std::ios::in | std::ios::binary);
                        std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                        manifest.entries.push_back({
                            .relative_path = folder + "/" + fname,
                            .type = is_script ? PackageEntryType::Script : PackageEntryType::Asset,
                            .size_bytes = data.size(),
                            .checksum = compute_crc32(data)
                        });
                    }
                }
            }
        }
    });

    // 4. Write package_manifest.json
    std::string manifest_json = manifest.to_json();
    std::filesystem::path manifest_path = output_package_dir / "package_manifest.json";
    {
        std::ofstream m_out(manifest_path, std::ios::out | std::ios::binary | std::ios::trunc);
        m_out.write(manifest_json.data(), manifest_json.size());
    }

    // 5. Verification if requested
    if (options.verify_after_packaging) {
        auto report = verify_package(output_package_dir);
        if (!report.is_valid()) {
            return base::Result<PackageManifest>::err({
                .code = 2503,
                .domain = base::ErrorDomain::Validation,
                .severity = base::Severity::Error,
                .message = "Package self-verification failed after creation",
                .details = report.issues().empty() ? "" : report.issues()[0].message
            });
        }
    }

    return manifest;
}

base::ValidationReport ProjectPackager::verify_package(
    const std::filesystem::path& package_dir
) const {
    base::ValidationReport report;
    std::filesystem::path manifest_path = package_dir / "package_manifest.json";

    if (!std::filesystem::exists(manifest_path)) {
        report.add_error("package_manifest.json", "Missing package_manifest.json in package", 2510);
        return report;
    }

    std::ifstream in(manifest_path);
    std::ostringstream ss;
    ss << in.rdbuf();
    in.close();

    auto parse_res = PackageManifest::from_json(ss.str());
    if (parse_res.is_err()) {
        report.add_error("package_manifest.json", "Failed to parse package manifest: " + parse_res.error().message, 2511);
        return report;
    }

    const auto& manifest = parse_res.value();
    for (const auto& entry : manifest.entries) {
        std::filesystem::path p = package_dir / entry.relative_path;
        if (!std::filesystem::exists(p)) {
            report.add_error(entry.relative_path, "Package entry file not found on disk: " + entry.relative_path, 2512);
            continue;
        }

        std::ifstream file_in(p, std::ios::in | std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(file_in)), std::istreambuf_iterator<char>());
        file_in.close();

        if (data.size() != entry.size_bytes) {
            report.add_error(entry.relative_path, "Size mismatch for package entry: expected " +
                std::to_string(entry.size_bytes) + ", got " + std::to_string(data.size()), 2513);
        }

        uint32_t actual_crc = compute_crc32(data);
        if (actual_crc != entry.checksum) {
            report.add_error(entry.relative_path, "Checksum corruption detected for entry: " + entry.relative_path, 2514);
        }
    }

    return report;
}

base::Result<std::pair<std::shared_ptr<Project>, std::unique_ptr<SceneGraph>>> ProjectPackager::unpack_and_load(
    const std::filesystem::path& package_dir
) {
    auto verify_rep = verify_package(package_dir);
    if (!verify_rep.is_valid()) {
        return base::Result<std::pair<std::shared_ptr<Project>, std::unique_ptr<SceneGraph>>>::err({
            .code = 2520,
            .domain = base::ErrorDomain::Validation,
            .severity = base::Severity::Error,
            .message = "Cannot unpack package: package validation failed",
            .details = verify_rep.issues().empty() ? "" : verify_rep.issues()[0].message
        });
    }

    std::filesystem::path project_path = package_dir / "project.json";
    auto proj_res = Project::open_from_file(project_path.string());
    if (proj_res.is_err()) {
        return proj_res.error();
    }

    std::filesystem::path scene_path = package_dir / "scene.json";
    std::ifstream s_in(scene_path);
    std::ostringstream ss;
    ss << s_in.rdbuf();
    s_in.close();

    auto scene_res = SceneGraph::from_json(ss.str());
    if (scene_res.is_err()) {
        return scene_res.error();
    }

    return std::make_pair(proj_res.value(), std::move(scene_res.value()));
}

} // namespace shunya::core::domain
