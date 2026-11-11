#include "shunya/mat/material_mixing_lab.hpp"

#include "shunya/mat/composite_homogenizer.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::mat {

namespace {

core::base::Error make_mix_error(int32_t code, std::string msg, std::string details = {}) {
    return core::base::Error{
        .code = code,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = std::move(details)
    };
}

std::string escape_json(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

std::string json_str_val(std::string_view json, std::string_view key) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*\"([^\"]*)\"");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        return m[1].str();
    }
    return {};
}

double json_num_val(std::string_view json, std::string_view key, double fallback) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*([-+0-9eE\\.]+)");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        try {
            return std::stod(m[1].str());
        } catch (...) {
            return fallback;
        }
    }
    return fallback;
}

bool json_bool_val(std::string_view json, std::string_view key, bool fallback) {
    std::regex re("\"" + std::string(key) + "\"\\s*:\\s*(true|false)");
    std::cmatch m;
    if (std::regex_search(json.data(), json.data() + json.size(), m, re)) {
        return m[1].str() == "true";
    }
    return fallback;
}

std::string basis_to_string(QuantityInputBasis b) {
    switch (b) {
    case QuantityInputBasis::MassGrams:         return "MassGrams";
    case QuantityInputBasis::MassFractionPct:   return "MassFractionPct";
    case QuantityInputBasis::MoleCount:         return "MoleCount";
    case QuantityInputBasis::MoleFractionPct:   return "MoleFractionPct";
    case QuantityInputBasis::VolumeFractionPct: return "VolumeFractionPct";
    }
    return "MassFractionPct";
}

QuantityInputBasis basis_from_string(std::string_view s) {
    if (s == "MassGrams") return QuantityInputBasis::MassGrams;
    if (s == "MoleCount") return QuantityInputBasis::MoleCount;
    if (s == "MoleFractionPct") return QuantityInputBasis::MoleFractionPct;
    if (s == "VolumeFractionPct") return QuantityInputBasis::VolumeFractionPct;
    return QuantityInputBasis::MassFractionPct;
}

bool is_metallic_element(const matdb::ElementRecord& e) {
    if (e.electrical_type == "Conductor" && e.atomic_number != 6) return true;
    const int g = e.group.value_or(0);
    if (g >= 1 && g <= 12 && e.atomic_number != 1) return true;
    return false;
}

bool can_neutralize_valence(const std::vector<const matdb::ElementRecord*>& elems,
                            const std::vector<double>& mole_fracs) {
    if (elems.size() == 1) return true;
    // Check if any noble gas is present
    for (const auto* e : elems) {
        if (e->group == 18) return false;
    }
    // Check whether positive & negative oxidation states exist across constituents
    bool has_pos = false;
    bool has_neg = false;
    for (size_t i = 0; i < elems.size(); ++i) {
        if (mole_fracs[i] <= 1e-6) continue;
        const auto* cat = matdb::Elements118Catalog::find_by_z(elems[i]->atomic_number);
        if (!cat) continue;
        for (int ox : cat->common_oxidation_states) {
            if (ox > 0) has_pos = true;
            if (ox < 0) has_neg = true;
        }
    }
    return has_pos && has_neg;
}

} // namespace

core::base::Result<FormationScenario> formation_scenario_from_string(std::string_view s) {
    if (s == "PhysicalMixture")   return FormationScenario::PhysicalMixture;
    if (s == "Alloy")             return FormationScenario::Alloy;
    if (s == "Solution")          return FormationScenario::Solution;
    if (s == "ChemicalCompound")  return FormationScenario::ChemicalCompound;
    if (s == "LayeredMaterial")   return FormationScenario::LayeredMaterial;
    if (s == "CompositeMaterial") return FormationScenario::CompositeMaterial;
    return make_mix_error(4201, "Unknown formation scenario: " + std::string(s));
}

