#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/assembly_model.hpp"
#include "shunya/core/domain/material_assignment.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <chrono>

namespace shunya::eng {

struct ObjectFileMetadata {
    std::string schema_version{"5.0.0"};
    std::string author{"Shunya Engineer"};
    std::string application_version{"5.0.0"};
    std::string created_at;
    std::string modified_at;
    std::vector<std::string> tags;
    std::string notes;
    std::string provenance_checksum;
    std::string thumbnail_base64; // Visual 3D preview thumbnail (PNG/SVG)
};

struct VersionedObjectBundle {
    ObjectFileMetadata metadata;
    PhysicalObject object;
    std::vector<core::domain::Material> bundled_materials;
    std::vector<std::string> validation_warnings;
};

struct VersionedAssemblyBundle {
    ObjectFileMetadata metadata;
    AssemblyModel assembly;
    std::vector<PhysicalObject> bundled_objects;
    std::vector<core::domain::Material> bundled_materials;
    std::vector<std::string> validation_warnings;
};

struct ObjectImportPreview {
    std::string name;
    std::string author;
    std::string creation_date;
    std::string shape_type;
    size_t region_count{0};
    std::vector<std::string> material_names;
    size_t bundled_materials_count{0};
    double mass_kg{0.0};
    double volume_m3{0.0};
    std::string thumbnail_base64;
    bool has_missing_materials{false};
    std::vector<std::string> missing_material_names;
};

enum class NameConflictResolution {
    ReplaceExisting,
    KeepBoth,
    Cancel
};

class ObjectPersistence {
public:
    static constexpr std::string_view CURRENT_SCHEMA_VERSION = "5.0.0";
    static constexpr size_t MAX_PAYLOAD_BYTES = 10 * 1024 * 1024; // 10 MB payload safety cap
    static constexpr size_t MAX_STRING_LENGTH = 1024;

    // String Sanitization
    [[nodiscard]] static std::string sanitize_string(std::string_view str) noexcept;

    // Object Serialization & Deserialization
    [[nodiscard]] static std::string serialize_object_bundle(
        const VersionedObjectBundle& bundle) noexcept;

    [[nodiscard]] static core::base::Result<VersionedObjectBundle> deserialize_object_bundle(
        std::string_view json_str) noexcept;

    // Assembly Serialization & Deserialization
    [[nodiscard]] static std::string serialize_assembly_bundle(
        const VersionedAssemblyBundle& bundle) noexcept;

    [[nodiscard]] static core::base::Result<VersionedAssemblyBundle> deserialize_assembly_bundle(
        std::string_view json_str) noexcept;

    // Atomic Disk I/O Protocol (writes to .tmp, fsyncs, renames)
    [[nodiscard]] static core::base::Result<void> save_object_atomic(
        const std::filesystem::path& file_path,
        const VersionedObjectBundle& bundle) noexcept;

    [[nodiscard]] static core::base::Result<VersionedObjectBundle> load_object_file(
        const std::filesystem::path& file_path) noexcept;

    [[nodiscard]] static core::base::Result<void> save_assembly_atomic(
        const std::filesystem::path& file_path,
        const VersionedAssemblyBundle& bundle) noexcept;

    [[nodiscard]] static core::base::Result<VersionedAssemblyBundle> load_assembly_file(
        const std::filesystem::path& file_path) noexcept;

    // Trash & Undo Recovery
    [[nodiscard]] static core::base::Result<std::filesystem::path> delete_to_trash(
        const std::filesystem::path& file_path,
        const std::filesystem::path& trash_dir) noexcept;

    [[nodiscard]] static core::base::Result<std::filesystem::path> restore_from_trash(
        const std::filesystem::path& trashed_file,
        const std::filesystem::path& target_dir) noexcept;

    // File Cataloging & Discovery
    [[nodiscard]] static std::vector<VersionedObjectBundle> list_objects(
        const std::filesystem::path& dir_path) noexcept;

    [[nodiscard]] static std::vector<VersionedAssemblyBundle> list_assemblies(
        const std::filesystem::path& dir_path) noexcept;

    // Name Conflict Resolution
    [[nodiscard]] static std::string resolve_name_conflict(
        const std::string& base_name,
        const std::vector<std::string>& existing_names) noexcept;

    // Import Preview & Dependent Material Resolution
    [[nodiscard]] static core::base::Result<ObjectImportPreview> generate_import_preview(
        std::string_view json_str,
        const core::domain::MaterialRegistry& existing_reg) noexcept;

    [[nodiscard]] static core::base::Result<PhysicalObject> import_and_resolve_materials(
        const VersionedObjectBundle& bundle,
        PhysicalObjectManager& obj_mgr,
        core::domain::MaterialRegistry& mat_reg,
        NameConflictResolution conflict_res = NameConflictResolution::KeepBoth) noexcept;

    // Workspace Folder Setup
    static void ensure_workspace_folders(const std::filesystem::path& workspace_root) noexcept;

    // Utility
    [[nodiscard]] static std::string current_iso8601_timestamp() noexcept;
    [[nodiscard]] static std::string compute_checksum(std::string_view content) noexcept;
    [[nodiscard]] static std::string generate_svg_thumbnail(const PhysicalObject& obj) noexcept;
};

/**
 * @brief Manages open objects dirty states, undo history, and autosave in the workspace.
 */
class ObjectWorkspaceSession {
public:
    void mark_dirty(const core::base::Uuid& id) noexcept {
        dirty_objects_.insert(id);
    }

    void mark_clean(const core::base::Uuid& id) noexcept {
        dirty_objects_.erase(id);
    }

    [[nodiscard]] bool is_dirty(const core::base::Uuid& id) const noexcept {
        return dirty_objects_.find(id) != dirty_objects_.end();
    }

    [[nodiscard]] bool has_any_dirty() const noexcept {
        return !dirty_objects_.empty();
    }

    void clear() noexcept {
        dirty_objects_.clear();
    }

private:
    std::unordered_set<core::base::Uuid> dirty_objects_;
};

} // namespace shunya::eng
