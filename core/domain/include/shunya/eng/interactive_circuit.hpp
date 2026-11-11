#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/uuid.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/core/domain/command_stack.hpp"
#include "shunya/eng/battery_engineer.hpp"
#include "shunya/eng/component_generator.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::eng {

enum class CircuitSymbolKind : uint8_t {
    VoltageSourceDC,
    VoltageSourceAC,
    Resistor,
    Capacitor,
    Inductor,
    Diode,
    LED,
    Switch,
    Battery,
    Ground,
    Voltmeter,
    Ammeter,
    HeatingElement,
    ConductorWire
};

[[nodiscard]] constexpr std::string_view circuit_symbol_kind_to_string(CircuitSymbolKind k) noexcept {
    switch (k) {
    case CircuitSymbolKind::VoltageSourceDC: return "VoltageSourceDC";
    case CircuitSymbolKind::VoltageSourceAC: return "VoltageSourceAC";
    case CircuitSymbolKind::Resistor:        return "Resistor";
    case CircuitSymbolKind::Capacitor:       return "Capacitor";
    case CircuitSymbolKind::Inductor:        return "Inductor";
    case CircuitSymbolKind::Diode:           return "Diode";
    case CircuitSymbolKind::LED:             return "LED";
    case CircuitSymbolKind::Switch:          return "Switch";
    case CircuitSymbolKind::Battery:         return "Battery";
    case CircuitSymbolKind::Ground:          return "Ground";
    case CircuitSymbolKind::Voltmeter:       return "Voltmeter";
    case CircuitSymbolKind::Ammeter:         return "Ammeter";
    case CircuitSymbolKind::HeatingElement:  return "HeatingElement";
    case CircuitSymbolKind::ConductorWire:   return "ConductorWire";
    }
    return "Resistor";
}

[[nodiscard]] core::base::Result<CircuitSymbolKind> circuit_symbol_kind_from_string(std::string_view s);

struct PlacedCircuitComponent {
    std::string id; // e.g., "V1", "R1", "BAT1", "LED1", "SW1", "GND1"
    std::string label;
    CircuitSymbolKind kind{CircuitSymbolKind::Resistor};
    std::optional<core::base::Uuid> generated_component_id;
    double x_mm{0.0};
    double y_mm{0.0};
    int rotation_deg{0}; // 0, 90, 180, 270
    double primary_value{1000.0}; // Ohm, F, H, V, etc.
    double secondary_value{0.0};  // Battery R0, AC freq Hz, LED Vf
    double tertiary_value{0.0};   // Battery capacity Ah, AC dc_offset
    bool switch_closed{true};
    double max_current_rating_a{10.0};
    double max_power_rating_w{50.0};
    double temperature_k{298.15};
    std::vector<std::string> terminal_nets; // resolved net names per terminal

    [[nodiscard]] std::vector<std::pair<std::string, core::base::math::Vector2<double>>>
    terminal_world_positions() const;
};

struct CircuitWire {
    std::string id;
    std::vector<core::base::math::Vector2<double>> points_mm;
    std::string net_name;
};

struct CircuitJunction {
    std::string id;
    core::base::math::Vector2<double> pos_mm{0.0, 0.0};
    std::string net_name;
};

/**
 * @brief Interactive schematic circuit document supporting component placement, wiring, junctions,
 *        undo/redo, connectivity resolution, ERC validation, and netlist compilation.
 */
class InteractiveCircuitDocument {
public:
    explicit InteractiveCircuitDocument(std::string name = "Main_Circuit")
        : id_(core::base::Uuid::generate_random()), name_(std::move(name)) {}

    [[nodiscard]] const core::base::Uuid& id() const noexcept { return id_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string n) { name_ = std::move(n); }

    void add_component(PlacedCircuitComponent comp);
    bool move_component(std::string_view id, double x_mm, double y_mm);
    bool rotate_component(std::string_view id, int delta_deg = 90);
    bool remove_component(std::string_view id);
    [[nodiscard]] PlacedCircuitComponent* find_component_mut(std::string_view id) noexcept;
    [[nodiscard]] const PlacedCircuitComponent* find_component(std::string_view id) const noexcept;

    void add_wire(CircuitWire wire);
    bool remove_wire(std::string_view id);
    void add_junction(CircuitJunction junc);
    bool remove_junction(std::string_view id);
    void clear();

    [[nodiscard]] const std::vector<PlacedCircuitComponent>& components() const noexcept { return components_; }
    [[nodiscard]] const std::vector<CircuitWire>& wires() const noexcept { return wires_; }
    [[nodiscard]] const std::vector<CircuitJunction>& junctions() const noexcept { return junctions_; }

    // Synchronize parameters from generated component library
    void sync_from_component_library(const ComponentLibraryManager& lib);

    // Union-Find geometric connectivity -> assigns terminal_nets & wire/junction net_names
    void resolve_connectivity(double snap_tol_mm = 1.0);

    [[nodiscard]] core::base::Result<core::domain::CircuitNetlist> compile_netlist() const;
    [[nodiscard]] std::vector<core::domain::CircuitDiagnosticIssue> validate() const;

    [[nodiscard]] InteractiveCircuitDocument duplicate(std::string new_name) const;
    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static core::base::Result<InteractiveCircuitDocument> from_json(std::string_view json_str);

private:
    core::base::Uuid id_;
    std::string name_;
    std::vector<PlacedCircuitComponent> components_;
    std::vector<CircuitWire> wires_;
    std::vector<CircuitJunction> junctions_;
};

struct CircuitComponentMetrics {
    std::string component_id;
    CircuitSymbolKind kind{CircuitSymbolKind::Resistor};
    double voltage_v{0.0};
    double current_a{0.0};
    double power_w{0.0};
    double effective_resistance_ohm{0.0};
    double temperature_k{298.15};
    double soc{1.0};
    bool led_lit{false};
    bool over_limit{false};
};

struct CircuitDcSummary {
    bool converged{false};
    double kcl_max_residual_a{0.0};
    std::unordered_map<std::string, double> node_voltages;
    std::vector<CircuitComponentMetrics> component_metrics;
    std::vector<std::string> warnings;
    std::string error_message;
};

struct CircuitTransientSummary {
    bool success{false};
    std::vector<double> time_s;
    std::unordered_map<std::string, std::vector<double>> node_voltages;
    std::unordered_map<std::string, std::vector<double>> component_currents_a;
    std::unordered_map<std::string, std::vector<double>> component_voltages_v;
    std::unordered_map<std::string, std::vector<double>> component_powers_w;
    std::unordered_map<std::string, std::vector<double>> component_temperatures_k;
    std::vector<double> battery_soc;
    double kcl_max_residual_a{0.0};
    std::vector<std::string> warnings;
    std::string error_message;
};

class CircuitSimulationRunner {
public:
    [[nodiscard]] static core::base::Result<CircuitDcSummary> run_dc(
        const InteractiveCircuitDocument& doc);

    [[nodiscard]] static core::base::Result<CircuitTransientSummary> run_transient(
        const InteractiveCircuitDocument& doc,
        double duration_s,
        double dt_s,
        const ComponentLibraryManager* lib = nullptr,
        double ambient_temp_k = 298.15);
};

} // namespace shunya::eng
