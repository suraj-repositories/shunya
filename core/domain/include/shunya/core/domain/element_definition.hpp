#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/units.hpp"
#include "shunya/core/base/math.hpp"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <variant>
#include <optional>
#include <memory>

namespace shunya::core::domain {

// -------------------------------------------------------------
// Element Categories (ELM-06)
// -------------------------------------------------------------
enum class ElementCategory : uint8_t {
    PhysicalObject, // 3D solid structural, geometric component
    Material,       // Material / constitutive property block
    Field,          // Ambient field (magnetic, thermal, gravity)
    Source,         // Active excitation (voltage, current, force, pressure)
    Sensor,         // Measuring transducer (tachometer, thermocouple, voltmeter)
    Actuator,       // Converter (PMSM motor, solenoid, piezo)
    Device,         // Passive / active device (resistor, valve, gear)
    LogicBlock,     // Controller / block diagram (PID, filter, state machine)
    AbstractNode    // Reference frame, electrical ground, junction
};

[[nodiscard]] constexpr std::string_view element_category_to_string(ElementCategory cat) noexcept {
    switch (cat) {
        case ElementCategory::PhysicalObject: return "PhysicalObject";
        case ElementCategory::Material:       return "Material";
        case ElementCategory::Field:          return "Field";
        case ElementCategory::Source:         return "Source";
        case ElementCategory::Sensor:         return "Sensor";
        case ElementCategory::Actuator:       return "Actuator";
        case ElementCategory::Device:         return "Device";
        case ElementCategory::LogicBlock:     return "LogicBlock";
        case ElementCategory::AbstractNode:   return "AbstractNode";
    }
    return "PhysicalObject";
}

[[nodiscard]] std::optional<ElementCategory> element_category_from_string(std::string_view s) noexcept;

// -------------------------------------------------------------
// Parameter Types and Values (ELM-03)
// -------------------------------------------------------------
enum class ParameterType : uint8_t {
    Double,
    Int64,
    Bool,
    String,
    Quantity,
    Vec3,
    Enum
};

[[nodiscard]] constexpr std::string_view parameter_type_to_string(ParameterType type) noexcept {
    switch (type) {
        case ParameterType::Double:   return "Double";
        case ParameterType::Int64:    return "Int64";
        case ParameterType::Bool:     return "Bool";
        case ParameterType::String:   return "String";
        case ParameterType::Quantity: return "Quantity";
        case ParameterType::Vec3:     return "Vec3";
        case ParameterType::Enum:     return "Enum";
    }
    return "Double";
}

class ParameterValue {
public:
    using Storage = std::variant<
        double,
        int64_t,
        bool,
        std::string,
        base::units::Quantity,
        base::math::Vec3
    >;

    ParameterValue() : storage_(0.0) {}
    ParameterValue(double val) : storage_(val) {}
    ParameterValue(int64_t val) : storage_(val) {}
    ParameterValue(int val) : storage_(static_cast<int64_t>(val)) {}
    ParameterValue(bool val) : storage_(val) {}
    ParameterValue(std::string val) : storage_(std::move(val)) {}
    ParameterValue(const char* val) : storage_(std::string(val)) {}
    ParameterValue(base::units::Quantity val) : storage_(val) {}
    ParameterValue(base::math::Vec3 val) : storage_(val) {}

    [[nodiscard]] ParameterType type() const noexcept {
        return std::visit([](auto&& arg) -> ParameterType {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, double>) return ParameterType::Double;
            else if constexpr (std::is_same_v<T, int64_t>) return ParameterType::Int64;
            else if constexpr (std::is_same_v<T, bool>) return ParameterType::Bool;
            else if constexpr (std::is_same_v<T, std::string>) return ParameterType::String;
            else if constexpr (std::is_same_v<T, base::units::Quantity>) return ParameterType::Quantity;
            else if constexpr (std::is_same_v<T, base::math::Vec3>) return ParameterType::Vec3;
            return ParameterType::Double;
        }, storage_);
    }

    [[nodiscard]] double as_double() const { return std::get<double>(storage_); }
    [[nodiscard]] int64_t as_int64() const { return std::get<int64_t>(storage_); }
    [[nodiscard]] bool as_bool() const { return std::get<bool>(storage_); }
    [[nodiscard]] const std::string& as_string() const { return std::get<std::string>(storage_); }
    [[nodiscard]] const base::units::Quantity& as_quantity() const { return std::get<base::units::Quantity>(storage_); }
    [[nodiscard]] const base::math::Vec3& as_vec3() const { return std::get<base::math::Vec3>(storage_); }

    [[nodiscard]] std::string to_string() const;
    [[nodiscard]] const Storage& raw() const noexcept { return storage_; }

    bool operator==(const ParameterValue& rhs) const = default;

private:
    Storage storage_;
};

