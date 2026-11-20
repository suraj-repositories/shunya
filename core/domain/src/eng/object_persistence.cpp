#include "shunya/eng/object_persistence.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::eng {

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

core::base::Error make_persist_error(int code, std::string msg) {
    core::base::Error err;
    err.code = code;
    err.domain = core::base::ErrorDomain::IO;
    err.severity = core::base::Severity::Error;
    err.message = std::move(msg);
    return err;
}

} // namespace

std::string ObjectPersistence::sanitize_string(std::string_view in) noexcept {
    std::string s;
    s.reserve(std::min(in.size(), MAX_STRING_LENGTH));
    for (char c : in) {
        if (s.size() >= MAX_STRING_LENGTH) break;
        if (c >= 32 && c <= 126) {
            s.push_back(c);
        } else if (c == '\n' || c == '\t' || c == '\r') {
            s.push_back(' ');
        }
    }
    return s;
}

std::string ObjectPersistence::current_iso8601_timestamp() noexcept {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
#if defined(_WIN32)
    gmtime_s(&tm_buf, &tt);
#else
    gmtime_r(&tt, &tm_buf);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

std::string ObjectPersistence::compute_checksum(std::string_view content) noexcept {
    uint32_t hash = 2166136261u;
    for (char c : content) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 16777619u;
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%08X", hash);
    return std::string(buf);
}

std::string ObjectPersistence::generate_svg_thumbnail(const PhysicalObject& obj) noexcept {
    std::ostringstream ss;
    ss << "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 200 200' width='200' height='200'>";
    ss << "<rect width='200' height='200' fill='#18181b' rx='8'/>";
    ss << "<text x='100' y='30' fill='#a1a1aa' font-size='12' text-anchor='middle'>" << escape_json(obj.name) << "</text>";
    // Draw wireframe schematic based on shape kind
    switch (obj.geometry.kind) {
    case ObjectShapeKind::Sphere:
        ss << "<circle cx='100' cy='110' r='50' fill='none' stroke='#38bdf8' stroke-width='2'/>";
        ss << "<ellipse cx='100' cy='110' rx='50' ry='15' fill='none' stroke='#38bdf8' stroke-width='1' stroke-dasharray='3,3'/>";
        break;
    case ObjectShapeKind::Cylinder:
    case ObjectShapeKind::Pipe:
        ss << "<ellipse cx='100' cy='70' rx='40' ry='15' fill='none' stroke='#38bdf8' stroke-width='2'/>";
        ss << "<ellipse cx='100' cy='150' rx='40' ry='15' fill='none' stroke='#38bdf8' stroke-width='2'/>";
        ss << "<line x1='60' y1='70' x2='60' y2='150' stroke='#38bdf8' stroke-width='2'/>";
        ss << "<line x1='140' y1='70' x2='140' y2='150' stroke='#38bdf8' stroke-width='2'/>";
        break;
    case ObjectShapeKind::Cone:
        ss << "<ellipse cx='100' cy='150' rx='45' ry='15' fill='none' stroke='#38bdf8' stroke-width='2'/>";
        ss << "<line x1='55' y1='150' x2='100' y2='65' stroke='#38bdf8' stroke-width='2'/>";
        ss << "<line x1='145' y1='150' x2='100' y2='65' stroke='#38bdf8' stroke-width='2'/>";
        break;
    default: // Box and others
        ss << "<rect x='60' y='70' width='80' height='80' fill='none' stroke='#38bdf8' stroke-width='2' rx='2'/>";
        ss << "<line x1='60' y1='70' x2='80' y2='50' stroke='#38bdf8' stroke-width='1'/>";
        ss << "<line x1='140' y1='70' x2='160' y2='50' stroke='#38bdf8' stroke-width='1'/>";
        ss << "<line x1='140' y1='150' x2='160' y2='130' stroke='#38bdf8' stroke-width='1'/>";
        ss << "<line x1='80' y1='50' x2='160' y2='50' stroke='#38bdf8' stroke-width='1'/>";
        ss << "<line x1='160' y1='50' x2='160' y2='130' stroke='#38bdf8' stroke-width='1'/>";
        break;
    }
    ss << "</svg>";
    return ss.str();
}

std::string ObjectPersistence::serialize_object_bundle(
    const VersionedObjectBundle& bundle) noexcept {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schemaVersion\": \"" << escape_json(bundle.metadata.schema_version) << "\",\n";
    ss << "  \"applicationVersion\": \"" << escape_json(bundle.metadata.application_version) << "\",\n";
    ss << "  \"author\": \"" << escape_json(bundle.metadata.author) << "\",\n";
    ss << "  \"createdAt\": \"" << escape_json(bundle.metadata.created_at.empty() ? current_iso8601_timestamp() : bundle.metadata.created_at) << "\",\n";
    ss << "  \"modifiedAt\": \"" << escape_json(current_iso8601_timestamp()) << "\",\n";
    ss << "  \"notes\": \"" << escape_json(bundle.metadata.notes) << "\",\n";

    std::string thumb = bundle.metadata.thumbnail_base64;
    if (thumb.empty()) {
        thumb = generate_svg_thumbnail(bundle.object);
    }
    ss << "  \"thumbnail\": \"" << escape_json(thumb) << "\",\n";

    // Serialize object
    ss << "  \"object\": " << bundle.object.to_json() << ",\n";

    // Serialize bundled materials
    ss << "  \"bundledMaterials\": [\n";
    for (size_t i = 0; i < bundle.bundled_materials.size(); ++i) {
        ss << "    " << bundle.bundled_materials[i].to_json();
        if (i + 1 < bundle.bundled_materials.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<VersionedObjectBundle> ObjectPersistence::deserialize_object_bundle(
    std::string_view json_str) noexcept {
    if (json_str.size() > MAX_PAYLOAD_BYTES) {
        return make_persist_error(4401, "Object payload exceeds maximum allowed size (10 MB)");
    }
    if (json_str.empty()) {
        return make_persist_error(4402, "Empty JSON string");
    }

    VersionedObjectBundle bundle;
    bundle.metadata.schema_version = sanitize_string(json_str_val(json_str, "schemaVersion", "5.0.0"));
    bundle.metadata.application_version = sanitize_string(json_str_val(json_str, "applicationVersion", "5.0.0"));
    bundle.metadata.author = sanitize_string(json_str_val(json_str, "author", "Unknown Author"));
    bundle.metadata.created_at = json_str_val(json_str, "createdAt", "");
    bundle.metadata.modified_at = json_str_val(json_str, "modifiedAt", "");
    bundle.metadata.notes = sanitize_string(json_str_val(json_str, "notes", ""));
    bundle.metadata.thumbnail_base64 = json_str_val(json_str, "thumbnail", "");

    // Extract "object" block
    const size_t obj_pos = json_str.find("\"object\"");
    if (obj_pos == std::string_view::npos) {
        return make_persist_error(4403, "Missing 'object' definition in JSON");
    }
    const size_t brace_open = json_str.find('{', obj_pos);
    if (brace_open == std::string_view::npos) {
        return make_persist_error(4404, "Malformed 'object' block");
    }

    // Match braces for object
    int depth = 0;
    size_t brace_close = std::string_view::npos;
    for (size_t i = brace_open; i < json_str.size(); ++i) {
        if (json_str[i] == '{') ++depth;
        else if (json_str[i] == '}') {
            --depth;
            if (depth == 0) {
                brace_close = i;
                break;
            }
        }
    }
    if (brace_close == std::string_view::npos) {
        return make_persist_error(4405, "Unclosed 'object' block in JSON");
    }

    std::string_view obj_json = json_str.substr(brace_open, brace_close - brace_open + 1);
    auto obj_res = PhysicalObject::from_json(obj_json);
    if (obj_res.is_err()) {
        return obj_res.error();
    }
    bundle.object = std::move(obj_res.value());

    // Extract bundled materials with proper brace nesting support
    const size_t mat_pos = json_str.find("\"bundledMaterials\"");
    if (mat_pos != std::string_view::npos) {
        const size_t arr_open = json_str.find('[', mat_pos);
        if (arr_open != std::string_view::npos) {
            int depth = 0;
            size_t mat_start = std::string_view::npos;
            for (size_t i = arr_open + 1; i < json_str.size(); ++i) {
                char c = json_str[i];
                if (c == ']' && depth == 0) {
                    break;
                } else if (c == '{') {
                    if (depth == 0) {
                        mat_start = i;
                    }
                    ++depth;
                } else if (c == '}') {
                    --depth;
                    if (depth == 0 && mat_start != std::string_view::npos) {
                        std::string_view single_mat = json_str.substr(mat_start, i - mat_start + 1);
                        auto m_res = core::domain::Material::from_json(single_mat);
                        if (m_res.is_ok()) {
                            bundle.bundled_materials.push_back(std::move(m_res.value()));
                        }
                        mat_start = std::string_view::npos;
                    }
                }
            }
        }
    }

    return bundle;
}

std::string ObjectPersistence::serialize_assembly_bundle(
    const VersionedAssemblyBundle& bundle) noexcept {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"schemaVersion\": \"" << escape_json(bundle.metadata.schema_version) << "\",\n";
    ss << "  \"applicationVersion\": \"" << escape_json(bundle.metadata.application_version) << "\",\n";
    ss << "  \"author\": \"" << escape_json(bundle.metadata.author) << "\",\n";
    ss << "  \"createdAt\": \"" << escape_json(bundle.metadata.created_at.empty() ? current_iso8601_timestamp() : bundle.metadata.created_at) << "\",\n";
    ss << "  \"modifiedAt\": \"" << escape_json(current_iso8601_timestamp()) << "\",\n";
    ss << "  \"notes\": \"" << escape_json(bundle.metadata.notes) << "\",\n";
    ss << "  \"assembly\": " << bundle.assembly.to_json() << ",\n";
    ss << "  \"bundledObjects\": [\n";
    for (size_t i = 0; i < bundle.bundled_objects.size(); ++i) {
        ss << "    " << bundle.bundled_objects[i].to_json();
        if (i + 1 < bundle.bundled_objects.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ],\n";
    ss << "  \"bundledMaterials\": [\n";
    for (size_t i = 0; i < bundle.bundled_materials.size(); ++i) {
        ss << "    " << bundle.bundled_materials[i].to_json();
        if (i + 1 < bundle.bundled_materials.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<VersionedAssemblyBundle> ObjectPersistence::deserialize_assembly_bundle(
    std::string_view json_str) noexcept {
    if (json_str.size() > MAX_PAYLOAD_BYTES) {
        return make_persist_error(4411, "Assembly payload exceeds maximum allowed size (10 MB)");
    }
    if (json_str.empty()) {
        return make_persist_error(4412, "Empty JSON string");
    }

    VersionedAssemblyBundle bundle;
    bundle.metadata.schema_version = sanitize_string(json_str_val(json_str, "schemaVersion", "5.0.0"));
    bundle.metadata.application_version = sanitize_string(json_str_val(json_str, "applicationVersion", "5.0.0"));
    bundle.metadata.author = sanitize_string(json_str_val(json_str, "author", "Unknown Author"));
    bundle.metadata.created_at = json_str_val(json_str, "createdAt", "");
    bundle.metadata.modified_at = json_str_val(json_str, "modifiedAt", "");
    bundle.metadata.notes = sanitize_string(json_str_val(json_str, "notes", ""));

    const size_t assy_pos = json_str.find("\"assembly\"");
    if (assy_pos == std::string_view::npos) {
        return make_persist_error(4413, "Missing 'assembly' definition in JSON");
    }
    const size_t brace_open = json_str.find('{', assy_pos);
    if (brace_open == std::string_view::npos) {
        return make_persist_error(4414, "Malformed 'assembly' block");
    }
    int depth = 0;
    size_t brace_close = std::string_view::npos;
    for (size_t i = brace_open; i < json_str.size(); ++i) {
        if (json_str[i] == '{') ++depth;
        else if (json_str[i] == '}') {
            --depth;
            if (depth == 0) {
                brace_close = i;
                break;
            }
        }
    }
    if (brace_close == std::string_view::npos) {
        return make_persist_error(4415, "Unclosed 'assembly' block in JSON");
    }

    std::string_view assy_json = json_str.substr(brace_open, brace_close - brace_open + 1);
    auto assy_res = AssemblyModel::from_json(assy_json);
    if (assy_res.is_err()) {
        return assy_res.error();
    }
    bundle.assembly = std::move(assy_res.value());
    return bundle;
}

core::base::Result<void> ObjectPersistence::save_object_atomic(
    const std::filesystem::path& file_path,
    const VersionedObjectBundle& bundle) noexcept {
    try {
        std::filesystem::create_directories(file_path.parent_path());
        std::filesystem::path tmp_path = file_path;
        tmp_path += ".tmp";

        std::string json_data = serialize_object_bundle(bundle);
        {
            std::ofstream out(tmp_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                return make_persist_error(4420, "Failed to open temporary file for writing: " + tmp_path.string());
            }
            out.write(json_data.data(), static_cast<std::streamsize>(json_data.size()));
            out.flush();
            if (!out.good()) {
                return make_persist_error(4421, "Failed writing atomic data to temporary file");
            }
        }

        std::error_code ec;
        std::filesystem::rename(tmp_path, file_path, ec);
        if (ec) {
            std::filesystem::copy_file(tmp_path, file_path, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmp_path, ec);
        }
        return core::base::Result<void>::ok();
    } catch (const std::exception& e) {
        return make_persist_error(4422, std::string("Exception saving object atomically: ") + e.what());
    }
}

core::base::Result<VersionedObjectBundle> ObjectPersistence::load_object_file(
    const std::filesystem::path& file_path) noexcept {
    try {
        if (!std::filesystem::exists(file_path)) {
            return make_persist_error(4423, "Object file does not exist: " + file_path.string());
        }
        std::ifstream in(file_path, std::ios::in | std::ios::binary);
        if (!in.is_open()) {
            return make_persist_error(4424, "Failed to open object file: " + file_path.string());
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return deserialize_object_bundle(content);
    } catch (const std::exception& e) {
        return make_persist_error(4425, std::string("Exception loading object file: ") + e.what());
    }
}

core::base::Result<void> ObjectPersistence::save_assembly_atomic(
    const std::filesystem::path& file_path,
    const VersionedAssemblyBundle& bundle) noexcept {
    try {
        std::filesystem::create_directories(file_path.parent_path());
        std::filesystem::path tmp_path = file_path;
        tmp_path += ".tmp";

        std::string json_data = serialize_assembly_bundle(bundle);
        {
            std::ofstream out(tmp_path, std::ios::out | std::ios::binary | std::ios::trunc);
            if (!out.is_open()) {
                return make_persist_error(4430, "Failed to open temporary file for writing: " + tmp_path.string());
            }
            out.write(json_data.data(), static_cast<std::streamsize>(json_data.size()));
            out.flush();
            if (!out.good()) {
                return make_persist_error(4431, "Failed writing atomic data to temporary file");
            }
        }

        std::error_code ec;
        std::filesystem::rename(tmp_path, file_path, ec);
        if (ec) {
            std::filesystem::copy_file(tmp_path, file_path, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(tmp_path, ec);
        }
        return core::base::Result<void>::ok();
    } catch (const std::exception& e) {
        return make_persist_error(4432, std::string("Exception saving assembly atomically: ") + e.what());
    }
}

core::base::Result<VersionedAssemblyBundle> ObjectPersistence::load_assembly_file(
    const std::filesystem::path& file_path) noexcept {
    try {
        if (!std::filesystem::exists(file_path)) {
            return make_persist_error(4433, "Assembly file does not exist: " + file_path.string());
        }
        std::ifstream in(file_path, std::ios::in | std::ios::binary);
        if (!in.is_open()) {
            return make_persist_error(4434, "Failed to open assembly file: " + file_path.string());
        }
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        return deserialize_assembly_bundle(content);
    } catch (const std::exception& e) {
        return make_persist_error(4435, std::string("Exception loading assembly file: ") + e.what());
    }
}

core::base::Result<std::filesystem::path> ObjectPersistence::delete_to_trash(
    const std::filesystem::path& file_path,
    const std::filesystem::path& trash_dir) noexcept {
    try {
        if (!std::filesystem::exists(file_path)) {
            return make_persist_error(4440, "Cannot delete non-existent file: " + file_path.string());
        }
        std::filesystem::create_directories(trash_dir);
        std::filesystem::path target = trash_dir / file_path.filename();

        std::error_code ec;
        std::filesystem::rename(file_path, target, ec);
        if (ec) {
            std::filesystem::copy_file(file_path, target, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(file_path, ec);
        }
        return target;
    } catch (const std::exception& e) {
        return make_persist_error(4441, std::string("Failed to move file to trash: ") + e.what());
    }
}

core::base::Result<std::filesystem::path> ObjectPersistence::restore_from_trash(
    const std::filesystem::path& trashed_file,
    const std::filesystem::path& target_dir) noexcept {
    try {
        if (!std::filesystem::exists(trashed_file)) {
            return make_persist_error(4442, "Trashed file does not exist: " + trashed_file.string());
        }
        std::filesystem::create_directories(target_dir);
        std::filesystem::path target = target_dir / trashed_file.filename();

        std::error_code ec;
        std::filesystem::rename(trashed_file, target, ec);
        if (ec) {
            std::filesystem::copy_file(trashed_file, target, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::remove(trashed_file, ec);
        }
        return target;
    } catch (const std::exception& e) {
        return make_persist_error(4443, std::string("Failed to restore file from trash: ") + e.what());
    }
}

std::vector<VersionedObjectBundle> ObjectPersistence::list_objects(
    const std::filesystem::path& dir_path) noexcept {
    std::vector<VersionedObjectBundle> list;
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        return list;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (entry.is_regular_file(ec)) {
            const auto ext = entry.path().extension().string();
            if (ext == ".json" || ext == ".shunya-obj") {
                auto res = load_object_file(entry.path());
                if (res.is_ok()) {
                    list.push_back(std::move(res.value()));
                }
            }
        }
    }
    return list;
}

std::vector<VersionedAssemblyBundle> ObjectPersistence::list_assemblies(
    const std::filesystem::path& dir_path) noexcept {
    std::vector<VersionedAssemblyBundle> list;
    std::error_code ec;
    if (!std::filesystem::exists(dir_path, ec) || !std::filesystem::is_directory(dir_path, ec)) {
        return list;
    }
    for (const auto& entry : std::filesystem::directory_iterator(dir_path, ec)) {
        if (entry.is_regular_file(ec)) {
            const auto ext = entry.path().extension().string();
            if (ext == ".json" || ext == ".shunya-model") {
                auto res = load_assembly_file(entry.path());
                if (res.is_ok()) {
                    list.push_back(std::move(res.value()));
                }
            }
        }
    }
    return list;
}

std::string ObjectPersistence::resolve_name_conflict(
    const std::string& base_name,
    const std::vector<std::string>& existing_names) noexcept {
    auto exists = [&](const std::string& n) {
        for (const auto& ex : existing_names) {
            if (ex == n) return true;
        }
        return false;
    };

    if (!exists(base_name)) return base_name;

    for (int i = 1; ; ++i) {
        std::string cand = base_name + " (" + std::to_string(i) + ")";
        if (!exists(cand)) return cand;
    }
}

core::base::Result<ObjectImportPreview> ObjectPersistence::generate_import_preview(
    std::string_view json_str,
    const core::domain::MaterialRegistry& existing_reg) noexcept {
    auto b_res = deserialize_object_bundle(json_str);
    if (b_res.is_err()) {
        return b_res.error();
    }
    const auto& bundle = b_res.value();
    ObjectImportPreview prev;
    prev.name = bundle.object.name;
    prev.author = bundle.metadata.author;
    prev.creation_date = bundle.metadata.created_at;
    prev.shape_type = std::string(object_shape_kind_to_string(bundle.object.geometry.kind));
    prev.region_count = bundle.object.regions.size();
    prev.bundled_materials_count = bundle.bundled_materials.size();
    prev.mass_kg = bundle.object.computed.mass_kg;
    prev.volume_m3 = bundle.object.computed.volume_m3;
    prev.thumbnail_base64 = bundle.metadata.thumbnail_base64;

    for (const auto& r : bundle.object.regions) {
        if (!existing_reg.has_material(r.material_id)) {
            // Check if present in bundled materials
            bool found_in_bundle = false;
            for (const auto& bm : bundle.bundled_materials) {
                if (bm.id() == r.material_id) {
                    found_in_bundle = true;
                    prev.material_names.push_back(bm.name() + " (bundled)");
                    break;
                }
            }
            if (!found_in_bundle) {
                prev.has_missing_materials = true;
                prev.missing_material_names.push_back(r.name);
            }
        } else {
            const auto* m = existing_reg.get_material(r.material_id);
            prev.material_names.push_back(m ? m->name() : "Standard Material");
        }
    }

    return prev;
}

core::base::Result<PhysicalObject> ObjectPersistence::import_and_resolve_materials(
    const VersionedObjectBundle& bundle,
    PhysicalObjectManager& obj_mgr,
    core::domain::MaterialRegistry& mat_reg,
    NameConflictResolution conflict_res) noexcept {
    if (conflict_res == NameConflictResolution::Cancel) {
        return make_persist_error(4450, "Object import cancelled by user");
    }

    // 1. Unbundle and inject any missing custom materials into mat_reg
    for (const auto& bm : bundle.bundled_materials) {
        if (!mat_reg.has_material(bm.id())) {
            auto reg_res = mat_reg.register_material(bm);
            (void)reg_res;
        }
    }

    PhysicalObject obj_to_import = bundle.object;

    // 2. Name conflict handling
    if (obj_mgr.find_by_name(obj_to_import.name) != nullptr) {
        if (conflict_res == NameConflictResolution::KeepBoth) {
            std::vector<std::string> existing_names;
            for (const auto& o : obj_mgr.objects()) {
                existing_names.push_back(o.name);
            }
            obj_to_import.name = resolve_name_conflict(obj_to_import.name, existing_names);
            obj_to_import.id = core::base::Uuid::generate_random();
        } else if (conflict_res == NameConflictResolution::ReplaceExisting) {
            const auto* existing = obj_mgr.find_by_name(obj_to_import.name);
            if (existing) {
                obj_to_import.id = existing->id;
                auto update_res = obj_mgr.update_object(obj_to_import, mat_reg);
                if (update_res.is_err()) return update_res.error();
                return obj_to_import;
            }
        }
    }

    auto add_res = obj_mgr.add_object(obj_to_import, mat_reg);
    if (add_res.is_err()) {
        return add_res.error();
    }

    return obj_to_import;
}

void ObjectPersistence::ensure_workspace_folders(const std::filesystem::path& workspace_root) noexcept {
    std::error_code ec;
    std::filesystem::create_directories(workspace_root / "objects", ec);
    std::filesystem::create_directories(workspace_root / "models", ec);
    std::filesystem::create_directories(workspace_root / ".trash", ec);
}

} // namespace shunya::eng
