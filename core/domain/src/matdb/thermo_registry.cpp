#include "shunya/matdb/thermo_registry.hpp"

#include <cmath>
#include <sstream>

namespace shunya::matdb {

namespace {

std::vector<ThermodynamicStateRecord> build_thermo_table() {
    // Reference state entries: Delta_f H deg = 0.0 kJ/mol, Delta_f G deg = 0.0 kJ/mol
    return {
        {"H2(g)",       1,  "gas",                 true,  0.0,     0.0,     130.680, 28.836},
        {"H(g)",        1,  "gas (monatomic)",     false, 217.998, 203.276, 114.717, 20.786},
        {"He(g)",       2,  "gas",                 true,  0.0,     0.0,     126.153, 20.786},
        {"Li(s)",       3,  "solid (BCC)",         true,  0.0,     0.0,      29.120, 24.860},
        {"Be(s)",       4,  "solid (HCP)",         true,  0.0,     0.0,       9.500, 16.440},
        {"B(s)",        5,  "solid (beta-rhomb)",  true,  0.0,     0.0,       5.900, 11.090},
        {"C(graphite)", 6,  "solid (graphite)",    true,  0.0,     0.0,       5.740,  8.517},
        {"C(diamond)",  6,  "solid (diamond)",     false, 1.895,   2.900,   2.377,  6.115},
        {"N2(g)",       7,  "gas",                 true,  0.0,     0.0,     191.609, 29.124},
        {"O2(g)",       8,  "gas",                 true,  0.0,     0.0,     205.152, 29.378},
        {"O3(g)",       8,  "gas (ozone)",         false, 142.670, 163.140, 238.932, 39.238},
        {"F2(g)",       9,  "gas",                 true,  0.0,     0.0,     202.791, 31.304},
        {"Ne(g)",       10, "gas",                 true,  0.0,     0.0,     146.328, 20.786},
        {"Na(s)",       11, "solid (BCC)",         true,  0.0,     0.0,      51.300, 28.230},
        {"Mg(s)",       12, "solid (HCP)",         true,  0.0,     0.0,      32.670, 24.869},
        {"Al(s)",       13, "solid (FCC)",         true,  0.0,     0.0,      28.300, 24.200},
        {"Si(s)",       14, "solid (diamond)",     true,  0.0,     0.0,      18.810, 19.789},
        {"P(white)",    15, "solid (white)",       true,  0.0,     0.0,      41.090, 23.824},
        {"P(red)",      15, "solid (red)",         false, -17.600, -12.460,  22.800, 21.210},
        {"S(rhombic)",  16, "solid (alpha-S8)",    true,  0.0,     0.0,      32.054, 22.750},
        {"S(monoclinic)",16,"solid (beta)",        false, 0.330,   0.096,   32.840, 23.600},
        {"Cl2(g)",      17, "gas",                 true,  0.0,     0.0,     223.081, 33.949},
        {"Ar(g)",       18, "gas",                 true,  0.0,     0.0,     154.846, 20.786},
        {"K(s)",        19, "solid (BCC)",         true,  0.0,     0.0,      64.680, 29.600},
        {"Ca(s)",       20, "solid (FCC)",         true,  0.0,     0.0,      41.590, 25.929},
        {"Ti(s)",       22, "solid (HCP)",         true,  0.0,     0.0,      30.720, 25.060},
        {"Cr(s)",       24, "solid (BCC)",         true,  0.0,     0.0,      23.770, 23.350},
        {"Fe(s)",       26, "solid (alpha-BCC)",   true,  0.0,     0.0,      27.285, 25.098},
        {"Co(s)",       27, "solid (HCP)",         true,  0.0,     0.0,      30.040, 24.810},
        {"Ni(s)",       28, "solid (FCC)",         true,  0.0,     0.0,      29.870, 26.070},
        {"Cu(s)",       29, "solid (FCC)",         true,  0.0,     0.0,      33.150, 24.440},
        {"Zn(s)",       30, "solid (HCP)",         true,  0.0,     0.0,      41.630, 25.390},
        {"Br2(l)",      35, "liquid",              true,  0.0,     0.0,     152.210, 75.690},
        {"Ag(s)",       47, "solid (FCC)",         true,  0.0,     0.0,      42.550, 25.350},
        {"Sn(white)",   50, "solid (beta-BCT)",    true,  0.0,     0.0,      51.180, 27.112},
        {"Sn(gray)",    50, "solid (alpha-diam)",  false, -2.090,  0.130,   44.140, 25.770},
        {"I2(s)",       53, "solid",               true,  0.0,     0.0,     116.140, 54.440},
        {"W(s)",        74, "solid (BCC)",         true,  0.0,     0.0,      32.640, 24.270},
        {"Pt(s)",       78, "solid (FCC)",         true,  0.0,     0.0,      41.630, 25.860},
        {"Au(s)",       79, "solid (FCC)",         true,  0.0,     0.0,      47.400, 25.418},
        {"Hg(l)",       80, "liquid",              true,  0.0,     0.0,      75.900, 27.983},
        {"Pb(s)",       82, "solid (FCC)",         true,  0.0,     0.0,      64.800, 26.650}
    };
}

} // namespace

const std::vector<ThermodynamicStateRecord>& ThermodynamicStateRegistry::all_records() {
    static const std::vector<ThermodynamicStateRecord> table = build_thermo_table();
    return table;
}

const ThermodynamicStateRecord* ThermodynamicStateRegistry::find_by_id(std::string_view id) noexcept {
    for (const auto& r : all_records()) {
        if (r.id == id) return &r;
    }
    return nullptr;
}

const ThermodynamicStateRecord* ThermodynamicStateRegistry::reference_state_for_element(
    int atomic_number) noexcept {
    for (const auto& r : all_records()) {
        if (r.atomic_number == atomic_number && r.is_reference_state) {
            return &r;
        }
    }
    return nullptr;
}

std::vector<ThermodynamicStateRecord> ThermodynamicStateRegistry::records_for_element(
    int atomic_number) {
    std::vector<ThermodynamicStateRecord> out;
    for (const auto& r : all_records()) {
        if (r.atomic_number == atomic_number) out.push_back(r);
    }
    return out;
}

std::string ThermodynamicStateRegistry::to_json() {
    std::ostringstream out;
    out << "[\n";
    const auto& tbl = all_records();
    for (size_t i = 0; i < tbl.size(); ++i) {
        const auto& r = tbl[i];
        out << "  {\"id\":\"" << r.id << "\""
            << ",\"atomic_number\":" << r.atomic_number
            << ",\"is_reference_state\":" << (r.is_reference_state ? "true" : "false")
            << ",\"delta_f_h_298_kj_mol\":" << r.delta_f_h_298_kj_mol
            << ",\"s0_298_j_mol_k\":" << r.s0_298_j_mol_k
            << ",\"delta_f_g_298_kj_mol\":" << r.delta_f_g_298_kj_mol
            << "}";
        if (i + 1 < tbl.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

core::base::ValidationReport ThermodynamicStateRegistry::audit_reference_states() {
    core::base::ValidationReport report;
    for (const auto& r : all_records()) {
        if (r.is_reference_state) {
            if (std::abs(r.delta_f_h_298_kj_mol) > 1e-9 || std::abs(r.delta_f_g_298_kj_mol) > 1e-9) {
                report.add_error(r.id, "Reference element must have Delta_f H deg = 0 and Delta_f G deg = 0");
            }
        }
        if (r.s0_298_j_mol_k < 0.0 || r.cp_298_j_mol_k <= 0.0) {
            report.add_error(r.id, "Invalid entropy or heat capacity at 298.15 K");
        }
    }
    return report;
}

} // namespace shunya::matdb