struct ParameterSchema {
    std::string name;
    std::string display_name;
    std::string description;
    ParameterType type = ParameterType::Double;
    ParameterValue default_value = 0.0;
    std::string unit = "";
    std::optional<double> min_value = std::nullopt;
    std::optional<double> max_value = std::nullopt;
    std::vector<std::string> enum_options;
    bool is_read_only = false;
    bool is_visible = true;

    [[nodiscard]] base::Result<void> validate(const ParameterValue& value) const;
};

// -------------------------------------------------------------
// Multi-Domain Ports (ELM-04)
// -------------------------------------------------------------
enum class PortDomain : uint8_t {
    Electrical,
    MechanicalTranslational,
    MechanicalRotational,
    Thermal,
    Fluid,
    Signal,
    Optical
};

[[nodiscard]] constexpr std::string_view port_domain_to_string(PortDomain dom) noexcept {
    switch (dom) {
        case PortDomain::Electrical:              return "Electrical";
        case PortDomain::MechanicalTranslational: return "MechanicalTranslational";
        case PortDomain::MechanicalRotational:    return "MechanicalRotational";
        case PortDomain::Thermal:                 return "Thermal";
        case PortDomain::Fluid:                   return "Fluid";
        case PortDomain::Signal:                  return "Signal";
        case PortDomain::Optical:                 return "Optical";
    }
    return "Electrical";
}

[[nodiscard]] std::optional<PortDomain> port_domain_from_string(std::string_view s) noexcept;

enum class PortDirection : uint8_t {
    Input,
    Output,
    Bidirectional
};

[[nodiscard]] constexpr std::string_view port_direction_to_string(PortDirection dir) noexcept {
    switch (dir) {
        case PortDirection::Input:         return "Input";
        case PortDirection::Output:        return "Output";
        case PortDirection::Bidirectional: return "Bidirectional";
    }
    return "Bidirectional";
}

enum class PortCardinality : uint8_t {
    Single,     // 1-to-1 connection
    Multiple    // 1-to-many (e.g. common bus / node)
};

struct PortSchema {
    std::string name;
    std::string display_name;
    std::string description;
    PortDomain domain = PortDomain::Electrical;
    PortDirection direction = PortDirection::Bidirectional;
    PortCardinality cardinality = PortCardinality::Single;
    std::string expected_unit = "";

    [[nodiscard]] bool can_connect_to(const PortSchema& other) const noexcept;
};

// -------------------------------------------------------------
// Element Definition (ELM-01)
// -------------------------------------------------------------
class ElementDefinition {
public:
    ElementDefinition(
        std::string type_name,
        std::string display_name,
        ElementCategory category,
        base::Version version = {1, 0, 0, "final"},
        base::Uuid id = base::Uuid::generate_random()
    );

    [[nodiscard]] const base::Uuid& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& type_name() const noexcept { return type_name_; }
    [[nodiscard]] const std::string& display_name() const noexcept { return display_name_; }
    [[nodiscard]] ElementCategory category() const noexcept { return category_; }
    [[nodiscard]] const base::Version& version() const noexcept { return version_; }
    [[nodiscard]] const std::string& description() const noexcept { return description_; }
    void set_description(std::string desc) { description_ = std::move(desc); }

    [[nodiscard]] const std::string& author() const noexcept { return author_; }
    void set_author(std::string auth) { author_ = std::move(auth); }

    // Parameters
    void add_parameter(ParameterSchema schema);
    [[nodiscard]] bool has_parameter(std::string_view param_name) const noexcept;
    [[nodiscard]] const ParameterSchema* get_parameter_schema(std::string_view param_name) const noexcept;
    [[nodiscard]] const std::vector<ParameterSchema>& parameters() const noexcept { return parameters_; }

    // Ports
    void add_port(PortSchema schema);
    [[nodiscard]] bool has_port(std::string_view port_name) const noexcept;
    [[nodiscard]] const PortSchema* get_port_schema(std::string_view port_name) const noexcept;
    [[nodiscard]] const std::vector<PortSchema>& ports() const noexcept { return ports_; }

    // Metadata
    void set_metadata(std::string key, std::string value) { metadata_[std::move(key)] = std::move(value); }
    [[nodiscard]] std::optional<std::string_view> get_metadata(std::string_view key) const noexcept;
    [[nodiscard]] const std::unordered_map<std::string, std::string>& metadata() const noexcept { return metadata_; }

    // Serialization
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<std::shared_ptr<ElementDefinition>> from_json(std::string_view json_str);

private:
    base::Uuid id_;
    std::string type_name_;
    std::string display_name_;
    ElementCategory category_;
    base::Version version_;
    std::string description_;
    std::string author_ = "Shunya Platform";

    std::vector<ParameterSchema> parameters_;
    std::vector<PortSchema> ports_;
    std::unordered_map<std::string, std::string> metadata_;
};

} // namespace shunya::core::domain
