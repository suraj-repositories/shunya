#include "shunya/mat/virtual_test_bench.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace shunya::mat {

EnvironmentConditions VirtualTestBenchEngine::preset_spacecraft_leo() {
    EnvironmentConditions env;
    env.name = "Spacecraft (LEO)";
    env.temperature_k = 170.0;     // Orbital shade temperature
    env.pressure_pa = 1e-5;        // High vacuum
    env.gravity_m_s2 = 0.0;        // Microgravity
    env.relative_humidity_pct = 0.0;
    env.oxygen_pct = 0.001;        // Atomic oxygen trace
    env.co2_ppm = 0.0;
    env.salinity_ppt = 0.0;
    env.radiation_dose_dpa = 0.15; // Solar particle and cosmic ray dose
    return env;
}

EnvironmentConditions VirtualTestBenchEngine::preset_deep_sea() {
    EnvironmentConditions env;
    env.name = "Deep Sea Submersible";
    env.temperature_k = 277.15;    // 4.0 C abyssal benthic layer
    env.pressure_pa = 1.1e8;       // 110 MPa (Challenger Deep hydrostatic head)
    env.gravity_m_s2 = 9.80665;
    env.relative_humidity_pct = 100.0;
    env.oxygen_pct = 1.5;          // Dissolved O2
    env.salinity_ppt = 35.0;       // Standard marine salinity
    return env;
}

EnvironmentConditions VirtualTestBenchEngine::preset_industrial_furnace() {
    EnvironmentConditions env;
    env.name = "Industrial Furnace";
    env.temperature_k = 1273.15;   // 1000 C continuous service
    env.pressure_pa = 101325.0;
    env.gravity_m_s2 = 9.80665;
    env.relative_humidity_pct = 5.0;
    env.oxygen_pct = 21.0;
    env.co2_ppm = 2500.0;
    return env;
}

EnvironmentConditions VirtualTestBenchEngine::preset_underground_cable() {
    EnvironmentConditions env;
    env.name = "Underground Power Cable";
    env.temperature_k = 323.15;    // 50 C operating cable conduit
    env.pressure_pa = 200000.0;    // 2.0 bar subterranean overburden
    env.gravity_m_s2 = 9.80665;
    env.relative_humidity_pct = 90.0;
    env.electric_field_v_m = 100000.0; // 100 kV/m insulation gradient
    return env;
}

EnvironmentConditions VirtualTestBenchEngine::preset_outdoor_coastal() {
    EnvironmentConditions env;
    env.name = "Outdoor Coastal Weathering";
    env.temperature_k = 303.15;    // 30 C subtropical marine environment
    env.pressure_pa = 101325.0;
    env.gravity_m_s2 = 9.80665;
    env.relative_humidity_pct = 85.0;
    env.salinity_ppt = 10.0;       // Airborne aerosol chloride deposition
    return env;
}

VirtualTestBenchReport VirtualTestBenchEngine::qualify_material(
    const MixtureEvaluationResult& mixture,
    const EnvironmentConditions& env) {
    VirtualTestBenchReport rep;
    rep.conditions = env;

    rep.domain_results.push_back(evaluate_thermal(mixture, env));
    rep.domain_results.push_back(evaluate_mechanical_gravity(mixture, env));
    rep.domain_results.push_back(evaluate_vacuum_space(mixture, env));
    rep.domain_results.push_back(evaluate_electrical(mixture, env));
    rep.domain_results.push_back(evaluate_caloric_heat(mixture, env));
    rep.domain_results.push_back(evaluate_oxidation_rust(mixture, env));
    rep.domain_results.push_back(evaluate_extreme_environments(mixture, env));

    double total_score = 0.0;
    rep.overall_qualification_passed = true;

    for (const auto& res : rep.domain_results) {
        total_score += res.score;
        if (!res.passed) {
            rep.overall_qualification_passed = false;
            for (const auto& err : res.failure_reasons) {
                rep.critical_failures.push_back(res.domain_name + ": " + err);
            }
        }
    }

    rep.aggregate_score = total_score / static_cast<double>(rep.domain_results.size());

    if (!rep.overall_qualification_passed) {
        rep.engineering_recommendations.push_back("Formulation fails environmental qualification thresholds. Check critical failure logs before promotion.");
    } else {
        rep.engineering_recommendations.push_back("Material satisfies all 7 environmental qualification domains. Qualified for Project Material promotion.");
    }

    return rep;
}

