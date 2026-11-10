#pragma once

#include "shunya/pcb/schematic_hierarchy.hpp"

#include <cstdint>

namespace shunya::pcb {

enum class ErcSeverity : uint8_t {
    Ok,
    Warning,
    Error
};

/**
 * @brief IEEE/KiCad standard Electrical Rule Check compatibility lookup matrix.
 */
class ErcMatrix {
public:
    [[nodiscard]] static constexpr ErcSeverity check_pin_pair(PinElectricalType a,
                                                              PinElectricalType b) noexcept {
        // Normalize symmetric order for driver-conflict checks
        auto is_push_driver = [](PinElectricalType t) {
            return t == PinElectricalType::Output || t == PinElectricalType::PowerOutput;
        };
        if (is_push_driver(a) && is_push_driver(b)) {
            return ErcSeverity::Error; // Output-Output, PowerOutput-Output, or PowerOutput-PowerOutput
        }
        if ((a == PinElectricalType::TriState && is_push_driver(b)) ||
            (b == PinElectricalType::TriState && is_push_driver(a))) {
            return ErcSeverity::Warning;
        }
        return ErcSeverity::Ok;
    }
};

} // namespace shunya::pcb
