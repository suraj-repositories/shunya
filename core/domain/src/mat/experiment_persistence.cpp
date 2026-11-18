#include "shunya/mat/experiment_persistence.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::mat {

namespace {

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            } else {
                out += c;
            }
        }
    }
    return out;
}

std::string sanitize_str(std::string_view in, size_t max_len = ExperimentPersistence::MAX_STRING_LENGTH) {
    std::string s;
    s.reserve(std::min(in.size(), max_len));
    for (char c : in) {
        if (s.size() >= max_len) break;
        // Block control characters and dangerous script characters
        if (c >= 32 && c <= 126) {
            s.push_back(c);
        } else if (c == '\n' || c == '\t' || c == '\r') {
            s.push_back(' ');
        }
    }
    return s;
}

std::string json_str_val(std::string_view json, std::string_view key, std::string_view def = "") {
    std::string pattern = "\"" + std::string(key) + "\"\\s*:\\s*\"([^\"\n\r\t\\\\]*)\"";
    std::regex re(pattern);
    std::cmatch match;
    if (std::regex_search(json.data(), json.data() + json.size(), match, re)) {
        return match[1].str();
    }
    return std::string(def);
}

[[maybe_unused]] double json_num_val(std::string_view json, std::string_view key, double def = 0.0) {
    std::string pattern = "\"" + std::string(key) + "\"\\s*:\\s*([-+]?[0-9]*\\.?[0-9]+([eE][-+]?[0-9]+)?)";
    std::regex re(pattern);
    std::cmatch match;
    if (std::regex_search(json.data(), json.data() + json.size(), match, re)) {
        try {
            return std::stod(match[1].str());
        } catch (...) {
            return def;
        }
    }
    return def;
}

[[maybe_unused]] bool json_bool_val(std::string_view json, std::string_view key, bool def = false) {
    std::string pattern = "\"" + std::string(key) + "\"\\s*:\\s*(true|false)";
    std::regex re(pattern);
    std::cmatch match;
    if (std::regex_search(json.data(), json.data() + json.size(), match, re)) {
        return match[1].str() == "true";
    }
    return def;
}

core::base::Error make_persist_error(int code, std::string msg) {
    core::base::Error err;
    err.code = code;
    err.domain = core::base::ErrorDomain::IO;
    err.severity = core::base::Severity::Error;
    err.message = std::move(msg);
    return err;
}

} // namespace

