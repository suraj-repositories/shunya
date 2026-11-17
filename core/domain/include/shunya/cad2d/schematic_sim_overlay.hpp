#pragma once

#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/schematic_netlabel_bus.hpp"
#include "shunya/cad2d/schematic_pin_tool.hpp"
#include "shunya/cad2d/schematic_symbol_engine.hpp"
#include "shunya/cad2d/schematic_wire_router.hpp"
#include "shunya/core/domain/circuit.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::cad2d {

struct VoltageBadgeItem {
    std::string node_name;
    Point2D position;
    double voltage_v{0.0};
    std::string formatted_text;
    std::string color_hex;
    std::string graphic_entity_id;
};

struct CurrentProbeItem {
    std::string component_or_wire_id;
    Point2D position;
    Point2D direction_arrow_end;
    double current_a{0.0};
    std::string formatted_text;
    std::string graphic_entity_id;
};

struct PowerBadgeItem {
    std::string component_id;
    Point2D position;
    double power_w{0.0};
    std::string formatted_text;
    std::string graphic_entity_id;
};

struct ProbeMeasurement {
    bool has_data{false};
    std::string target_label;
    double voltage_v{0.0};
    double current_a{0.0};
    double power_w{0.0};
    std::string formatted_summary;
};

/**
 * @brief Real-time simulation overlay renderer displaying live node voltages,
 * branch currents, and component power dissipation directly onto schematic wires and pins.
 */
class SchematicSimOverlay {
public:
    SchematicSimOverlay() = default;

    // Display options
    void set_show_voltages(bool show) noexcept { show_voltages_ = show; }
    void set_show_currents(bool show) noexcept { show_currents_ = show; }
    void set_show_power(bool show) noexcept { show_power_ = show; }

    [[nodiscard]] bool show_voltages() const noexcept { return show_voltages_; }
    [[nodiscard]] bool show_currents() const noexcept { return show_currents_; }
    [[nodiscard]] bool show_power() const noexcept { return show_power_; }

    // SI formatting helpers
    [[nodiscard]] static std::string format_voltage(double volts);
    [[nodiscard]] static std::string format_current(double amperes);
    [[nodiscard]] static std::string format_power(double watts);
    [[nodiscard]] static std::string voltage_to_color(double volts);

    /**
     * @brief Generates overlay visual badges and indicators in doc based on live simulation operating point.
     */
    void render_overlay(
        SketchDocument2D& doc,
        const core::domain::DCOperatingPointResult& sim_result,
        const SchematicDraftingEngine& symbol_engine,
        const SchematicWireRouter& wire_router,
        const SchematicPinPlacementTool* pin_tool = nullptr
    );

    /**
     * @brief Clears all overlay visual badges from document.
     */
    void clear_overlay(SketchDocument2D& doc);

    // Interactive probe tool
    [[nodiscard]] ProbeMeasurement probe_at(
        Point2D pos,
        const core::domain::DCOperatingPointResult& sim_result,
        const SchematicDraftingEngine& symbol_engine,
        const SchematicWireRouter& wire_router,
        double tol = 2.0
    ) const;

    [[nodiscard]] const std::vector<VoltageBadgeItem>& voltage_badges() const noexcept { return voltage_badges_; }
    [[nodiscard]] const std::vector<CurrentProbeItem>& current_probes() const noexcept { return current_probes_; }
    [[nodiscard]] const std::vector<PowerBadgeItem>& power_badges() const noexcept { return power_badges_; }

private:
    bool show_voltages_{true};
    bool show_currents_{true};
    bool show_power_{true};

    std::vector<VoltageBadgeItem> voltage_badges_;
    std::vector<CurrentProbeItem> current_probes_;
    std::vector<PowerBadgeItem> power_badges_;
};

} // namespace shunya::cad2d
