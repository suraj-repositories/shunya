#pragma once

#include "shunya/core/domain/colormap_tables.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace shunya::matdb {

enum class HeatmapProperty : uint8_t {
    FamilyCategory,
    BlockCategory,
    AtomicMass,
    Electronegativity,
    FirstIonizationEnergy,
    CovalentRadius,
    MeltingPointK,
    DensityGcm3,
    ThermalConductivityWmK
};

struct GridCoordinate {
    int row{0}; // 0..6 main periods; 7 = Lanthanides; 8 = Actinides
    int col{0}; // 0..17
};

struct PropertyRangeStats {
    HeatmapProperty property{HeatmapProperty::FamilyCategory};
    double min_value{0.0};
    double max_value{1.0};
    int populated_count{0};
    std::string unit;
};

struct PeriodicCellViewData {
    int atomic_number{1};
    std::string symbol;
    std::string name;
    int grid_row{0};
    int grid_col{0};
    char block{'s'};
    ElementFamily family{ElementFamily::ReactiveNonmetal};
    double atomic_weight_amu{1.008};
    std::optional<double> raw_property_value;
    double normalized_heat{0.0};
    bool has_property_value{true};
    bool visible{true};
    bool highlighted{true};
    std::string color_hex{"#3b4cc0"};
    std::string formatted_value;
};

struct HeatmapFilterOptions {
    HeatmapProperty property{HeatmapProperty::FamilyCategory};
    core::domain::ColormapType colormap{core::domain::ColormapType::CoolWarm};
    std::optional<char> filter_block;
    std::optional<ElementFamily> filter_family;
    bool hide_f_block{false};
};

/**
 * @brief Computes 18x9 periodic grid geometry, property normalization, and colormapped overlays.
 */
class PeriodicHeatmapFilter {
public:
    [[nodiscard]] static GridCoordinate grid_position(int atomic_number) noexcept;
    [[nodiscard]] static std::optional<double> raw_property_value(int atomic_number,
                                                                  HeatmapProperty prop) noexcept;
    [[nodiscard]] static PropertyRangeStats compute_property_range(HeatmapProperty prop);
    [[nodiscard]] static std::string family_color_hex(ElementFamily family) noexcept;
    [[nodiscard]] static std::string block_color_hex(char block) noexcept;

    /**
     * @brief Safe scalar normalization in [0.0, 1.0] preventing divide-by-zero when min == max.
     */
    [[nodiscard]] static double safe_normalize(double value, double min_val, double max_val) noexcept;

    [[nodiscard]] static std::vector<PeriodicCellViewData> build_grid(const HeatmapFilterOptions& opts);
    [[nodiscard]] static std::string property_ranges_json();
};

} // namespace shunya::matdb