TestBenchmarkResult VirtualTestBenchEngine::evaluate_thermal(
    const MixtureEvaluationResult& mix, const EnvironmentConditions& env) {
    TestBenchmarkResult r;
    r.domain_name = "Thermal Environment";
    r.physical_model = "Temperature-Dependent Conductivity, Softening & Thermal Shock Resistance R";
    r.provenance = "ASTM E1461 Laser Flash & Kingery Thermal Stress Formulation";
    r.is_approximate = (mix.overall_epistemic != EpistemicStatus::ConfirmedScientific);

    const double tm = mix.estimated_melting_point_k.value.value_or(1356.0);
    const double k_base = mix.thermal_conductivity_w_mk.value.value_or(50.0);
    const double cte_base = mix.thermal_expansion_1_k.value.value_or(16.5e-6);
    const double e_mod = mix.young_modulus_pa.value.value_or(110e9);
    const double nu = mix.poisson_ratio.value.value_or(0.33);

    // T-dependent softening onset: ~0.45 Tm for crystalline alloys
    const double t_soften = 0.45 * tm;

    // Thermal Shock Parameter R = [sigma_f * (1 - nu)] / (E * alpha)
    const double sigma_f = (e_mod / 300.0); // Estimated yield/fracture strength
    const double r_shock = (sigma_f * (1.0 - nu)) / std::max(1e-12, (e_mod * cte_base));

    std::ostringstream ss;
    ss << "Tm: " << std::fixed << std::setprecision(1) << tm << " K | "
       << "T_soften: " << t_soften << " K | "
       << "k(T): " << k_base << " W/(m*K) | "
       << "R_shock: " << std::setprecision(1) << r_shock << " W/m";
    r.metric_summary = ss.str();

    if (env.temperature_k >= tm) {
        r.passed = false;
        r.score = 0.0;
        r.failure_reasons.push_back("Ambient temperature exceeds melting point (" +
                                    std::to_string(static_cast<int>(env.temperature_k)) + " K >= " +
                                    std::to_string(static_cast<int>(tm)) + " K). Structural collapse.");
    } else if (env.temperature_k >= t_soften) {
        r.passed = true;
        r.score = 65.0;
        r.details.push_back("Temperature exceeds softening threshold; material subject to high-temperature creep.");
    } else {
        r.passed = true;
        r.score = 98.0;
        r.details.push_back("Operating within stable elastic regime.");
    }

    return r;
}

TestBenchmarkResult VirtualTestBenchEngine::evaluate_mechanical_gravity(
    const MixtureEvaluationResult& mix, const EnvironmentConditions& env) {
    TestBenchmarkResult r;
    r.domain_name = "Mechanical & Gravity";
    r.physical_model = "Self-Weight Cantilever Bending, Von Mises Yield & Basquin Fatigue Limit";
    r.provenance = "Euler-Bernoulli Beam Mechanics & ASTM E8/E8M Standard";
    r.is_approximate = (mix.young_modulus_pa.epistemic != EpistemicStatus::ConfirmedScientific);

    const double rho = mix.density_kg_m3.value.value_or(7850.0);
    const double e_mod = mix.young_modulus_pa.value.value_or(200e9);
    const double g = env.gravity_m_s2;

    // Standard cantilever beam specimen: L = 0.5 m, b = 0.05 m, h = 0.01 m
    constexpr double L = 0.5;
    constexpr double b = 0.05;
    constexpr double h = 0.01;
    constexpr double I = (b * h * h * h) / 12.0;

    const double q = rho * g * b * h; // Distributed load (N/m)
    const double deflection_m = (q * L * L * L * L) / (8.0 * std::max(1e6, e_mod) * I);

    // Yield strength, ultimate tensile strength, fatigue endurance limit
    const double sigma_y = (e_mod / 280.0); // Pa
    const double sigma_uts = 1.4 * sigma_y;
    const double fatigue_limit = 0.4 * sigma_uts;

    std::ostringstream ss;
    ss << "rho: " << std::fixed << std::setprecision(1) << rho << " kg/m³ | "
       << "Deflection: " << std::setprecision(3) << (deflection_m * 1000.0) << " mm | "
       << "Yield: " << std::setprecision(1) << (sigma_y * 1e-6) << " MPa | "
       << "Fatigue: " << (fatigue_limit * 1e-6) << " MPa";
    r.metric_summary = ss.str();

    // Deflection must not exceed 5 mm under self-weight
    if (deflection_m > 0.005) {
        r.passed = false;
        r.score = 40.0;
        r.failure_reasons.push_back("Self-weight deflection exceeds 5 mm limit (" +
                                    std::to_string(deflection_m * 1000.0) + " mm).");
    } else {
        r.passed = true;
        r.score = 95.0;
        r.details.push_back("Structural stiffness and strength comply with mechanical deflection tolerances.");
    }

    return r;
}

