#include "shunya/core/domain/material_ui.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::core::domain::matui {

// =============================================================================
// PeriodicTableBrowserViewModel Implementation
// =============================================================================

PeriodicTableBrowserViewModel::PeriodicTableBrowserViewModel(PeriodicTableDatabase table)
    : table_(std::move(table)) {
    // Populate view model representations from periodic table
    for (const auto& [z, elem] : table_.elements()) {
        (void)z;
        ElementDetailViewModel vm;
        vm.atomic_number = elem.atomic_number;
        vm.symbol = elem.symbol;
        vm.name = elem.name;
        vm.atomic_weight = elem.standard_atomic_weight;
        vm.block = elem.block;
        vm.period = elem.period;
        vm.group = elem.group;
        vm.melting_point_k = elem.melting_point_k.value_or(0.0);
        vm.boiling_point_k = elem.boiling_point_k.value_or(0.0);
        vm.density_at_stp = elem.density_at_stp.value_or(0.0);
        element_list_.push_back(vm);
    }
}

std::vector<ElementDetailViewModel> PeriodicTableBrowserViewModel::filter(const ElementFilter& filter) const {
    std::vector<ElementDetailViewModel> results;

    std::string lower_query = filter.search_query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

    for (const auto& elem : element_list_) {
        if (filter.group.has_value() && elem.group != *filter.group) {
            continue;
        }
        if (filter.period.has_value() && elem.period != *filter.period) {
            continue;
        }
        if (filter.block.has_value() && elem.block != *filter.block) {
            continue;
        }

        if (!lower_query.empty()) {
            std::string name_lower = elem.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::string sym_lower = elem.symbol;
            std::transform(sym_lower.begin(), sym_lower.end(), sym_lower.begin(), ::tolower);

            if (name_lower.find(lower_query) == std::string::npos &&
                sym_lower.find(lower_query) == std::string::npos) {
                continue;
            }
        }

        results.push_back(elem);
    }

    return results;
}

std::optional<ElementDetailViewModel> PeriodicTableBrowserViewModel::get_element_detail(int atomic_number) const {
    for (const auto& elem : element_list_) {
        if (elem.atomic_number == atomic_number) {
            return elem;
        }
    }
    return std::nullopt;
}

// =============================================================================
// MaterialBuilder Implementation
// =============================================================================

base::Result<CustomMaterialDefinition> MaterialBuilder::build_material(
    std::string name,
    MaterialOriginType origin,
    std::vector<CompositionConstituent> constituents,
    double base_density) {
    if (constituents.empty()) {
        return base::Result<CustomMaterialDefinition>::err(base::Error{
            .code = 1,
            .domain = base::ErrorDomain::Materials,
            .message = "Material must contain at least one constituent",
            .details = {}
        });
    }

    double total_fraction = 0.0;
    for (const auto& c : constituents) {
        if (c.mass_fraction <= 0.0) {
            return base::Result<CustomMaterialDefinition>::err(base::Error{
                .code = 2,
                .domain = base::ErrorDomain::Materials,
                .message = "Constituent mass fraction must be positive",
                .details = {}
            });
        }
        total_fraction += c.mass_fraction;
    }

    if (std::abs(total_fraction - 1.0) > 1e-4) {
        return base::Result<CustomMaterialDefinition>::err(base::Error{
            .code = 3,
            .domain = base::ErrorDomain::Materials,
            .message = "Total constituent fractions must sum to 1.0",
            .details = {}
        });
    }

    CustomMaterialDefinition mat;
    mat.name = std::move(name);
    mat.origin_type = origin;
    mat.constituents = std::move(constituents);
    mat.density = base_density;
    mat.specific_heat = 900.0;
    mat.thermal_conductivity = 15.0;
    mat.electrical_conductivity = 1.0e6;
    mat.youngs_modulus = 7.0e10;

    return base::Result<CustomMaterialDefinition>::ok(mat);
}

// =============================================================================
// PhaseInspector Implementation
// =============================================================================

