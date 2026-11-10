#include "tests/test_framework.hpp"

#include "shunya/matdb/periodic_heatmap_filter.hpp"

#include <set>
#include <utility>

using namespace shunya::matdb;

TEST_CASE("PeriodicHeatmapFilter: 118-element 18x9 grid layout uniqueness and block filtering") {
    HeatmapFilterOptions opts;
    opts.property = HeatmapProperty::FamilyCategory;
    auto cells = PeriodicHeatmapFilter::build_grid(opts);
    REQUIRE(cells.size() == 118);

    std::set<std::pair<int, int>> occupied;
    for (const auto& c : cells) {
        REQUIRE(c.grid_row >= 0 && c.grid_row <= 8);
        REQUIRE(c.grid_col >= 0 && c.grid_col <= 17);
        REQUIRE(occupied.insert({c.grid_row, c.grid_col}).second);
        REQUIRE(c.visible);
        REQUIRE(c.highlighted);
    }

    // Hide f-block hides 30 Lanthanide/Actinide f-block elements (or all 28 f-block members per Elements118Catalog)
    HeatmapFilterOptions f_opts;
    f_opts.hide_f_block = true;
    f_opts.filter_block = 'p';
    auto filtered = PeriodicHeatmapFilter::build_grid(f_opts);
    int hidden_count = 0;
    int p_highlighted = 0;
    for (const auto& c : filtered) {
        if (!c.visible) ++hidden_count;
        if (c.highlighted) ++p_highlighted;
    }
    REQUIRE(hidden_count == 28);
    REQUIRE(p_highlighted > 0);
}

TEST_CASE("PeriodicHeatmapFilter: melting point heatmap polarity and zero-span normalization safety") {
    HeatmapFilterOptions opts;
    opts.property = HeatmapProperty::MeltingPointK;
    opts.colormap = shunya::core::domain::ColormapType::CoolWarm;
    auto cells = PeriodicHeatmapFilter::build_grid(opts);
    REQUIRE(cells.size() == 118);

    const PeriodicCellViewData* carbon = nullptr;
    const PeriodicCellViewData* tungsten = nullptr;
    const PeriodicCellViewData* mercury = nullptr;
    const PeriodicCellViewData* cesium = nullptr;
    for (const auto& c : cells) {
        if (c.atomic_number == 6)  carbon = &c;
        if (c.atomic_number == 74) tungsten = &c;
        if (c.atomic_number == 80) mercury = &c;
        if (c.atomic_number == 55) cesium = &c;
    }
    REQUIRE(carbon != nullptr);
    REQUIRE(tungsten != nullptr);
    REQUIRE(mercury != nullptr);
    REQUIRE(cesium != nullptr);

    // Carbon and Tungsten have top heat intensities; Mercury and Cesium near minimum
    REQUIRE(carbon->normalized_heat > 0.95);
    REQUIRE(tungsten->normalized_heat > 0.90);
    REQUIRE(mercury->normalized_heat < 0.10);
    REQUIRE(cesium->normalized_heat < 0.10);

    // Guard divide-by-zero when min == max
    REQUIRE(PeriodicHeatmapFilter::safe_normalize(42.0, 42.0, 42.0) == 0.5);
    REQUIRE(!PeriodicHeatmapFilter::property_ranges_json().empty());
}
