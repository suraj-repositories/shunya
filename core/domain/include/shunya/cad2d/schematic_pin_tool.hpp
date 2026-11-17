#pragma once

#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/schematic_symbol_engine.hpp"
#include "shunya/pcb/schematic_hierarchy.hpp"
#include "shunya/pcb/symbol_library.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::cad2d {

enum class PinOrientation {
    Right, // Stalk extends right (+X)
    Left,  // Stalk extends left (-X)
    Up,    // Stalk extends up (+Y)
    Down   // Stalk extends down (-Y)
};

[[nodiscard]] constexpr std::string_view pin_orientation_to_string(PinOrientation o) noexcept {
    switch (o) {
        case PinOrientation::Right: return "Right";
        case PinOrientation::Left:  return "Left";
        case PinOrientation::Up:    return "Up";
        case PinOrientation::Down:  return "Down";
    }
    return "Right";
}

struct SchematicPinSpec {
    std::string pin_id;
    std::string pin_number{"1"};
    std::string pin_name{"PIN"};
    pcb::PinElectricalType electrical_type{pcb::PinElectricalType::Passive};
    PinOrientation orientation{PinOrientation::Right};
    Point2D position{0.0, 0.0};
    double length_mm{5.08};
    bool is_clock{false};
    bool is_inverted{false};
    std::vector<std::string> graphic_entity_ids;
};

/**
 * @brief Schematic Pin Placement Tool for defining electrical terminals,
 *        pin numbers, names, electrical types, and orientations in 2D schematics.
 */
class SchematicPinPlacementTool {
public:
    SchematicPinPlacementTool() = default;

    // Default configuration for subsequent pin placements
    void set_next_pin_number(int num) noexcept { next_number_ = num; auto_increment_ = true; }
    void set_next_pin_name(std::string name) { next_name_ = std::move(name); }
    void set_default_electrical_type(pcb::PinElectricalType type) noexcept { default_type_ = type; }
    void set_default_orientation(PinOrientation ori) noexcept { default_orientation_ = ori; }
    void set_default_length(double len_mm) noexcept { default_length_mm_ = len_mm; }
    void set_auto_increment(bool enable) noexcept { auto_increment_ = enable; }

    [[nodiscard]] int next_pin_number() const noexcept { return next_number_; }
    [[nodiscard]] std::string_view next_pin_name() const noexcept { return next_name_; }
    [[nodiscard]] pcb::PinElectricalType default_electrical_type() const noexcept { return default_type_; }
    [[nodiscard]] PinOrientation default_orientation() const noexcept { return default_orientation_; }
    [[nodiscard]] double default_length() const noexcept { return default_length_mm_; }

    /**
     * @brief Places a new schematic pin, synthesizing terminal stalk and terminal anchor dot into doc.
     */
    std::string place_pin(
        SketchDocument2D& doc,
        Point2D position,
        std::optional<std::string> pin_number = std::nullopt,
        std::optional<std::string> pin_name = std::nullopt,
        std::optional<pcb::PinElectricalType> electrical_type = std::nullopt,
        std::optional<PinOrientation> orientation = std::nullopt
    );

    bool remove_pin(SketchDocument2D& doc, std::string_view pin_id);
    void clear(SketchDocument2D& doc);

    [[nodiscard]] const SchematicPinSpec* find_pin(std::string_view pin_id) const noexcept;
    [[nodiscard]] const std::vector<SchematicPinSpec>& all_pins() const noexcept { return pins_; }
    [[nodiscard]] size_t pin_count() const noexcept { return pins_.size(); }

    // Geometric anchor for wire connections
    [[nodiscard]] Point2D terminal_anchor(const SchematicPinSpec& pin) const noexcept;
    [[nodiscard]] std::string hit_test_pin(Point2D pos, double tol = 2.0) const;

    // Export to standard pcb symbol definition pins
    [[nodiscard]] std::vector<pcb::SymbolPinDef> export_symbol_pin_defs() const;

private:
    int next_number_{1};
    std::string next_name_{"PIN"};
    pcb::PinElectricalType default_type_{pcb::PinElectricalType::Passive};
    PinOrientation default_orientation_{PinOrientation::Right};
    double default_length_mm_{5.08};
    bool auto_increment_{true};

    uint64_t next_pin_idx_{1};
    std::vector<SchematicPinSpec> pins_;
};

} // namespace shunya::cad2d
