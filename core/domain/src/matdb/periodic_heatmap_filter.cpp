#include "shunya/matdb/periodic_heatmap_filter.hpp"

#include "shunya/matdb/ionization_repository.hpp"
#include "shunya/matdb/periodic_table_extended.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <limits>
#include <sstream>

namespace shunya::matdb {

namespace {

// Melting points in Kelvin (1 atm; C sublimation ~ 3823 K; 0 = unknown)
std::optional<double> lookup_melting_point_k(int z) noexcept {
    static const std::array<double, 118> mp = {
        14.01,  0.95,  453.69, 1560.0, 2349.0, 3823.0, 63.15,  54.36,  53.53,  24.56, // 1..10
        370.87, 923.0, 933.47, 1687.0, 317.3,  388.36, 171.6,  83.80,  336.53, 1115.0,// 11..20
        1814.0, 1941.0,2183.0, 2180.0, 1519.0, 1811.0, 1768.0, 1728.0, 1357.77,692.68,// 21..30
        302.91, 1211.4,1090.0, 494.0,  265.8,  115.79, 312.46, 1050.0, 1799.0, 2128.0,// 31..40
        2750.0, 2896.0,2430.0, 2607.0, 2237.0, 1828.05,1234.93,594.22, 429.75, 505.08,// 41..50
        903.78, 722.66,386.85, 161.4,  301.59, 1000.0, 1193.0, 1068.0, 1208.0, 1297.0,// 51..60
        1315.0, 1345.0,1099.0, 1585.0, 1629.0, 1680.0, 1734.0, 1802.0, 1818.0, 1097.0,// 61..70
        1925.0, 2506.0,3290.0, 3695.0, 3459.0, 3306.0, 2719.0, 2041.4, 1337.33,234.32,// 71..80
        577.0,  600.61,544.7,  527.0,  575.0,  202.0,  300.0,  973.0,  1323.0, 2023.0,// 81..90
        1841.0, 1405.3,917.0,  912.5,  1449.0, 1613.0, 1259.0, 1173.0, 1133.0, 1800.0,// 91..100
        1100.0, 1100.0,1900.0, 2400.0, 0.0,    0.0,    0.0,    0.0,    0.0,    0.0,   // 101..110
        0.0,    0.0,   700.0,  200.0,  670.0,  700.0,  700.0,  325.0                 // 111..118
    };
    if (z < 1 || z > 118) return std::nullopt;
    const double val = mp[static_cast<size_t>(z - 1)];
    if (val <= 0.0) return std::nullopt;
    return val;
}

// Density at STP / solid state in g/cm^3 (0 = unknown)
std::optional<double> lookup_density_g_cm3(int z) noexcept {
    static const std::array<double, 118> rho = {
        0.00008988, 0.0001785, 0.534, 1.85, 2.34, 2.267, 0.0012506, 0.001429, 0.001696, 0.0008999,
        0.968,  1.738,  2.70,   2.329,  1.823,  2.07,   0.003214,  0.001784, 0.89,   1.55,
        2.985,  4.506,  6.11,   7.19,   7.21,   7.874,  8.90,      8.908,    8.96,   7.14,
        5.91,   5.323,  5.727,  4.81,   3.1028, 0.003733, 1.532,   2.64,     4.472,  6.52,
        8.57,   10.28,  11.0,   12.45,  12.41,  12.023, 10.49,     8.65,     7.31,   7.265,
        6.697,  6.24,   4.933,  0.005887, 1.93, 3.51,   6.162,     6.77,     6.77,   7.01,
        7.26,   7.52,   5.244,  7.90,   8.23,   8.54,   8.79,      9.066,    9.32,   6.90,
        9.841,  13.31,  16.65,  19.25,  21.02,  22.59,  22.56,     21.45,    19.30,  13.534,
        11.85,  11.34,  9.78,   9.196,  6.35,   0.00973, 1.87,     5.50,     10.07,  11.72,
        15.37,  19.1,   20.45,  19.816, 12.0,   13.51,  14.78,     15.1,     8.84,   9.7,
        0.0,    0.0,    0.0,    23.2,   29.3,   35.0,   37.1,      40.7,     37.4,   34.8,
        28.7,   14.0,   16.0,   14.0,   13.5,   12.9,   7.2,       5.0
    };
    if (z < 1 || z > 118) return std::nullopt;
    const double val = rho[static_cast<size_t>(z - 1)];
    if (val <= 0.0) return std::nullopt;
    return val;
}

// Room-temperature thermal conductivity W/(m*K) for prominent elements (0 = unknown)
std::optional<double> lookup_thermal_k(int z) noexcept {
    static const std::array<double, 118> k = {
        0.1805, 0.1513, 84.8,   200.0,  27.4,   129.0,  0.0258, 0.0266, 0.0277, 0.0491,
        142.0,  156.0,  237.0,  149.0,  0.236,  0.205,  0.0089, 0.0177, 102.5,  200.0,
        15.8,   21.9,   30.7,   93.9,   7.81,   80.4,   100.0,  90.9,   401.0,  116.0,
        40.6,   60.2,   50.2,   0.519,  0.122,  0.0094, 58.2,   35.4,   17.2,   22.6,
        53.7,   138.0,  50.6,   117.0,  150.0,  71.8,   429.0,  96.6,   81.8,   66.8,
        24.4,   3.0,    0.449,  0.0057, 35.9,   18.4,   13.4,   11.3,   12.5,   16.5,
        17.9,   13.3,   13.9,   10.6,   11.1,   10.7,   16.2,   14.5,   16.9,   38.5,
        16.4,   23.0,   57.5,   173.0,  48.0,   87.6,   147.0,  71.6,   318.0,  8.30,
        46.1,   35.3,   7.97,   20.0,   1.7,    0.0036, 15.0,   18.6,   12.0,   54.0,
        47.0,   27.5,   6.3,    6.74,   10.0,   8.8,    10.0,   10.0,   10.0,   0.0,
        0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,
        0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0,    0.0
    };
    if (z < 1 || z > 118) return std::nullopt;
    const double val = k[static_cast<size_t>(z - 1)];
    if (val <= 0.0) return std::nullopt;
    return val;
}

std::string property_unit(HeatmapProperty prop) {
    switch (prop) {
    case HeatmapProperty::FamilyCategory:        return "";
    case HeatmapProperty::BlockCategory:         return "";
    case HeatmapProperty::AtomicMass:            return "u";
    case HeatmapProperty::Electronegativity:     return "Pauling";
    case HeatmapProperty::FirstIonizationEnergy: return "eV";
    case HeatmapProperty::CovalentRadius:        return "pm";
    case HeatmapProperty::MeltingPointK:         return "K";
    case HeatmapProperty::DensityGcm3:           return "g/cm^3";
    case HeatmapProperty::ThermalConductivityWmK:return "W/(m*K)";
    }
    return "";
}

} // namespace

GridCoordinate PeriodicHeatmapFilter::grid_position(int atomic_number) noexcept {
    if (atomic_number >= 57 && atomic_number <= 71) {
        return {7, 2 + (atomic_number - 57)};
    }
    if (atomic_number >= 89 && atomic_number <= 103) {
        return {8, 2 + (atomic_number - 89)};
    }
    const auto* e = Elements118Catalog::find_by_z(atomic_number);
    if (!e) return {0, 0};
    return {e->period - 1, e->group - 1};
}

std::optional<double> PeriodicHeatmapFilter::raw_property_value(int atomic_number,
                                                                  HeatmapProperty prop) noexcept {
    switch (prop) {
    case HeatmapProperty::FamilyCategory:
    case HeatmapProperty::BlockCategory:
        return static_cast<double>(atomic_number);
    case HeatmapProperty::AtomicMass: {
        const auto* e = Elements118Catalog::find_by_z(atomic_number);
        return e ? std::optional<double>(e->atomic_weight_amu) : std::nullopt;
    }
    case HeatmapProperty::Electronegativity: {
        const auto* ext = PeriodicTableExtended::find_by_z(atomic_number);
        return ext ? ext->electronegativity.pauling : std::nullopt;
    }
    case HeatmapProperty::FirstIonizationEnergy:
        return IonizationEnergyRepository::first_ionization_ev(atomic_number);
    case HeatmapProperty::CovalentRadius: {
        const auto* ext = PeriodicTableExtended::find_by_z(atomic_number);
        return ext ? ext->radii_pm.covalent_single_pm : std::nullopt;
    }
    case HeatmapProperty::MeltingPointK:
        return lookup_melting_point_k(atomic_number);
    case HeatmapProperty::DensityGcm3:
        return lookup_density_g_cm3(atomic_number);
    case HeatmapProperty::ThermalConductivityWmK:
        return lookup_thermal_k(atomic_number);
    }
    return std::nullopt;
}

PropertyRangeStats PeriodicHeatmapFilter::compute_property_range(HeatmapProperty prop) {
    PropertyRangeStats stats;
    stats.property = prop;
    stats.unit = property_unit(prop);
    stats.min_value = std::numeric_limits<double>::infinity();
    stats.max_value = -std::numeric_limits<double>::infinity();

    for (int z = 1; z <= 118; ++z) {
        auto val = raw_property_value(z, prop);
        if (!val) continue;
        ++stats.populated_count;
        stats.min_value = std::min(stats.min_value, *val);
        stats.max_value = std::max(stats.max_value, *val);
    }
    if (stats.populated_count == 0) {
        stats.min_value = 0.0;
        stats.max_value = 1.0;
    }
    return stats;
}

std::string PeriodicHeatmapFilter::family_color_hex(ElementFamily family) noexcept {
    switch (family) {
    case ElementFamily::AlkaliMetal:         return "#b91c1c";
    case ElementFamily::AlkalineEarth:       return "#c2410c";
    case ElementFamily::TransitionMetal:     return "#1d4ed8";
    case ElementFamily::PostTransitionMetal: return "#0f766e";
    case ElementFamily::Metalloid:           return "#047857";
    case ElementFamily::ReactiveNonmetal:    return "#6d28d9";
    case ElementFamily::NobleGas:            return "#be185d";
    case ElementFamily::Lanthanide:          return "#a16207";
    case ElementFamily::Actinide:            return "#7c2d12";
    }
    return "#334155";
}

std::string PeriodicHeatmapFilter::block_color_hex(char block) noexcept {
    switch (block) {
    case 's': return "#dc2626";
    case 'p': return "#2563eb";
    case 'd': return "#059669";
    case 'f': return "#d97706";
    default:  return "#475569";
    }
}

double PeriodicHeatmapFilter::safe_normalize(double value, double min_val, double max_val) noexcept {
    const double span = max_val - min_val;
    if (span <= 1e-12) {
        return 0.5;
    }
    return std::clamp((value - min_val) / span, 0.0, 1.0);
}

std::vector<PeriodicCellViewData> PeriodicHeatmapFilter::build_grid(const HeatmapFilterOptions& opts) {
    const auto range = compute_property_range(opts.property);
    const auto& elems = Elements118Catalog::all_elements();
    std::vector<PeriodicCellViewData> out;
    out.reserve(elems.size());

    for (const auto& e : elems) {
        const auto pos = grid_position(e.atomic_number);
        PeriodicCellViewData cell;
        cell.atomic_number = e.atomic_number;
        cell.symbol = e.symbol;
        cell.name = e.name;
        cell.grid_row = pos.row;
        cell.grid_col = pos.col;
        cell.block = e.block;
        cell.family = e.family;
        cell.atomic_weight_amu = e.atomic_weight_amu;

        if (opts.hide_f_block && e.block == 'f') {
            cell.visible = false;
        }
        bool hl = true;
        if (opts.filter_block && e.block != *opts.filter_block) hl = false;
        if (opts.filter_family && e.family != *opts.filter_family) hl = false;
        cell.highlighted = hl;

        if (opts.property == HeatmapProperty::FamilyCategory) {
            cell.has_property_value = true;
            cell.normalized_heat = 0.0;
            cell.color_hex = family_color_hex(e.family);
            cell.formatted_value = std::to_string(e.atomic_weight_amu);
        } else if (opts.property == HeatmapProperty::BlockCategory) {
            cell.has_property_value = true;
            cell.normalized_heat = 0.0;
            cell.color_hex = block_color_hex(e.block);
            cell.formatted_value = std::string(1, e.block) + "-block";
        } else {
            cell.raw_property_value = raw_property_value(e.atomic_number, opts.property);
            if (cell.raw_property_value) {
                cell.has_property_value = true;
                cell.normalized_heat = safe_normalize(*cell.raw_property_value,
                                                      range.min_value, range.max_value);
                cell.color_hex = core::domain::ColormapTables::sample_rgb8(
                                     opts.colormap, cell.normalized_heat).to_hex();
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << *cell.raw_property_value;
                if (!range.unit.empty()) oss << " " << range.unit;
                cell.formatted_value = oss.str();
            } else {
                cell.has_property_value = false;
                cell.normalized_heat = 0.0;
                cell.color_hex = "#374151";
                cell.formatted_value = "N/A";
            }
        }
        out.push_back(std::move(cell));
    }
    return out;
}

std::string PeriodicHeatmapFilter::property_ranges_json() {
    const HeatmapProperty props[] = {
        HeatmapProperty::AtomicMass,
        HeatmapProperty::Electronegativity,
        HeatmapProperty::FirstIonizationEnergy,
        HeatmapProperty::CovalentRadius,
        HeatmapProperty::MeltingPointK,
        HeatmapProperty::DensityGcm3,
        HeatmapProperty::ThermalConductivityWmK
    };
    const char* names[] = {
        "AtomicMass", "Electronegativity", "FirstIonizationEnergy",
        "CovalentRadius", "MeltingPointK", "DensityGcm3", "ThermalConductivityWmK"
    };
    std::ostringstream out;
    out << "{\n";
    for (size_t i = 0; i < 7; ++i) {
        const auto s = compute_property_range(props[i]);
        out << "  \"" << names[i] << "\": {\"min\":" << s.min_value
            << ",\"max\":" << s.max_value << ",\"unit\":\"" << s.unit << "\"}";
        if (i + 1 < 7) out << ",";
        out << "\n";
    }
    out << "}\n";
    return out.str();
}

} // namespace shunya::matdb
