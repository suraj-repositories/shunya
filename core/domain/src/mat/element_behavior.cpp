#include "shunya/mat/element_behavior.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

namespace {
constexpr double kGasConstantR = 8.31446261815324; // J/(mol*K)
constexpr double kRefTempK = 293.15;
constexpr double kStdPressurePa = 101325.0;
} // namespace

std::optional<double> ElementBehaviorEngine::standard_reduction_potential_v(int atomic_number) noexcept {
    // Standard aqueous reduction potentials (V vs SHE at 298.15 K) for key engineering metals
    switch (atomic_number) {
    case 3:  return -3.04; // Li+ / Li
    case 11: return -2.71; // Na+ / Na
    case 12: return -2.37; // Mg2+ / Mg
    case 13: return -1.66; // Al3+ / Al
    case 22: return -1.63; // Ti2+ / Ti
    case 25: return -1.18; // Mn2+ / Mn
    case 30: return -0.76; // Zn2+ / Zn
    case 24: return -0.74; // Cr3+ / Cr
    case 26: return -0.44; // Fe2+ / Fe
    case 27: return -0.28; // Co2+ / Co
    case 28: return -0.25; // Ni2+ / Ni
    case 50: return -0.14; // Sn2+ / Sn
    case 82: return -0.13; // Pb2+ / Pb
    case 29: return +0.34; // Cu2+ / Cu
    case 47: return +0.80; // Ag+ / Ag
    case 78: return +1.18; // Pt2+ / Pt
    case 79: return +1.50; // Au3+ / Au
    default: return std::nullopt;
    }
}

