#include "shunya/core/domain/validator.hpp"

#include <unordered_set>
#include <algorithm>

namespace shunya::core::domain {

base::ValidationReport ProjectValidator::validate_manifest(
    const ProjectManifest& manifest,
    const ProjectValidationOptions& options
) const {
    base::ValidationReport report;

    // Project name check
    if (manifest.name.empty()) {
        report.add_error("manifest.name", "Project name cannot be empty", 2401);
    }

    // UUID nil check
    if (manifest.id.is_nil()) {
        report.add_error("manifest.id", "Project ID cannot be nil UUID", 2402);
    }

    // Schema version check
    if (manifest.schema_version.major > base::CURRENT_VERSION.major) {
        report.add_error("manifest.schema_version",
            "Unsupported future major schema version: " + manifest.schema_version.as_string(), 2403);
    } else if (manifest.schema_version.minor > base::CURRENT_VERSION.minor && !options.allow_future_minor_versions) {
        report.add_warning("manifest.schema_version",
            "Project uses newer minor schema version: " + manifest.schema_version.as_string(), 2404);
    }

    // Dependencies check
    for (std::size_t i = 0; i < manifest.dependencies.size(); ++i) {
        const auto& dep = manifest.dependencies[i];
        if (dep.name.empty()) {
            report.add_error("manifest.dependencies[" + std::to_string(i) + "].name",
                "Dependency has empty name", 2405);
        }
        if (!dep.version.empty()) {
            auto ver_res = base::Version::parse(dep.version);
            if (ver_res.is_err()) {
                report.add_warning("manifest.dependencies[" + std::to_string(i) + "].version",
                    "Dependency '" + dep.name + "' has non-standard version string: " + dep.version, 2406);
            }
        }
    }

    return report;
}

base::ValidationReport ProjectValidator::validate_scene_graph(
    const SceneGraph& scene_graph,
    const ProjectValidationOptions& options
) const {
    base::ValidationReport report;
    std::unordered_set<std::string> seen_names;

    scene_graph.traverse_depth_first([&](const std::shared_ptr<Entity>& entity) {
        std::string entity_path = "scene.entities[" + entity->id().to_string() + "]";

        // Nil UUID check
        if (entity->id().is_nil()) {
            report.add_error(entity_path + ".id", "Entity has nil UUID: " + entity->name(), 2410);
        }

        // Unique entity names check
        if (options.require_unique_entity_names) {
            if (!seen_names.insert(entity->name()).second) {
                report.add_warning(entity_path + ".name", "Duplicate entity name detected: " + entity->name(), 2414);
            }
        }

        // Parent reference check
        if (entity->parent_id().has_value()) {
            auto parent = scene_graph.get_entity(entity->parent_id().value());
            if (!parent) {
                report.add_error(entity_path + ".parent_id",
                    "Broken parent reference in entity '" + entity->name() + "': parent ID not found", 2411);
            } else {
                // Verify asymmetric link
                const auto& p_children = parent->children_ids();
                if (std::find(p_children.begin(), p_children.end(), entity->id()) == p_children.end()) {
                    report.add_error(entity_path + ".parent_id",
                        "Asymmetric parent-child link between '" + parent->name() + "' and '" + entity->name() + "'", 2413);
                }
            }
        }

        // Children reference check
        for (const auto& child_id : entity->children_ids()) {
            auto child = scene_graph.get_entity(child_id);
            if (!child) {
                report.add_error(entity_path + ".children_ids",
                    "Broken child reference in entity '" + entity->name() + "': child ID not found", 2412);
            }
        }

        // Missing asset files check
        if (options.check_asset_files_exist) {
            for (const auto& [key, uri_or_path] : entity->domain_attachments()) {
                bool is_asset_key = (key == "cad_mesh" || key == "step_file" ||
                                     key == "spice_subcircuit_file" || key == "script_file" ||
                                     key == "texture_map" || key == "mesh_file" ||
                                     key.ends_with("_file") || key.ends_with("_path"));
                if (is_asset_key && !uri_or_path.empty()) {
                    // Ignore URI schemes
                    if (uri_or_path.starts_with("http://") ||
                        uri_or_path.starts_with("https://") ||
                        uri_or_path.starts_with("shunya://")) {
                        continue;
                    }

                    std::filesystem::path p(uri_or_path);
                    if (p.is_relative() && !options.project_root_path.empty()) {
                        p = options.project_root_path / p;
                    }

                    if (!std::filesystem::exists(p)) {
                        report.add_error(entity_path + ".attachments[" + key + "]",
                            "Asset file not found on disk: " + uri_or_path, 2415);
                    }
                }
            }
        }
    });

    return report;
}

base::ValidationReport ProjectValidator::validate(
    const Project& project,
    const SceneGraph& scene_graph,
    const ProjectValidationOptions& options
) const {
    ProjectValidationOptions resolved_opts = options;
    if (resolved_opts.project_root_path.empty() && !project.file_path().empty()) {
        std::filesystem::path fp(project.file_path());
        resolved_opts.project_root_path = fp.parent_path();
    }

    auto manifest_rep = validate_manifest(project.manifest(), resolved_opts);
    auto scene_rep = validate_scene_graph(scene_graph, resolved_opts);

    manifest_rep.merge(scene_rep);
    return manifest_rep;
}

} // namespace shunya::core::domain
