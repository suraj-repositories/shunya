#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/thread_pool.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/core/domain/command_stack.hpp"
#include "shunya/core/domain/material_assignment.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/scene_graph.hpp"
#include "shunya/eng/battery_engineer.hpp"
#include "shunya/eng/component_generator.hpp"
#include "shunya/eng/entity_relationship_graph.hpp"
#include "shunya/eng/interactive_circuit.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/physics_runtime.hpp"
#include "shunya/mat/material_mixing_lab.hpp"
#include "shunya/matdb/element_database.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
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
    EnvironmentUpdated,
    SimulationStepped,
    AssemblyUpdated
};

struct WorkspaceChangeEvent {
    DependencyChangeKind kind{DependencyChangeKind::ProjectReset};
    core::base::Uuid target_id{};
    uint64_t workspace_version{0};
    std::string summary;
};

struct SubsystemTicks {
    uint64_t global{1};
    uint64_t project{1};
    uint64_t element_database{1};
    uint64_t materials{1};
    uint64_t mixtures{1};
    uint64_t objects{1};
    uint64_t components{1};
    uint64_t batteries{1};
    uint64_t circuits{1};
    uint64_t environment{1};
    uint64_t simulation{1};
};

struct StateMutationEntry {
    uint64_t tick{0};
    DependencyChangeKind kind{DependencyChangeKind::ProjectReset};
    core::base::Uuid target_id{};
    std::string summary;
};

struct WorkspaceCheckpoint {
    uint64_t id{0};
    std::string name;
    uint64_t tick{0};
    std::string bundle_snapshot;
};

struct Shunya4SaveOptions {
    bool incremental{false};
    uint64_t base_tick{0};
    bool binary_format{false};
};

struct ProjectLoadProgress {
    double fraction{0.0};
    std::string stage;
};

using LoadProgressCallback = std::function<void(const ProjectLoadProgress& progress)>;

struct ProjectSaveProgress {
    double fraction{0.0};
    std::string stage;
};

using SaveProgressCallback = std::function<void(const ProjectSaveProgress& progress)>;

struct GarbageCollectionReport {
    size_t pruned_materials{0};
    size_t repaired_regions{0};
    size_t pruned_components{0};
    size_t pruned_circuit_instances{0};
    size_t pruned_dangling_wires{0};
    std::vector<std::string> actions_taken;
};

struct CorruptionRecoveryReport {
    bool recovered{false};
    size_t salvaged_materials{0};
    size_t salvaged_objects{0};
    size_t salvaged_components{0};
    size_t salvaged_circuits{0};
    GarbageCollectionReport gc_report;
    std::vector<std::string> repairs_applied;
    std::vector<std::string> unrecoverable_errors;
};

