#include "shunya/chem/reaction_enthalpy.hpp"

#include <cmath>

namespace shunya::chem {

void ReactionEnthalpyCalculator::register_defaults(
    std::unordered_map<std::string, ThermochemicalSpecies>& db) {
    // NIST Chemistry WebBook standard enthalpies of formation at 298.15 K [kJ/mol]
    db["CH4"]   = {"CH4",   -74.87,   35.69, "CH4"};
    db["C3H8"]  = {"C3H8",  -104.68,  73.60, ""};
    db["C8H18"] = {"C8H18", -250.30,  239.1, ""}; // liquid n/iso-octane standard
    db["C2H5OH"]= {"C2H5OH",-277.69,  112.3, ""}; // liquid ethanol
    db["O2"]    = {"O2",      0.00,   29.38, "O2"};
    db["N2"]    = {"N2",      0.00,   29.12, "N2"};
    db["H2"]    = {"H2",      0.00,   28.84, "H2"};
    db["CO2"]   = {"CO2",  -393.51,   37.13, "CO2"};
    db["CO"]    = {"CO",   -110.53,   29.14, "CO"};
    db["H2O"]   = {"H2O",  -285.83,   75.38, ""}; // liquid water (HHV combustion reference)
    db["H2O(l)"]= {"H2O(l)",-285.83,  75.38, ""};
    db["H2O(g)"]= {"H2O(g)",-241.826, 33.59, "H2O"};
}

const std::unordered_map<std::string, ThermochemicalSpecies>&
ReactionEnthalpyCalculator::default_database() {
    static const auto db = [] {
        std::unordered_map<std::string, ThermochemicalSpecies> m;
        register_defaults(m);
        return m;
    }();
    return db;
}

core::base::Result<double> ReactionEnthalpyCalculator::species_enthalpy_kj_mol(
    const ThermochemicalSpecies& sp,
    double temp_k) {
    if (temp_k <= 0.0) {
        return core::base::Result<double>::err(
            core::base::Error{100, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                              "Temperature must be positive Kelvin", ""});
    }

    double delta_h_kj = 0.0;
    if (!sp.shomate_id.empty()) {
        auto sh_res = matdb::ShomateEvaluator::evaluate_delta_h_kj_mol(sp.shomate_id, temp_k);
        if (sh_res.is_ok()) {
            delta_h_kj = sh_res.value();
        } else {
            delta_h_kj = (sp.cp_298_j_mol_k * (temp_k - 298.15)) * 1e-3;
        }
    } else {
        delta_h_kj = (sp.cp_298_j_mol_k * (temp_k - 298.15)) * 1e-3;
    }

    return core::base::Result<double>::ok(sp.delta_hf_298_kj_mol + delta_h_kj);
}

core::base::Result<double> ReactionEnthalpyCalculator::reaction_enthalpy_kj_mol(
    const std::vector<StoichTerm>& terms,
    double temp_k,
    const std::unordered_map<std::string, ThermochemicalSpecies>* custom_db) {
    const auto& db = custom_db ? *custom_db : default_database();
    double sum_kj = 0.0;

    for (const auto& t : terms) {
        auto it = db.find(t.formula);
        if (it == db.end()) {
            return core::base::Result<double>::err(
                core::base::Error{101, core::base::ErrorDomain::Validation, core::base::Severity::Error,
                                  "Thermochemical species not found: " + t.formula, ""});
        }
        auto h_res = species_enthalpy_kj_mol(it->second, temp_k);
        if (!h_res.is_ok()) return h_res;
        sum_kj += t.nu * h_res.value();
    }
    return core::base::Result<double>::ok(sum_kj);
}

} // namespace shunya::chem
