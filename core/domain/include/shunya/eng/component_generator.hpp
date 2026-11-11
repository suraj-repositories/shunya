#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/eng/physical_object.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::eng {

enum class ComponentKind : uint8_t {
    Resistor,
    ConductorWire,
    Switch,
    Capacitor,
    Inductor,
    Battery,
    Connector,
    HeatingElement,
    Motor
};

[[nodiscard]] constexpr std::string_view component_kind_to_string(ComponentKind k) noexcept {
    switch (k) {
    case ComponentKind::Resistor:       return "Resistor";
    case ComponentKind::ConductorWire:  return "ConductorWire";
    case ComponentKind::Switch:         return "Switch";
    case ComponentKind::Capacitor:      return "Capacitor";
    case ComponentKind::Inductor:       return "Inductor";
    case ComponentKind::Battery:        return "Battery";
    case ComponentKind::Connector:      return "Connector";
    case ComponentKind::HeatingElement: return "HeatingElement";
    case ComponentKind::Motor:          return "Motor";
    }
    return "Resistor";
}

[[nodiscard]] core::base::Result<ComponentKind> component_kind_from_string(std::string_view s);

struct ComponentTerminal {
    std::string name{"T1"};
    core::domain::ElectricalPinPolarity polarity{core::domain::ElectricalPinPolarity::PassiveBiDirectional};
    core::base::math::Vector3<double> local_anchor{0.0, 0.0, 0.0};
    std::string target_region_id{"whole"};
};

struct ComponentTypeConfig {
    // Type-specific parameters configured during Object-to-Component generation
    double conductor_length_m{0.0};       // 0 -> infer from object characteristic length
    double cross_section_area_m2{0.0};    // 0 -> infer from object cross-section
    double dielectric_thickness_m{1e-3};  // Capacitor plate spacing
    double relative_permittivity{3.5};    // Capacitor dielectric eps_r
    int coil_turns{100};                  // Inductor / Motor turns
    double core_relative_permeability{1.0};
    double contact_resistance_ohm{1e-3};  // Switch / Connector contact resistance
    double max_operating_temp_k{420.0};
    bool initially_closed{true};          // Switch default state

    // Battery-specific derived specs (populated by BatteryEngineer)
    double nominal_voltage_v{3.7};
    double capacity_ah{2.5};
    double internal_resistance_ohm{0.05};
    double initial_soc{1.0};
};

struct DerivedComponentSpecs {
    bool is_qualified{false};
    double primary_value{0.0};        // R (Ohm), C (F), L (H), V_nom (V)
    double series_resistance_ohm{0.0};
    double thermal_mass_j_k{0.0};
    double thermal_conductance_w_k{0.0};
    double max_current_a{0.0};
    double max_power_w{0.0};
    double max_voltage_v{1e6};
    std::vector<std::string> qualification_errors;
    std::vector<std::string> warnings;
};

struct GeneratedComponentDefinition {
    core::base::Uuid id{core::base::Uuid::generate_random()};
    std::string name{"Component"};
    ComponentKind kind{ComponentKind::Resistor};
    core::base::Uuid source_object_id; // Live link; does not duplicate object/material
    std::optional<core::base::Uuid> battery_spec_id;
    std::vector<ComponentTerminal> terminals;
    ComponentTypeConfig config;
    DerivedComponentSpecs derived;

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static core::base::Result<GeneratedComponentDefinition> from_json(std::string_view json_str);
};

/**
 * @brief Converts material-backed PhysicalObjects into qualified functional circuit components,
 *        keeping derived parameters synchronized with object geometry and material properties.
 */
class ComponentGenerator {
public:
    [[nodiscard]] static DerivedComponentSpecs qualify_and_derive(
        const PhysicalObject& obj,
        ComponentKind kind,
        const ComponentTypeConfig& cfg,
        const std::vector<ComponentTerminal>& terminals,
        const core::domain::MaterialRegistry& reg);

    [[nodiscard]] static core::base::Result<GeneratedComponentDefinition> generate(
        std::string name,
        const PhysicalObject& obj,
        ComponentKind kind,
        ComponentTypeConfig cfg,
        std::vector<ComponentTerminal> terminals,
        const core::domain::MaterialRegistry& reg);
};

class ComponentLibraryManager {
public:
    ComponentLibraryManager() = default;

    [[nodiscard]] core::base::Result<core::base::Uuid> register_component(GeneratedComponentDefinition comp);
    bool remove_component(const core::base::Uuid& id);
    [[nodiscard]] const GeneratedComponentDefinition* get_component(const core::base::Uuid& id) const noexcept;
    [[nodiscard]] GeneratedComponentDefinition* get_component_mut(const core::base::Uuid& id) noexcept;
    [[nodiscard]] const GeneratedComponentDefinition* find_by_name(std::string_view name) const noexcept;
    [[nodiscard]] const std::vector<GeneratedComponentDefinition>& components() const noexcept { return components_; }
    [[nodiscard]] size_t count() const noexcept { return components_.size(); }
    void clear() noexcept { components_.clear(); }

    void recalculate_for_object(const PhysicalObject& obj, const core::domain::MaterialRegistry& reg);
    void recalculate_all(const PhysicalObjectManager& objs, const core::domain::MaterialRegistry& reg);

private:
    std::vector<GeneratedComponentDefinition> components_;
};

} // namespace shunya::eng