ElementBehaviorReport ElementBehaviorEngine::evaluate(
    const matdb::ElementRecord& elem,
    double temperature_k,
    double pressure_pa,
    bool oxidizing_atmosphere) {
    ElementBehaviorReport r;
    r.atomic_number = elem.atomic_number;
    r.symbol = elem.iupac_symbol.empty() ? elem.symbol : elem.iupac_symbol;
    r.name = elem.iupac_name.empty() ? elem.name : elem.iupac_name;
    r.temperature_k = temperature_k;
    r.pressure_pa = pressure_pa;

    if (!std::isfinite(temperature_k) || temperature_k <= 0.0) {
        r.stable_at_conditions = false;
        r.warnings.push_back("Temperature must be finite and strictly positive (K).");
        return r;
    }
    if (!std::isfinite(pressure_pa) || pressure_pa < 0.0) {
        r.stable_at_conditions = false;
        r.warnings.push_back("Pressure must be finite and non-negative (Pa).");
        return r;
    }

    // 1. Phase transitions
    r.has_melting_point = elem.melting_point.has_value();
    r.has_boiling_point = elem.boiling_point.has_value();
    if (r.has_melting_point) r.effective_melting_point_k = *elem.melting_point;

    if (r.has_boiling_point) {
        double tb = *elem.boiling_point;
        if (pressure_pa > 1.0 && std::abs(pressure_pa - kStdPressurePa) > 100.0 &&
            elem.heat_of_vaporization.has_value() && *elem.heat_of_vaporization > 0.0) {
            const double dh_vap_j_mol = (*elem.heat_of_vaporization) * 1000.0;
            const double inv_tb = (1.0 / tb) - (kGasConstantR * std::log(pressure_pa / kStdPressurePa)) / dh_vap_j_mol;
            if (inv_tb > 1e-6) {
                tb = 1.0 / inv_tb;
                r.phase_note = "Boiling point adjusted for pressure via Clausius-Clapeyron approximation.";
            }
        }
        r.effective_boiling_point_k = tb;
    }

    if (elem.critical_temperature.has_value() && elem.critical_pressure.has_value() &&
        temperature_k >= *elem.critical_temperature && pressure_pa >= *elem.critical_pressure) {
        r.phase = EvaluatedPhase::Supercritical;
        r.phase_note = "Above CSV Critical_Temperature and Critical_Pressure.";
    } else if (r.has_melting_point && r.has_boiling_point) {
        const double tm = r.effective_melting_point_k;
        const double tb = r.effective_boiling_point_k;
        if (std::abs(temperature_k - tm) <= 0.5) {
            r.phase = EvaluatedPhase::MeltingTwoPhase;
        } else if (temperature_k < tm) {
            r.phase = EvaluatedPhase::Solid;
        } else if (std::abs(temperature_k - tb) <= 0.5) {
            r.phase = EvaluatedPhase::BoilingTwoPhase;
        } else if (temperature_k < tb) {
            r.phase = EvaluatedPhase::Liquid;
        } else {
            r.phase = EvaluatedPhase::Gas;
        }
        if (r.phase_note.empty()) {
            r.phase_note = "Determined from CSV Melting_Point and Boiling_Point.";
        }
    } else if (r.has_melting_point) {
        r.phase = (temperature_k < r.effective_melting_point_k) ? EvaluatedPhase::Solid : EvaluatedPhase::Liquid;
        r.phase_note = "Boiling_Point unavailable in CSV; solid/liquid boundary only.";
        r.unsupported_properties.push_back("Boiling_Point");
    } else {
        r.phase = EvaluatedPhase::Unknown;
        r.phase_note = "Melting_Point and Boiling_Point missing in CSV.";
        r.unsupported_properties.push_back("Melting_Point");
        r.unsupported_properties.push_back("Boiling_Point");
    }

    // 2. Thermal expansion & strain
    r.thermal_expansion_1_k.unit = "1/K";
    r.thermal_strain_fraction.unit = "dimensionless";
    if (elem.thermal_expansion.has_value()) {
        r.thermal_expansion_1_k.value = *elem.thermal_expansion;
        r.thermal_expansion_1_k.epistemic = EpistemicStatus::ConfirmedScientific;
        r.thermal_expansion_1_k.model_note = "CSV linear thermal expansion coefficient.";
        if (r.phase == EvaluatedPhase::Solid || r.phase == EvaluatedPhase::MeltingTwoPhase) {
            r.thermal_strain_fraction.value = (*elem.thermal_expansion) * (temperature_k - kRefTempK);
            r.thermal_strain_fraction.epistemic = EpistemicStatus::ApproximateSimulation;
            r.thermal_strain_fraction.model_note = "Linear approximation epsilon = alpha * (T - 293.15 K).";
        }
    } else {
        r.unsupported_properties.push_back("Thermal_Expansion");
    }

    // 3. Density(T, P)
    r.density_kg_m3.unit = "kg/m^3";
    if (r.phase == EvaluatedPhase::Gas || r.phase == EvaluatedPhase::Supercritical) {
        if (elem.atomic_weight.has_value() && pressure_pa > 0.0) {
            double atomicity = 1.0;
            if (elem.gas_phase == "Diatomic") atomicity = 2.0;
            const double molar_mass_kg = (*elem.atomic_weight) * atomicity * 1e-3;
            r.density_kg_m3.value = (pressure_pa * molar_mass_kg) / (kGasConstantR * temperature_k);
            r.density_kg_m3.epistemic = (r.phase == EvaluatedPhase::Gas)
                ? EpistemicStatus::ApproximateSimulation
                : EpistemicStatus::ExperimentalAssumption;
            r.density_kg_m3.model_note = (atomicity > 1.5)
                ? "Ideal gas EOS with diatomic molar mass (Gas_phase = Diatomic)."
                : "Ideal gas EOS with monoatomic molar mass.";
        } else if (elem.density.has_value()) {
            r.density_kg_m3.value = *elem.density;
            r.density_kg_m3.epistemic = EpistemicStatus::ConfirmedScientific;
            r.density_kg_m3.model_note = "STP reference density from CSV.";
        } else {
            r.unsupported_properties.push_back("Density");
        }
    } else if (r.phase == EvaluatedPhase::Liquid) {
        if (elem.density_liquid.has_value()) {
            r.density_kg_m3.value = *elem.density_liquid;
            r.density_kg_m3.epistemic = EpistemicStatus::ConfirmedScientific;
            r.density_kg_m3.model_note = "CSV liquid density (Density_Liquid).";
        } else if (elem.density.has_value()) {
            r.density_kg_m3.value = *elem.density;
            r.density_kg_m3.epistemic = EpistemicStatus::ApproximateSimulation;
            r.density_kg_m3.model_note = "Density_Liquid missing; falling back to solid STP Density.";
        } else {
            r.unsupported_properties.push_back("Density");
        }
    } else if (elem.density.has_value()) {
        if (elem.thermal_expansion.has_value() && std::abs(temperature_k - kRefTempK) > 1.0) {
            const double vol_factor = std::max(0.5, 1.0 + 3.0 * (*elem.thermal_expansion) * (temperature_k - kRefTempK));
            r.density_kg_m3.value = (*elem.density) / vol_factor;
            r.density_kg_m3.epistemic = EpistemicStatus::ApproximateSimulation;
            r.density_kg_m3.model_note = "Volumetric thermal expansion rho(T) = rho0 / (1 + 3*alpha*dT).";
        } else {
            r.density_kg_m3.value = *elem.density;
            r.density_kg_m3.epistemic = EpistemicStatus::ConfirmedScientific;
            r.density_kg_m3.model_note = "CSV STP density.";
        }
    } else {
        r.unsupported_properties.push_back("Density");
    }

    // 4. Specific heat & Thermal conductivity
    r.specific_heat_j_kgk.unit = "J/(kg*K)";
    if (elem.specific_heat.has_value()) {
        r.specific_heat_j_kgk.value = *elem.specific_heat;
        r.specific_heat_j_kgk.epistemic = EpistemicStatus::ConfirmedScientific;
        r.specific_heat_j_kgk.model_note = "CSV Specific_Heat (constant-Cp baseline).";
    } else {
        r.unsupported_properties.push_back("Specific_Heat");
    }

    r.thermal_conductivity_w_mk.unit = "W/(m*K)";
    if (elem.thermal_conductivity.has_value()) {
        double k = *elem.thermal_conductivity;
        if (r.phase == EvaluatedPhase::Gas && elem.phase == "Solid") {
            r.thermal_conductivity_w_mk.epistemic = EpistemicStatus::ExperimentalAssumption;
            r.thermal_conductivity_w_mk.model_note = "Vapor-phase thermal conductivity not measured in CSV; solid k retained as reference only.";
        } else {
            r.thermal_conductivity_w_mk.epistemic = (std::abs(temperature_k - kRefTempK) <= 20.0)
                ? EpistemicStatus::ConfirmedScientific
                : EpistemicStatus::ApproximateSimulation;
            r.thermal_conductivity_w_mk.model_note = "CSV Thermal_Conductivity at reference state.";
        }
        r.thermal_conductivity_w_mk.value = k;
    } else {
        r.unsupported_properties.push_back("Thermal_Conductivity");
    }

    // 5. Electrical behavior & superconductivity
    r.electrical_conductivity_s_m.unit = "S/m";
    r.electrical_resistivity_ohm_m.unit = "Ohm*m";
    if (elem.superconducting_point.has_value() && temperature_k <= *elem.superconducting_point) {
        r.is_superconducting = true;
        r.electrical_resistivity_ohm_m.value = 0.0;
        r.electrical_resistivity_ohm_m.epistemic = EpistemicStatus::ConfirmedScientific;
        r.electrical_resistivity_ohm_m.model_note = "Below CSV Superconducting_Point Tc.";
        r.electrical_conductivity_s_m.epistemic = EpistemicStatus::ConfirmedScientific;
        r.electrical_conductivity_s_m.model_note = "Superconducting zero-DC-resistance state (sigma -> inf).";
    } else if (auto rho0 = elem.effective_resistivity()) {
        double rho_t = *rho0;
        if (elem.electrical_type == "Conductor" && std::abs(temperature_k - kRefTempK) > 1.0) {
            constexpr double kApproxMetallicTcr = 0.0039; // ~0.0039 / K typical pure metal approximation
            const double scale = std::max(0.05, 1.0 + kApproxMetallicTcr * (temperature_k - kRefTempK));
            rho_t = (*rho0) * scale;
            r.electrical_resistivity_ohm_m.epistemic = EpistemicStatus::ApproximateSimulation;
            r.electrical_resistivity_ohm_m.model_note = "Linear metallic TCR estimate (alpha_R ~ 0.0039/K).";
            r.electrical_conductivity_s_m.epistemic = EpistemicStatus::ApproximateSimulation;
            r.electrical_conductivity_s_m.model_note = "Derived sigma(T) = 1 / rho(T).";
        } else if (elem.electrical_type == "Semiconductor" && std::abs(temperature_k - kRefTempK) > 1.0) {
            // Intrinsic carrier thermal activation qualitative scaling (not calibrated bandgap)
            const double factor = std::exp(std::clamp(2500.0 * ((1.0 / temperature_k) - (1.0 / kRefTempK)), -10.0, 10.0));
            rho_t = (*rho0) * factor;
            r.electrical_resistivity_ohm_m.epistemic = EpistemicStatus::ExperimentalAssumption;
            r.electrical_resistivity_ohm_m.model_note = "Qualitative NTC semiconductor scaling (bandgap not in CSV).";
            r.electrical_conductivity_s_m.epistemic = EpistemicStatus::ExperimentalAssumption;
            r.electrical_conductivity_s_m.model_note = "Derived sigma(T) = 1 / rho(T).";
        } else {
            r.electrical_resistivity_ohm_m.epistemic = EpistemicStatus::ConfirmedScientific;
            r.electrical_resistivity_ohm_m.model_note = "CSV Resistivity / Electrical_Conductivity.";
            r.electrical_conductivity_s_m.epistemic = EpistemicStatus::ConfirmedScientific;
            r.electrical_conductivity_s_m.model_note = "CSV Electrical_Conductivity / 1/Resistivity.";
        }
        r.electrical_resistivity_ohm_m.value = rho_t;
        r.electrical_conductivity_s_m.value = (rho_t > 1e-18) ? (1.0 / rho_t) : 0.0;
    } else {
        r.unsupported_properties.push_back("Electrical_Conductivity");
    }

    // 6. Magnetic state transitions
    r.magnetic_state = elem.magnetic_type.empty() ? "Unknown" : elem.magnetic_type;
    if (elem.curie_point.has_value() && temperature_k > *elem.curie_point) {
        r.magnetic_state = "Paramagnetic (above Curie Point " + std::to_string(*elem.curie_point) + " K)";
    } else if (elem.neel_point.has_value() && temperature_k > *elem.neel_point) {
        r.magnetic_state = "Paramagnetic (above Neel Point " + std::to_string(*elem.neel_point) + " K)";
    }

    // 7. Chemical reactivity, radioactivity, corrosion & combustion
    r.is_radioactive = elem.is_radioactive_in_csv;
    if (r.is_radioactive) {
        r.warnings.push_back("Radioactive element (CSV Decay_Mode: " +
                             (elem.decay_mode.empty() ? "Unspecified" : elem.decay_mode) + ").");
    }

    const int grp = elem.group.value_or(0);
    if (grp == 18) {
        r.reactivity_class = "Noble Gas (Chemically Inert)";
        r.reactivity_index = 0.05;
    } else if (grp == 1 && elem.period >= 2) {
        r.reactivity_class = "Alkali Metal (Highly Reactive)";
        r.reactivity_index = 0.95;
        r.oxidation_susceptible = true;
        r.corrosion_susceptible = true;
        if (oxidizing_atmosphere) {
            r.warnings.push_back(r.symbol + " reacts vigorously with air/moisture and requires inert atmosphere or sealing.");
        }
    } else if (grp == 2) {
        r.reactivity_class = "Alkaline Earth Metal (Reactive)";
        r.reactivity_index = 0.75;
        r.oxidation_susceptible = true;
        r.corrosion_susceptible = true;
    } else if (grp == 17) {
        r.reactivity_class = "Halogen (Strong Oxidizer / Corrosive)";
        r.reactivity_index = 0.90;
        r.corrosion_susceptible = true;
    } else if (r.atomic_number == 13 || r.atomic_number == 22 || r.atomic_number == 24 || r.atomic_number == 14) {
        r.reactivity_class = "Passivating Metal / Metalloid";
        r.reactivity_index = 0.35;
        r.oxidation_susceptible = true;
        r.corrosion_susceptible = false; // self-passivating native oxide film
    } else if (r.atomic_number == 79 || r.atomic_number == 78 || r.atomic_number == 47 || r.atomic_number == 77) {
        r.reactivity_class = "Noble / Precious Metal";
        r.reactivity_index = 0.10;
        r.oxidation_susceptible = false;
        r.corrosion_susceptible = false;
    } else if (elem.electrical_type == "Conductor") {
        r.reactivity_class = "Transition / Post-Transition Metal";
        r.reactivity_index = 0.45;
        r.oxidation_susceptible = true;
        auto e0 = standard_reduction_potential_v(r.atomic_number);
        r.corrosion_susceptible = e0.has_value() ? (*e0 < 0.0) : true;
    } else {
        r.reactivity_class = "Nonmetal / Metalloid";
        r.reactivity_index = 0.50;
    }

    if (oxidizing_atmosphere && elem.autoignition_point.has_value() && temperature_k >= *elem.autoignition_point) {
        r.combustion_risk = true;
        r.stable_at_conditions = false;
        r.warnings.push_back("COMBUSTION HAZARD: Temperature (" + std::to_string(temperature_k) +
                             " K) exceeds CSV Autoignition_Point (" + std::to_string(*elem.autoignition_point) + " K).");
    } else if (oxidizing_atmosphere && elem.flashpoint.has_value() && temperature_k >= *elem.flashpoint) {
        r.combustion_risk = true;
        r.warnings.push_back("FLAMMABILITY WARNING: Temperature exceeds CSV Flashpoint (" +
                             std::to_string(*elem.flashpoint) + " K).");
    }

    if (r.phase == EvaluatedPhase::Liquid || r.phase == EvaluatedPhase::Gas || r.phase == EvaluatedPhase::MeltingTwoPhase) {
        if (elem.phase == "Solid") {
            r.warnings.push_back("Structural phase transition: solid-at-STP element is " +
                                 std::string(evaluated_phase_to_string(r.phase)) + " at " +
                                 std::to_string(temperature_k) + " K.");
        }
    }

    return r;
}

core::base::Result<ElementBehaviorReport> ElementBehaviorEngine::evaluate_by_symbol(
    std::string_view symbol,
    double temperature_k,
    double pressure_pa,
    bool oxidizing_atmosphere) {
    const auto* elem = matdb::ElementDatabase::global().find_by_symbol(symbol);
    if (!elem) {
        return core::base::Error{
            .code = 4150,
            .domain = core::base::ErrorDomain::Materials,
            .severity = core::base::Severity::Error,
            .message = "Element not found in ElementDatabase: " + std::string(symbol),
            .details = ""
        };
    }
    return evaluate(*elem, temperature_k, pressure_pa, oxidizing_atmosphere);
}

} // namespace shunya::mat
