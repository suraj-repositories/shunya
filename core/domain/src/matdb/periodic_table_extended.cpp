#include "shunya/matdb/periodic_table_extended.hpp"

#include <sstream>

namespace shunya::matdb {

namespace {

std::optional<double> opt(double v) noexcept {
    if (v <= 0.0) return std::nullopt;
    return v;
}

ExtendedPhysicochemicalRecord make_rec(int z, double pauling, double allen, double mulliken,
                                       double emp, double cov1, double cov2, double cov3, double vdw) {
    return ExtendedPhysicochemicalRecord{
        z,
        ElectronegativityScales{opt(pauling), opt(allen), opt(mulliken)},
        AtomicRadiiPm{opt(emp), opt(cov1), opt(cov2), opt(cov3), opt(vdw)}
    };
}

std::vector<ExtendedPhysicochemicalRecord> build_extended_table() {
    std::vector<ExtendedPhysicochemicalRecord> t;
    t.reserve(118);

    // Z, Pauling, Allen, Mulliken(eV), Empirical, Cov1, Cov2, Cov3, vdW (pm; 0 = N/A)
    // Period 1
    t.push_back(make_rec(1,  2.20, 2.300, 7.17,  25,  32,  0,   0,   120)); // H
    t.push_back(make_rec(2,  0.0,  4.160, 0.0,   31,  46,  0,   0,   140)); // He

    // Period 2
    t.push_back(make_rec(3,  0.98, 0.912, 3.01,  145, 133, 124, 0,   182)); // Li
    t.push_back(make_rec(4,  1.57, 1.576, 4.90,  105, 102, 90,  85,  153)); // Be
    t.push_back(make_rec(5,  2.04, 2.051, 4.29,  85,  85,  78,  73,  192)); // B
    t.push_back(make_rec(6,  2.55, 2.544, 6.27,  70,  75,  67,  60,  170)); // C
    t.push_back(make_rec(7,  3.04, 3.066, 7.30,  65,  71,  60,  54,  155)); // N
    t.push_back(make_rec(8,  3.44, 3.610, 7.54,  60,  63,  57,  53,  152)); // O
    t.push_back(make_rec(9,  3.98, 4.193, 10.41, 50,  64,  59,  53,  147)); // F
    t.push_back(make_rec(10, 0.0,  4.787, 0.0,   38,  67,  96,  0,   154)); // Ne

    // Period 3
    t.push_back(make_rec(11, 0.93, 0.869, 2.85,  180, 155, 160, 0,   227)); // Na
    t.push_back(make_rec(12, 1.31, 1.293, 3.75,  150, 139, 132, 127, 173)); // Mg
    t.push_back(make_rec(13, 1.61, 1.613, 3.23,  125, 126, 113, 111, 184)); // Al
    t.push_back(make_rec(14, 1.90, 1.916, 4.77,  110, 116, 107, 102, 210)); // Si
    t.push_back(make_rec(15, 2.19, 2.253, 5.62,  100, 111, 102, 94,  180)); // P
    t.push_back(make_rec(16, 2.58, 2.589, 6.22,  100, 103, 94,  95,  180)); // S
    t.push_back(make_rec(17, 3.16, 2.869, 8.30,  100, 99,  95,  93,  175)); // Cl
    t.push_back(make_rec(18, 0.0,  3.242, 0.0,   71,  96,  107, 96,  188)); // Ar

    // Period 4
    t.push_back(make_rec(19, 0.82, 0.734, 2.42,  220, 196, 193, 0,   275)); // K
    t.push_back(make_rec(20, 1.00, 1.034, 2.20,  180, 171, 147, 133, 231)); // Ca
    t.push_back(make_rec(21, 1.36, 1.19,  3.34,  160, 148, 116, 114, 211)); // Sc
    t.push_back(make_rec(22, 1.54, 1.38,  3.45,  140, 136, 117, 108, 200)); // Ti
    t.push_back(make_rec(23, 1.63, 1.53,  3.60,  135, 134, 112, 106, 200)); // V
    t.push_back(make_rec(24, 1.66, 1.65,  3.72,  140, 122, 111, 103, 200)); // Cr
    t.push_back(make_rec(25, 1.55, 1.75,  3.72,  140, 119, 105, 103, 200)); // Mn
    t.push_back(make_rec(26, 1.83, 1.80,  4.06,  140, 116, 109, 102, 200)); // Fe
    t.push_back(make_rec(27, 1.88, 1.84,  4.30,  135, 111, 103, 96,  200)); // Co
    t.push_back(make_rec(28, 1.91, 1.88,  4.40,  135, 110, 101, 101, 163)); // Ni
    t.push_back(make_rec(29, 1.90, 1.85,  4.48,  135, 112, 115, 120, 140)); // Cu
    t.push_back(make_rec(30, 1.65, 1.59,  4.45,  135, 118, 120, 0,   139)); // Zn
    t.push_back(make_rec(31, 1.81, 1.756, 3.20,  130, 124, 117, 121, 187)); // Ga
    t.push_back(make_rec(32, 2.01, 1.994, 4.60,  125, 121, 111, 114, 211)); // Ge
    t.push_back(make_rec(33, 2.18, 2.211, 5.30,  115, 121, 114, 106, 185)); // As
    t.push_back(make_rec(34, 2.55, 2.424, 5.89,  115, 116, 107, 107, 190)); // Se
    t.push_back(make_rec(35, 2.96, 2.685, 7.59,  115, 114, 109, 110, 185)); // Br
    t.push_back(make_rec(36, 3.00, 2.966, 0.0,   88,  117, 121, 108, 202)); // Kr

    // Period 5
    t.push_back(make_rec(37, 0.82, 0.706, 2.34,  235, 210, 202, 0,   303)); // Rb
    t.push_back(make_rec(38, 0.95, 0.963, 2.00,  200, 185, 157, 139, 249)); // Sr
    t.push_back(make_rec(39, 1.22, 1.12,  3.19,  180, 163, 130, 124, 232)); // Y
    t.push_back(make_rec(40, 1.33, 1.32,  3.64,  155, 154, 127, 121, 223)); // Zr
    t.push_back(make_rec(41, 1.60, 1.41,  4.00,  145, 147, 125, 116, 218)); // Nb
    t.push_back(make_rec(42, 2.16, 1.47,  3.90,  145, 138, 121, 113, 217)); // Mo
    t.push_back(make_rec(43, 1.90, 1.51,  3.91,  135, 128, 120, 110, 216)); // Tc
    t.push_back(make_rec(44, 2.20, 1.54,  4.50,  130, 125, 114, 103, 213)); // Ru
    t.push_back(make_rec(45, 2.28, 1.56,  4.30,  135, 125, 110, 106, 210)); // Rh
    t.push_back(make_rec(46, 2.20, 1.58,  4.45,  140, 120, 117, 112, 163)); // Pd
    t.push_back(make_rec(47, 1.93, 1.87,  4.44,  160, 128, 139, 137, 172)); // Ag
    t.push_back(make_rec(48, 1.69, 1.52,  4.33,  155, 136, 144, 0,   158)); // Cd
    t.push_back(make_rec(49, 1.78, 1.656, 3.10,  155, 142, 136, 146, 193)); // In
    t.push_back(make_rec(50, 1.96, 1.824, 4.30,  145, 140, 130, 132, 217)); // Sn
    t.push_back(make_rec(51, 2.05, 1.984, 4.85,  145, 140, 133, 127, 206)); // Sb
    t.push_back(make_rec(52, 2.10, 2.158, 5.49,  140, 136, 128, 121, 206)); // Te
    t.push_back(make_rec(53, 2.66, 2.359, 6.76,  140, 133, 129, 125, 198)); // I
    t.push_back(make_rec(54, 2.60, 2.582, 0.0,   108, 131, 135, 122, 216)); // Xe

    // Period 6
    t.push_back(make_rec(55, 0.79, 0.659, 2.18,  260, 232, 209, 0,   343)); // Cs
    t.push_back(make_rec(56, 0.89, 0.881, 2.60,  215, 196, 161, 149, 268)); // Ba
    t.push_back(make_rec(57, 1.10, 1.09,  3.10,  195, 180, 139, 139, 243)); // La
    t.push_back(make_rec(58, 1.12, 1.08,  3.10,  185, 163, 137, 131, 242)); // Ce
    t.push_back(make_rec(59, 1.13, 1.08,  3.10,  185, 176, 138, 128, 240)); // Pr
    t.push_back(make_rec(60, 1.14, 1.09,  3.10,  185, 174, 137, 0,   239)); // Nd
    t.push_back(make_rec(61, 1.13, 1.09,  3.10,  185, 173, 135, 0,   238)); // Pm
    t.push_back(make_rec(62, 1.17, 1.10,  3.20,  185, 172, 134, 0,   236)); // Sm
    t.push_back(make_rec(63, 1.20, 1.10,  3.20,  185, 168, 134, 0,   235)); // Eu
    t.push_back(make_rec(64, 1.20, 1.11,  3.30,  180, 169, 135, 132, 234)); // Gd
    t.push_back(make_rec(65, 1.10, 1.12,  3.20,  175, 168, 135, 0,   233)); // Tb
    t.push_back(make_rec(66, 1.22, 1.13,  3.20,  175, 167, 133, 0,   231)); // Dy
    t.push_back(make_rec(67, 1.23, 1.13,  3.20,  175, 166, 133, 0,   230)); // Ho
    t.push_back(make_rec(68, 1.24, 1.14,  3.30,  175, 165, 133, 0,   229)); // Er
    t.push_back(make_rec(69, 1.25, 1.15,  3.30,  175, 164, 131, 0,   227)); // Tm
    t.push_back(make_rec(70, 1.10, 1.15,  3.10,  175, 170, 129, 0,   226)); // Yb
    t.push_back(make_rec(71, 1.27, 1.09,  3.10,  175, 162, 131, 131, 224)); // Lu
    t.push_back(make_rec(72, 1.30, 1.16,  3.80,  155, 152, 128, 122, 223)); // Hf
    t.push_back(make_rec(73, 1.50, 1.34,  4.11,  145, 146, 126, 119, 222)); // Ta
    t.push_back(make_rec(74, 2.36, 1.47,  4.40,  135, 137, 120, 115, 218)); // W
    t.push_back(make_rec(75, 1.90, 1.60,  4.02,  135, 131, 119, 110, 216)); // Re
    t.push_back(make_rec(76, 2.20, 1.65,  4.90,  130, 129, 116, 109, 216)); // Os
    t.push_back(make_rec(77, 2.20, 1.68,  5.40,  135, 122, 115, 107, 213)); // Ir
    t.push_back(make_rec(78, 2.28, 1.72,  5.60,  135, 123, 112, 110, 175)); // Pt
    t.push_back(make_rec(79, 2.54, 1.92,  5.77,  135, 124, 121, 123, 166)); // Au
    t.push_back(make_rec(80, 2.00, 1.76,  4.91,  150, 133, 142, 0,   155)); // Hg
    t.push_back(make_rec(81, 1.62, 1.789, 3.20,  190, 144, 142, 150, 196)); // Tl
    t.push_back(make_rec(82, 2.33, 1.854, 3.90,  180, 144, 135, 137, 202)); // Pb
    t.push_back(make_rec(83, 2.02, 2.01,  4.69,  160, 151, 141, 135, 207)); // Bi
    t.push_back(make_rec(84, 2.00, 2.19,  4.80,  190, 145, 135, 129, 197)); // Po
    t.push_back(make_rec(85, 2.20, 2.39,  6.00,  150, 147, 138, 138, 202)); // At
    t.push_back(make_rec(86, 2.20, 2.60,  0.0,   120, 142, 145, 133, 220)); // Rn

    // Period 7
    t.push_back(make_rec(87, 0.70, 0.67,  2.10,  270, 240, 218, 0,   348)); // Fr
    t.push_back(make_rec(88, 0.90, 0.89,  2.60,  215, 201, 173, 159, 283)); // Ra
    t.push_back(make_rec(89, 1.10, 1.09,  3.10,  195, 186, 153, 140, 260)); // Ac
    t.push_back(make_rec(90, 1.30, 1.11,  3.20,  180, 175, 143, 136, 245)); // Th
    t.push_back(make_rec(91, 1.50, 1.14,  3.30,  180, 169, 138, 129, 243)); // Pa
    t.push_back(make_rec(92, 1.38, 1.16,  3.40,  175, 170, 134, 118, 240)); // U
    t.push_back(make_rec(93, 1.36, 1.18,  3.40,  175, 171, 136, 116, 239)); // Np
    t.push_back(make_rec(94, 1.28, 1.20,  3.40,  175, 172, 135, 0,   238)); // Pu
    t.push_back(make_rec(95, 1.13, 1.21,  3.40,  175, 166, 135, 0,   237)); // Am
    t.push_back(make_rec(96, 1.28, 1.22,  3.40,  176, 166, 136, 0,   236)); // Cm
    t.push_back(make_rec(97, 1.30, 1.23,  3.40,  176, 168, 139, 0,   235)); // Bk
    t.push_back(make_rec(98, 1.30, 1.24,  3.40,  176, 168, 140, 0,   234)); // Cf
    t.push_back(make_rec(99, 1.30, 1.25,  3.40,  176, 165, 140, 0,   233)); // Es
    t.push_back(make_rec(100,1.30, 1.26,  3.40,  176, 167, 0,   0,   232)); // Fm
    t.push_back(make_rec(101,1.30, 1.27,  3.40,  176, 173, 139, 0,   231)); // Md
    t.push_back(make_rec(102,1.30, 1.28,  3.40,  176, 176, 0,   0,   230)); // No
    t.push_back(make_rec(103,1.30, 1.29,  3.40,  176, 161, 141, 0,   229)); // Lr
    t.push_back(make_rec(104,0.0,  0.0,   0.0,   0,   157, 140, 131, 0));   // Rf
    t.push_back(make_rec(105,0.0,  0.0,   0.0,   0,   149, 136, 126, 0));   // Db
    t.push_back(make_rec(106,0.0,  0.0,   0.0,   0,   143, 128, 121, 0));   // Sg
    t.push_back(make_rec(107,0.0,  0.0,   0.0,   0,   141, 128, 119, 0));   // Bh
    t.push_back(make_rec(108,0.0,  0.0,   0.0,   0,   134, 125, 118, 0));   // Hs
    t.push_back(make_rec(109,0.0,  0.0,   0.0,   0,   129, 125, 113, 0));   // Mt
    t.push_back(make_rec(110,0.0,  0.0,   0.0,   0,   128, 116, 112, 0));   // Ds
    t.push_back(make_rec(111,0.0,  0.0,   0.0,   0,   121, 116, 118, 0));   // Rg
    t.push_back(make_rec(112,0.0,  0.0,   0.0,   0,   122, 137, 130, 0));   // Cn
    t.push_back(make_rec(113,0.0,  0.0,   0.0,   0,   136, 0,   0,   0));   // Nh
    t.push_back(make_rec(114,0.0,  0.0,   0.0,   0,   143, 0,   0,   0));   // Fl
    t.push_back(make_rec(115,0.0,  0.0,   0.0,   0,   162, 0,   0,   0));   // Mc
    t.push_back(make_rec(116,0.0,  0.0,   0.0,   0,   175, 0,   0,   0));   // Lv
    t.push_back(make_rec(117,0.0,  0.0,   0.0,   0,   165, 0,   0,   0));   // Ts
    t.push_back(make_rec(118,0.0,  0.0,   0.0,   0,   157, 0,   0,   230)); // Og

    return t;
}

} // namespace

const std::vector<ExtendedPhysicochemicalRecord>& PeriodicTableExtended::all_records() {
    static const std::vector<ExtendedPhysicochemicalRecord> table = build_extended_table();
    return table;
}

const ExtendedPhysicochemicalRecord* PeriodicTableExtended::find_by_z(int atomic_number) noexcept {
    if (atomic_number < 1 || atomic_number > 118) return nullptr;
    const auto& tbl = all_records();
    const auto& rec = tbl[static_cast<size_t>(atomic_number - 1)];
    return (rec.atomic_number == atomic_number) ? &rec : nullptr;
}

void PeriodicTableExtended::populate_electronegativity(core::domain::PeriodicTableDatabase& db) {
    for (int z = 1; z <= 118; ++z) {
        const auto* existing = db.get_element(z);
        const auto* ext = find_by_z(z);
        if (!existing || !ext) continue;
        core::domain::PeriodicElement updated = *existing;
        updated.electronegativity = ext->electronegativity.pauling;
        (void)db.add_element(std::move(updated));
    }
}

std::string PeriodicTableExtended::to_json() {
    std::ostringstream out;
    out << "[\n";
    const auto& tbl = all_records();
    for (size_t i = 0; i < tbl.size(); ++i) {
        const auto& r = tbl[i];
        out << "  {\"atomic_number\":" << r.atomic_number;
        if (r.electronegativity.pauling) {
            out << ",\"pauling\":" << *r.electronegativity.pauling;
        } else {
            out << ",\"pauling\":null";
        }
        if (r.radii_pm.covalent_single_pm) {
            out << ",\"covalent_single_pm\":" << *r.radii_pm.covalent_single_pm;
        } else {
            out << ",\"covalent_single_pm\":null";
        }
        if (r.radii_pm.vdw_pm) {
            out << ",\"vdw_pm\":" << *r.radii_pm.vdw_pm;
        } else {
            out << ",\"vdw_pm\":null";
        }
        out << "}";
        if (i + 1 < tbl.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

core::base::ValidationReport PeriodicTableExtended::audit_consistency() {
    core::base::ValidationReport report;
    const auto& tbl = all_records();
    if (tbl.size() != 118) {
        report.add_error("extended_records", "Expected 118 extended records, got " + std::to_string(tbl.size()));
        return report;
    }
    for (int z = 1; z <= 118; ++z) {
        std::string path = "Z=" + std::to_string(z);
        const auto* r = find_by_z(z);
        if (!r) {
            report.add_error(path, "Missing extended record");
            continue;
        }
        if (r->electronegativity.pauling && (*r->electronegativity.pauling <= 0.0 || *r->electronegativity.pauling > 4.2)) {
            report.add_error(path, "Out-of-range Pauling electronegativity");
        }
        if (r->radii_pm.covalent_single_pm && *r->radii_pm.covalent_single_pm <= 0.0) {
            report.add_error(path, "Invalid covalent radius");
        }
    }
    return report;
}

} // namespace shunya::matdb
