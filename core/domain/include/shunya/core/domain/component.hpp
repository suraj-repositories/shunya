#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/element_definition.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <optional>
#include <memory>

namespace shunya::core::domain {

// -------------------------------------------------------------
// Encapsulation and Access Control (CMP-03)
// -------------------------------------------------------------
enum class EncapsulationLevel : uint8_t {
    Public,           // Observable and connectable from external parent
    Protected,        // Connectable within library / subsystem hierarchy
    InternalPrivate   // Strictly internal; hidden topology
};

[[nodiscard]] constexpr std::string_view encapsulation_level_to_string(EncapsulationLevel lvl) noexcept {
    switch (lvl) {
        case EncapsulationLevel::Public:          return "Public";
        case EncapsulationLevel::Protected:       return "Protected";
        case EncapsulationLevel::InternalPrivate: return "InternalPrivate";
    }
    return "Public";
}

[[nodiscard]] base::Result<EncapsulationLevel> encapsulation_level_from_string(std::string_view s);

// -------------------------------------------------------------
// Component Port (CMP-01, CMP-03)
// -------------------------------------------------------------
struct ComponentPort {
    PortSchema schema;
    EncapsulationLevel encapsulation{EncapsulationLevel::Public};
    std::string internal_binding_target; // optional internal child/port binding
    bool is_inspectable{true};

    bool operator==(const ComponentPort& o) const noexcept {
        return schema.name == o.schema.name &&
               schema.domain == o.schema.domain &&
               schema.direction == o.schema.direction &&
               encapsulation == o.encapsulation &&
               internal_binding_target == o.internal_binding_target &&
               is_inspectable == o.is_inspectable;
    }
};

// -------------------------------------------------------------
// Internal Topology Wiring (CMP-02, CMP-03)
// -------------------------------------------------------------
struct InternalConnection {
    base::Uuid id{base::Uuid::generate_random()};
    std::string from_child; // empty string indicates parent/self component boundary port
    std::string from_port;
    std::string to_child;   // empty string indicates parent/self component boundary port
    std::string to_port;

    bool operator==(const InternalConnection& o) const noexcept {
        return id == o.id &&
               from_child == o.from_child &&
               from_port == o.from_port &&
               to_child == o.to_child &&
               to_port == o.to_port;
    }
};

// -------------------------------------------------------------
// Child Component Specification (CMP-02)
// -------------------------------------------------------------
struct ChildComponentNode {
    std::string instance_name;
    base::Uuid definition_id;
    std::string type_name;
    std::unordered_map<std::string, ParameterValue> parameter_overrides;

    bool operator==(const ChildComponentNode& o) const noexcept {
        return instance_name == o.instance_name &&
               definition_id == o.definition_id &&
               type_name == o.type_name &&
               parameter_overrides == o.parameter_overrides;
    }
};

// -------------------------------------------------------------
// Component Model Variant (CMP-04)
// -------------------------------------------------------------
struct ComponentVariant {
    std::string variant_id{"default"};
    std::string display_name{"Default Variant"};
    FidelityTier fidelity{FidelityTier::L1_Functional};
    std::string implementation_type{"Analytical"}; // "Analytical", "MNA_Subcircuit", "StateSpace", "TransferFunction", "Script"
    std::string model_expression;                 // Mathematical equation / formula / behavioral script
    std::unordered_map<std::string, ParameterValue> default_overrides;
    std::vector<std::string> supported_solvers;    // e.g. "Transient", "AC_Frequency", "DC_OperatingPoint"

    bool operator==(const ComponentVariant& o) const noexcept {
        return variant_id == o.variant_id &&
               display_name == o.display_name &&
               fidelity == o.fidelity &&
               implementation_type == o.implementation_type &&
               model_expression == o.model_expression &&
               default_overrides == o.default_overrides &&
               supported_solvers == o.supported_solvers;
    }
};

// -------------------------------------------------------------
// Component Definition (CMP-01, CMP-02, CMP-03, CMP-04, CMP-05)
// -------------------------------------------------------------
class ComponentDefinition {
public:
    ComponentDefinition();
    ComponentDefinition(std::string type_name, std::string display_name, base::Version version = {1, 0, 0});
    ComponentDefinition(base::Uuid id, std::string type_name, std::string display_name, base::Version version = {1, 0, 0});

    // Identity & Metadata
    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    void set_id(const base::Uuid& id) noexcept { id_ = id; }

    [[nodiscard]] const std::string& type_name() const noexcept { return type_name_; }
    void set_type_name(std::string name) { type_name_ = std::move(name); }

