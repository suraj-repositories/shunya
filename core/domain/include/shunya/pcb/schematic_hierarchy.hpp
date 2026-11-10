#pragma once

#include "shunya/core/base/math.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::pcb {

enum class PinElectricalType : uint8_t {
    Input,
    Output,
    Bidirectional,
    TriState,
    OpenCollector,
    PowerInput,
    PowerOutput,
    Passive,
    Unconnected
};

enum class SheetPortDirection : uint8_t {
    Input,
    Output,
    Bidirectional
};

struct SchematicPin {
    std::string component_ref;
    std::string pin_number;
    std::string pin_name;
    PinElectricalType electrical_type{PinElectricalType::Passive};
    core::base::math::Vector2<double> pos_mm;
    bool no_connect{false};
};

struct SchematicComponent {
    std::string ref_des;
    std::string value;
    core::base::math::Vector2<double> center_mm;
    double half_width_mm{5.08};
    double half_height_mm{5.08};
    std::vector<SchematicPin> pins;
};

struct WireSegment2D {
    core::base::math::Vector2<double> start_mm;
    core::base::math::Vector2<double> end_mm;
    std::string net_hint;
};

struct NetLabel {
    std::string net_name;
    bool is_global{false};
    core::base::math::Vector2<double> pos_mm;
};

struct SheetPort {
    std::string name;
    SheetPortDirection direction{SheetPortDirection::Bidirectional};
    core::base::math::Vector2<double> pos_mm;
};

struct SubSheetInstance {
    std::string instance_name;
    std::string target_sheet_id;
    std::vector<SheetPort> sheet_pins;
};

/**
 * @brief Expands bus signal names like "DATA[0..7]" into indexed individual signal names.
 */
[[nodiscard]] std::vector<std::string> expand_bus_signal(std::string_view signal_expr);

} // namespace shunya::pcb
