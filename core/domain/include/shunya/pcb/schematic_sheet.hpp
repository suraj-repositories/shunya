#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/pcb/schematic_hierarchy.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::pcb {

struct SchematicSheet {
    std::string id;
    std::string title;
    std::vector<SchematicComponent> components;
    std::vector<WireSegment2D> wires;
    std::vector<NetLabel> labels;
    std::vector<SheetPort> ports;
    std::vector<SubSheetInstance> subsheets;
    std::vector<core::base::math::Vector2<double>> junctions;
};

struct CompiledNetPin {
    std::string sheet_path;
    std::string component_ref;
    std::string pin_number;
    std::string pin_name;
    PinElectricalType electrical_type{PinElectricalType::Passive};
};

struct CompiledNet {
    std::string name;
    std::vector<CompiledNetPin> pins;
};

struct CompiledNetlist {
    std::vector<CompiledNet> nets;
    std::vector<CompiledNetPin> unconnected_pins;

    [[nodiscard]] const CompiledNet* find_net(std::string_view name) const noexcept;
};

/**
 * @brief Multi-sheet hierarchical schematic manager extracting global, port-mapped, and bus-expanded netlists.
 */
class SchematicDocument {
public:
    explicit SchematicDocument(std::string project_name = "SchematicProject")
        : project_name_(std::move(project_name)) {}

    void set_root_sheet_id(std::string id) { root_sheet_id_ = std::move(id); }
    [[nodiscard]] const std::string& root_sheet_id() const noexcept { return root_sheet_id_; }

    core::base::Result<void> add_sheet(SchematicSheet sheet);
    [[nodiscard]] SchematicSheet* get_sheet(std::string_view id) noexcept;
    [[nodiscard]] const SchematicSheet* get_sheet(std::string_view id) const noexcept;
    [[nodiscard]] size_t sheet_count() const noexcept { return sheets_.size(); }

    /**
     * @brief Flattens multi-level hierarchical sheets, global nets, sheet ports, and buses into a unified netlist.
     */
    [[nodiscard]] CompiledNetlist compile_netlist() const;

private:
    std::string project_name_;
    std::string root_sheet_id_{"root"};
    std::unordered_map<std::string, SchematicSheet> sheets_;
};

} // namespace shunya::pcb
