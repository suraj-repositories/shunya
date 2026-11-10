#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/pcb/schematic_hierarchy.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::pcb {

enum class SymbolStandard : uint8_t {
    IEEE_US,
    IEC_EU
};

struct SymbolPrimitive {
    enum class Kind : uint8_t { Line, Rect, Circle };
    Kind kind{Kind::Line};
    core::base::math::Vector2<double> p0;
    core::base::math::Vector2<double> p1; // end point for Line, half-extents for Rect, radius.x for Circle
};

struct SymbolPinDef {
    std::string number;
    std::string name;
    PinElectricalType electrical_type{PinElectricalType::Passive};
    core::base::math::Vector2<double> pos_mm;
};

struct SymbolDefinition {
    std::string id;
    std::string name;
    std::string ref_prefix{"U"};
    std::vector<SymbolPrimitive> graphics;
    std::vector<SymbolPinDef> pins;

    [[nodiscard]] SymbolDefinition transformed(int rotation_deg, bool mirror_x, bool mirror_y) const;
};

/**
 * @brief IEEE/ANSI and IEC 60617 schematic symbol catalog supporting instant US/EU theme switching.
 */
class SymbolLibraryManager {
public:
    explicit SymbolLibraryManager(SymbolStandard standard = SymbolStandard::IEEE_US)
        : standard_(standard) {}

    void set_standard(SymbolStandard std) noexcept { standard_ = std; }
    [[nodiscard]] SymbolStandard standard() const noexcept { return standard_; }

    [[nodiscard]] std::vector<SymbolDefinition> all_symbols() const;
    [[nodiscard]] SymbolDefinition get_symbol(std::string_view id) const;
    [[nodiscard]] std::string to_json() const;

private:
    SymbolStandard standard_{SymbolStandard::IEEE_US};
};

} // namespace shunya::pcb