#pragma pack(push, 1)
struct Shunya4BinaryHeader {
    char magic[8]{'S', 'H', 'U', 'N', 'Y', 'A', '4', '\0'};
    uint32_t version{4};
    uint32_t flags{0}; // Bit 0: incremental, Bit 1: binary chunk payload
    uint64_t base_tick{0};
    uint64_t current_tick{0};
    uint32_t checksum{0};
    uint32_t reserved{0};
    uint64_t payload_size{0};
};
#pragma pack(pop)

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

    // Complete Project Persistence (.shunya4 / .shunya3 bundle preserving all UUID references)
    [[nodiscard]] std::string to_bundle_json(const Shunya4SaveOptions& options = {}) const;
    [[nodiscard]] std::vector<uint8_t> to_shunya4_binary(const Shunya4SaveOptions& options = {}) const;
    [[nodiscard]] core::base::Result<void> from_bundle_json(std::string_view json_str);
    [[nodiscard]] core::base::Result<void> from_shunya4_binary(const uint8_t* data, size_t size);
    [[nodiscard]] core::base::Result<void> from_shunya4_binary(const std::vector<uint8_t>& binary_data);
    [[nodiscard]] core::base::Result<void> save_to_file(
        const std::filesystem::path& path,
        const Shunya4SaveOptions& options = {});
    [[nodiscard]] core::base::Result<void> save_to_file_with_progress(
        const std::filesystem::path& path,
        const Shunya4SaveOptions& options = {},
        SaveProgressCallback progress_cb = nullptr,
        core::base::CancellationToken cancel_token = {});
    [[nodiscard]] std::future<core::base::Result<void>> save_to_file_async(
        const std::filesystem::path& path,
        const Shunya4SaveOptions& options = {},
        SaveProgressCallback progress_cb = nullptr,
        core::base::CancellationToken cancel_token = {});
    [[nodiscard]] core::base::Result<void> load_from_file(const std::filesystem::path& path);
    [[nodiscard]] core::base::Result<void> load_from_file_with_progress(
        const std::filesystem::path& path,
        LoadProgressCallback progress_cb = nullptr,
        core::base::CancellationToken cancel_token = {});
    [[nodiscard]] std::future<core::base::Result<void>> load_from_file_async(
        const std::filesystem::path& path,
        LoadProgressCallback progress_cb = nullptr,
        core::base::CancellationToken cancel_token = {});

    // Entity-Relationship Graph (PH4-01.03.005)
    [[nodiscard]] EntityRelationshipGraph build_entity_graph() const;

    // Automated Data Corruption Recovery & Garbage Collection (PH4-01.03.006)
    GarbageCollectionReport collect_garbage_and_prune_orphans(bool prune_unreferenced_materials = false);
    core::base::Result<CorruptionRecoveryReport> recover_from_corrupted_bundle(std::string_view corrupted_data);
    core::base::Result<CorruptionRecoveryReport> recover_from_corrupted_file(const std::filesystem::path& path);

    // Seed complete real Lithium Battery -> Circuit workflow using ElementDatabase
    [[nodiscard]] core::base::Result<void> seed_lithium_battery_workflow();

    // Transactional Checkpoints and Multi-Domain Undo/Redo (PH4-01.03.002)
    uint64_t create_checkpoint(std::string name);
    core::base::Result<void> restore_checkpoint(uint64_t checkpoint_id);
    [[nodiscard]] std::vector<WorkspaceCheckpoint> checkpoints() const;
    [[nodiscard]] std::optional<WorkspaceCheckpoint> find_checkpoint(uint64_t id) const;
    [[nodiscard]] std::optional<WorkspaceCheckpoint> find_checkpoint(std::string_view name) const;
    bool delete_checkpoint(uint64_t id);

    core::base::Result<void> execute_transaction(
        std::string name,
        std::function<core::base::Result<void>()> action);

    core::base::Result<void> undo();
    core::base::Result<void> redo();
    [[nodiscard]] bool can_undo() const noexcept { return command_stack_.can_undo(); }
    [[nodiscard]] bool can_redo() const noexcept { return command_stack_.can_redo(); }

    [[nodiscard]] uint64_t version() const noexcept { return ticks_.global; }
    [[nodiscard]] uint64_t global_tick() const noexcept { return ticks_.global; }
    [[nodiscard]] uint64_t tick_for(DependencyChangeKind kind) const noexcept;
    [[nodiscard]] bool is_dirty_since(DependencyChangeKind kind, uint64_t last_seen_tick) const noexcept;
    [[nodiscard]] bool is_any_dirty_since(uint64_t last_seen_global_tick) const noexcept;
    [[nodiscard]] const SubsystemTicks& ticks() const noexcept { return ticks_; }
    void touch(DependencyChangeKind kind, const core::base::Uuid& target_id = {}, std::string summary = "");
    [[nodiscard]] std::vector<StateMutationEntry> recent_mutations(size_t limit = 50) const;

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
    core::domain::CommandStack command_stack_{0}; // 0 = unlimited undo/redo history
    core::domain::CircuitNetlist legacy_circuit_;
    AmbientEnvironmentConfig env_;

    SubsystemTicks ticks_{};
    std::vector<StateMutationEntry> mutation_history_;
    uint64_t next_checkpoint_id_{1};
    std::vector<WorkspaceCheckpoint> checkpoints_;
    uint64_t next_token_{1};
    std::unordered_map<uint64_t, ChangeCallback> listeners_;
    std::vector<DependentInvalidator> material_invalidators_;
};

} // namespace shunya::eng
