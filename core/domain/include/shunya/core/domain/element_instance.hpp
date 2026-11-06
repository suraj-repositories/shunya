#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/domain/element_definition.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <optional>
#include <functional>

namespace shunya::core::domain {

// -------------------------------------------------------------
// Element Lifecycle States (ELM-05)
// -------------------------------------------------------------
enum class LifecycleState : uint8_t {
    Uninitialized,
    Configured,
    Active,
    Paused,
    Error,
    Terminated
};

[[nodiscard]] constexpr std::string_view lifecycle_state_to_string(LifecycleState state) noexcept {
    switch (state) {
        case LifecycleState::Uninitialized: return "Uninitialized";
        case LifecycleState::Configured:    return "Configured";
        case LifecycleState::Active:        return "Active";
        case LifecycleState::Paused:        return "Paused";
        case LifecycleState::Error:         return "Error";
        case LifecycleState::Terminated:    return "Terminated";
    }
    return "Uninitialized";
}

// -------------------------------------------------------------
// Port Instance & Connections (ELM-02, ELM-04)
// -------------------------------------------------------------
struct PortConnectionTarget {
    base::Uuid target_instance_id;
    std::string target_port_name;

    bool operator==(const PortConnectionTarget& rhs) const = default;
};

class PortInstance {
public:
    explicit PortInstance(std::string name, PortSchema schema)
        : name_(std::move(name)), schema_(std::move(schema)) {}

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] const PortSchema& schema() const noexcept { return schema_; }

    [[nodiscard]] bool is_connected() const noexcept { return !connections_.empty(); }
    [[nodiscard]] const std::vector<PortConnectionTarget>& connections() const noexcept { return connections_; }

    [[nodiscard]] base::Result<void> connect_to(const base::Uuid& target_instance, std::string target_port);
    void disconnect_from(const base::Uuid& target_instance, std::string_view target_port);
    void disconnect_all() noexcept { connections_.clear(); }

private:
    std::string name_;
    PortSchema schema_;
    std::vector<PortConnectionTarget> connections_;
};

// -------------------------------------------------------------
// Element Instance (ELM-02, ELM-05)
// -------------------------------------------------------------
class ElementInstance {
public:
    ElementInstance(
        std::shared_ptr<const ElementDefinition> definition,
        std::string instance_name,
        base::Uuid id = base::Uuid::generate_random()
    );

    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] std::shared_ptr<const ElementDefinition> definition() const noexcept { return definition_; }

    [[nodiscard]] const base::math::Xform& transform() const noexcept { return transform_; }
    base::math::Xform& transform() noexcept { return transform_; }
    void set_transform(const base::math::Xform& xform) noexcept { transform_ = xform; }

    // Parameter value access & overrides
    [[nodiscard]] base::Result<ParameterValue> get_parameter(std::string_view param_name) const;
    [[nodiscard]] base::Result<void> set_parameter(std::string_view param_name, ParameterValue value);
    [[nodiscard]] bool has_parameter_override(std::string_view param_name) const noexcept;
    void reset_parameter_to_default(std::string_view param_name);
    [[nodiscard]] const std::unordered_map<std::string, ParameterValue>& parameter_overrides() const noexcept {
        return parameter_overrides_;
    }

    // Ports
    [[nodiscard]] PortInstance* get_port(std::string_view port_name) noexcept;
    [[nodiscard]] const PortInstance* get_port(std::string_view port_name) const noexcept;
    [[nodiscard]] const std::vector<PortInstance>& ports() const noexcept { return ports_; }

    // Lifecycle operations (ELM-05)
    [[nodiscard]] LifecycleState lifecycle_state() const noexcept { return state_; }
    [[nodiscard]] base::Result<void> initialize();
    [[nodiscard]] base::Result<void> update(double dt);
    [[nodiscard]] base::Result<void> reset();
    [[nodiscard]] base::Result<void> pause();
    [[nodiscard]] base::Result<void> resume();
    [[nodiscard]] base::Result<void> terminate();

    [[nodiscard]] double simulated_time() const noexcept { return simulated_time_; }

    // Duplication (ELM-05)
    [[nodiscard]] std::shared_ptr<ElementInstance> duplicate(
        std::string new_name = "",
        const base::math::Vec3& position_offset = {0.0, 0.0, 0.0}
    ) const;

    // Serialization
    [[nodiscard]] std::string to_json() const;

