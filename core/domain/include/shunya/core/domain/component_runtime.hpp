#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/domain/component.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

namespace shunya::core::domain {

// -------------------------------------------------------------
// Component Runtime Instance (CMP-07)
// -------------------------------------------------------------
enum class RuntimeLifecycleState : uint8_t {
    Uninitialized,
    Ready,
    Running,
    Paused,
    ErrorTerminated
};

[[nodiscard]] constexpr std::string_view runtime_lifecycle_state_to_string(RuntimeLifecycleState s) noexcept {
    switch (s) {
        case RuntimeLifecycleState::Uninitialized:   return "Uninitialized";
        case RuntimeLifecycleState::Ready:           return "Ready";
        case RuntimeLifecycleState::Running:         return "Running";
        case RuntimeLifecycleState::Paused:          return "Paused";
        case RuntimeLifecycleState::ErrorTerminated: return "ErrorTerminated";
    }
    return "Uninitialized";
}

// Step delegate function type: (double dt, double current_time, inputs, state_vars) -> returns updated state
using StepFunction = std::function<base::Result<void>(double dt, double sim_time,
                                                      const std::unordered_map<std::string, double>& inputs,
                                                      std::unordered_map<std::string, double>& state_vars,
                                                      std::unordered_map<std::string, double>& outputs)>;

class ComponentRuntimeInstance {
public:
    ComponentRuntimeInstance();
    explicit ComponentRuntimeInstance(std::shared_ptr<const ComponentDefinition> definition,
                                      std::string instance_name = "instance");

    [[nodiscard]] const base::Uuid& instance_id() const noexcept { return instance_id_; }
    [[nodiscard]] const std::string& instance_name() const noexcept { return instance_name_; }
    void set_instance_name(std::string name) { instance_name_ = std::move(name); }

    [[nodiscard]] std::shared_ptr<const ComponentDefinition> definition() const noexcept { return definition_; }
    void set_definition(std::shared_ptr<const ComponentDefinition> def);

    [[nodiscard]] RuntimeLifecycleState lifecycle_state() const noexcept { return lifecycle_state_; }
    [[nodiscard]] double simulated_time() const noexcept { return simulated_time_; }
    [[nodiscard]] uint64_t step_count() const noexcept { return step_count_; }

    // Parameters
    void set_parameter(std::string name, ParameterValue val);
    [[nodiscard]] std::optional<ParameterValue> get_parameter(std::string_view name) const;
    [[nodiscard]] const std::unordered_map<std::string, ParameterValue>& parameters() const noexcept { return parameters_; }

    // Ports / IO Buffers
    void set_input_value(std::string port_name, double val);
    [[nodiscard]] double get_input_value(std::string_view port_name) const noexcept;
    void set_output_value(std::string port_name, double val);
    [[nodiscard]] double get_output_value(std::string_view port_name) const noexcept;

    // State Variables (CMP-07)
    void set_state_variable(std::string var_name, double val);
    [[nodiscard]] double get_state_variable(std::string_view var_name) const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, double>& state_variables() const noexcept { return state_vars_; }

    // Lifecycle transitions
    [[nodiscard]] base::Result<void> initialize();
    [[nodiscard]] base::Result<void> step(double dt);
    [[nodiscard]] base::Result<void> reset();
    void set_custom_step_evaluator(StepFunction step_fn) { custom_step_ = std::move(step_fn); }

    // State Persistence & Restart (CMP-07)
    [[nodiscard]] std::string serialize_state_json() const;
    [[nodiscard]] base::Result<void> restore_state_json(std::string_view json_str);

private:
    base::Uuid instance_id_{base::Uuid::generate_random()};
    std::string instance_name_{"instance"};
    std::shared_ptr<const ComponentDefinition> definition_;
    RuntimeLifecycleState lifecycle_state_{RuntimeLifecycleState::Uninitialized};
    double simulated_time_{0.0};
    uint64_t step_count_{0};

    std::unordered_map<std::string, ParameterValue> parameters_;
    std::unordered_map<std::string, double> input_values_;
    std::unordered_map<std::string, double> output_values_;
    std::unordered_map<std::string, double> state_vars_;
    StepFunction custom_step_;
};

// -------------------------------------------------------------
// Component Editor & Fluent Builder (CMP-06)
// -------------------------------------------------------------
class ComponentBuilder {
public:
    ComponentBuilder(std::string type_name, std::string display_name);

    ComponentBuilder& set_id(base::Uuid id);
    ComponentBuilder& set_version(base::Version version);
    ComponentBuilder& set_description(std::string desc);
    ComponentBuilder& set_author(std::string author);
    ComponentBuilder& set_license(std::string license);
    ComponentBuilder& add_tag(std::string tag);
    ComponentBuilder& set_symbol_svg(std::string svg);
    ComponentBuilder& set_geometry_ref(std::string ref);

