#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace shunya::core::domain {

struct ProjectValidationOptions {
    bool check_asset_files_exist = true;
    bool allow_future_minor_versions = true;
    bool require_unique_entity_names = false;
    std::filesystem::path project_root_path = "";
};

class ProjectValidator {
public:
    ProjectValidator() = default;

    [[nodiscard]] base::ValidationReport validate(
        const Project& project,
        const SceneGraph& scene_graph,
        const ProjectValidationOptions& options = {}
    ) const;

    [[nodiscard]] base::ValidationReport validate_manifest(
        const ProjectManifest& manifest,
        const ProjectValidationOptions& options = {}
    ) const;

    [[nodiscard]] base::ValidationReport validate_scene_graph(
        const SceneGraph& scene_graph,
        const ProjectValidationOptions& options = {}
    ) const;
};

} // namespace shunya::core::domain