    [[nodiscard]] const std::string& display_name() const noexcept { return display_name_; }
    void set_display_name(std::string name) { display_name_ = std::move(name); }

    [[nodiscard]] const std::string& description() const noexcept { return description_; }
    void set_description(std::string desc) { description_ = std::move(desc); }

    [[nodiscard]] const std::string& author() const noexcept { return author_; }
    void set_author(std::string auth) { author_ = std::move(auth); }

    [[nodiscard]] const std::string& license() const noexcept { return license_; }
    void set_license(std::string lic) { license_ = std::move(lic); }

    [[nodiscard]] const base::Version& version() const noexcept { return version_; }
    void set_version(const base::Version& v) noexcept { version_ = v; }

    [[nodiscard]] const std::vector<std::string>& tags() const noexcept { return tags_; }
    void add_tag(std::string tag);
    [[nodiscard]] bool has_tag(std::string_view tag) const noexcept;

    // Symbol & Visual Presentation
    [[nodiscard]] const std::string& symbol_svg() const noexcept { return symbol_svg_; }
    void set_symbol_svg(std::string svg) { symbol_svg_ = std::move(svg); }

    [[nodiscard]] const std::string& geometry_model_ref() const noexcept { return geometry_model_ref_; }
    void set_geometry_model_ref(std::string ref) { geometry_model_ref_ = std::move(ref); }

    // Ports (CMP-01, CMP-03)
    [[nodiscard]] base::Result<void> add_port(ComponentPort port);
    [[nodiscard]] bool remove_port(std::string_view port_name);
    [[nodiscard]] const ComponentPort* get_port(std::string_view port_name) const noexcept;
    [[nodiscard]] bool has_port(std::string_view port_name) const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, ComponentPort>& ports() const noexcept { return ports_; }
    [[nodiscard]] std::vector<ComponentPort> public_ports() const;

    // Parameters (CMP-01)
    [[nodiscard]] base::Result<void> add_parameter(ParameterSchema param);
    [[nodiscard]] bool remove_parameter(std::string_view param_name);
    [[nodiscard]] const ParameterSchema* get_parameter(std::string_view param_name) const noexcept;
    [[nodiscard]] bool has_parameter(std::string_view param_name) const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, ParameterSchema>& parameters() const noexcept { return parameters_; }

    // Hierarchical Children (CMP-02)
    [[nodiscard]] base::Result<void> add_child(ChildComponentNode child);
    [[nodiscard]] bool remove_child(std::string_view instance_name);
    [[nodiscard]] const ChildComponentNode* get_child(std::string_view instance_name) const noexcept;
    [[nodiscard]] bool has_child(std::string_view instance_name) const noexcept;
    [[nodiscard]] const std::vector<ChildComponentNode>& children() const noexcept { return children_; }
    [[nodiscard]] bool is_composite() const noexcept { return !children_.empty(); }

    // Internal Connections (CMP-02, CMP-03)
    [[nodiscard]] base::Result<void> add_connection(InternalConnection conn);
    [[nodiscard]] bool remove_connection(const base::Uuid& conn_id);
    [[nodiscard]] const std::vector<InternalConnection>& internal_connections() const noexcept { return connections_; }

    // Model Variants (CMP-04)
    [[nodiscard]] base::Result<void> add_variant(ComponentVariant variant);
    [[nodiscard]] bool set_active_variant(std::string_view variant_id);
    [[nodiscard]] const ComponentVariant* get_variant(std::string_view variant_id) const noexcept;
    [[nodiscard]] const ComponentVariant& active_variant() const noexcept;
    [[nodiscard]] const std::vector<ComponentVariant>& variants() const noexcept { return variants_; }

    // Validation (CMP-05)
    [[nodiscard]] base::ValidationReport validate() const;

    // Serialization
    [[nodiscard]] std::string to_json() const;
    static base::Result<ComponentDefinition> from_json(std::string_view json_str);

private:
    base::Uuid id_{base::Uuid::generate_random()};
    std::string type_name_{"GenericComponent"};
    std::string display_name_{"Generic Component"};
    std::string description_;
    std::string author_{"Shunya Team"};
    std::string license_{"Proprietary"};
    base::Version version_{1, 0, 0};
    std::vector<std::string> tags_;
    std::string symbol_svg_;
    std::string geometry_model_ref_;

    std::unordered_map<std::string, ComponentPort> ports_;
    std::unordered_map<std::string, ParameterSchema> parameters_;
    std::vector<ChildComponentNode> children_;
    std::vector<InternalConnection> connections_;
    std::vector<ComponentVariant> variants_;
    std::string active_variant_id_{"default"};
};

} // namespace shunya::core::domain