    // Fluent Ports
    ComponentBuilder& add_electrical_port(std::string name, PortDirection dir = PortDirection::Bidirectional,
                                         EncapsulationLevel enc = EncapsulationLevel::Public);
    ComponentBuilder& add_thermal_port(std::string name, PortDirection dir = PortDirection::Bidirectional,
                                      EncapsulationLevel enc = EncapsulationLevel::Public);
    ComponentBuilder& add_mechanical_port(std::string name, bool is_rotational = false,
                                          PortDirection dir = PortDirection::Bidirectional,
                                          EncapsulationLevel enc = EncapsulationLevel::Public);
    ComponentBuilder& add_fluid_port(std::string name, PortDirection dir = PortDirection::Bidirectional,
                                     EncapsulationLevel enc = EncapsulationLevel::Public);
    ComponentBuilder& add_signal_port(std::string name, PortDirection dir = PortDirection::Input,
                                      EncapsulationLevel enc = EncapsulationLevel::Public);
    ComponentBuilder& add_port(ComponentPort port);

    // Fluent Parameters
    ComponentBuilder& add_numeric_param(std::string name, double default_val, std::string unit = "",
                                        std::optional<double> min_val = std::nullopt,
                                        std::optional<double> max_val = std::nullopt);
    ComponentBuilder& add_string_param(std::string name, std::string default_val);
    ComponentBuilder& add_bool_param(std::string name, bool default_val);
    ComponentBuilder& add_parameter(ParameterSchema schema);

    // Fluent Variants & Composition
    ComponentBuilder& add_variant(ComponentVariant variant);
    ComponentBuilder& add_child(std::string instance_name, base::Uuid def_id, std::string type_name);
    ComponentBuilder& connect(std::string from_child, std::string from_port,
                              std::string to_child, std::string to_port);

    [[nodiscard]] base::Result<ComponentDefinition> build();

private:
    ComponentDefinition comp_;
};

// -------------------------------------------------------------
// Component Packaging (CMP-08)
// -------------------------------------------------------------
struct DependencyLockEntry {
    std::string package_name;
    base::Version required_version;
    std::string content_hash_sha256;

    bool operator==(const DependencyLockEntry& o) const noexcept {
        return package_name == o.package_name &&
               required_version == o.required_version &&
               content_hash_sha256 == o.content_hash_sha256;
    }
};

struct AssetEntry {
    std::string relative_path;
    std::string mime_type;
    std::string content_sha256;
    std::string text_payload; // for script/svg/text assets
};

class ComponentPackage {
public:
    ComponentPackage();
    ComponentPackage(std::string package_id, std::string name, base::Version version = {1, 0, 0});

    [[nodiscard]] const std::string& package_id() const noexcept { return package_id_; }
    void set_package_id(std::string id) { package_id_ = std::move(id); }

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] const base::Version& version() const noexcept { return version_; }
    void set_version(const base::Version& v) noexcept { version_ = v; }

    [[nodiscard]] const std::string& author() const noexcept { return author_; }
    void set_author(std::string auth) { author_ = std::move(auth); }

    [[nodiscard]] const std::string& description() const noexcept { return description_; }
    void set_description(std::string desc) { description_ = std::move(desc); }

    // Contained Component Definitions
    void add_component(ComponentDefinition comp);
    [[nodiscard]] const ComponentDefinition* get_component(std::string_view type_name) const noexcept;
    [[nodiscard]] const std::vector<ComponentDefinition>& components() const noexcept { return components_; }

    // Dependency Lockfile (CMP-08)
    void add_dependency_lock(DependencyLockEntry entry);
    [[nodiscard]] const std::vector<DependencyLockEntry>& dependency_locks() const noexcept { return dependencies_; }

    // Assets
    void add_asset(AssetEntry asset);
    [[nodiscard]] const std::vector<AssetEntry>& assets() const noexcept { return assets_; }

    // Validation & Integrity Check
    [[nodiscard]] base::ValidationReport validate_package() const;
    [[nodiscard]] uint32_t compute_package_crc32() const;

    // Archive Serialization
    [[nodiscard]] std::string serialize_manifest_json() const;
    static base::Result<ComponentPackage> deserialize_manifest_json(std::string_view json_str);

private:
    std::string package_id_{"org.shunya.package"};
    std::string name_{"Unnamed Package"};
    base::Version version_{1, 0, 0};
    std::string author_{"Shunya Team"};
    std::string description_;
    std::vector<ComponentDefinition> components_;
    std::vector<DependencyLockEntry> dependencies_;
    std::vector<AssetEntry> assets_;
};

} // namespace shunya::core::domain
