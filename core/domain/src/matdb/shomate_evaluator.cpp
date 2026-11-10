#include "shunya/matdb/shomate_evaluator.hpp"

#include "shunya/matdb/elements_118_catalog.hpp"

#include <cmath>
#include <sstream>

namespace shunya::matdb {

namespace {

core::base::Error make_sho_error(std::string msg) {
    return core::base::Error{
        .code = 4120,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

const ShomateSegment* select_segment(const ShomateSpecies& sp, double temp_k) noexcept {
    for (const auto& seg : sp.segments) {
        if (temp_k >= seg.t_min_k - 1e-6 && temp_k <= seg.t_max_k + 1e-6) {
            return &seg;
        }
    }
    return nullptr;
}

std::vector<ShomateSpecies> build_shomate_table() {
    std::vector<ShomateSpecies> out;

    // N2(g): NIST WebBook 100 - 6000 K
    out.push_back({"N2(g)", 7, {
        {100.0,  500.0,  "gas", 28.98641,  1.853978, -9.647459, 16.63537,   0.000117, -8.671914, 226.4168, 0.0},
        {500.0,  2000.0, "gas", 19.50583, 19.88705,  -8.598535,  1.369784,  0.527601, -4.935202, 212.3900, 0.0},
        {2000.0, 6000.0, "gas", 35.51872,  1.128728, -0.196103,  0.014662, -4.553760,-18.97091,  224.9810, 0.0}
    }});

    // O2(g): NIST WebBook 100 - 6000 K
    out.push_back({"O2(g)", 8, {
        {100.0,  700.0,  "gas", 31.32234,-20.23531,  57.86644, -36.50624, -0.007374, -8.903471, 246.7945, 0.0},
        {700.0,  2000.0, "gas", 30.03235,  8.772972, -3.988133,  0.788313,-0.741599,-11.32468,  236.1663, 0.0},
        {2000.0, 6000.0, "gas", 20.91111, 10.72071,  -2.020498,  0.146449, 9.245722,  5.337651, 237.6185, 0.0}
    }});

    // H2(g): NIST WebBook 298 - 6000 K
    out.push_back({"H2(g)", 1, {
        {298.0,  1000.0, "gas", 33.066178,-11.363417, 11.432816,-2.772874, -0.158558,-9.980797, 172.707974, 0.0},
        {1000.0, 2500.0, "gas", 18.563083, 12.257357, -2.859786, 0.268238,  1.977990,-1.147438, 156.288133, 0.0},
        {2500.0, 6000.0, "gas", 43.413560, -4.293079,  1.272428,-0.096876,-20.533862,-38.515158,162.081354, 0.0}
    }});

    // Fe (condensed & liquid): NIST WebBook
    out.push_back({"Fe(s/l)", 26, {
        {298.0,  700.0,  "solid-alpha", 18.42868,  24.64301, -8.913720,  9.664706,-0.012643,-6.573022,  42.51488, 0.0},
        {700.0,  1042.0, "solid-alpha",-57767.65, 137919.7, -122773.2, 38682.42, 3993.080, 24078.67, -87364.01,  0.0},
        {1809.0, 3134.0, "liquid",      46.024,    0.0,       0.0,       0.0,      0.0,     -11.676,    77.590,   -7.274}
    }});

    // Cu (condensed & liquid): NIST WebBook
    out.push_back({"Cu(s/l)", 29, {
        {298.0,  1358.0, "solid",  17.72891, 28.09870, -31.25289, 13.97243,  0.068611, -6.056591, 47.89592, 0.0},
        {1358.0, 2835.0, "liquid", 32.84450, -0.000084,  0.000032,-0.000004,-0.000028,-2.212767, 69.23726, 0.0}
    }});

    // Al (condensed & liquid): NIST WebBook
    out.push_back({"Al(s/l)", 13, {
        {298.0,  933.47, "solid",  28.08920, -5.414910, 8.560423, 3.427370, -0.277375, -8.149280, 61.67460, 0.0},
        {933.47, 2790.0, "liquid", 31.75104,  0.000039,-0.000018, 0.000003, -0.000003,  1.236765, 76.78480, 0.0}
    }});

    return out;
}

} // namespace

double ShomateSegment::eval_cp_j_mol_k(double temp_k) const noexcept {
    const double t = temp_k / 1000.0;
    return A + B * t + C * t * t + D * t * t * t + E / (t * t);
}

double ShomateSegment::eval_delta_h_kj_mol(double temp_k) const noexcept {
    const double t = temp_k / 1000.0;
    const double t2 = t * t;
    return A * t + 0.5 * B * t2 + (1.0 / 3.0) * C * t2 * t + 0.25 * D * t2 * t2 - E / t + F - H;
}

double ShomateSegment::eval_s_j_mol_k(double temp_k) const noexcept {
    const double t = temp_k / 1000.0;
    const double t2 = t * t;
    return A * std::log(t) + B * t + 0.5 * C * t2 + (1.0 / 3.0) * D * t2 * t - E / (2.0 * t2) + G;
}

const std::vector<ShomateSpecies>& ShomateEvaluator::all_species() {
    static const std::vector<ShomateSpecies> tbl = build_shomate_table();
    return tbl;
}

const ShomateSpecies* ShomateEvaluator::find_by_id(std::string_view id) noexcept {
    for (const auto& s : all_species()) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

core::base::Result<double> ShomateEvaluator::evaluate_cp_j_mol_k(std::string_view species_id, double temp_k) {
    if (temp_k <= 0.0) return make_sho_error("Temperature must be positive (Kelvin)");
    const auto* sp = find_by_id(species_id);
    if (!sp) return make_sho_error("Unknown Shomate species: " + std::string(species_id));
    const auto* seg = select_segment(*sp, temp_k);
    if (!seg) return make_sho_error("Temperature out of tabulated Shomate bounds");
    return seg->eval_cp_j_mol_k(temp_k);
}

core::base::Result<double> ShomateEvaluator::evaluate_specific_heat_j_kg_k(std::string_view species_id,
                                                                            double temp_k) {
    auto cp_res = evaluate_cp_j_mol_k(species_id, temp_k);
    if (!cp_res) return cp_res.error();
    const auto* sp = find_by_id(species_id);
    const auto* elem = Elements118Catalog::find_by_z(sp->atomic_number);
    if (!elem || elem->atomic_weight_amu <= 0.0) {
        return make_sho_error("Missing atomic mass for Shomate specific heat conversion");
    }
    // Diatomic gases store per-mol-of-O2/N2/H2 (2 atoms/molecule) when molecular
    const double factor = (species_id == "N2(g)" || species_id == "O2(g)" || species_id == "H2(g)") ? 2.0 : 1.0;
    const double molar_mass_kg = (elem->atomic_weight_amu * factor) * 1e-3;
    return cp_res.value() / molar_mass_kg;
}

core::base::Result<double> ShomateEvaluator::evaluate_delta_h_kj_mol(std::string_view species_id, double temp_k) {
    if (temp_k <= 0.0) return make_sho_error("Temperature must be positive (Kelvin)");
    const auto* sp = find_by_id(species_id);
    if (!sp) return make_sho_error("Unknown Shomate species: " + std::string(species_id));
    const auto* seg = select_segment(*sp, temp_k);
    if (!seg) return make_sho_error("Temperature out of tabulated Shomate bounds");
    return seg->eval_delta_h_kj_mol(temp_k);
}

core::base::Result<double> ShomateEvaluator::evaluate_entropy_j_mol_k(std::string_view species_id, double temp_k) {
    if (temp_k <= 0.0) return make_sho_error("Temperature must be positive (Kelvin)");
    const auto* sp = find_by_id(species_id);
    if (!sp) return make_sho_error("Unknown Shomate species: " + std::string(species_id));
    const auto* seg = select_segment(*sp, temp_k);
    if (!seg) return make_sho_error("Temperature out of tabulated Shomate bounds");
    return seg->eval_s_j_mol_k(temp_k);
}

std::string ShomateEvaluator::to_json() {
    std::ostringstream out;
    out << "[\n";
    const auto& tbl = all_species();
    for (size_t i = 0; i < tbl.size(); ++i) {
        const auto& s = tbl[i];
        out << "  {\"id\":\"" << s.id << "\",\"atomic_number\":" << s.atomic_number
            << ",\"segment_count\":" << s.segments.size() << "}";
        if (i + 1 < tbl.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

core::base::ValidationReport ShomateEvaluator::audit_segment_continuity(double max_rel_jump_pct) {
    core::base::ValidationReport report;
    for (const auto& sp : all_species()) {
        for (size_t i = 0; i + 1 < sp.segments.size(); ++i) {
            const auto& left = sp.segments[i];
            const auto& right = sp.segments[i + 1];
            if (std::abs(left.t_max_k - right.t_min_k) > 1e-6 || left.phase != right.phase) {
                continue; // Phase transition boundary or disjoint interval
            }
            const double cp_l = left.eval_cp_j_mol_k(left.t_max_k);
            const double cp_r = right.eval_cp_j_mol_k(right.t_min_k);
            const double rel_pct = 100.0 * std::abs(cp_l - cp_r) / std::max(1.0, std::abs(cp_l));
            if (rel_pct > max_rel_jump_pct) {
                report.add_error(sp.id, "Shomate Cp discontinuity of " + std::to_string(rel_pct) +
                                        "% at T=" + std::to_string(left.t_max_k) + " K");
            }
        }
    }
    return report;
}

} // namespace shunya::matdb