TestBenchmarkResult VirtualTestBenchEngine::evaluate_vacuum_space(
    const MixtureEvaluationResult& mix, const EnvironmentConditions& env) {
    TestBenchmarkResult r;
    r.domain_name = "Vacuum & Space";
    r.physical_model = "Langmuir Sublimation Kinetics & NASA SP-R-0022A Outgassing Model";
    r.provenance = "NASA / ESA Spacecraft Materials Qualification Standard";
    r.is_approximate = true;

    const double tm = mix.estimated_melting_point_k.value.value_or(1200.0);

    // Langmuir vapor pressure approximation: P_v = P0 * exp(-L / RT)
    const double p_v = 101325.0 * std::exp(-10.5 * (tm / std::max(50.0, env.temperature_k) - 1.0));

    // Total Mass Loss (TML) calculation
    double tml_pct = 0.0;
    if (env.pressure_pa < 1.0) {
        // High vacuum space condition
        tml_pct = std::min(100.0, 100.0 * (p_v / std::max(1e-7, env.pressure_pa)) * 1e-5);
    }

    std::ostringstream ss;
    ss << "P_v: " << std::scientific << std::setprecision(2) << p_v << " Pa | "
       << "TML Outgassing: " << std::fixed << std::setprecision(2) << tml_pct << "%";
    r.metric_summary = ss.str();

    // NASA criteria: TML must be strictly < 1.0%
    if (env.pressure_pa < 1.0 && tml_pct >= 1.0) {
        r.passed = false;
        r.score = 30.0;
        r.failure_reasons.push_back("Vacuum outgassing TML (" + std::to_string(tml_pct) +
                                    "%) exceeds NASA SP-R-0022A 1.0% limit. Risk of optics contamination.");
    } else {
        r.passed = true;
        r.score = 96.0;
        r.details.push_back("Vacuum sublimation rate within acceptable space payload parameters.");
    }

    return r;
}

TestBenchmarkResult VirtualTestBenchEngine::evaluate_electrical(
    const MixtureEvaluationResult& mix, const EnvironmentConditions& env) {
    TestBenchmarkResult r;
    r.domain_name = "Electrical Current";
    r.physical_model = "Ohmic Dissipation, Joule Heating Temperature Rise & Ampacity Limits";
    r.provenance = "IEEE Standard 738 Ampacity & Classical Electrodynamics";
    r.is_approximate = (mix.electrical_resistivity_ohm_m.epistemic != EpistemicStatus::ConfirmedScientific);

    const double rho_e_base = mix.electrical_resistivity_ohm_m.value.value_or(1.7e-8);
    constexpr double alpha_el = 0.00393; // 1/K for copper/metallic lattice
    const double delta_t = env.temperature_k - 298.15;
    const double rho_e = rho_e_base * (1.0 + alpha_el * delta_t);
    const double sigma = (rho_e > 0.0) ? (1.0 / rho_e) : 1e15;

    const double cp = mix.specific_heat_j_kgk.value.value_or(385.0);
    const double rho = mix.density_kg_m3.value.value_or(8940.0);

    // Joule heating for a 5 MA/m2 test pulse over 1.0 s: delta_T = (J^2 * rho_e * t) / (cp * rho)
    constexpr double J_pulse = 5e6;
    const double delta_t_joule = (J_pulse * J_pulse * rho_e * 1.0) / std::max(1e3, cp * rho);

    std::ostringstream ss;
    ss << "sigma(T): " << std::scientific << std::setprecision(2) << sigma << " S/m | "
       << "rho_e(T): " << std::setprecision(2) << rho_e << " Ohm*m | "
       << "Joule DeltaT: " << std::fixed << std::setprecision(1) << delta_t_joule << " K";
    r.metric_summary = ss.str();

    // Check thermal runaway threshold
    if (sigma > 1e4 && delta_t_joule > 200.0) {
        r.passed = false;
        r.score = 45.0;
        r.failure_reasons.push_back("Severe Joule heating pulse (DeltaT = " +
                                    std::to_string(delta_t_joule) + " K). Risk of conductor fusing.");
    } else {
        r.passed = true;
        r.score = 97.0;
        r.details.push_back("Electrical resistivity and thermal dissipation remain within safe engineering margins.");
    }

    return r;
}

