#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/ecad3d/component_designer_workspace.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::ecad3d {

/**
 * @brief Representation of a 2D schematic symbol pin.
 */
struct SchematicSymbolPin {
    std::string pin_id;                 // e.g., "1", "2", "ANODE", "CATHODE"
    int pin_number{1};
    std::string pin_label;              // Display label (e.g. "+", "-", "IN", "OUT")
    core::domain::ElectricalPinPolarity polarity{core::domain::ElectricalPinPolarity::PassiveBiDirectional};
    core::base::math::Vec2 position{0.0, 0.0}; // Local 2D schematic coordinates
    core::base::math::Vec2 orientation{1.0, 0.0}; // Connection lead direction

    SchematicSymbolPin() = default;
    SchematicSymbolPin(
        std::string id,
        int num,
        std::string label,
        core::domain::ElectricalPinPolarity pol = core::domain::ElectricalPinPolarity::PassiveBiDirectional,
        core::base::math::Vec2 pos = {0.0, 0.0},
        core::base::math::Vec2 orient = {1.0, 0.0}
    ) : pin_id(std::move(id)), pin_number(num), pin_label(std::move(label)),
        polarity(pol), position(pos), orientation(orient) {}
};

/**
 * @brief 2D Circuit Schematic Symbol coupled to a 3D physical component.
 */
struct SchematicSymbol2D {
    std::string symbol_id;
    std::string standard{"IEEE"};       // "IEEE" or "IEC"
    eng::ComponentKind component_kind{eng::ComponentKind::Resistor};
    std::vector<SchematicSymbolPin> pins;
    std::string primitive_shape;        // "zigzag", "rectangle", "parallel_plates", "inductor_loops", "diode_arrow"
    core::base::math::Vec2 bounding_box_size{20.0, 10.0};
};

/**
 * @brief Mapping entry linking a 3D physical terminal to a 2D schematic pin.
 */
struct PinTerminalMappingEntry {
    std::string terminal_3d_id;
    std::string symbol_pin_id;
    int pin_number{1};
    bool verified{false};
};

/**
 * @brief Spatial orientation mapping for 3D component mounting on PCB/substrate.
 */
struct SpatialMountingOrientation {
    cad3d::Vec3d mounting_normal{0.0, 0.0, 1.0}; // Surface normal for PCB foot
    cad3d::Vec3d primary_alignment_axis{1.0, 0.0, 0.0}; // Direction of Pin 1 -> Pin 2
    double default_rotation_deg{0.0};
    cad3d::Point3D center_offset{0.0, 0.0, 0.0};
};

/**
 * @brief Automated 2D symbol generator synthesizing standard IEEE/IEC schematic symbols.
 */
class SchematicSymbolSynthesizer {
public:
    [[nodiscard]] static SchematicSymbol2D synthesize_symbol(
        eng::ComponentKind kind,
        std::string_view standard = "IEEE",
        int pin_count = 2
    );
};

/**
 * @brief Interactive Pin-to-Terminal Mapper and bidirectional selection coordinator.
 */
class PinTerminalMapper {
public:
    PinTerminalMapper() = default;

    void initialize(const std::vector<PhysicalTerminal3D>& terminals, const SchematicSymbol2D& symbol);

    [[nodiscard]] bool is_complete() const noexcept;
    [[nodiscard]] const std::vector<PinTerminalMappingEntry>& mappings() const noexcept { return mappings_; }

    core::base::Result<void> map_pin(const std::string& terminal_3d_id, const std::string& symbol_pin_id);
    core::base::Result<void> auto_map_by_number_and_polarity();

    // Bidirectional selection lookup
    [[nodiscard]] std::optional<std::string> get_symbol_pin_for_terminal(const std::string& terminal_3d_id) const;
    [[nodiscard]] std::optional<std::string> get_terminal_for_symbol_pin(const std::string& symbol_pin_id) const;

    // Spatial orientation
    void set_mounting_orientation(const SpatialMountingOrientation& orientation) { orientation_ = orientation; }
    [[nodiscard]] const SpatialMountingOrientation& mounting_orientation() const noexcept { return orientation_; }

private:
    std::vector<PhysicalTerminal3D> terminals_;
    SchematicSymbol2D symbol_;
    std::vector<PinTerminalMappingEntry> mappings_;
    SpatialMountingOrientation orientation_;
};

/**
 * @brief Architectural invariant verifier: prevents 3D models from becoming electronic components
 *        without verified, coupled 2D circuit schematic symbols.
 */
class ComponentSymbolCouplingEngine {
public:
    struct VerificationResult {
        bool is_coupled{false};
        std::vector<std::string> rejection_reasons;
    };

    [[nodiscard]] static VerificationResult verify_coupling(
        const ComponentAuthoringData& component_3d,
        const SchematicSymbol2D& symbol_2d,
        const PinTerminalMapper& mapper
    );
};

} // namespace shunya::ecad3d