std::string MixtureExperiment::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"id\": \"" << id.to_string() << "\",\n";
    ss << "  \"name\": \"" << escape_json(name) << "\",\n";
    ss << "  \"description\": \"" << escape_json(description) << "\",\n";
    ss << "  \"scenario\": \"" << formation_scenario_to_string(scenario) << "\",\n";
    ss << "  \"temperature_k\": " << std::setprecision(10) << temperature_k << ",\n";
    ss << "  \"pressure_pa\": " << std::setprecision(10) << pressure_pa << ",\n";
    ss << "  \"oxidizing_atmosphere\": " << (oxidizing_atmosphere ? "true" : "false") << ",\n";
    ss << "  \"user_density_override_kg_m3\": " << user_density_override_kg_m3 << ",\n";
    if (promoted_material_id.has_value()) {
        ss << "  \"promoted_material_id\": \"" << promoted_material_id->to_string() << "\",\n";
    }
    ss << "  \"inputs\": [\n";
    for (size_t i = 0; i < inputs.size(); ++i) {
        const auto& in = inputs[i];
        ss << "    {\"symbol_or_id\": \"" << escape_json(in.symbol_or_id)
           << "\", \"basis\": \"" << basis_to_string(in.basis)
           << "\", \"amount\": " << std::setprecision(10) << in.amount
           << ", \"layer_thickness_m\": " << in.layer_thickness_m
           << ", \"is_matrix\": " << (in.is_matrix ? "true" : "false") << "}";
        if (i + 1 < inputs.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

core::base::Result<MixtureExperiment> MixtureExperiment::from_json(std::string_view json_str) {
    if (json_str.empty()) {
        return make_mix_error(4202, "Cannot parse empty MixtureExperiment JSON");
    }
    MixtureExperiment exp;
    if (auto id_opt = core::base::Uuid::from_string(json_str_val(json_str, "id"))) {
        exp.id = *id_opt;
    }
    std::string nm = json_str_val(json_str, "name");
    if (!nm.empty()) exp.name = std::move(nm);
    exp.description = json_str_val(json_str, "description");
    if (auto sc_res = formation_scenario_from_string(json_str_val(json_str, "scenario")); sc_res.is_ok()) {
        exp.scenario = sc_res.value();
    }
    exp.temperature_k = json_num_val(json_str, "temperature_k", 298.15);
    exp.pressure_pa = json_num_val(json_str, "pressure_pa", 101325.0);
    exp.oxidizing_atmosphere = json_bool_val(json_str, "oxidizing_atmosphere", true);
    exp.user_density_override_kg_m3 = json_num_val(json_str, "user_density_override_kg_m3", 0.0);
    std::string pmat = json_str_val(json_str, "promoted_material_id");
    if (!pmat.empty()) {
        exp.promoted_material_id = core::base::Uuid::from_string(pmat);
    }

    const size_t arr_pos = json_str.find("\"inputs\"");
    if (arr_pos != std::string_view::npos) {
        std::string_view sub = json_str.substr(arr_pos);
        std::regex item_re(R"(\{[^\{\}]*\})");
        auto begin = std::regex_iterator(sub.begin(), sub.end(), item_re);
        auto end = std::regex_iterator<std::string_view::const_iterator>();
        for (auto it = begin; it != end; ++it) {
            std::string item = it->str();
            MixtureConstituentInput in;
            in.symbol_or_id = json_str_val(item, "symbol_or_id");
            in.basis = basis_from_string(json_str_val(item, "basis"));
            in.amount = json_num_val(item, "amount", 0.0);
            in.layer_thickness_m = json_num_val(item, "layer_thickness_m", 1e-3);
            in.is_matrix = json_bool_val(item, "is_matrix", false);
            if (!in.symbol_or_id.empty()) {
                exp.inputs.push_back(std::move(in));
            }
        }
    }
    return exp;
}

core::base::Result<MixtureEvaluationResult> MaterialMixingLab::evaluate(
    const MixtureExperiment& experiment,
    const matdb::ElementDatabase& db) {
    if (experiment.inputs.empty()) {
        return make_mix_error(4210, "Mixture experiment must have at least one constituent");
    }
    if (!std::isfinite(experiment.temperature_k) || experiment.temperature_k <= 0.0) {
        return make_mix_error(4211, "Experiment temperature must be strictly positive (K)");
    }
    if (!std::isfinite(experiment.pressure_pa) || experiment.pressure_pa < 0.0) {
        return make_mix_error(4212, "Experiment pressure cannot be negative (Pa)");
    }

    std::vector<const matdb::ElementRecord*> elems;
    std::vector<ElementBehaviorReport> behaviors;
    elems.reserve(experiment.inputs.size());
    behaviors.reserve(experiment.inputs.size());

    double total_amount = 0.0;
    for (const auto& in : experiment.inputs) {
        if (!std::isfinite(in.amount) || in.amount < 0.0) {
            return make_mix_error(4213, "Constituent amount cannot be negative or non-finite for " + in.symbol_or_id);
        }
        total_amount += in.amount;
        const auto* el = db.find_by_symbol(in.symbol_or_id);
        if (!el) el = db.find_by_name(in.symbol_or_id);
        if (!el) {
            return make_mix_error(4214, "Unknown element in mixture: " + in.symbol_or_id);
        }
        elems.push_back(el);
        behaviors.push_back(ElementBehaviorEngine::evaluate(
            *el, experiment.temperature_k, experiment.pressure_pa, experiment.oxidizing_atmosphere));
    }

    if (total_amount <= 0.0) {
        return make_mix_error(4215, "Total constituent quantity/proportion must be positive");
    }

    // 1. Resolve mass, moles, and volume for every constituent
    MixtureEvaluationResult res;
    const size_t N = elems.size();
    std::vector<double> masses_kg(N, 0.0);
    std::vector<double> moles_vec(N, 0.0);
    std::vector<double> vols_m3(N, 0.0);
    double sum_mass_kg = 0.0;
    double sum_moles = 0.0;
    double sum_vol = 0.0;

    bool has_condensed = false;
    for (const auto& b : behaviors) {
        if (b.phase == EvaluatedPhase::Solid || b.phase == EvaluatedPhase::Liquid) {
            has_condensed = true;
            break;
        }
    }

    for (size_t i = 0; i < N; ++i) {
        const auto& in = experiment.inputs[i];
        const double mw_g_mol = elems[i]->atomic_weight.value_or(1.0);
        double rho = behaviors[i].density_kg_m3.value.value_or(elems[i]->density.value_or(1000.0));
        // Bound light anions (H, N, O, F, Cl) in a condensed ChemicalCompound occupy liquid/ionic lattice volume, not STP ideal gas volume.
        if (experiment.scenario == FormationScenario::ChemicalCompound && has_condensed &&
            behaviors[i].phase == EvaluatedPhase::Gas) {
            double bound_rho = elems[i]->density_liquid.value_or(0.0);
            if (bound_rho <= 10.0) {
                switch (elems[i]->atomic_number) {
                case 1:  bound_rho = 700.0;  break; // bound H in hydrides/hydroxides
                case 7:  bound_rho = 1100.0; break; // nitrides
                case 8:  bound_rho = 1450.0; break; // oxides/phosphates
                case 9:  bound_rho = 1505.0; break; // fluorides
                case 17: bound_rho = 1560.0; break; // chlorides
                default: bound_rho = 1000.0; break;
                }
            }
            rho = bound_rho;
            behaviors[i].density_kg_m3.value = bound_rho;
        }
        if (!behaviors[i].density_kg_m3.available()) {
            res.warnings.push_back("Density missing for " + elems[i]->symbol + "; volume fraction approximated.");
        }

        if (experiment.scenario == FormationScenario::LayeredMaterial && in.layer_thickness_m > 0.0 &&
            in.basis == QuantityInputBasis::VolumeFractionPct) {
            vols_m3[i] = in.layer_thickness_m;
            masses_kg[i] = vols_m3[i] * rho;
            moles_vec[i] = (masses_kg[i] * 1000.0) / mw_g_mol;
        } else {
            switch (in.basis) {
            case QuantityInputBasis::MassGrams:
                masses_kg[i] = in.amount * 1e-3;
                moles_vec[i] = in.amount / mw_g_mol;
                vols_m3[i] = (rho > 0.0) ? (masses_kg[i] / rho) : 0.0;
                break;
            case QuantityInputBasis::MassFractionPct:
                masses_kg[i] = (in.amount / total_amount) * 1.0; // normalized to 1 kg basis
                moles_vec[i] = (masses_kg[i] * 1000.0) / mw_g_mol;
                vols_m3[i] = (rho > 0.0) ? (masses_kg[i] / rho) : 0.0;
                break;
            case QuantityInputBasis::MoleCount:
                moles_vec[i] = in.amount;
                masses_kg[i] = (in.amount * mw_g_mol) * 1e-3;
                vols_m3[i] = (rho > 0.0) ? (masses_kg[i] / rho) : 0.0;
                break;
            case QuantityInputBasis::MoleFractionPct:
                moles_vec[i] = in.amount / total_amount;
                masses_kg[i] = (moles_vec[i] * mw_g_mol) * 1e-3;
                vols_m3[i] = (rho > 0.0) ? (masses_kg[i] / rho) : 0.0;
                break;
            case QuantityInputBasis::VolumeFractionPct:
                vols_m3[i] = in.amount / total_amount;
                masses_kg[i] = vols_m3[i] * rho;
                moles_vec[i] = (masses_kg[i] * 1000.0) / mw_g_mol;
                break;
            }
        }
        sum_mass_kg += masses_kg[i];
        sum_moles += moles_vec[i];
        sum_vol += vols_m3[i];
    }

    if (sum_mass_kg <= 0.0 || sum_moles <= 0.0 || sum_vol <= 0.0) {
        return make_mix_error(4216, "Resolved mixture mass/moles/volume must be positive");
    }

    res.total_mass_kg = sum_mass_kg;
    res.constituents.reserve(N);
    std::vector<double> mole_fracs(N, 0.0);

    for (size_t i = 0; i < N; ++i) {
        NormalizedConstituent nc;
        nc.symbol = elems[i]->iupac_symbol.empty() ? elems[i]->symbol : elems[i]->iupac_symbol;
        nc.name = elems[i]->iupac_name.empty() ? elems[i]->name : elems[i]->iupac_name;
        nc.atomic_number = elems[i]->atomic_number;
        nc.mass_fraction = masses_kg[i] / sum_mass_kg;
        nc.mole_fraction = moles_vec[i] / sum_moles;
        nc.volume_fraction = vols_m3[i] / sum_vol;
        nc.mass_kg = masses_kg[i];
        nc.moles = moles_vec[i];
        nc.layer_thickness_m = experiment.inputs[i].layer_thickness_m;
        nc.pure_density_kg_m3 = behaviors[i].density_kg_m3.value;
        nc.pure_melting_point_k = elems[i]->melting_point;
        nc.pure_thermal_conductivity = behaviors[i].thermal_conductivity_w_mk.value;
        nc.pure_electrical_conductivity = behaviors[i].electrical_conductivity_s_m.value;
        nc.pure_specific_heat = behaviors[i].specific_heat_j_kgk.value;
        nc.pure_young_modulus_pa = elems[i]->young_modulus;
        nc.pure_thermal_expansion = elems[i]->thermal_expansion;
        mole_fracs[i] = nc.mole_fraction;
        res.effective_molar_mass_g_mol += nc.mole_fraction * elems[i]->atomic_weight.value_or(1.0);
        res.constituents.push_back(std::move(nc));
    }

    // 2. Formation scenario scientific validation
    for (const auto& b : behaviors) {
        for (const auto& w : b.warnings) {
            res.warnings.push_back("[" + b.symbol + "] " + w);
        }
    }

    if (experiment.scenario == FormationScenario::ChemicalCompound) {
        if (N < 2) {
            res.scientific_notes.push_back("Single-element substance treated as pure elemental species.");
        } else if (!can_neutralize_valence(elems, mole_fracs)) {
            res.is_chemically_valid = false;
            res.validation_errors.push_back(
                "Invalid ChemicalCompound: constituents cannot achieve formal valence/charge balance "
                "(e.g., noble gases or all-cation metallic elements form an alloy/mixture rather than a stoichiometric compound).");
        } else {
            res.warnings.push_back(
                "Compound properties computed via elemental mixing bounds; crystal bonding enthalpies/bandgaps require experimental compound data.");
        }
    } else if (experiment.scenario == FormationScenario::Alloy) {
        double nonmetal_mass_frac = 0.0;
        double min_r = 1e9, max_r = 0.0;
        for (size_t i = 0; i < N; ++i) {
            if (!is_metallic_element(*elems[i]) && res.constituents[i].mass_fraction > 0.0) {
                nonmetal_mass_frac += res.constituents[i].mass_fraction;
            }
            if (elems[i]->atomic_radius.has_value()) {
                min_r = std::min(min_r, *elems[i]->atomic_radius);
                max_r = std::max(max_r, *elems[i]->atomic_radius);
            }
        }
        if (nonmetal_mass_frac > 0.25) {
            res.is_chemically_valid = false;
            res.validation_errors.push_back(
                "Invalid Alloy: non-metallic content exceeds 25 wt%. Consider ChemicalCompound or CompositeMaterial.");
        } else if (nonmetal_mass_frac > 0.02) {
            res.warnings.push_back(
                "Non-metallic alloying additions (>2 wt%) may form brittle carbides/oxides/intermetallics.");
        }
        if (max_r > 0.0 && min_r < 1e8 && ((max_r - min_r) / min_r) > 0.15) {
            res.warnings.push_back(
                "Hume-Rothery size mismatch > 15%: limited solid solubility expected (intermetallic/two-phase risk).");
        }
    } else if (experiment.scenario == FormationScenario::Solution) {
        bool has_liquid = false;
        for (const auto& b : behaviors) {
            if (b.phase == EvaluatedPhase::Liquid) has_liquid = true;
        }
        if (!has_liquid) {
            res.warnings.push_back(
                "No constituent is liquid at T=" + std::to_string(experiment.temperature_k) +
                " K; modeling as solid/gas solution assumption.");
        }
    }

    // 3. Calculate Effective Properties (MATPROP)
    // Density
    res.density_kg_m3.unit = "kg/m^3";
    if (experiment.user_density_override_kg_m3 > 0.0) {
        res.density_kg_m3.value = experiment.user_density_override_kg_m3;
        res.density_kg_m3.epistemic = EpistemicStatus::ExperimentalAssumption;
        res.density_kg_m3.model_note = "User experimental density override.";
        res.overall_epistemic = EpistemicStatus::ExperimentalAssumption;
    } else {
        double spec_vol = 0.0;
        bool all_rho = true;
        for (size_t i = 0; i < N; ++i) {
            if (!res.constituents[i].pure_density_kg_m3.has_value() || *res.constituents[i].pure_density_kg_m3 <= 0.0) {
                all_rho = false;
                break;
            }
            spec_vol += res.constituents[i].mass_fraction / (*res.constituents[i].pure_density_kg_m3);
        }
        if (all_rho && spec_vol > 0.0) {
            res.density_kg_m3.value = 1.0 / spec_vol;
            res.density_kg_m3.epistemic = (N == 1) ? EpistemicStatus::ConfirmedScientific : EpistemicStatus::ApproximateSimulation;
            res.density_kg_m3.model_note = (N == 1) ? "Pure element density." : "Volume-additive rule of mixtures.";
        }
    }

    // Specific heat (Kopp-Neumann mass-fraction average over available constituents)
    res.specific_heat_j_kgk.unit = "J/(kg*K)";
    {
        double cp_sum = 0.0, w_cov = 0.0;
        for (const auto& c : res.constituents) {
            if (c.pure_specific_heat.has_value()) {
                cp_sum += c.mass_fraction * (*c.pure_specific_heat);
                w_cov += c.mass_fraction;
            }
        }
        if (w_cov > 0.5) {
            res.specific_heat_j_kgk.value = cp_sum / w_cov;
            res.specific_heat_j_kgk.epistemic = EpistemicStatus::ApproximateSimulation;
            res.specific_heat_j_kgk.model_note = (w_cov < 0.999)
                ? "Partial Kopp-Neumann mass average (some elements missing CSV Specific_Heat)."
                : "Kopp-Neumann mass-weighted specific heat.";
        }
    }

    // Thermal & Electrical conductivity
    res.thermal_conductivity_w_mk.unit = "W/(m*K)";
    res.thermal_conductivity_through_plane_w_mk.unit = "W/(m*K)";
    res.electrical_conductivity_s_m.unit = "S/m";
    res.electrical_resistivity_ohm_m.unit = "Ohm*m";
    res.electrical_conductivity_through_plane_s_m.unit = "S/m";

    if (experiment.scenario == FormationScenario::LayeredMaterial) {
        double tot_t = 0.0;
        for (const auto& c : res.constituents) tot_t += std::max(1e-9, c.layer_thickness_m);
        double k_par = 0.0, r_th_ser = 0.0, sig_par = 0.0, r_el_ser = 0.0;
        bool has_k = true, has_sig = true;
        for (const auto& c : res.constituents) {
            const double tf = std::max(1e-9, c.layer_thickness_m) / tot_t;
            if (c.pure_thermal_conductivity.has_value() && *c.pure_thermal_conductivity > 0.0) {
                k_par += tf * (*c.pure_thermal_conductivity);
                r_th_ser += tf / (*c.pure_thermal_conductivity);
            } else {
                has_k = false;
            }
            if (c.pure_electrical_conductivity.has_value() && *c.pure_electrical_conductivity > 0.0) {
                sig_par += tf * (*c.pure_electrical_conductivity);
                r_el_ser += tf / (*c.pure_electrical_conductivity);
            } else {
                has_sig = false;
            }
        }
        if (has_k && r_th_ser > 0.0) {
            res.thermal_conductivity_w_mk.value = k_par;
            res.thermal_conductivity_through_plane_w_mk.value = 1.0 / r_th_ser;
            res.thermal_conductivity_w_mk.epistemic = EpistemicStatus::ApproximateSimulation;
            res.thermal_conductivity_through_plane_w_mk.epistemic = EpistemicStatus::ApproximateSimulation;
            res.thermal_conductivity_w_mk.model_note = "In-plane parallel layer rule; through-plane series thermal resistance.";
        }
        if (has_sig && r_el_ser > 0.0) {
            res.electrical_conductivity_s_m.value = sig_par;
            res.electrical_resistivity_ohm_m.value = (sig_par > 0.0) ? (1.0 / sig_par) : 0.0;
            res.electrical_conductivity_through_plane_s_m.value = 1.0 / r_el_ser;
            res.electrical_conductivity_s_m.epistemic = EpistemicStatus::ApproximateSimulation;
            res.electrical_resistivity_ohm_m.epistemic = EpistemicStatus::ApproximateSimulation;
            res.electrical_conductivity_through_plane_s_m.epistemic = EpistemicStatus::ApproximateSimulation;
            res.electrical_conductivity_s_m.model_note = "In-plane parallel sheet conductance; through-plane series resistance.";
        }
    } else {
        // Thermal conductivity: volume-weighted parallel/series geometric average
        double k_par = 0.0, k_inv = 0.0, v_cov_k = 0.0;
        for (const auto& c : res.constituents) {
            if (c.pure_thermal_conductivity.has_value() && *c.pure_thermal_conductivity > 0.0) {
                k_par += c.volume_fraction * (*c.pure_thermal_conductivity);
                k_inv += c.volume_fraction / (*c.pure_thermal_conductivity);
                v_cov_k += c.volume_fraction;
            }
        }
        if (v_cov_k > 0.5 && k_inv > 0.0) {
            res.thermal_conductivity_w_mk.value = 0.5 * ((k_par / v_cov_k) + (v_cov_k / k_inv));
            res.thermal_conductivity_w_mk.epistemic = (N == 1) ? EpistemicStatus::ConfirmedScientific : EpistemicStatus::ApproximateSimulation;
            res.thermal_conductivity_w_mk.model_note = (N == 1) ? "Pure element thermal conductivity." : "Voigt-Reuss-Hill effective thermal conductivity.";
        }

        // Electrical conductivity / resistivity
        double rho_lin = 0.0, v_cov_e = 0.0;
        for (const auto& c : res.constituents) {
            if (c.pure_electrical_conductivity.has_value() && *c.pure_electrical_conductivity > 0.0) {
                rho_lin += c.volume_fraction * (1.0 / (*c.pure_electrical_conductivity));
                v_cov_e += c.volume_fraction;
            }
        }
        if (v_cov_e > 0.5 && rho_lin > 0.0) {
            double rho_eff = rho_lin / v_cov_e;
            // Nordheim alloy scattering term for multi-element metallic alloys
            if (experiment.scenario == FormationScenario::Alloy && N > 1) {
                double nordheim_prod = 0.0;
                for (size_t i = 0; i < N; ++i) {
                    for (size_t j = i + 1; j < N; ++j) {
                        nordheim_prod += res.constituents[i].mole_fraction * res.constituents[j].mole_fraction;
                    }
                }
                constexpr double kNordheimCoeff = 1.2e-7; // Ohm*m order-of-magnitude solute scattering estimate
                rho_eff += kNordheimCoeff * nordheim_prod;
            }
            res.electrical_resistivity_ohm_m.value = rho_eff;
            res.electrical_conductivity_s_m.value = 1.0 / rho_eff;
            res.electrical_resistivity_ohm_m.epistemic = (N == 1) ? EpistemicStatus::ConfirmedScientific : EpistemicStatus::ApproximateSimulation;
            res.electrical_conductivity_s_m.epistemic = res.electrical_resistivity_ohm_m.epistemic;
            res.electrical_resistivity_ohm_m.model_note = (experiment.scenario == FormationScenario::Alloy && N > 1)
                ? "Volume-weighted resistivity + Nordheim alloy scattering estimate."
                : "Effective resistivity from constituent conductivities.";
        }
    }

    // Mechanical homogenization via CompositeHomogenizer where bulk & shear moduli exist
    res.young_modulus_pa.unit = "Pa";
    res.bulk_modulus_pa.unit = "Pa";
    res.shear_modulus_pa.unit = "Pa";
    res.poisson_ratio.unit = "dimensionless";
    {
        std::vector<ElasticPhase> phases;
        double v_sum = 0.0;
        for (size_t i = 0; i < N; ++i) {
            if (elems[i]->bulk_modulus.has_value() && elems[i]->shear_modulus.has_value() &&
                *elems[i]->bulk_modulus > 0.0 && *elems[i]->shear_modulus > 0.0) {
                phases.push_back({
                    res.constituents[i].symbol,
                    res.constituents[i].volume_fraction,
                    (*elems[i]->bulk_modulus) * 1e-9,
                    (*elems[i]->shear_modulus) * 1e-9
                });
                v_sum += res.constituents[i].volume_fraction;
            }
        }
        if (!phases.empty() && v_sum > 0.6) {
            for (auto& p : phases) p.volume_fraction /= v_sum;
            auto h_res = CompositeHomogenizer::evaluate(phases);
            if (h_res.is_ok()) {
                const auto& hb = h_res.value();
                res.bulk_modulus_pa.value = hb.bulk_hill_gpa * 1e9;
                res.shear_modulus_pa.value = hb.shear_hill_gpa * 1e9;
                res.young_modulus_pa.value = hb.effective_youngs_gpa * 1e9;
                res.poisson_ratio.value = hb.effective_poisson;
                res.young_voigt_pa = (9.0 * hb.bulk_voigt_gpa * hb.shear_voigt_gpa) /
                                     (3.0 * hb.bulk_voigt_gpa + hb.shear_voigt_gpa) * 1e9;
                res.young_reuss_pa = (9.0 * hb.bulk_reuss_gpa * hb.shear_reuss_gpa) /
                                     (3.0 * hb.bulk_reuss_gpa + hb.shear_reuss_gpa) * 1e9;
                res.young_hs_lower_pa = (9.0 * hb.bulk_hs_lower_gpa * hb.shear_hs_lower_gpa) /
                                        (3.0 * hb.bulk_hs_lower_gpa + hb.shear_hs_lower_gpa) * 1e9;
                res.young_hs_upper_pa = (9.0 * hb.bulk_hs_upper_gpa * hb.shear_hs_upper_gpa) /
                                        (3.0 * hb.bulk_hs_upper_gpa + hb.shear_hs_upper_gpa) * 1e9;
                res.young_modulus_pa.epistemic = EpistemicStatus::ApproximateSimulation;
                res.bulk_modulus_pa.epistemic = EpistemicStatus::ApproximateSimulation;
                res.shear_modulus_pa.epistemic = EpistemicStatus::ApproximateSimulation;
                res.poisson_ratio.epistemic = EpistemicStatus::ApproximateSimulation;
                res.young_modulus_pa.model_note = "Voigt-Reuss-Hill + Hashin-Shtrikman bounds.";
            }
        }
    }

    // Thermal expansion (Turner/Voigt average) & Melting point estimate
    res.thermal_expansion_1_k.unit = "1/K";
    res.estimated_melting_point_k.unit = "K";
    {
        double a_sum = 0.0, v_cov = 0.0, mp_sum = 0.0, m_cov = 0.0;
        for (const auto& c : res.constituents) {
            if (c.pure_thermal_expansion.has_value()) {
                a_sum += c.volume_fraction * (*c.pure_thermal_expansion);
                v_cov += c.volume_fraction;
            }
            if (c.pure_melting_point_k.has_value()) {
                mp_sum += c.mole_fraction * (*c.pure_melting_point_k);
                m_cov += c.mole_fraction;
            }
        }
        if (v_cov > 0.5) {
            res.thermal_expansion_1_k.value = a_sum / v_cov;
            res.thermal_expansion_1_k.epistemic = EpistemicStatus::ApproximateSimulation;
            res.thermal_expansion_1_k.model_note = "Volume-fraction linear thermal expansion.";
        }
        if (m_cov > 0.5) {
            const double tm_est = mp_sum / m_cov;
            res.estimated_melting_point_k.value = tm_est;
            res.estimated_melting_point_k.epistemic = (N == 1) ? EpistemicStatus::ConfirmedScientific : EpistemicStatus::ApproximateSimulation;
            res.estimated_melting_point_k.model_note = (N == 1) ? "CSV Melting_Point." : "Linear liquidus estimate (eutectic depression not modeled).";
            res.predicted_phase = (experiment.temperature_k < tm_est) ? EvaluatedPhase::Solid : EvaluatedPhase::Liquid;
        }
    }

    // 4. Build element-vs-mixture comparison table
    auto add_cmp = [&](std::string prop_name, std::string unit,
                       const EvaluatedProperty<double>& mix_prop,
                       auto getter) {
        ElementComparisonRow row;
        row.property_name = std::move(prop_name);
        row.unit = std::move(unit);
        row.mixture_value = mix_prop.value;
        row.epistemic = mix_prop.epistemic;
        for (const auto& c : res.constituents) {
            row.constituent_values.emplace_back(c.symbol, getter(c));
        }
        res.comparison_rows.push_back(std::move(row));
    };

    add_cmp("Density", "kg/m^3", res.density_kg_m3,
            [](const NormalizedConstituent& c) { return c.pure_density_kg_m3; });
    add_cmp("Thermal Conductivity", "W/(m*K)", res.thermal_conductivity_w_mk,
            [](const NormalizedConstituent& c) { return c.pure_thermal_conductivity; });
    add_cmp("Electrical Conductivity", "S/m", res.electrical_conductivity_s_m,
            [](const NormalizedConstituent& c) { return c.pure_electrical_conductivity; });
    add_cmp("Specific Heat", "J/(kg*K)", res.specific_heat_j_kgk,
            [](const NormalizedConstituent& c) { return c.pure_specific_heat; });
    add_cmp("Thermal Expansion", "1/K", res.thermal_expansion_1_k,
            [](const NormalizedConstituent& c) { return c.pure_thermal_expansion; });
    add_cmp("Young's Modulus", "Pa", res.young_modulus_pa,
            [](const NormalizedConstituent& c) { return c.pure_young_modulus_pa; });

    res.has_warnings = !res.warnings.empty();
    if (!res.is_chemically_valid) {
        res.overall_epistemic = EpistemicStatus::Unsupported;
    } else if (N == 1 && experiment.user_density_override_kg_m3 <= 0.0) {
        res.overall_epistemic = EpistemicStatus::ConfirmedScientific;
    }
    return res;
}

core::base::Result<core::domain::Material> MaterialMixingLab::promote_to_material(
    MixtureExperiment& experiment,
    const matdb::ElementDatabase& db) {
    auto eval_res = evaluate(experiment, db);
    if (eval_res.is_err()) return eval_res.error();
    const auto& ev = eval_res.value();
    if (!ev.is_chemically_valid) {
        std::string reason = ev.validation_errors.empty() ? "Invalid chemical/material combination" : ev.validation_errors.front();
        return make_mix_error(4220, "Cannot promote invalid mixture to reusable Material", std::move(reason));
    }

    core::domain::MaterialPhysicalState state = core::domain::MaterialPhysicalState::Solid;
    if (ev.predicted_phase == EvaluatedPhase::Liquid) state = core::domain::MaterialPhysicalState::Liquid;
    else if (ev.predicted_phase == EvaluatedPhase::Gas || ev.predicted_phase == EvaluatedPhase::Supercritical) {
        state = core::domain::MaterialPhysicalState::Gas;
    }

    core::domain::Material mat(
        core::base::Uuid::generate_random(),
        experiment.name,
        state,
        ev.density_kg_m3.value.value_or(1000.0));

    std::ostringstream comp;
    for (size_t i = 0; i < ev.constituents.size(); ++i) {
        if (i > 0) comp << ", ";
        comp << ev.constituents[i].symbol << " ("
             << std::fixed << std::setprecision(1) << (ev.constituents[i].mass_fraction * 100.0) << " wt%)";
    }
    mat.set_composition(comp.str());
    mat.set_description(experiment.description);
    mat.set_provenance("MixLab:" + std::string(formation_scenario_to_string(experiment.scenario)) +
                       "|" + std::string(epistemic_status_to_string(ev.overall_epistemic)));
    mat.set_source("Experiment:" + experiment.id.to_string());

    auto& thm = mat.thermal();
    thm.thermal_conductivity = ev.thermal_conductivity_w_mk.value;
    thm.specific_heat_capacity = ev.specific_heat_j_kgk.value;
    thm.thermal_expansion_coeff = ev.thermal_expansion_1_k.value;
    thm.melting_point_k = ev.estimated_melting_point_k.value;

    auto& elec = mat.electrical();
    elec.electrical_conductivity = ev.electrical_conductivity_s_m.value;
    elec.electrical_resistivity = ev.electrical_resistivity_ohm_m.value;

    auto& mech = mat.mechanical();
    mech.youngs_modulus_pa = ev.young_modulus_pa.value;
    mech.bulk_modulus_pa = ev.bulk_modulus_pa.value;
    mech.shear_modulus_pa = ev.shear_modulus_pa.value;
    mech.poissons_ratio = ev.poisson_ratio.value;

    experiment.promoted_material_id = mat.id();
    return mat;
}

core::base::Result<std::vector<std::pair<double, MixtureEvaluationResult>>> MaterialMixingLab::sweep_temperature(
    const MixtureExperiment& experiment,
    double t_min_k,
    double t_max_k,
    size_t steps,
    const matdb::ElementDatabase& db) {
    if (steps < 2 || t_max_k <= t_min_k || t_min_k <= 0.0) {
        return make_mix_error(4221, "Invalid temperature sweep range or step count");
    }
    std::vector<std::pair<double, MixtureEvaluationResult>> out;
    out.reserve(steps);
    const double dt = (t_max_k - t_min_k) / static_cast<double>(steps - 1);
    for (size_t i = 0; i < steps; ++i) {
        MixtureExperiment copy = experiment;
        copy.temperature_k = t_min_k + static_cast<double>(i) * dt;
        auto res = evaluate(copy, db);
        if (res.is_err()) return res.error();
        out.emplace_back(copy.temperature_k, std::move(res.value()));
    }
    return out;
}

std::vector<MixtureExperiment> MaterialMixingLab::standard_presets() {
    std::vector<MixtureExperiment> list;

    // 1. Carbon Steel (Fe-C Alloy)
    MixtureExperiment steel;
    steel.name = "Carbon Steel (Fe-1C)";
    steel.description = "Structural Fe-C alloy for plates, rods, and housings.";
    steel.scenario = FormationScenario::Alloy;
    steel.inputs = {
        {"Fe", QuantityInputBasis::MassFractionPct, 99.0, 1e-3, false},
        {"C",  QuantityInputBasis::MassFractionPct, 1.0,  1e-3, false}
    };
    list.push_back(std::move(steel));

    // 2. Cartridge Brass (Cu-Zn Alloy)
    MixtureExperiment brass;
    brass.name = "Cartridge Brass (Cu-30Zn)";
    brass.description = "Conductive Cu-Zn engineering alloy.";
    brass.scenario = FormationScenario::Alloy;
    brass.inputs = {
        {"Cu", QuantityInputBasis::MassFractionPct, 70.0, 1e-3, false},
        {"Zn", QuantityInputBasis::MassFractionPct, 30.0, 1e-3, false}
    };
    list.push_back(std::move(brass));

    // 3. LiFePO4 Cathode Active Material (Stoichiometric Compound)
    MixtureExperiment lfp;
    lfp.name = "LiFePO4 Cathode Material";
    lfp.description = "Lithium iron phosphate cathode active compound (1:1:1:4 molar).";
    lfp.scenario = FormationScenario::ChemicalCompound;
    lfp.inputs = {
        {"Li", QuantityInputBasis::MoleCount, 1.0, 1e-3, false},
        {"Fe", QuantityInputBasis::MoleCount, 1.0, 1e-3, false},
        {"P",  QuantityInputBasis::MoleCount, 1.0, 1e-3, false},
        {"O",  QuantityInputBasis::MoleCount, 4.0, 1e-3, false}
    };
    list.push_back(std::move(lfp));

    // 4. Al-Cu Clad Busbar (Layered Material)
    MixtureExperiment clad;
    clad.name = "Cu-Al Clad Busbar";
    clad.description = "Layered copper-aluminum conductor stackup.";
    clad.scenario = FormationScenario::LayeredMaterial;
    clad.inputs = {
        {"Cu", QuantityInputBasis::VolumeFractionPct, 30.0, 0.3e-3, false},
        {"Al", QuantityInputBasis::VolumeFractionPct, 70.0, 0.7e-3, false}
    };
    list.push_back(std::move(clad));

    return list;
}

} // namespace shunya::mat
