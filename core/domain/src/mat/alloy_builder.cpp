#include "shunya/mat/alloy_builder.hpp"

#include "shunya/matdb/element_database.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"
#include "shunya/matdb/periodic_heatmap_filter.hpp"

#include <cmath>
#include <sstream>

namespace shunya::mat {

namespace {

core::base::Error make_alloy_error(std::string msg) {
    return core::base::Error{
        .code = 4235,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

double element_resistivity_ohm_m(int z) noexcept {
    if (matdb::ElementDatabase::global().is_loaded()) {
        if (const auto* rec = matdb::ElementDatabase::global().find_by_z(z)) {
            if (rec->resistivity && *rec->resistivity > 0.0) return *rec->resistivity;
            if (rec->electrical_conductivity && *rec->electrical_conductivity > 0.0) {
                return 1.0 / *rec->electrical_conductivity;
            }
        }
    }
    switch (z) {
    case 13: return 2.65e-8;  // Al
    case 22: return 4.20e-7;  // Ti
    case 23: return 1.97e-7;  // V
    case 24: return 1.25e-7;  // Cr
    case 25: return 1.44e-6;  // Mn
    case 26: return 9.71e-8;  // Fe
    case 28: return 6.93e-8;  // Ni
    case 29: return 1.68e-8;  // Cu
    case 30: return 5.90e-8;  // Zn
    case 42: return 5.34e-8;  // Mo
    default: return 1.50e-7;
    }
}

double element_cte_per_k(int z) noexcept {
    if (matdb::ElementDatabase::global().is_loaded()) {
        if (const auto* rec = matdb::ElementDatabase::global().find_by_z(z)) {
            if (rec->thermal_expansion && *rec->thermal_expansion > 0.0) return *rec->thermal_expansion;
        }
    }
    switch (z) {
    case 13: return 23.1e-6;
    case 22: return 8.6e-6;
    case 23: return 8.4e-6;
    case 24: return 4.9e-6;
    case 26: return 11.8e-6;
    case 28: return 13.4e-6;
    case 29: return 16.5e-6;
    case 30: return 30.2e-6;
    case 42: return 4.8e-6;
    default: return 12.0e-6;
    }
}

} // namespace

core::base::Result<AlloySpecification> AlloyBuilder::build_from_weight_pct(
    std::string name,
    std::vector<AlloyInputElement> inputs,
    bool auto_balance_first,
    double lattice_contraction_factor) {
    if (inputs.empty()) {
        return make_alloy_error("Alloy must contain at least one element");
    }

    double sum = 0.0;
    for (const auto& in : inputs) {
        if (in.fraction_pct < 0.0) {
            return make_alloy_error("Element weight percentage cannot be negative");
        }
        sum += in.fraction_pct;
    }

    if (auto_balance_first && inputs.size() > 1 && sum < 100.0) {
        inputs[0].fraction_pct += (100.0 - sum);
        sum = 100.0;
    }
    if (sum <= 0.0) {
        return make_alloy_error("Total alloy weight percentage must be positive");
    }

    AlloySpecification spec;
    spec.name = std::move(name);

    double total_moles = 0.0;
    std::vector<double> moles(inputs.size(), 0.0);
    std::vector<double> pure_rho(inputs.size(), 0.0);

    for (size_t i = 0; i < inputs.size(); ++i) {
        const double w_pct = (inputs[i].fraction_pct / sum) * 100.0;
        const auto* elem = matdb::Elements118Catalog::find_by_z(inputs[i].atomic_number);
        if (!elem) {
            return make_alloy_error("Unknown atomic number Z=" + std::to_string(inputs[i].atomic_number));
        }
        const auto d_opt = matdb::PeriodicHeatmapFilter::raw_property_value(
            inputs[i].atomic_number, matdb::HeatmapProperty::DensityGcm3);
        pure_rho[i] = d_opt.value_or(5.0) * 1000.0;
        moles[i] = w_pct / elem->atomic_weight_amu;
        total_moles += moles[i];

        BalancedAlloyElement b;
        b.atomic_number = inputs[i].atomic_number;
        b.symbol = elem->symbol;
        b.weight_pct = w_pct;
        b.pure_density_kg_m3 = pure_rho[i];
        spec.elements.push_back(std::move(b));
    }

    double specific_vol = 0.0;
    double weighted_mass = 0.0;
    for (size_t i = 0; i < spec.elements.size(); ++i) {
        spec.elements[i].molar_pct = (moles[i] / total_moles) * 100.0;
        const double w_frac = spec.elements[i].weight_pct * 0.01;
        specific_vol += w_frac / pure_rho[i];
        const auto* elem = matdb::Elements118Catalog::find_by_z(spec.elements[i].atomic_number);
        weighted_mass += (moles[i] / total_moles) * elem->atomic_weight_amu;
    }

    const double ideal_rho = 1.0 / specific_vol;
    spec.theoretical_density_kg_m3 = ideal_rho * (1.0 + lattice_contraction_factor);
    spec.effective_molar_mass_g_mol = weighted_mass;
    spec.atomic_volume_cm3_mol = weighted_mass / (spec.theoretical_density_kg_m3 * 0.001);

    // Volume-fraction weighted CTE and Nordheim-corrected linear resistivity estimate
    double total_v = specific_vol;
    for (size_t i = 0; i < spec.elements.size(); ++i) {
        const double w_frac = spec.elements[i].weight_pct * 0.01;
        const double v_frac = (w_frac / pure_rho[i]) / total_v;
        spec.thermal_expansion_per_k += v_frac * element_cte_per_k(spec.elements[i].atomic_number);
        spec.electrical_resistivity_ohm_m +=
            v_frac * element_resistivity_ohm_m(spec.elements[i].atomic_number);
    }

    return spec;
}

core::base::Result<AlloySpecification> AlloyBuilder::build_from_molar_pct(
    std::string name,
    std::vector<AlloyInputElement> inputs) {
    if (inputs.empty()) return make_alloy_error("Empty alloy composition");
    double m_sum = 0.0;
    for (const auto& in : inputs) {
        if (in.fraction_pct < 0.0) return make_alloy_error("Negative molar fraction");
        m_sum += in.fraction_pct;
    }
    if (m_sum <= 0.0) return make_alloy_error("Total molar fraction must be positive");

    std::vector<AlloyInputElement> weights;
    weights.reserve(inputs.size());
    for (const auto& in : inputs) {
        const auto* elem = matdb::Elements118Catalog::find_by_z(in.atomic_number);
        if (!elem) return make_alloy_error("Invalid Z in molar alloy input");
        weights.push_back({in.atomic_number, (in.fraction_pct / m_sum) * elem->atomic_weight_amu});
    }
    return build_from_weight_pct(std::move(name), std::move(weights), false, 0.0);
}

AlloySpecification AlloyBuilder::preset_ti_6al_4v() {
    // Ti-6Al-4V (Grade 5) alpha+beta solid solution exhibits ~1.2% volume contraction (4430 kg/m^3)
    return build_from_weight_pct("Ti-6Al-4V (Grade 5)", {
        {22, 90.0}, // Ti
        {13,  6.0}, // Al
        {23,  4.0}  // V
    }, false, 0.0122).value();
}

AlloySpecification AlloyBuilder::preset_ss_316l() {
    return build_from_weight_pct("Stainless Steel 316L", {
        {26, 65.5}, // Fe (balance)
        {24, 17.0}, // Cr
        {28, 12.0}, // Ni
        {42,  2.5}, // Mo
        {25,  2.0}, // Mn
        {14,  1.0}  // Si
    }).value();
}

AlloySpecification AlloyBuilder::preset_brass_c260() {
    return build_from_weight_pct("Cartridge Brass (C260)", {
        {29, 70.0}, // Cu
        {30, 30.0}  // Zn
    }).value();
}

std::string AlloyBuilder::standard_alloys_json() {
    const AlloySpecification presets[] = {
        preset_ti_6al_4v(),
        preset_ss_316l(),
        preset_brass_c260()
    };
    std::ostringstream out;
    out << "[\n";
    for (size_t i = 0; i < 3; ++i) {
        const auto& a = presets[i];
        out << "  {\"name\":\"" << a.name << "\""
            << ",\"density_kg_m3\":" << a.theoretical_density_kg_m3
            << ",\"molar_mass_g_mol\":" << a.effective_molar_mass_g_mol
            << ",\"cte_per_k\":" << a.thermal_expansion_per_k
            << "}";
        if (i < 2) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

} // namespace shunya::mat