private:
    base::Uuid id_;
    std::string name_;
    std::shared_ptr<const ElementDefinition> definition_;
    base::math::Xform transform_{};
    std::unordered_map<std::string, ParameterValue> parameter_overrides_;
    std::vector<PortInstance> ports_;

    LifecycleState state_ = LifecycleState::Uninitialized;
    double simulated_time_ = 0.0;
};

// -------------------------------------------------------------
// Element Inspector & Introspection (ELM-07)
// -------------------------------------------------------------
struct InspectorProperty {
    std::string name;
    std::string display_label;
    std::string group;
    std::string current_value_str;
    std::string default_value_str;
    std::string unit;
    ParameterType type = ParameterType::Double;
    bool is_overridden = false;
    bool is_read_only = false;
    std::optional<double> min_value;
    std::optional<double> max_value;
    std::vector<std::string> enum_options;
    std::string contextual_help;
    bool is_valid = true;
    std::string validation_error;
};

class ElementInspector {
public:
    [[nodiscard]] static std::vector<InspectorProperty> inspect(const ElementInstance& instance);
    [[nodiscard]] static base::Result<void> apply_change(
        ElementInstance& instance,
        std::string_view property_name,
        const ParameterValue& new_value
    );
};

// -------------------------------------------------------------
// Element Registry & Connection Harness (ELM-08)
// -------------------------------------------------------------
class ElementRegistry {
public:
    ElementRegistry() = default;

    // Definition catalog
    void register_definition(std::shared_ptr<const ElementDefinition> def);
    [[nodiscard]] bool has_definition(const base::Uuid& id) const noexcept;
    [[nodiscard]] bool has_definition(std::string_view type_name) const noexcept;
    [[nodiscard]] std::shared_ptr<const ElementDefinition> get_definition(const base::Uuid& id) const noexcept;
    [[nodiscard]] std::shared_ptr<const ElementDefinition> get_definition(std::string_view type_name) const noexcept;
    [[nodiscard]] std::vector<std::shared_ptr<const ElementDefinition>> list_definitions() const;
    [[nodiscard]] std::vector<std::shared_ptr<const ElementDefinition>> list_by_category(ElementCategory cat) const;

    // Instance management
    [[nodiscard]] base::Result<std::shared_ptr<ElementInstance>> create_instance(
        std::string_view type_name,
        std::string instance_name
    );
    [[nodiscard]] std::shared_ptr<ElementInstance> get_instance(const base::Uuid& id) const noexcept;
    [[nodiscard]] bool remove_instance(const base::Uuid& id);
    [[nodiscard]] std::size_t instance_count() const noexcept { return instances_.size(); }
    void clear_instances() noexcept;

    // Connection wiring and contract verification (ELM-08)
    [[nodiscard]] base::Result<void> connect(
        const base::Uuid& src_instance_id,
        std::string_view src_port_name,
        const base::Uuid& dst_instance_id,
        std::string_view dst_port_name
    );

    [[nodiscard]] base::Result<void> disconnect(
        const base::Uuid& src_instance_id,
        std::string_view src_port_name,
        const base::Uuid& dst_instance_id,
        std::string_view dst_port_name
    );

private:
    std::unordered_map<base::Uuid, std::shared_ptr<const ElementDefinition>> definitions_by_id_;
    std::unordered_map<std::string, std::shared_ptr<const ElementDefinition>> definitions_by_type_;
    std::unordered_map<base::Uuid, std::shared_ptr<ElementInstance>> instances_;
};

} // namespace shunya::core::domain