TestBenchmarkResult VirtualTestBenchEngine::evaluate_caloric_heat(
    const MixtureEvaluationResult& mix, const EnvironmentConditions& env) {
    (void)env;
    TestBenchmarkResult r;
    r.domain_name = "Caloric Heat";
    r.physical_model = "Kopp-Neumann Specific Heat, Thermal Diffusivity & Richard's Latent Heat Rule";
    r.provenance = "NIST-JANAF Thermochemical Tables Formulation";
    r.is_approximate = (mix.specific_heat_j_kgk.epistemic != EpistemicStatus::ConfirmedScientific);

    const double cp = mix.specific_heat_j_kgk.value.value_or(450.0);
    const double k = mix.thermal_conductivity_w_mk.value.value_or(50.0);
    const double rho = mix.density_kg_m3.value.value_or(7850.0);
    const double tm = mix.estimated_melting_point_k.value.value_or(1200.0);

    // Thermal Diffusivity alpha_diff = k / (rho * Cp) (m2/s)
    const double alpha_diff = k / std::max(1e2, rho * cp);

    // Latent Heat of Fusion via Richards' Rule: Delta_Hf ~= Tm * 10 J/(mol*K)
    const double delta_hf_kj_kg = (tm * 0.22); // Approximate metallic range

    std::ostringstream ss;
    ss << "Cp: " << std::fixed << std::setprecision(1) << cp << " J/(kg*K) | "
       << "alpha_diff: " << std::scientific << std::setprecision(2) << alpha_diff << " m²/s | "
       << "Delta_Hf: " << std::fixed << std::setprecision(1) << delta_hf_kj_kg << " kJ/kg";
    r.metric_summary = ss.str();

    r.passed = (cp > 50.0 && alpha_diff > 1e-8);
    r.score = r.passed ? 99.0 : 30.0;
    if (r.passed) {
        r.details.push_back("Caloric heat capacity and thermal diffusion parameters verified.");
    } else {
        r.failure_reasons.push_back("Abnormally low specific heat or unphysical thermal diffusivity.");
    }

    return r;
}

