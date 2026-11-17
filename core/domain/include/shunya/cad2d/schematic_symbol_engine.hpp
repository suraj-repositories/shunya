#pragma once

#include "shunya/cad2d/construction_layer.hpp"
#include "shunya/cad2d/curve_2d.hpp"
#include "shunya/cad2d/layer_manager.hpp"
#include "shunya/pcb/symbol_library.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::cad2d {

enum class DraftingDomain {
    Mechanical,
    Schematic
};

[[nodiscard]] constexpr std::string_view drafting_domain_to_string(DraftingDomain d) noexcept {
    switch (d) {
        case DraftingDomain::Mechanical: return "Mechanical";
        case DraftingDomain::Schematic:  return "Schematic";
    }
    return "Mechanical";
}

struct SchematicPinInstance {
    std::string pin_id;
    std::string symbol_instance_id;
    std::string pin_number;
    std::string pin_name;
    pcb::PinElectricalType electrical_type{pcb::PinElectricalType::Passive};
    Point2D world_pos;
};

struct SchematicComponentInstance {
    std::string instance_id;
    std::string symbol_id;
    std::string reference_designator; // e.g. "R1", "C2", "U1"
    std::string value;                // e.g. "10k", "100nF", "NE555"
    Point2D position;
    int rotation_deg{0};
    bool mirror_x{false};
    bool mirror_y{false};
    std::vector<std::string> graphic_entity_ids;
    std::vector<SchematicPinInstance> pins;
};

/**
 * @brief Seamless integration engine coupling electronic schematic symbol drafting
 *        with the 2D CAD environment while strictly isolating mechanical drafting modes.
 */
class SchematicDraftingEngine {
public:
    static constexpr std::string_view SCHEMATIC_LAYER_SYMBOLS = "Schematic_Symbols";
    static constexpr std::string_view SCHEMATIC_LAYER_PINS = "Schematic_Pins";
    static constexpr std::string_view SCHEMATIC_LAYER_WIRES = "Schematic_Wires";
    static constexpr std::string_view SCHEMATIC_LAYER_ANNOTATIONS = "Schematic_Annotations";

    explicit SchematicDraftingEngine(pcb::SymbolStandard standard = pcb::SymbolStandard::IEEE_US);

    // Active domain mode
    [[nodiscard]] DraftingDomain current_mode() const noexcept { return current_mode_; }
    void set_mode(DraftingDomain mode) noexcept { current_mode_ = mode; }

    // Symbol catalog
    [[nodiscard]] pcb::SymbolLibraryManager& catalog() noexcept { return catalog_; }
    [[nodiscard]] const pcb::SymbolLibraryManager& catalog() const noexcept { return catalog_; }

    // Layer initialization
    void ensure_schematic_layers(LayerManager& layer_mgr);

    // Symbol placement & drafting
    std::string place_symbol(
        SketchDocument2D& doc,
        std::string_view symbol_id,
        Point2D position,
        int rotation_deg = 0,
        std::string ref_des = "",
        std::string value = ""
    );

    bool remove_symbol(SketchDocument2D& doc, std::string_view instance_id);
    void clear_symbols(SketchDocument2D& doc);

    [[nodiscard]] const SchematicComponentInstance* find_instance(std::string_view instance_id) const noexcept;
    [[nodiscard]] const SchematicComponentInstance* find_instance_by_pin(std::string_view pin_id) const noexcept;
    [[nodiscard]] const SchematicPinInstance* find_pin(std::string_view pin_id) const noexcept;
    [[nodiscard]] std::vector<const SchematicComponentInstance*> all_instances() const;
    [[nodiscard]] size_t instance_count() const noexcept { return instances_.size(); }

    // All pin query for snapping
    [[nodiscard]] std::vector<SchematicPinInstance> all_pins() const;

    // Mechanical vs Schematic Isolation
    [[nodiscard]] bool is_schematic_entity(const SketchDocument2D& doc, std::string_view entity_id) const;
    [[nodiscard]] std::vector<std::string> filter_mechanical_entities(const SketchDocument2D& doc) const;
    [[nodiscard]] std::vector<std::string> filter_schematic_entities(const SketchDocument2D& doc) const;

    /**
     * @brief Extracts solid profile loops purely from mechanical entities, guaranteeing
     * zero contamination from schematic symbols, wires, or pins.
     */
    [[nodiscard]] Paths2D extract_pure_mechanical_profiles(
        const SketchDocument2D& doc,
        size_t tessellation_segments = 32,
        double endpoint_tol = 1e-6
    ) const;

    /**
     * @brief Asserts and validates that mechanical extrusion is 100% uncontaminated.
     */
    [[nodiscard]] bool validate_mechanical_isolation(const SketchDocument2D& doc) const;

private:
    DraftingDomain current_mode_{DraftingDomain::Mechanical};
    pcb::SymbolLibraryManager catalog_;
    uint64_t next_instance_idx_{1};
    uint64_t next_pin_idx_{1};
    std::unordered_map<std::string, SchematicComponentInstance> instances_;
    std::unordered_map<std::string, std::string> entity_to_instance_map_;
    std::unordered_map<std::string, std::string> pin_to_instance_map_;
};

} // namespace shunya::cad2d
