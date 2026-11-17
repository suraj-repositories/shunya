#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/ecad3d/component_designer_workspace.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
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
 * (Roadmap task PH4-06.02.005)
 */
enum class MountingPlane {
    XY_TopSurface,     // Standard top SMD / through-hole
    XY_BottomSurface,  // Bottom side of PCB
    XZ_Vertical,       // Vertical standing component (e.g. radial capacitor, TO-220 standing)
    YZ_Vertical,       // Side edge mount
    ThroughHole_Z      // Pin leads penetrate through substrate in -Z direction
};

struct SpatialMountingOrientation {
    MountingPlane mounting_plane{MountingPlane::XY_TopSurface};
    cad3d::Vec3d mounting_normal{0.0, 0.0, 1.0}; // Surface normal for PCB foot
    cad3d::Vec3d primary_alignment_axis{1.0, 0.0, 0.0}; // Direction of Pin 1 -> Pin 2
    cad3d::Vec3d up_axis{0.0, 0.0, 1.0};
    double default_rotation_deg{0.0};
    double seating_height_mm{0.0}; // Standoff distance from PCB surface
    double rotation_step_deg{90.0}; // Pick-and-place rotation granularity
    cad3d::Point3D center_offset{0.0, 0.0, 0.0};

    [[nodiscard]] cad3d::Point3D transform_terminal_to_pcb(
        const cad3d::Point3D& local_pos,
        double yaw_deg = 0.0,
        bool mirror_bottom = false
    ) const;

    [[nodiscard]] std::pair<cad3d::Point3D, cad3d::Point3D> compute_mounting_footprint_bounds(
        const std::vector<PhysicalTerminal3D>& terminals,
        double clearance_mm = 0.5
    ) const;

    [[nodiscard]] bool is_valid() const noexcept {
        return primary_alignment_axis.length_squared() > 1e-6 &&
               mounting_normal.length_squared() > 1e-6;
    }
};

/**
 * @brief Selection highlight coordinator for bidirectional 3D <-> 2D synchronization.
 * (Roadmap task PH4-06.02.006)
 */
enum class SelectionSource {
    None,
    Viewport3D,
    Schematic2D
};

struct SelectionHighlightEvent {
    std::string terminal_3d_id;
    std::string symbol_pin_id;
    int pin_number{-1};
    SelectionSource source{SelectionSource::None};
};

class SelectionHighlightCoordinator {
public:
    using Listener = std::function<void(const SelectionHighlightEvent&)>;

    void select_from_3d(const std::string& terminal_3d_id, const class PinTerminalMapper& mapper);
    void select_from_2d(const std::string& symbol_pin_id, const class PinTerminalMapper& mapper);
    void select_by_pin_number(int pin_number, const class PinTerminalMapper& mapper);
    void clear_selection();

    [[nodiscard]] bool is_terminal_highlighted(const std::string& terminal_3d_id) const noexcept {
        return !current_.terminal_3d_id.empty() && current_.terminal_3d_id == terminal_3d_id;
    }

    [[nodiscard]] bool is_pin_highlighted(const std::string& symbol_pin_id) const noexcept {
        return !current_.symbol_pin_id.empty() && current_.symbol_pin_id == symbol_pin_id;
    }

    [[nodiscard]] const SelectionHighlightEvent& current_selection() const noexcept { return current_; }

    void set_listener(Listener listener) { listener_ = std::move(listener); }

private:
    SelectionHighlightEvent current_;
    Listener listener_;
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

    /**
     * @brief Strict pin parity report checking parity between 3D terminals and 2D symbol pins.
     * (Roadmap task PH4-06.02.004)
     */
    struct PinParityReport {
        bool is_valid{true};
        std::vector<std::string> parity_violations;
    };

    [[nodiscard]] static PinParityReport verify_pin_parity(
        const ComponentAuthoringData& component_3d,
        const SchematicSymbol2D& symbol_2d
    );

    [[nodiscard]] static VerificationResult verify_coupling(
        const ComponentAuthoringData& component_3d,
        const SchematicSymbol2D& symbol_2d,
        const PinTerminalMapper& mapper
    );
};

} // namespace shunya::ecad3d
