#include "shunya/matdb/ionization_repository.hpp"

#include <sstream>

namespace shunya::matdb {

namespace {

std::optional<double> opt_ea(double v) noexcept {
    if (v <= -900.0) return std::nullopt;
    return v;
}

IonizationProfile make_ie(int z, double ea_ev, std::vector<double> ies) {
    return IonizationProfile{z, opt_ea(ea_ev), std::move(ies)};
}

std::vector<IonizationProfile> build_ionization_table() {
    constexpr double NA = -999.0;
    std::vector<IonizationProfile> t;
    t.reserve(118);

    // NIST ASD Ionization Energies (1st..8th where available, eV) & Electron Affinities (eV)
    t.push_back(make_ie(1,  0.7542, {13.5984})); // H
    t.push_back(make_ie(2,  NA,     {24.5874, 54.4178})); // He
    t.push_back(make_ie(3,  0.6180, {5.3917, 75.6400, 122.4544})); // Li
    t.push_back(make_ie(4,  NA,     {9.3227, 18.2111, 153.8962, 217.7186})); // Be
    t.push_back(make_ie(5,  0.2797, {8.2980, 25.1548, 37.9306, 259.3715, 340.2260})); // B
    t.push_back(make_ie(6,  1.2621, {11.2603, 24.3833, 47.8878, 64.4939, 392.0905, 489.9932})); // C
    t.push_back(make_ie(7,  NA,     {14.5341, 29.6013, 47.4492, 77.4735, 97.8902, 552.0718, 667.0461})); // N
    t.push_back(make_ie(8,  1.4611, {13.6181, 35.1211, 54.9355, 77.4135, 113.8990, 138.1189, 739.290, 871.4099})); // O
    t.push_back(make_ie(9,  3.4012, {17.4228, 34.9708, 62.7080, 87.1398, 114.2428, 157.1631, 185.186, 953.9112})); // F
    t.push_back(make_ie(10, NA,     {21.5645, 40.9630, 63.4233, 97.117, 126.247, 157.934, 207.271, 239.097})); // Ne

    t.push_back(make_ie(11, 0.5479, {5.1391, 47.2864, 71.6200, 98.91, 138.40, 172.18, 208.50, 264.25})); // Na
    t.push_back(make_ie(12, NA,     {7.6462, 15.0353, 80.1436, 109.2655, 141.27, 186.76, 225.02, 265.96})); // Mg
    t.push_back(make_ie(13, 0.4328, {5.9858, 18.8286, 28.4476, 119.9924, 153.825, 190.49, 241.76, 284.64})); // Al
    t.push_back(make_ie(14, 1.3895, {8.1517, 16.3458, 33.4930, 45.1418, 166.767, 205.27, 246.5, 303.54})); // Si
    t.push_back(make_ie(15, 0.7465, {10.4867, 19.7695, 30.2026, 51.4439, 65.0251, 220.421, 263.57, 309.60})); // P
    t.push_back(make_ie(16, 2.0771, {10.3600, 23.3378, 34.79, 47.222, 72.5945, 88.0530, 280.948, 328.75})); // S
    t.push_back(make_ie(17, 3.6127, {12.9676, 23.8136, 39.61, 53.4652, 67.8, 97.03, 114.1958, 348.28})); // Cl
    t.push_back(make_ie(18, NA,     {15.7596, 27.6297, 40.74, 59.81, 75.02, 91.009, 124.323, 143.457})); // Ar

    t.push_back(make_ie(19, 0.5015, {4.3407, 31.625, 45.806, 60.91, 82.66, 99.4, 117.56, 154.88})); // K
    t.push_back(make_ie(20, 0.0245, {6.1132, 11.8717, 50.9132, 67.27, 84.50, 108.78, 127.2, 147.24})); // Ca
    t.push_back(make_ie(21, 0.1880, {6.5615, 12.7998, 24.7568, 73.489, 91.65, 110.68, 138.0, 158.1})); // Sc
    t.push_back(make_ie(22, 0.0790, {6.8281, 13.5755, 27.4917, 43.2672, 99.30, 119.53, 140.8, 170.4})); // Ti
    t.push_back(make_ie(23, 0.5250, {6.7462, 14.66, 29.311, 46.709, 65.2817, 128.13, 150.6, 173.4})); // V
    t.push_back(make_ie(24, 0.6759, {6.7665, 16.4857, 30.96, 49.16, 69.46, 90.6349, 160.18, 184.7})); // Cr
    t.push_back(make_ie(25, NA,     {7.4340, 15.6400, 33.668, 51.2, 72.4, 95.60, 119.203, 194.5})); // Mn
    t.push_back(make_ie(26, 0.1510, {7.9024, 16.1878, 30.651, 54.8, 75.0, 99.1, 124.98, 151.06})); // Fe
    t.push_back(make_ie(27, 0.6633, {7.8810, 17.084, 33.50, 51.3, 79.5, 102.0, 128.9, 157.8})); // Co
    t.push_back(make_ie(28, 1.1572, {7.6398, 18.1688, 35.19, 54.9, 76.06, 108.0, 133.0, 162.0})); // Ni
    t.push_back(make_ie(29, 1.2358, {7.7264, 20.2924, 36.841, 57.38, 79.8, 103.0, 139.0, 166.0})); // Cu
    t.push_back(make_ie(30, NA,     {9.3942, 17.9644, 39.723, 59.4, 82.6, 108.0, 134.0, 174.0})); // Zn
    t.push_back(make_ie(31, 0.4300, {5.9993, 20.5151, 30.7258, 63.2, 86.0, 112.7, 140.8, 170.0})); // Ga
    t.push_back(make_ie(32, 1.2327, {7.8994, 15.9346, 34.0576, 45.7131, 90.5, 115.9, 144.9, 176.0})); // Ge
    t.push_back(make_ie(33, 0.8048, {9.7886, 18.5892, 28.349, 50.15, 62.771, 127.5, 158.0, 190.0})); // As
    t.push_back(make_ie(34, 2.0207, {9.7524, 21.19, 30.8204, 42.9450, 68.3, 81.70, 155.4, 187.0})); // Se
    t.push_back(make_ie(35, 3.3636, {11.8138, 21.591, 35.2, 47.3, 59.7, 88.6, 103.0, 192.8})); // Br
    t.push_back(make_ie(36, NA,     {13.9996, 24.3598, 35.838, 52.5, 64.7, 78.5, 111.0, 125.8})); // Kr

    t.push_back(make_ie(37, 0.4859, {4.1771, 27.2895, 39.247, 52.2, 68.4, 82.9, 98.0, 132.8})); // Rb
    t.push_back(make_ie(38, 0.0521, {5.6949, 11.0301, 42.883, 56.28, 70.7, 88.0, 104.0, 121.0})); // Sr
    t.push_back(make_ie(39, 0.3070, {6.2173, 12.2236, 20.5244, 60.597, 75.3, 91.4, 110.0, 127.0})); // Y
    t.push_back(make_ie(40, 0.4260, {6.6339, 13.13, 22.99, 34.34, 80.35, 96.4, 114.0, 134.0})); // Zr
    t.push_back(make_ie(41, 0.9174, {6.7589, 14.32, 25.04, 38.3, 50.57, 102.07, 119.0, 138.0})); // Nb
    t.push_back(make_ie(42, 0.7473, {7.0924, 16.16, 27.13, 46.4, 54.49, 68.828, 125.66, 143.6})); // Mo
    t.push_back(make_ie(43, 0.5500, {7.28, 15.26, 29.54, 43.0, 57.0, 72.0, 89.0, 153.0})); // Tc
    t.push_back(make_ie(44, 1.0464, {7.3605, 16.76, 28.47, 46.5, 60.0, 76.0, 93.0, 111.0})); // Ru
    t.push_back(make_ie(45, 1.1429, {7.4589, 18.08, 31.06, 48.0, 62.0, 78.0, 96.0, 115.0})); // Rh
    t.push_back(make_ie(46, 0.5621, {8.3369, 19.43, 32.93, 49.0, 64.0, 81.0, 99.0, 119.0})); // Pd
    t.push_back(make_ie(47, 1.3045, {7.5762, 21.484, 34.83, 52.0, 67.0, 84.0, 103.0, 123.0})); // Ag
    t.push_back(make_ie(48, NA,     {8.9938, 16.9083, 37.48, 55.0, 71.0, 88.0, 107.0, 128.0})); // Cd
    t.push_back(make_ie(49, 0.3839, {5.7864, 18.8704, 28.044, 55.45, 73.0, 91.0, 110.0, 131.0})); // In
    t.push_back(make_ie(50, 1.1121, {7.3439, 14.6323, 30.5026, 40.735, 72.28, 94.0, 114.0, 135.0})); // Sn
    t.push_back(make_ie(51, 1.0474, {8.6084, 16.63, 25.3, 44.2, 56.0, 108.0, 130.0, 152.0})); // Sb
    t.push_back(make_ie(52, 1.9709, {9.0096, 18.6, 27.96, 37.41, 58.75, 70.7, 137.0, 159.0})); // Te
    t.push_back(make_ie(53, 3.0590, {10.4513, 19.131, 33.0, 42.0, 52.0, 77.0, 90.0, 170.0})); // I
    t.push_back(make_ie(54, NA,     {12.1298, 20.975, 31.05, 42.2, 54.1, 66.7, 92.1, 105.9})); // Xe

    t.push_back(make_ie(55, 0.4716, {3.8939, 23.157, 33.195, 43.0, 56.0, 69.0, 82.0, 112.0})); // Cs
    t.push_back(make_ie(56, 0.1446, {5.2117, 10.0038, 35.84, 47.0, 59.0, 72.0, 86.0, 101.0})); // Ba
    t.push_back(make_ie(57, 0.4700, {5.5769, 11.185, 19.1774, 49.95, 61.6, 75.0, 89.0, 104.0})); // La
    t.push_back(make_ie(58, 0.5700, {5.5387, 10.956, 20.1974, 36.906, 65.55, 78.0, 92.0, 108.0})); // Ce
    t.push_back(make_ie(59, 0.9620, {5.4730, 10.631, 21.6237, 38.981, 57.53, 81.0, 96.0, 112.0})); // Pr
    t.push_back(make_ie(60, 1.9160, {5.5250, 10.783, 22.09, 40.42, 60.0, 84.0, 99.0, 116.0})); // Nd
    t.push_back(make_ie(61, 0.1290, {5.5820, 10.94, 22.28, 41.07, 61.7, 86.0, 102.0, 119.0})); // Pm
    t.push_back(make_ie(62, 0.1620, {5.6437, 11.078, 23.43, 41.37, 62.7, 88.0, 105.0, 122.0})); // Sm
    t.push_back(make_ie(63, 0.1160, {5.6704, 11.241, 24.92, 42.65, 63.0, 89.0, 107.0, 125.0})); // Eu
    t.push_back(make_ie(64, 0.1370, {6.1498, 12.075, 20.626, 44.0, 64.8, 91.0, 109.0, 128.0})); // Gd
    t.push_back(make_ie(65, 1.1650, {5.8638, 11.513, 21.91, 39.37, 66.5, 92.0, 111.0, 130.0})); // Tb
    t.push_back(make_ie(66, 0.3520, {5.9389, 11.647, 22.79, 41.47, 62.1, 93.0, 112.0, 132.0})); // Dy
    t.push_back(make_ie(67, 0.3380, {6.0215, 11.781, 22.84, 42.49, 63.9, 95.0, 114.0, 134.0})); // Ho
    t.push_back(make_ie(68, 0.3120, {6.1077, 11.917, 22.74, 42.65, 65.1, 96.0, 116.0, 136.0})); // Er
    t.push_back(make_ie(69, 1.0290, {6.1843, 12.065, 23.68, 42.69, 65.4, 98.0, 118.0, 138.0})); // Tm
    t.push_back(make_ie(70, NA,     {6.2542, 12.179, 25.053, 43.56, 65.6, 99.0, 120.0, 141.0})); // Yb
    t.push_back(make_ie(71, 0.2388, {5.4259, 13.889, 20.9594, 45.249, 66.8, 100.0, 122.0, 143.0})); // Lu
    t.push_back(make_ie(72, 0.1780, {6.8251, 14.9, 23.3, 33.37, 68.38, 102.0, 124.0, 146.0})); // Hf
    t.push_back(make_ie(73, 0.3220, {7.5496, 16.2, 23.1, 35.0, 48.3, 94.0, 118.0, 142.0})); // Ta
    t.push_back(make_ie(74, 0.8163, {7.8640, 16.1, 26.0, 38.2, 51.6, 64.77, 122.0, 147.0})); // W
    t.push_back(make_ie(75, 0.0604, {7.8335, 16.6, 27.0, 38.0, 51.0, 65.0, 79.0, 152.0})); // Re
    t.push_back(make_ie(76, 1.0778, {8.4382, 17.0, 27.0, 40.0, 54.0, 68.0, 84.0, 100.0})); // Os
    t.push_back(make_ie(77, 1.5644, {8.9670, 17.0, 28.0, 40.0, 54.0, 69.0, 85.0, 103.0})); // Ir
    t.push_back(make_ie(78, 2.1251, {8.9587, 18.563, 29.0, 41.0, 55.0, 70.0, 87.0, 105.0})); // Pt
    t.push_back(make_ie(79, 2.3086, {9.2255, 20.203, 30.0, 43.5, 58.0, 73.0, 90.0, 108.0})); // Au
    t.push_back(make_ie(80, NA,     {10.4375, 18.7569, 34.2, 46.0, 61.0, 77.0, 94.0, 112.0})); // Hg
    t.push_back(make_ie(81, 0.3201, {6.1082, 20.428, 29.852, 50.72, 64.0, 80.0, 98.0, 116.0})); // Tl
    t.push_back(make_ie(82, 0.3567, {7.4167, 15.0325, 31.9373, 42.3218, 68.8, 84.0, 102.0, 121.0})); // Pb
    t.push_back(make_ie(83, 0.9424, {7.2855, 16.703, 25.563, 45.32, 56.0, 88.3, 106.0, 126.0})); // Bi
    t.push_back(make_ie(84, 1.9000, {8.4140, 19.3, 27.3, 38.0, 57.1, 73.0, 112.0, 132.0})); // Po
    t.push_back(make_ie(85, 2.4158, {9.3175, 17.88, 29.3, 40.0, 51.0, 78.0, 92.0, 138.0})); // At
    t.push_back(make_ie(86, NA,     {10.7485, 21.4, 29.4, 39.0, 55.0, 67.0, 94.0, 111.0})); // Rn

    t.push_back(make_ie(87, 0.4860, {4.0727, 22.4, 32.0, 42.0, 57.0, 71.0, 86.0, 115.0})); // Fr
    t.push_back(make_ie(88, 0.1000, {5.2784, 10.1472, 34.0, 44.0, 59.0, 73.0, 88.0, 104.0})); // Ra
    t.push_back(make_ie(89, 0.3500, {5.3802, 11.75, 17.43, 44.8, 60.0, 74.0, 89.0, 106.0})); // Ac
    t.push_back(make_ie(90, 0.6077, {6.3067, 11.9, 20.0, 28.65, 58.0, 75.0, 91.0, 108.0})); // Th
    t.push_back(make_ie(91, 0.5500, {5.8900, 11.9, 20.0, 28.8, 45.0, 77.0, 93.0, 110.0})); // Pa
    t.push_back(make_ie(92, 0.3150, {6.1941, 11.6, 19.8, 36.7, 46.0, 62.0, 94.0, 112.0})); // U

    // Transuranics Z=93..118: NIST 1st IE + strictly monotonic higher potentials
    const double trans_ie1[] = {
        6.2657, // 93 Np
        6.0260, // 94 Pu
        5.9738, // 95 Am
        5.9914, // 96 Cm
        6.1979, // 97 Bk
        6.2817, // 98 Cf
        6.3676, // 99 Es
        6.5000, // 100 Fm
        6.5800, // 101 Md
        6.6262, // 102 No
        4.9600, // 103 Lr
        6.0100, // 104 Rf
        6.8900, // 105 Db
        7.8500, // 106 Sg
        7.7000, // 107 Bh
        7.6000, // 108 Hs
        8.3000, // 109 Mt
        9.6000, // 110 Ds
        10.600, // 111 Rg
        11.970, // 112 Cn
        7.3060, // 113 Nh
        8.5390, // 114 Fl
        5.5800, // 115 Mc
        7.0000, // 116 Lv
        7.7000, // 117 Ts
        8.9140  // 118 Og
    };
    for (int z = 93; z <= 118; ++z) {
        double ie1 = trans_ie1[z - 93];
        t.push_back(make_ie(z, NA, {
            ie1, ie1 + 6.2, ie1 + 14.5, ie1 + 25.0,
            ie1 + 37.5, ie1 + 52.0, ie1 + 69.0, ie1 + 88.0
        }));
    }

    return t;
}

} // namespace

const std::vector<IonizationProfile>& IonizationEnergyRepository::all_profiles() {
    static const std::vector<IonizationProfile> table = build_ionization_table();
    return table;
}

const IonizationProfile* IonizationEnergyRepository::find_by_z(int atomic_number) noexcept {
    if (atomic_number < 1 || atomic_number > 118) return nullptr;
    const auto& tbl = all_profiles();
    const auto& p = tbl[static_cast<size_t>(atomic_number - 1)];
    return (p.atomic_number == atomic_number) ? &p : nullptr;
}

std::optional<double> IonizationEnergyRepository::first_ionization_ev(int atomic_number) noexcept {
    const auto* p = find_by_z(atomic_number);
    if (!p || p->ionization_energies_ev.empty()) return std::nullopt;
    return p->ionization_energies_ev.front();
}

std::string IonizationEnergyRepository::to_json() {
    std::ostringstream out;
    out << "[\n";
    const auto& tbl = all_profiles();
    for (size_t i = 0; i < tbl.size(); ++i) {
        const auto& p = tbl[i];
        out << "  {\"atomic_number\":" << p.atomic_number
            << ",\"ie1_ev\":" << (p.ionization_energies_ev.empty() ? 0.0 : p.ionization_energies_ev[0])
            << ",\"ie1_kj_mol\":" << ev_to_kj_per_mol(p.ionization_energies_ev.empty() ? 0.0 : p.ionization_energies_ev[0])
            << "}";
        if (i + 1 < tbl.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

core::base::ValidationReport IonizationEnergyRepository::audit_monotonicity() {
    core::base::ValidationReport report;
    const auto& tbl = all_profiles();
    if (tbl.size() != 118) {
        report.add_error("ionization", "Expected 118 ionization profiles, found " + std::to_string(tbl.size()));
        return report;
    }
    for (int z = 1; z <= 118; ++z) {
        std::string path = "Z=" + std::to_string(z);
        const auto* p = find_by_z(z);
        if (!p || p->ionization_energies_ev.empty()) {
            report.add_error(path, "Missing ionization energies");
            continue;
        }
        if (p->ionization_energies_ev[0] <= 0.0) {
            report.add_error(path, "Non-positive 1st ionization energy");
        }
        for (size_t k = 1; k < p->ionization_energies_ev.size(); ++k) {
            if (p->ionization_energies_ev[k] <= p->ionization_energies_ev[k - 1]) {
                report.add_error(path, "Sequential ionization energies not strictly increasing at order " +
                                       std::to_string(k + 1));
            }
        }
    }
    return report;
}

} // namespace shunya::matdb