std::string ExperimentPersistence::current_iso8601_timestamp() noexcept {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

std::string ExperimentPersistence::serialize_to_json(
    const VersionedExperimentBundle& bundle) noexcept {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schemaVersion\": \"" << escape_json(bundle.metadata.schema_version.empty() ? CURRENT_SCHEMA_VERSION : bundle.metadata.schema_version) << "\",\n";
    ss << "  \"applicationVersion\": \"" << escape_json(bundle.metadata.application_version.empty() ? CURRENT_SCHEMA_VERSION : bundle.metadata.application_version) << "\",\n";
    ss << "  \"author\": \"" << escape_json(bundle.metadata.author) << "\",\n";
    ss << "  \"createdAt\": \"" << escape_json(bundle.metadata.created_at.empty() ? current_iso8601_timestamp() : bundle.metadata.created_at) << "\",\n";
    ss << "  \"modifiedAt\": \"" << escape_json(bundle.metadata.modified_at.empty() ? current_iso8601_timestamp() : bundle.metadata.modified_at) << "\",\n";
    ss << "  \"notes\": \"" << escape_json(bundle.metadata.notes) << "\",\n";

    ss << "  \"tags\": [";
    for (size_t i = 0; i < bundle.metadata.tags.size(); ++i) {
        ss << "\"" << escape_json(bundle.metadata.tags[i]) << "\"";
        if (i + 1 < bundle.metadata.tags.size()) ss << ", ";
    }
    ss << "],\n";

    const auto& exp = bundle.experiment;
    ss << "  \"experiment\": {\n";
    ss << "    \"id\": \"" << exp.id.to_string() << "\",\n";
    ss << "    \"name\": \"" << escape_json(exp.name) << "\",\n";
    ss << "    \"description\": \"" << escape_json(exp.description) << "\",\n";
    ss << "    \"scenario\": \"" << formation_scenario_to_string(exp.scenario) << "\",\n";
    ss << "    \"temperature_k\": " << std::setprecision(10) << exp.temperature_k << ",\n";
    ss << "    \"pressure_pa\": " << std::setprecision(10) << exp.pressure_pa << ",\n";
    ss << "    \"oxidizing_atmosphere\": " << (exp.oxidizing_atmosphere ? "true" : "false") << ",\n";
    ss << "    \"user_density_override_kg_m3\": " << exp.user_density_override_kg_m3 << ",\n";
    if (exp.promoted_material_id.has_value()) {
        ss << "    \"promoted_material_id\": \"" << exp.promoted_material_id->to_string() << "\",\n";
    }

    ss << "    \"inputs\": [\n";
    for (size_t i = 0; i < exp.inputs.size(); ++i) {
        const auto& in = exp.inputs[i];
        ss << "      {\"symbol_or_id\": \"" << escape_json(in.symbol_or_id)
           << "\", \"basis\": \"" << basis_to_string(in.basis)
           << "\", \"amount\": " << std::setprecision(10) << in.amount
           << ", \"layer_thickness_m\": " << in.layer_thickness_m
           << ", \"is_matrix\": " << (in.is_matrix ? "true" : "false") << "}";
        if (i + 1 < exp.inputs.size()) ss << ",";
        ss << "\n";
    }
    ss << "    ]\n";
    ss << "  }\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<VersionedExperimentBundle> ExperimentPersistence::deserialize_from_json(
    std::string_view json_str,
    const matdb::ElementDatabase& db) noexcept {
    if (json_str.empty()) {
        return make_persist_error(5001, "Cannot parse empty experiment JSON payload");
    }
    if (json_str.size() > MAX_PAYLOAD_BYTES) {
        return make_persist_error(5002, "Experiment file exceeds maximum allowed size (10 MB)");
    }

    VersionedExperimentBundle bundle;
    std::string schema = json_str_val(json_str, "schemaVersion");
    if (schema.empty()) {
        schema = json_str_val(json_str, "schema_version", "3.0.0"); // Legacy fallback
    }
    bundle.metadata.schema_version = schema;

    bundle.metadata.author = sanitize_str(json_str_val(json_str, "author", "Shunya Scientist"));
    bundle.metadata.application_version = sanitize_str(json_str_val(json_str, "applicationVersion", "5.0.0"));
    bundle.metadata.created_at = json_str_val(json_str, "createdAt");
    if (bundle.metadata.created_at.empty()) bundle.metadata.created_at = current_iso8601_timestamp();
    bundle.metadata.modified_at = json_str_val(json_str, "modifiedAt");
    if (bundle.metadata.modified_at.empty()) bundle.metadata.modified_at = current_iso8601_timestamp();
    bundle.metadata.notes = sanitize_str(json_str_val(json_str, "notes"));

    // Extract tags
    const size_t tags_pos = json_str.find("\"tags\"");
    if (tags_pos != std::string_view::npos) {
        const size_t open_bracket = json_str.find('[', tags_pos);
        const size_t close_bracket = json_str.find(']', open_bracket);
        if (open_bracket != std::string_view::npos && close_bracket != std::string_view::npos) {
            std::string_view sub = json_str.substr(open_bracket, close_bracket - open_bracket + 1);
            std::regex tag_re(R"(\"([^\"\n\r]*)\")");
            auto begin = std::regex_iterator(sub.begin(), sub.end(), tag_re);
            auto end = std::regex_iterator<std::string_view::const_iterator>();
            for (auto it = begin; it != end; ++it) {
                std::string t = sanitize_str((*it)[1].str(), 64);
                if (!t.empty()) bundle.metadata.tags.push_back(std::move(t));
            }
        }
    }

    // Determine if experiment is under "experiment": { ... } or top-level (legacy)
    std::string_view exp_json = json_str;
    const size_t exp_block_pos = json_str.find("\"experiment\"");
    if (exp_block_pos != std::string_view::npos) {
        const size_t open_brace = json_str.find('{', exp_block_pos);
        if (open_brace != std::string_view::npos) {
            exp_json = json_str.substr(open_brace);
        }
    }

    auto exp_res = MixtureExperiment::from_json(exp_json);
    if (exp_res.is_err()) {
        return make_persist_error(5003, "Malformed experiment structure: " + exp_res.error().message);
    }
    bundle.experiment = exp_res.value();
    bundle.experiment.name = sanitize_str(bundle.experiment.name, 128);
    bundle.experiment.description = sanitize_str(bundle.experiment.description, 512);

    // Validate constituent elements against ElementDatabase
    for (const auto& in : bundle.experiment.inputs) {
        const auto* el = db.find_by_symbol(in.symbol_or_id);
        if (!el) {
            bundle.validation_warnings.push_back(
                "Constituent element '" + in.symbol_or_id + "' is not present in local elements database."
            );
        }
    }

    // Schema upgrade notice
    if (schema != CURRENT_SCHEMA_VERSION) {
        bundle.validation_warnings.push_back(
            "Upgraded experiment schema from legacy " + schema + " to Phase 5 " + std::string(CURRENT_SCHEMA_VERSION) + "."
        );
        bundle.metadata.schema_version = CURRENT_SCHEMA_VERSION;
    }

    return bundle;
}

core::base::Result<void> ExperimentPersistence::save_atomic(
    const std::filesystem::path& file_path,
    const VersionedExperimentBundle& bundle) noexcept {
    try {
        if (file_path.has_parent_path()) {
            std::filesystem::create_directories(file_path.parent_path());
        }

        std::filesystem::path tmp_path = file_path;
        tmp_path += ".tmp";

        std::string json_data = serialize_to_json(bundle);

        {
            std::ofstream ofs(tmp_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!ofs.is_open()) {
                return make_persist_error(5010, "Failed to open temporary file for atomic write: " + tmp_path.string());
            }
            ofs.write(json_data.data(), static_cast<std::streamsize>(json_data.size()));
            ofs.flush();
            if (!ofs.good()) {
                return make_persist_error(5011, "Disk write error while flushing experiment: " + tmp_path.string());
            }
        }

        // Atomic swap
        std::error_code ec;
#if defined(_WIN32)
        if (std::filesystem::exists(file_path, ec)) {
            std::filesystem::remove(file_path, ec);
        }
#endif
        std::filesystem::rename(tmp_path, file_path, ec);
        if (ec) {
            // Fallback: copy and remove tmp
            std::filesystem::copy_file(tmp_path, file_path, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmp_path, ec);
            if (ec) {
                return make_persist_error(5012, "Failed to atomically commit experiment file: " + ec.message());
            }
        }

        return {};
    } catch (const std::exception& e) {
        return make_persist_error(5013, std::string("Filesystem exception during save_atomic: ") + e.what());
    }
}

core::base::Result<VersionedExperimentBundle> ExperimentPersistence::load_file(
    const std::filesystem::path& file_path,
    const matdb::ElementDatabase& db) noexcept {
    try {
        if (!std::filesystem::exists(file_path)) {
            return make_persist_error(5020, "File does not exist: " + file_path.string());
        }

        std::ifstream ifs(file_path, std::ios::in | std::ios::binary);
        if (!ifs.is_open()) {
            return make_persist_error(5021, "Permission denied or failed to open file: " + file_path.string());
        }

        std::ostringstream ss;
        ss << ifs.rdbuf();
        return deserialize_from_json(ss.str(), db);
    } catch (const std::exception& e) {
        return make_persist_error(5022, std::string("Filesystem exception during load_file: ") + e.what());
    }
}

core::base::Result<std::filesystem::path> ExperimentPersistence::delete_to_trash(
    const std::filesystem::path& file_path,
    const std::filesystem::path& trash_dir) noexcept {
    try {
        if (!std::filesystem::exists(file_path)) {
            return make_persist_error(5030, "File to delete does not exist: " + file_path.string());
        }
        std::filesystem::create_directories(trash_dir);

        std::string filename = file_path.filename().string();
        std::filesystem::path dest = trash_dir / filename;

        std::error_code ec;
        if (std::filesystem::exists(dest, ec)) {
            std::filesystem::remove(dest, ec);
        }
        std::filesystem::rename(file_path, dest, ec);
        if (ec) {
            std::filesystem::copy_file(file_path, dest, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(file_path, ec);
        }
        return dest;
    } catch (const std::exception& e) {
        return make_persist_error(5031, std::string("Failed to move file to trash: ") + e.what());
    }
}

core::base::Result<std::filesystem::path> ExperimentPersistence::restore_from_trash(
    const std::filesystem::path& trashed_file,
    const std::filesystem::path& target_dir) noexcept {
    try {
        if (!std::filesystem::exists(trashed_file)) {
            return make_persist_error(5040, "Trashed file does not exist: " + trashed_file.string());
        }
        std::filesystem::create_directories(target_dir);

        std::string filename = trashed_file.filename().string();
        std::filesystem::path dest = target_dir / filename;

        std::error_code ec;
        if (std::filesystem::exists(dest, ec)) {
            std::filesystem::remove(dest, ec);
        }
        std::filesystem::rename(trashed_file, dest, ec);
        return dest;
    } catch (const std::exception& e) {
        return make_persist_error(5041, std::string("Failed to restore file from trash: ") + e.what());
    }
}

std::vector<VersionedExperimentBundle> ExperimentPersistence::list_experiments(
    const std::filesystem::path& dir_path,
    const matdb::ElementDatabase& db) noexcept {
    std::vector<VersionedExperimentBundle> list;
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        return list;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            if (ext == ".json") {
                auto res = load_file(entry.path(), db);
                if (res.is_ok()) {
                    list.push_back(std::move(res.value()));
                }
            }
        }
    }
    return list;
}

std::string ExperimentPersistence::resolve_name_conflict(
    const std::string& base_name,
    const std::vector<std::string>& existing_names) noexcept {
    auto contains = [&](const std::string& candidate) {
        for (const auto& ex : existing_names) {
            if (ex == candidate) return true;
        }
        return false;
    };

    if (!contains(base_name)) return base_name;

    int counter = 1;
    while (true) {
        std::string candidate = base_name + "_" + std::to_string(counter);
        if (!contains(candidate)) return candidate;
        ++counter;
    }
}

} // namespace shunya::mat