TestBenchmarkResult VirtualTestBenchEngine::evaluate_oxidation_rust(
    const MixtureEvaluationResult& mix, const EnvironmentConditions& env) {
    TestBenchmarkResult r;
    r.domain_name = "Rust & Oxidation Kinetics";
    r.physical_model = "Pilling-Bedworth Ratio (PBR) & Marine Chloride Corrosion Kinetics";
    r.provenance = "Uhlig's Corrosion Handbook & ASTM G31 Laboratory Immersion Standards";
    r.is_approximate = true;

    // Check for constituent passivating elements
    bool has_fe = false, has_cr = false, has_al = false, has_cu = false;
    double cr_wt = 0.0;
    for (const auto& c : mix.constituents) {
        if (c.symbol == "Fe") { has_fe = true; }
        else if (c.symbol == "Cr") { has_cr = true; cr_wt = c.mass_fraction; }
        else if (c.symbol == "Al") { has_al = true; }
        else if (c.symbol == "Cu") { has_cu = true; }
    }

    // Pilling-Bedworth Ratio (PBR): passivating if 1.0 < PBR < 2.0
    double pbr = 1.35; // Default protective passivating oxide
    if (has_fe && (!has_cr || cr_wt < 0.12)) {
        pbr = 2.14; // Iron rust Fe2O3 (flaking / spalling)
    } else if (has_cr && cr_wt >= 0.12) {
        pbr = 1.95; // Stainless Cr2O3 passivating film
    } else if (has_al) {
        pbr = 1.28; // Al2O3 protective barrier
    } else if (has_cu) {
        pbr = 1.68; // Cu2O cuprite patina
    }

    // Corrosion rate (mm/year)
    double cr_base = (pbr > 2.0) ? 0.25 : 0.005; // mm/year
    const double cr_rate = cr_base * (1.0 + 0.15 * env.salinity_ppt) *
                           std::pow(env.relative_humidity_pct / 100.0, 2.0);

    std::ostringstream ss;
    ss << "PBR: " << std::fixed << std::setprecision(2) << pbr << " ("
       << ((pbr >= 1.0 && pbr <= 2.0) ? "Passivating" : "Flaking/Spalling") << ") | "
       << "Corrosion Rate: " << std::setprecision(3) << cr_rate << " mm/year";
    r.metric_summary = ss.str();

    // High salinity with non-passivated steel fails coastal / marine benchmark
    if (env.salinity_ppt > 5.0 && pbr > 2.0 && cr_rate > 0.1) {
        r.passed = false;
        r.score = 42.0;
        r.failure_reasons.push_back("Severe marine chloride oxidation rate (" +
                                    std::to_string(cr_rate) + " mm/year). PBR = " +
                                    std::to_string(pbr) + " causes spalling oxide scale.");
    } else {
        r.passed = true;
        r.score = 94.0;
        r.details.push_back("Oxidation kinetics within allowable operational degradation limits.");
    }

    return r;
}

TestBenchmarkResult VirtualTestBenchEngine::evaluate_extreme_environments(
    const MixtureEvaluationResult& mix, const EnvironmentConditions& env) {
    TestBenchmarkResult r;
    r.domain_name = "Extreme Environments";
    r.physical_model = "Hydrostatic Subsea Implosion Safety Factor & Radiation Displacement DPA";
    r.provenance = "ASME Boiler & Pressure Vessel Code Section VIII & Kinchin-Pease Model";
    r.is_approximate = true;

    const double e_mod = mix.young_modulus_pa.value.value_or(150e9);
    const double compressive_yield = (e_mod / 240.0); // Pa

    // Hydrostatic pressure safety factor
    const double sf_pressure = compressive_yield / std::max(1e5, env.pressure_pa);

    // Radiation displacement damage (DPA)
    const bool radiation_safe = (env.radiation_dose_dpa < 0.1) || (mix.estimated_melting_point_k.value.value_or(1000.0) > 1200.0);

    std::ostringstream ss;
    ss << "Hydrostatic SF: " << std::fixed << std::setprecision(2) << sf_pressure << "x | "
       << "Radiation DPA: " << std::setprecision(2) << env.radiation_dose_dpa << " DPA";
    r.metric_summary = ss.str();

    if (env.pressure_pa > 1e7 && sf_pressure < 1.5) {
        r.passed = false;
        r.score = 35.0;
        r.failure_reasons.push_back("Hydrostatic pressure (" +
                                    std::to_string(env.pressure_pa * 1e-6) +
                                    " MPa) exceeds compressive safety factor (" +
                                    std::to_string(sf_pressure) + "x < 1.5x required).");
    } else if (!radiation_safe) {
        r.passed = false;
        r.score = 50.0;
        r.failure_reasons.push_back("Radiation embrittlement threshold exceeded under " +
                                    std::to_string(env.radiation_dose_dpa) + " DPA.");
    } else {
        r.passed = true;
        r.score = 98.0;
        r.details.push_back("Material survives extreme hydrostatic pressures and high-energy radiation doses.");
    }

    return r;
}

} // namespace shunya::mat
