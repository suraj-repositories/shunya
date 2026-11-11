#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/core/domain/command_stack.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"
#include "shunya/eng/battery_engineer.hpp"
#include "shunya/eng/component_generator.hpp"
#include "shunya/eng/interactive_circuit.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/physics_runtime.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace shunya::eng {

enum class DependencyChangeKind : uint8_t {
    ProjectReset,
    ElementDatabaseUpdated,
    MaterialUpdated,
    MixtureUpdated,
    ObjectUpdated,
    ComponentUpdated,
    BatteryUpdated,
    CircuitUpdated,
    EnvironmentUpdated
};

struct WorkspaceChangeEvent {
    DependencyChangeKind kind{DependencyChangeKind::ProjectReset};
    core::base::Uuid target_id{};
    uint64_t workspace_version{0};
    std::string summary;
};

struct AmbientEnvironmentConfig {
    double temperature_k{298.15};
    double pressure_pa{101325.0};
    double gravity_m_s2{9.80665};
    double convection_h_w_m2k{10.0};
    bool oxidizing_atmosphere{true};
};

/**
 * @brief Unified single-project engineering pipeline coordinator linking
 *        Elements -> Materials -> Objects -> Components -> Batteries -> Circuits -> Simulation.
 *        Strictly independent of Qt or any UI framework.
 */
class EngineeringWorkspaceState {
public:
    using ChangeCallback = std::function<void(const WorkspaceChangeEvent&)>;

    EngineeringWorkspaceState();

    void reset_project(std::shared_ptr<core::domain::Project> project);
    [[nodiscard]] std::shared_ptr<core::domain::Project> project() const noexcept { return project_; }

    [[nodiscard]] const matdb::ElementDatabase& element_database() const noexcept {
        return matdb::ElementDatabase::global();
    }

    [[nodiscard]] std::vector<mat::MixtureExperiment>& mixture_experiments() noexcept { return mixtures_; }
    [[nodiscard]] const std::vector<mat::MixtureExperiment>& mixture_experiments() const noexcept { return mixtures_; }

    [[nodiscard]] core::domain::MaterialRegistry& material_registry() noexcept { return materials_; }
    [[nodiscard]] const core::domain::MaterialRegistry& material_registry() const noexcept { return materials_; }

    [[nodiscard]] PhysicalObjectManager& object_manager() noexcept { return object_manager_; }
    [[nodiscard]] const PhysicalObjectManager& object_manager() const noexcept { return object_manager_; }

    [[nodiscard]] std::vector<BatteryDesignSpec>& battery_specs() noexcept { return batteries_; }
    [[nodiscard]] const std::vector<BatteryDesignSpec>& battery_specs() const noexcept { return batteries_; }

    [[nodiscard]] ComponentLibraryManager& component_library() noexcept { return component_library_; }
    [[nodiscard]] const ComponentLibraryManager& component_library() const noexcept { return component_library_; }

    [[nodiscard]] std::vector<InteractiveCircuitDocument>& circuits() noexcept { return circuits_; }
    [[nodiscard]] const std::vector<InteractiveCircuitDocument>& circuits() const noexcept { return circuits_; }
    [[nodiscard]] InteractiveCircuitDocument& active_circuit_doc();
    [[nodiscard]] const InteractiveCircuitDocument& active_circuit_doc() const;
    void set_active_circuit_index(size_t idx);
    [[nodiscard]] size_t active_circuit_index() const noexcept { return active_circuit_idx_; }

    [[nodiscard]] PhysicsSimulationRuntime& physics_runtime() noexcept { return physics_runtime_; }
    [[nodiscard]] const PhysicsSimulationRuntime& physics_runtime() const noexcept { return physics_runtime_; }

    [[nodiscard]] core::domain::SceneGraph& scene_graph() noexcept { return scene_graph_; }
    [[nodiscard]] const core::domain::SceneGraph& scene_graph() const noexcept { return scene_graph_; }

    [[nodiscard]] core::domain::CommandStack& command_stack() noexcept { return command_stack_; }
    [[nodiscard]] const core::domain::CommandStack& command_stack() const noexcept { return command_stack_; }

    [[nodiscard]] core::domain::CircuitNetlist& active_circuit() noexcept { return legacy_circuit_; }
    [[nodiscard]] const core::domain::CircuitNetlist& active_circuit() const noexcept { return legacy_circuit_; }

    [[nodiscard]] AmbientEnvironmentConfig& environment() noexcept { return env_; }
    [[nodiscard]] const AmbientEnvironmentConfig& environment() const noexcept { return env_; }
    void set_environment(const AmbientEnvironmentConfig& env);

    // Reactive dependency notification & propagation
    uint64_t subscribe(ChangeCallback cb);
    void unsubscribe(uint64_t token);
    void notify_change(DependencyChangeKind kind,
                       const core::base::Uuid& target_id = {},
                       std::string summary = "");

    using DependentInvalidator = std::function<void(const core::base::Uuid& material_id)>;
    void register_material_dependent_invalidator(DependentInvalidator fn);
    void notify_material_modified(const core::base::Uuid& material_id, std::string summary = "");
    void notify_object_modified(const core::base::Uuid& object_id, std::string summary = "");

    // Complete Project Persistence (v3.0 bundle preserving all UUID references)
    [[nodiscard]] std::string to_bundle_json() const;
    [[nodiscard]] core::base::Result<void> from_bundle_json(std::string_view json_str);
    [[nodiscard]] core::base::Result<void> save_to_file(const std::filesystem::path& path);
    [[nodiscard]] core::base::Result<void> load_from_file(const std::filesystem::path& path);

    // Seed complete real Lithium Battery -> Circuit workflow using ElementDatabase
    [[nodiscard]] core::base::Result<void> seed_lithium_battery_workflow();

    [[nodiscard]] uint64_t version() const noexcept { return version_; }
    [[nodiscard]] core::base::ValidationReport validate_workspace() const;

private:
    std::shared_ptr<core::domain::Project> project_;
    std::vector<mat::MixtureExperiment> mixtures_;
    core::domain::MaterialRegistry materials_;
    PhysicalObjectManager object_manager_;
    std::vector<BatteryDesignSpec> batteries_;
    ComponentLibraryManager component_library_;
    std::vector<InteractiveCircuitDocument> circuits_;
    size_t active_circuit_idx_{0};
    PhysicsSimulationRuntime physics_runtime_;
    core::domain::SceneGraph scene_graph_;
    core::domain::CommandStack command_stack_{200};
    core::domain::CircuitNetlist legacy_circuit_;
    AmbientEnvironmentConfig env_;

    uint64_t version_{1};
    uint64_t next_token_{1};
    std::unordered_map<uint64_t, ChangeCallback> listeners_;
    std::vector<DependentInvalidator> material_invalidators_;
};

} // namespace shunya::eng
