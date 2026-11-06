#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <memory>
#include <utility>

namespace shunya::core::domain {

enum class PackageEntryType {
    Manifest,
    Scene,
    Asset,
    Script,
    Result
};

[[nodiscard]] constexpr std::string_view package_entry_type_to_string(PackageEntryType type) noexcept {
    switch (type) {
        case PackageEntryType::Manifest: return "Manifest";
        case PackageEntryType::Scene: return "Scene";
        case PackageEntryType::Asset: return "Asset";
        case PackageEntryType::Script: return "Script";
        case PackageEntryType::Result: return "Result";
    }
    return "Asset";
}

struct PackageEntry {
    std::string relative_path;
    PackageEntryType type = PackageEntryType::Asset;
    std::size_t size_bytes = 0;
    uint32_t checksum = 0;
};

struct PackageManifest {
    base::Uuid package_id;
    base::Uuid project_id;
    std::string project_name;
    std::string created_at;
    base::Version format_version{1, 0, 0, "final"};
    std::vector<PackageEntry> entries;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<PackageManifest> from_json(std::string_view json_str);
};

struct PackagingOptions {
    bool include_results = false;
    bool verify_after_packaging = true;
};

class ProjectPackager {
public:
    ProjectPackager() = default;

    [[nodiscard]] base::Result<PackageManifest> create_package(
        const Project& project,
        const SceneGraph& scene_graph,
        const std::filesystem::path& asset_source_dir,
        const std::filesystem::path& output_package_dir,
        const PackagingOptions& options = {}
    );

    [[nodiscard]] base::ValidationReport verify_package(
        const std::filesystem::path& package_dir
    ) const;

    [[nodiscard]] base::Result<std::pair<std::shared_ptr<Project>, std::unique_ptr<SceneGraph>>> unpack_and_load(
        const std::filesystem::path& package_dir
    );
};

} // namespace shunya::core::domain