PhaseStatusReport PhaseInspector::inspect_phase(
    double current_temp_k,
    double melting_point_k,
    double boiling_point_k,
    double /*latent_heat_fusion*/) {
    PhaseStatusReport report;

    if (melting_point_k <= 0.0 || boiling_point_k <= melting_point_k) {
        report.phase = MaterialPhaseState::Solid;
        return report;
    }

    if (current_temp_k < melting_point_k - 0.5) {
        report.phase = MaterialPhaseState::Solid;
        report.liquid_fraction = 0.0;
    } else if (current_temp_k <= melting_point_k + 0.5) {
        report.phase = MaterialPhaseState::MeltingTwoPhase;
        double frac = (current_temp_k - (melting_point_k - 0.5)) / 1.0;
        report.liquid_fraction = std::clamp(frac, 0.0, 1.0);
        report.warnings.push_back("Material is undergoing solid-liquid phase transition");
    } else if (current_temp_k < boiling_point_k - 0.5) {
        report.phase = MaterialPhaseState::Liquid;
        report.liquid_fraction = 1.0;
    } else if (current_temp_k <= boiling_point_k + 0.5) {
        report.phase = MaterialPhaseState::BoilingTwoPhase;
        report.liquid_fraction = 1.0;
        report.warnings.push_back("Material is undergoing boiling phase transition");
    } else {
        report.phase = MaterialPhaseState::Gas;
        report.liquid_fraction = 1.0;
        report.warnings.push_back("Material in vapor/gas phase; structural rigidity lost");
    }

    return report;
}

// =============================================================================
// MaterialCurvePlotter Implementation
// =============================================================================

std::vector<CurveDataPoint> MaterialCurvePlotter::sample_curve(
    const std::function<double(double)>& property_func,
    double x_min,
    double x_max,
    size_t num_samples) {
    if (!property_func || num_samples < 2 || x_max <= x_min) {
        return {};
    }

    std::vector<CurveDataPoint> points;
    points.reserve(num_samples);

    double dx = (x_max - x_min) / static_cast<double>(num_samples - 1);
    for (size_t i = 0; i < num_samples; ++i) {
        double x = x_min + static_cast<double>(i) * dx;
        double y = property_func(x);
        points.push_back(CurveDataPoint{x, y});
    }

    return points;
}

// =============================================================================
// MaterialSolverRequirementValidator Implementation
// =============================================================================

MaterialValidationReport MaterialSolverRequirementValidator::validate_for_domain(
    const CustomMaterialDefinition& mat,
    SolverDomainDependency domain) {
    MaterialValidationReport report;
    report.material_name = mat.name;
    report.is_valid_for_run = true;

    if (domain == SolverDomainDependency::Thermal) {
        if (mat.thermal_conductivity <= 0.0) {
            report.is_valid_for_run = false;
            report.blocking_missing_properties.push_back("thermal_conductivity");
        }
        if (mat.specific_heat <= 0.0) {
            report.is_valid_for_run = false;
            report.blocking_missing_properties.push_back("specific_heat");
        }
        if (mat.density <= 0.0) {
            report.is_valid_for_run = false;
            report.blocking_missing_properties.push_back("density");
        }
    } else if (domain == SolverDomainDependency::Circuit) {
        if (mat.electrical_conductivity <= 0.0) {
            report.is_valid_for_run = false;
            report.blocking_missing_properties.push_back("electrical_conductivity");
        }
    } else if (domain == SolverDomainDependency::Mechanics) {
        if (mat.youngs_modulus <= 0.0) {
            report.is_valid_for_run = false;
            report.blocking_missing_properties.push_back("youngs_modulus");
        }
        if (mat.density <= 0.0) {
            report.is_valid_for_run = false;
            report.blocking_missing_properties.push_back("density");
        }
    } else if (domain == SolverDomainDependency::Fluid) {
        if (mat.density <= 0.0) {
            report.is_valid_for_run = false;
            report.blocking_missing_properties.push_back("density");
        }
    }

    if (!report.is_valid_for_run) {
        report.warnings.push_back("Simulation cannot proceed due to missing critical material properties");
    }

    return report;
}

// =============================================================================
// MaterialComparator Implementation
// =============================================================================

std::vector<PropertyComparisonRow> MaterialComparator::compare_materials(
    const CustomMaterialDefinition& a,
    const CustomMaterialDefinition& b) {
    std::vector<PropertyComparisonRow> rows;

    auto add_row = [&](std::string name, double v_a, double v_b, std::string unit) {
        PropertyComparisonRow row;
        row.property_name = std::move(name);
        row.mat_a_value = v_a;
        row.mat_b_value = v_b;
        row.absolute_diff = std::abs(v_a - v_b);
        row.percentage_diff = (std::abs(v_a) > 1e-12) ? (row.absolute_diff / std::abs(v_a) * 100.0) : 0.0;
        row.unit = std::move(unit);
        rows.push_back(std::move(row));
    };

    add_row("Density", a.density, b.density, "kg/m^3");
    add_row("Specific Heat", a.specific_heat, b.specific_heat, "J/(kg*K)");
    add_row("Thermal Conductivity", a.thermal_conductivity, b.thermal_conductivity, "W/(m*K)");
    add_row("Electrical Conductivity", a.electrical_conductivity, b.electrical_conductivity, "S/m");
    add_row("Young's Modulus", a.youngs_modulus, b.youngs_modulus, "Pa");

    return rows;
}

} // namespace shunya::core::domain::matui
