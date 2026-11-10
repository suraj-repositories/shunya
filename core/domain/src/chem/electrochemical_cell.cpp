#include "shunya/chem/electrochemical_cell.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace shunya::chem {

const std::unordered_map<std::string, HalfCellStandardPotential>&
ElectrochemicalCellModel::standard_potentials() {
    static const std::unordered_map<std::string, HalfCellStandardPotential> kTable{
        {"Li+/Li",     {"Li+/Li",     -3.0401, 1}},
        {"K+/K",       {"K+/K",       -2.9310, 1}},
        {"Ca2+/Ca",    {"Ca2+/Ca",    -2.8680, 2}},
        {"Na+/Na",     {"Na+/Na",     -2.7100, 1}},
        {"Mg2+/Mg",    {"Mg2+/Mg",    -2.3720, 2}},
        {"Al3+/Al",    {"Al3+/Al",    -1.6620, 3}},
        {"Zn2+/Zn",    {"Zn2+/Zn",    -0.7618, 2}},
        {"Fe2+/Fe",    {"Fe2+/Fe",    -0.4470, 2}},
        {"Cd2+/Cd",    {"Cd2+/Cd",    -0.4030, 2}},
        {"Ni2+/Ni",    {"Ni2+/Ni",    -0.2570, 2}},
        {"Sn2+/Sn",    {"Sn2+/Sn",    -0.1375, 2}},
        {"Pb2+/Pb",    {"Pb2+/Pb",    -0.1262, 2}},
        {"H+/H2",      {"H+/H2",       0.0000, 2}},
        {"Cu2+/Cu",    {"Cu2+/Cu",    +0.3419, 2}},
        {"I2/I-",      {"I2/I-",      +0.5355, 2}},
        {"Fe3+/Fe2+",  {"Fe3+/Fe2+",  +0.7710, 1}},
        {"Ag+/Ag",     {"Ag+/Ag",     +0.7996, 1}},
        {"Br2/Br-",    {"Br2/Br-",    +1.0660, 2}},
        {"O2/H2O",     {"O2/H2O",     +1.2290, 4}},
        {"Cl2/Cl-",    {"Cl2/Cl-",    +1.3583, 2}},
        {"Au3+/Au",    {"Au3+/Au",    +1.4980, 3}},
        {"F2/F-",      {"F2/F-",      +2.8660, 2}},
    };
    return kTable;
}

core::base::Result<double> ElectrochemicalCellModel::lookup_e0_volts(std::string_view half_cell_id) {
    const auto& table = standard_potentials();
    const auto it = table.find(std::string(half_cell_id));
    if (it == table.end()) {
        return core::base::Error{
            101,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Unknown electrochemical half-cell ID",
            std::string(half_cell_id)
        };
    }
    return it->second.e0_volts;
}

core::base::Result<double> ElectrochemicalCellModel::evaluate_nernst_cell_v(
    double e0_cathode_v,
    double e0_anode_v,
    int z_electrons,
    double reaction_quotient_q,
    double temp_k) {
    if (z_electrons <= 0) {
        return core::base::Error{
            102,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Transferred electrons z must be positive",
            ""
        };
    }
    if (reaction_quotient_q <= 0.0 || !std::isfinite(reaction_quotient_q)) {
        return core::base::Error{
            103,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Reaction quotient Q must be finite and strictly positive",
            ""
        };
    }
    if (temp_k <= 0.0 || !std::isfinite(temp_k)) {
        return core::base::Error{
            104,
            core::base::ErrorDomain::Validation,
            core::base::Severity::Error,
            "Temperature must be strictly positive (Kelvin)",
            ""
        };
    }

    const double e0_cell_v = e0_cathode_v - e0_anode_v;
    const double nernst_coeff = (kGasConstantR * temp_k) /
                                (static_cast<double>(z_electrons) * kFaradayF);
    return e0_cell_v - nernst_coeff * std::log(reaction_quotient_q);
}

double ElectrochemicalCellModel::equilibrium_constant_k(
    double e0_cell_v,
    int z_electrons,
    double temp_k) noexcept {
    if (z_electrons <= 0 || temp_k <= 0.0) {
        return 0.0;
    }
    const double exponent = (static_cast<double>(z_electrons) * kFaradayF * e0_cell_v) /
                            (kGasConstantR * temp_k);
    return std::exp(std::clamp(exponent, -700.0, 700.0));
}

double ElectrochemicalCellModel::faraday_mass_deposited_kg(
    double current_a,
    double time_s,
    double molar_mass_kg_mol,
    int z_electrons) noexcept {
    if (z_electrons <= 0 || current_a < 0.0 || time_s < 0.0 || molar_mass_kg_mol < 0.0) {
        return 0.0;
    }
    const double charge_c = current_a * time_s;
    const double moles_e = charge_c / kFaradayF;
    return (moles_e / static_cast<double>(z_electrons)) * molar_mass_kg_mol;
}

} // namespace shunya::chem
