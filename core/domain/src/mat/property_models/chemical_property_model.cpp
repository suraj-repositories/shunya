#include "shunya/mat/property_models/chemical_property_model.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

bool ChemicalPropertyModel::is_noble() const noexcept {
    if (corrosion_resistance == CorrosionResistanceClass::Immune) return true;
    for (const auto& c : constituents) {
        // Gold (79), Platinum (78), Iridium (77)
        if (c.atomic_number == 79 || c.atomic_number == 78 || c.atomic_number == 77) {
            return true;
        }
    }
    return false;
}

int ChemicalPropertyModel::net_oxidation_sum() const noexcept {
    int sum = 0;
    for (const auto& c : constituents) {
        sum += c.nominal_oxidation_state * c.stoichiometric_coefficient;
    }
    return sum;
}

bool ChemicalPropertyModel::is_chemically_balanced() const noexcept {
    if (constituents.empty()) return false;
    if (constituents.size() == 1) return true; // Pure element
    return (net_oxidation_sum() == 0);
}

std::string ChemicalPropertyModel::to_summary_string() const {
    std::ostringstream ss;
    ss << chemical_formula << " (" << iupac_name << "): "
       << "MW=" << molecular_weight_g_mol << " g/mol, "
       << "Reactivity=" << reactivity_rating_to_string(reactivity) << ", "
       << "Corrosion=" << corrosion_class_to_string(corrosion_resistance) << ", "
       << "Oxidation Stability=" << std::fixed << std::setprecision(2) << oxidation_resistance_index << ", "
       << "Balanced=" << (is_chemically_balanced() ? "YES" : "NO");
    return ss.str();
}

} // namespace shunya::mat
