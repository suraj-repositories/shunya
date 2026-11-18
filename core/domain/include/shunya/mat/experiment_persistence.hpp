#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/element_database.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::mat {

struct ExperimentFileMetadata {
    std::string schema_version{"5.0.0"};
    std::string author{"Shunya Scientist"};
    std::string application_version{"5.0.0"};
    std::string created_at;
    std::string modified_at;
    std::vector<std::string> tags;
    std::string notes;
    std::string provenance_checksum;
};

struct VersionedExperimentBundle {
    ExperimentFileMetadata metadata;
    MixtureExperiment experiment;
    std::vector<std::string> validation_warnings;
};

class ExperimentPersistence {
public:
    static constexpr std::string_view CURRENT_SCHEMA_VERSION = "5.0.0";
    static constexpr size_t MAX_PAYLOAD_BYTES = 10 * 1024 * 1024; // 10 MB limit for safety
    static constexpr size_t MAX_STRING_LENGTH = 1024;

    [[nodiscard]] static std::string serialize_to_json(
        const VersionedExperimentBundle& bundle) noexcept;

    [[nodiscard]] static core::base::Result<VersionedExperimentBundle> deserialize_from_json(
        std::string_view json_str,
        const matdb::ElementDatabase& db = matdb::ElementDatabase::global()) noexcept;

    [[nodiscard]] static core::base::Result<void> save_atomic(
        const std::filesystem::path& file_path,
        const VersionedExperimentBundle& bundle) noexcept;

    [[nodiscard]] static core::base::Result<VersionedExperimentBundle> load_file(
        const std::filesystem::path& file_path,
        const matdb::ElementDatabase& db = matdb::ElementDatabase::global()) noexcept;

    [[nodiscard]] static core::base::Result<std::filesystem::path> delete_to_trash(
        const std::filesystem::path& file_path,
        const std::filesystem::path& trash_dir) noexcept;

    [[nodiscard]] static core::base::Result<std::filesystem::path> restore_from_trash(
        const std::filesystem::path& trashed_file,
        const std::filesystem::path& target_dir) noexcept;

    [[nodiscard]] static std::vector<VersionedExperimentBundle> list_experiments(
        const std::filesystem::path& dir_path,
        const matdb::ElementDatabase& db = matdb::ElementDatabase::global()) noexcept;

    [[nodiscard]] static std::string resolve_name_conflict(
        const std::string& base_name,
        const std::vector<std::string>& existing_names) noexcept;

    [[nodiscard]] static std::string current_iso8601_timestamp() noexcept;
};

} // namespace shunya::mat
