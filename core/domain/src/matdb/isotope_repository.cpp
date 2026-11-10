#include "shunya/matdb/isotope_repository.hpp"

#include <cmath>
#include <map>
#include <optional>
#include <sstream>

namespace shunya::matdb {

double half_life_to_seconds(double value, TimeUnit unit) noexcept {
    switch (unit) {
    case TimeUnit::Nanoseconds:  return value * 1e-9;
    case TimeUnit::Microseconds: return value * 1e-6;
    case TimeUnit::Milliseconds: return value * 1e-3;
    case TimeUnit::Seconds:      return value;
    case TimeUnit::Minutes:      return value * 60.0;
    case TimeUnit::Hours:        return value * 3600.0;
    case TimeUnit::Days:         return value * 86400.0;
    case TimeUnit::Years:        return value * 31557600.0; // Julian year (365.25 d)
    }
    return value;
}

namespace {

std::vector<IsotopeRecord> build_isotope_table() {
    auto stable = [](int z, int a, double mass, double abund, std::string spin) {
        return IsotopeRecord{z, a, mass, abund, true, std::nullopt, std::move(spin),
                             {DecayMode::Stable}, {}};
    };
    auto radio = [](int z, int a, double mass, double abund, double hl_val, TimeUnit unit,
                    std::string spin, DecayMode mode, std::string daughter) {
        return IsotopeRecord{z, a, mass, abund, false, half_life_to_seconds(hl_val, unit),
                             std::move(spin), {mode}, {std::move(daughter)}};
    };

    std::vector<IsotopeRecord> v;
    v.reserve(320);

    // Z = 1 Hydrogen
    v.push_back(stable(1, 1, 1.007825, 99.9885, "1/2+"));
    v.push_back(stable(1, 2, 2.014102, 0.0115, "1+"));
    v.push_back(radio (1, 3, 3.016049, 0.0, 12.32, TimeUnit::Years, "1/2+", DecayMode::BetaMinus, "He-3"));

    // Z = 2 Helium
    v.push_back(stable(2, 3, 3.016029, 0.0002, "1/2+"));
    v.push_back(stable(2, 4, 4.002603, 99.9998, "0+"));

    // Z = 3 Lithium
    v.push_back(stable(3, 6, 6.015123, 7.59, "1+"));
    v.push_back(stable(3, 7, 7.016004, 92.41, "3/2-"));

    // Z = 4 Beryllium
    v.push_back(stable(4, 9, 9.012183, 100.0, "3/2-"));
    v.push_back(radio (4, 10, 10.013535, 0.0, 1.39e6, TimeUnit::Years, "0+", DecayMode::BetaMinus, "B-10"));

    // Z = 5 Boron
    v.push_back(stable(5, 10, 10.012937, 19.9, "3+"));
    v.push_back(stable(5, 11, 11.009305, 80.1, "3/2-"));

    // Z = 6 Carbon
    v.push_back(stable(6, 12, 12.000000, 98.93, "0+"));
    v.push_back(stable(6, 13, 13.003355, 1.07, "1/2-"));
    v.push_back(radio (6, 14, 14.003242, 0.0, 5730.0, TimeUnit::Years, "0+", DecayMode::BetaMinus, "N-14"));

    // Z = 7 Nitrogen
    v.push_back(stable(7, 14, 14.003074, 99.636, "1+"));
    v.push_back(stable(7, 15, 15.000109, 0.364, "1/2-"));

    // Z = 8 Oxygen
    v.push_back(stable(8, 16, 15.994915, 99.757, "0+"));
    v.push_back(stable(8, 17, 16.999132, 0.038, "5/2+"));
    v.push_back(stable(8, 18, 17.999160, 0.205, "0+"));

    // Z = 9 Fluorine
    v.push_back(stable(9, 19, 18.998403, 100.0, "1/2+"));

    // Z = 10 Neon
    v.push_back(stable(10, 20, 19.992440, 90.48, "0+"));
    v.push_back(stable(10, 21, 20.993847, 0.27, "3/2+"));
    v.push_back(stable(10, 22, 21.991385, 9.25, "0+"));

    // Z = 11 Sodium
    v.push_back(radio (11, 22, 21.994437, 0.0, 2.6019, TimeUnit::Years, "3+", DecayMode::BetaPlus, "Ne-22"));
    v.push_back(stable(11, 23, 22.989769, 100.0, "3/2+"));
    v.push_back(radio (11, 24, 23.990963, 0.0, 14.997, TimeUnit::Hours, "4+", DecayMode::BetaMinus, "Mg-24"));

    // Z = 12 Magnesium
    v.push_back(stable(12, 24, 23.985042, 78.99, "0+"));
    v.push_back(stable(12, 25, 24.985837, 10.00, "5/2+"));
    v.push_back(stable(12, 26, 25.982593, 11.01, "0+"));

    // Z = 13 Aluminum
    v.push_back(stable(13, 27, 26.981539, 100.0, "5/2+"));

    // Z = 14 Silicon
    v.push_back(stable(14, 28, 27.976927, 92.223, "0+"));
    v.push_back(stable(14, 29, 28.976495, 4.685, "1/2+"));
    v.push_back(stable(14, 30, 29.973770, 3.092, "0+"));

    // Z = 15 Phosphorus
    v.push_back(stable(15, 31, 30.973762, 100.0, "1/2+"));
    v.push_back(radio (15, 32, 31.973908, 0.0, 14.268, TimeUnit::Days, "1+", DecayMode::BetaMinus, "S-32"));

    // Z = 16 Sulfur
    v.push_back(stable(16, 32, 31.972071, 94.99, "0+"));
    v.push_back(stable(16, 33, 32.971459, 0.75, "3/2+"));
    v.push_back(stable(16, 34, 33.967867, 4.25, "0+"));
    v.push_back(stable(16, 36, 35.967081, 0.01, "0+"));

    // Z = 17 Chlorine
    v.push_back(stable(17, 35, 34.968853, 75.76, "3/2+"));
    v.push_back(stable(17, 37, 36.965903, 24.24, "3/2+"));

    // Z = 18 Argon
    v.push_back(stable(18, 36, 35.967545, 0.3336, "0+"));
    v.push_back(stable(18, 38, 37.962732, 0.0629, "0+"));
    v.push_back(stable(18, 40, 39.962383, 99.6035, "0+"));

    // Z = 19 Potassium
    v.push_back(stable(19, 39, 38.963707, 93.2581, "3/2+"));
    v.push_back(radio (19, 40, 39.963998, 0.0117, 1.248e9, TimeUnit::Years, "4-", DecayMode::BetaMinus, "Ca-40"));
    v.push_back(stable(19, 41, 40.961826, 6.7302, "3/2+"));

    // Z = 20 Calcium
    v.push_back(stable(20, 40, 39.962591, 96.941, "0+"));
    v.push_back(stable(20, 42, 41.958618, 0.647, "0+"));
    v.push_back(stable(20, 43, 42.958766, 0.135, "7/2-"));
    v.push_back(stable(20, 44, 43.955482, 2.086, "0+"));
    v.push_back(stable(20, 46, 45.953689, 0.004, "0+"));
    v.push_back(stable(20, 48, 47.952523, 0.187, "0+"));

    // Z = 21 Scandium
    v.push_back(stable(21, 45, 44.955908, 100.0, "7/2-"));

    // Z = 22 Titanium
    v.push_back(stable(22, 46, 45.952627, 8.25, "0+"));
    v.push_back(stable(22, 47, 46.951758, 7.44, "5/2-"));
    v.push_back(stable(22, 48, 47.947941, 73.72, "0+"));
    v.push_back(stable(22, 49, 48.947865, 5.41, "7/2-"));
    v.push_back(stable(22, 50, 49.944786, 5.18, "0+"));

    // Z = 23 Vanadium
    v.push_back(radio (23, 50, 49.947156, 0.250, 1.4e17, TimeUnit::Years, "6+", DecayMode::ElectronCapture, "Ti-50"));
    v.push_back(stable(23, 51, 50.943957, 99.750, "7/2-"));

    // Z = 24 Chromium
    v.push_back(stable(24, 50, 49.946042, 4.345, "0+"));
    v.push_back(stable(24, 52, 51.940506, 83.789, "0+"));
    v.push_back(stable(24, 53, 52.940648, 9.501, "3/2-"));
    v.push_back(stable(24, 54, 53.938879, 2.365, "0+"));

    // Z = 25 Manganese
    v.push_back(stable(25, 55, 54.938043, 100.0, "5/2-"));

    // Z = 26 Iron
    v.push_back(stable(26, 54, 53.939609, 5.845, "0+"));
    v.push_back(stable(26, 56, 55.934936, 91.754, "0+"));
    v.push_back(stable(26, 57, 56.935393, 2.119, "1/2-"));
    v.push_back(stable(26, 58, 57.933274, 0.282, "0+"));

    // Z = 27 Cobalt
    v.push_back(stable(27, 59, 58.933194, 100.0, "7/2-"));
    v.push_back(radio (27, 60, 59.933817, 0.0, 5.2714, TimeUnit::Years, "5+", DecayMode::BetaMinus, "Ni-60"));

    // Z = 28 Nickel
    v.push_back(stable(28, 58, 57.935342, 68.077, "0+"));
    v.push_back(stable(28, 60, 59.930785, 26.223, "0+"));
    v.push_back(stable(28, 61, 60.931055, 1.140, "3/2-"));
    v.push_back(stable(28, 62, 61.928345, 3.634, "0+"));
    v.push_back(stable(28, 64, 63.927967, 0.926, "0+"));

    // Z = 29 Copper
    v.push_back(stable(29, 63, 62.929598, 69.15, "3/2-"));
    v.push_back(stable(29, 65, 64.927790, 30.85, "3/2-"));

    // Z = 30 Zinc
    v.push_back(stable(30, 64, 63.929142, 49.17, "0+"));
    v.push_back(stable(30, 66, 65.926033, 27.73, "0+"));
    v.push_back(stable(30, 67, 66.927127, 4.04, "5/2-"));
    v.push_back(stable(30, 68, 67.924844, 18.45, "0+"));
    v.push_back(stable(30, 70, 69.925319, 0.61, "0+"));

    // Z = 31 Gallium
    v.push_back(stable(31, 69, 68.925574, 60.108, "3/2-"));
    v.push_back(stable(31, 71, 70.924703, 39.892, "3/2-"));

    // Z = 32 Germanium
    v.push_back(stable(32, 70, 69.924249, 20.57, "0+"));
    v.push_back(stable(32, 72, 71.922076, 27.45, "0+"));
    v.push_back(stable(32, 73, 72.923459, 7.75, "9/2+"));
    v.push_back(stable(32, 74, 73.921178, 36.50, "0+"));
    v.push_back(stable(32, 76, 75.921403, 7.73, "0+"));

    // Z = 33 Arsenic
    v.push_back(stable(33, 75, 74.921595, 100.0, "3/2-"));

    // Z = 34 Selenium
    v.push_back(stable(34, 74, 73.922476, 0.89, "0+"));
    v.push_back(stable(34, 76, 75.919214, 9.37, "0+"));
    v.push_back(stable(34, 77, 76.919914, 7.63, "1/2-"));
    v.push_back(stable(34, 78, 77.917309, 23.77, "0+"));
    v.push_back(stable(34, 80, 79.916522, 49.61, "0+"));
    v.push_back(stable(34, 82, 81.916699, 8.73, "0+"));

    // Z = 35 Bromine
    v.push_back(stable(35, 79, 78.918338, 50.69, "3/2-"));
    v.push_back(stable(35, 81, 80.916290, 49.31, "3/2-"));

    // Z = 36 Krypton
    v.push_back(stable(36, 78, 77.920365, 0.355, "0+"));
    v.push_back(stable(36, 80, 79.916379, 2.286, "0+"));
    v.push_back(stable(36, 82, 81.913483, 11.593, "0+"));
    v.push_back(stable(36, 83, 82.914127, 11.500, "9/2+"));
    v.push_back(stable(36, 84, 83.911498, 56.987, "0+"));
    v.push_back(stable(36, 86, 85.910611, 17.279, "0+"));

    // Z = 37 Rubidium
    v.push_back(stable(37, 85, 84.911790, 72.17, "5/2-"));
    v.push_back(radio (37, 87, 86.909181, 27.83, 4.97e10, TimeUnit::Years, "3/2-", DecayMode::BetaMinus, "Sr-87"));

    // Z = 38 Strontium
    v.push_back(stable(38, 84, 83.913419, 0.56, "0+"));
    v.push_back(stable(38, 86, 85.909261, 9.86, "0+"));
    v.push_back(stable(38, 87, 86.908877, 7.00, "9/2+"));
    v.push_back(stable(38, 88, 87.905612, 82.58, "0+"));
    v.push_back(radio (38, 90, 89.907738, 0.0, 28.79, TimeUnit::Years, "0+", DecayMode::BetaMinus, "Y-90"));

    // Z = 39 Yttrium
    v.push_back(stable(39, 89, 88.905840, 100.0, "1/2-"));

    // Z = 40 Zirconium
    v.push_back(stable(40, 90, 89.904698, 51.45, "0+"));
    v.push_back(stable(40, 91, 90.905640, 11.22, "5/2+"));
    v.push_back(stable(40, 92, 91.905035, 17.15, "0+"));
    v.push_back(stable(40, 94, 93.906311, 17.38, "0+"));
    v.push_back(stable(40, 96, 95.908271, 2.80, "0+"));

    // Z = 41 Niobium
    v.push_back(stable(41, 93, 92.906373, 100.0, "9/2+"));

    // Z = 42 Molybdenum
    v.push_back(stable(42, 92, 91.906808, 14.649, "0+"));
    v.push_back(stable(42, 94, 93.905085, 9.187, "0+"));
    v.push_back(stable(42, 95, 94.905837, 15.873, "5/2+"));
    v.push_back(stable(42, 96, 95.904676, 16.673, "0+"));
    v.push_back(stable(42, 97, 96.906018, 9.582, "5/2+"));
    v.push_back(stable(42, 98, 97.905405, 24.292, "0+"));
    v.push_back(stable(42, 100, 99.907472, 9.744, "0+"));

    // Z = 43 Technetium (synthetic/trace)
    v.push_back(radio (43, 99, 98.906255, 0.0, 2.111e5, TimeUnit::Years, "9/2+", DecayMode::BetaMinus, "Ru-99"));

    // Z = 44 Ruthenium
    v.push_back(stable(44, 96, 95.907590, 5.54, "0+"));
    v.push_back(stable(44, 98, 97.905287, 1.87, "0+"));
    v.push_back(stable(44, 99, 98.905934, 12.76, "5/2+"));
    v.push_back(stable(44, 100, 99.904214, 12.60, "0+"));
    v.push_back(stable(44, 101, 100.905577, 17.06, "5/2+"));
    v.push_back(stable(44, 102, 101.904344, 31.55, "0+"));
    v.push_back(stable(44, 104, 103.905428, 18.62, "0+"));

    // Z = 45 Rhodium
    v.push_back(stable(45, 103, 102.905498, 100.0, "1/2-"));

    // Z = 46 Palladium
    v.push_back(stable(46, 102, 101.905602, 1.02, "0+"));
    v.push_back(stable(46, 104, 103.904031, 11.14, "0+"));
    v.push_back(stable(46, 105, 104.905080, 22.33, "5/2+"));
    v.push_back(stable(46, 106, 105.903480, 27.33, "0+"));
    v.push_back(stable(46, 108, 107.903892, 26.46, "0+"));
    v.push_back(stable(46, 110, 109.905172, 11.72, "0+"));

    // Z = 47 Silver
    v.push_back(stable(47, 107, 106.905092, 51.839, "1/2-"));
    v.push_back(stable(47, 109, 108.904755, 48.161, "1/2-"));

    // Z = 48 Cadmium
    v.push_back(stable(48, 106, 105.906460, 1.25, "0+"));
    v.push_back(stable(48, 108, 107.904183, 0.89, "0+"));
    v.push_back(stable(48, 110, 109.903007, 12.49, "0+"));
    v.push_back(stable(48, 111, 110.904183, 12.80, "1/2+"));
    v.push_back(stable(48, 112, 111.902763, 24.13, "0+"));
    v.push_back(stable(48, 113, 112.904408, 12.22, "1/2+"));
    v.push_back(stable(48, 114, 113.903365, 28.73, "0+"));
    v.push_back(stable(48, 116, 115.904763, 7.49, "0+"));

    // Z = 49 Indium
    v.push_back(stable(49, 113, 112.904062, 4.29, "9/2+"));
    v.push_back(radio (49, 115, 114.903879, 95.71, 4.41e14, TimeUnit::Years, "9/2+", DecayMode::BetaMinus, "Sn-115"));

    // Z = 50 Tin
    v.push_back(stable(50, 112, 111.904824, 0.97, "0+"));
    v.push_back(stable(50, 114, 113.902783, 0.66, "0+"));
    v.push_back(stable(50, 115, 114.903345, 0.34, "1/2+"));
    v.push_back(stable(50, 116, 115.901743, 14.54, "0+"));
    v.push_back(stable(50, 117, 116.902954, 7.68, "1/2+"));
    v.push_back(stable(50, 118, 117.901607, 24.22, "0+"));
    v.push_back(stable(50, 119, 118.903311, 8.59, "1/2+"));
    v.push_back(stable(50, 120, 119.902202, 32.58, "0+"));
    v.push_back(stable(50, 122, 121.903444, 4.63, "0+"));
    v.push_back(stable(50, 124, 123.905277, 5.79, "0+"));

    // Z = 51 Antimony
    v.push_back(stable(51, 121, 120.903812, 57.21, "5/2+"));
    v.push_back(stable(51, 123, 122.904213, 42.79, "7/2+"));

    // Z = 52 Tellurium
    v.push_back(stable(52, 120, 119.904059, 0.09, "0+"));
    v.push_back(stable(52, 122, 121.903044, 2.55, "0+"));
    v.push_back(stable(52, 123, 122.904270, 0.89, "1/2+"));
    v.push_back(stable(52, 124, 123.902818, 4.74, "0+"));
    v.push_back(stable(52, 125, 124.904431, 7.07, "1/2+"));
    v.push_back(stable(52, 126, 125.903312, 18.84, "0+"));
    v.push_back(stable(52, 128, 127.904461, 31.74, "0+"));
    v.push_back(stable(52, 130, 129.906223, 34.08, "0+"));

    // Z = 53 Iodine
    v.push_back(stable(53, 127, 126.904472, 100.0, "5/2+"));
    v.push_back(radio (53, 131, 130.906125, 0.0, 8.0197, TimeUnit::Days, "7/2+", DecayMode::BetaMinus, "Xe-131"));

    // Z = 54 Xenon
    v.push_back(stable(54, 124, 123.905892, 0.0952, "0+"));
    v.push_back(stable(54, 126, 125.904298, 0.0890, "0+"));
    v.push_back(stable(54, 128, 127.903531, 1.9102, "0+"));
    v.push_back(stable(54, 129, 128.904781, 26.4006, "1/2+"));
    v.push_back(stable(54, 130, 129.903509, 4.0710, "0+"));
    v.push_back(stable(54, 131, 130.905084, 21.2324, "3/2+"));
    v.push_back(stable(54, 132, 131.904155, 26.9086, "0+"));
    v.push_back(stable(54, 134, 133.905395, 10.4357, "0+"));
    v.push_back(stable(54, 136, 135.907214, 8.8573, "0+"));

    // Z = 55 Cesium
    v.push_back(stable(55, 133, 132.905452, 100.0, "7/2+"));
    v.push_back(radio (55, 137, 136.907090, 0.0, 30.17, TimeUnit::Years, "7/2+", DecayMode::BetaMinus, "Ba-137"));

    // Z = 56 Barium
    v.push_back(stable(56, 130, 129.906321, 0.106, "0+"));
    v.push_back(stable(56, 132, 131.905061, 0.101, "0+"));
    v.push_back(stable(56, 134, 133.904509, 2.417, "0+"));
    v.push_back(stable(56, 135, 134.905689, 6.592, "3/2+"));
    v.push_back(stable(56, 136, 135.904576, 7.854, "0+"));
    v.push_back(stable(56, 137, 136.905827, 11.232, "3/2+"));
    v.push_back(stable(56, 138, 137.905247, 71.698, "0+"));

    // Z = 57 Lanthanum
    v.push_back(radio (57, 138, 137.907118, 0.0888, 1.02e11, TimeUnit::Years, "5+", DecayMode::ElectronCapture, "Ba-138"));
    v.push_back(stable(57, 139, 138.906356, 99.9112, "7/2+"));

    // Z = 58 Cerium
    v.push_back(stable(58, 136, 135.907129, 0.185, "0+"));
    v.push_back(stable(58, 138, 137.905991, 0.251, "0+"));
    v.push_back(stable(58, 140, 139.905443, 88.450, "0+"));
    v.push_back(stable(58, 142, 141.909250, 11.114, "0+"));

    // Z = 59 Praseodymium
    v.push_back(stable(59, 141, 140.907658, 100.0, "5/2+"));

    // Z = 60 Neodymium
    v.push_back(stable(60, 142, 141.907729, 27.152, "0+"));
    v.push_back(stable(60, 143, 142.909820, 12.174, "7/2-"));
    v.push_back(radio (60, 144, 143.910093, 23.798, 2.29e15, TimeUnit::Years, "0+", DecayMode::Alpha, "Ce-140"));
    v.push_back(stable(60, 145, 144.912579, 8.293, "7/2-"));
    v.push_back(stable(60, 146, 145.913123, 17.189, "0+"));
    v.push_back(stable(60, 148, 147.916899, 5.756, "0+"));
    v.push_back(stable(60, 150, 149.920895, 5.638, "0+"));

    // Z = 61 Promethium (synthetic)
    v.push_back(radio (61, 145, 144.912756, 0.0, 17.7, TimeUnit::Years, "5/2+", DecayMode::ElectronCapture, "Nd-145"));

    // Z = 62 Samarium
    v.push_back(stable(62, 144, 143.912006, 3.07, "0+"));
    v.push_back(radio (62, 147, 146.914904, 14.99, 1.06e11, TimeUnit::Years, "7/2-", DecayMode::Alpha, "Nd-143"));
    v.push_back(stable(62, 148, 147.914829, 11.24, "0+"));
    v.push_back(stable(62, 149, 148.917192, 13.82, "7/2-"));
    v.push_back(stable(62, 150, 149.917283, 7.38, "0+"));
    v.push_back(stable(62, 152, 151.919740, 26.75, "0+"));
    v.push_back(stable(62, 154, 153.922217, 22.75, "0+"));

    // Z = 63 Europium
    v.push_back(stable(63, 151, 150.919858, 47.81, "5/2+"));
    v.push_back(stable(63, 153, 152.921238, 52.19, "5/2+"));

    // Z = 64 Gadolinium
    v.push_back(radio (64, 152, 151.919799, 0.20, 1.08e14, TimeUnit::Years, "0+", DecayMode::Alpha, "Sm-148"));
    v.push_back(stable(64, 154, 153.920874, 2.18, "0+"));
    v.push_back(stable(64, 155, 154.922630, 14.80, "3/2-"));
    v.push_back(stable(64, 156, 155.922131, 20.47, "0+"));
    v.push_back(stable(64, 157, 156.923969, 15.65, "3/2-"));
    v.push_back(stable(64, 158, 157.924112, 24.84, "0+"));
    v.push_back(stable(64, 160, 159.927062, 21.86, "0+"));

    // Z = 65 Terbium
    v.push_back(stable(65, 159, 158.925355, 100.0, "3/2+"));

    // Z = 66 Dysprosium
    v.push_back(stable(66, 156, 155.924285, 0.056, "0+"));
    v.push_back(stable(66, 158, 157.924416, 0.095, "0+"));
    v.push_back(stable(66, 160, 159.925205, 2.329, "0+"));
    v.push_back(stable(66, 161, 160.926941, 18.889, "5/2+"));
    v.push_back(stable(66, 162, 161.926806, 25.475, "0+"));
    v.push_back(stable(66, 163, 162.928738, 24.896, "5/2-"));
    v.push_back(stable(66, 164, 163.929182, 28.260, "0+"));

    // Z = 67 Holmium
    v.push_back(stable(67, 165, 164.930329, 100.0, "7/2-"));

    // Z = 68 Erbium
    v.push_back(stable(68, 162, 161.928787, 0.139, "0+"));
    v.push_back(stable(68, 164, 163.929208, 1.601, "0+"));
    v.push_back(stable(68, 166, 165.930299, 33.503, "0+"));
    v.push_back(stable(68, 167, 166.932054, 22.869, "7/2+"));
    v.push_back(stable(68, 168, 167.932377, 26.978, "0+"));
    v.push_back(stable(68, 170, 169.935470, 14.910, "0+"));

    // Z = 69 Thulium
    v.push_back(stable(69, 169, 168.934218, 100.0, "1/2+"));

    // Z = 70 Ytterbium
    v.push_back(stable(70, 168, 167.933891, 0.123, "0+"));
    v.push_back(stable(70, 170, 169.934766, 2.982, "0+"));
    v.push_back(stable(70, 171, 170.936330, 14.090, "1/2-"));
    v.push_back(stable(70, 172, 171.936386, 21.680, "0+"));
    v.push_back(stable(70, 173, 172.938215, 16.103, "5/2-"));
    v.push_back(stable(70, 174, 173.938866, 32.026, "0+"));
    v.push_back(stable(70, 176, 175.942576, 12.996, "0+"));

    // Z = 71 Lutetium
    v.push_back(stable(71, 175, 174.940775, 97.401, "7/2+"));
    v.push_back(radio (71, 176, 175.942690, 2.599, 3.76e10, TimeUnit::Years, "7-", DecayMode::BetaMinus, "Hf-176"));

    // Z = 72 Hafnium
    v.push_back(radio (72, 174, 173.940046, 0.16, 2.0e15, TimeUnit::Years, "0+", DecayMode::Alpha, "Yb-170"));
    v.push_back(stable(72, 176, 175.941408, 5.26, "0+"));
    v.push_back(stable(72, 177, 176.943228, 18.60, "7/2-"));
    v.push_back(stable(72, 178, 177.943706, 27.28, "0+"));
    v.push_back(stable(72, 179, 178.945823, 13.62, "9/2+"));
    v.push_back(stable(72, 180, 179.946557, 35.08, "0+"));

    // Z = 73 Tantalum
    v.push_back(stable(73, 180, 179.947465, 0.012, "9-"));
    v.push_back(stable(73, 181, 180.947996, 99.988, "7/2+"));

    // Z = 74 Tungsten
    v.push_back(stable(74, 180, 179.946711, 0.12, "0+"));
    v.push_back(stable(74, 182, 181.948204, 26.50, "0+"));
    v.push_back(stable(74, 183, 182.950223, 14.31, "1/2-"));
    v.push_back(stable(74, 184, 183.950931, 30.64, "0+"));
    v.push_back(stable(74, 186, 185.954363, 28.43, "0+"));

    // Z = 75 Rhenium
    v.push_back(stable(75, 185, 184.952955, 37.40, "5/2+"));
    v.push_back(radio (75, 187, 186.955751, 62.60, 4.12e10, TimeUnit::Years, "5/2+", DecayMode::BetaMinus, "Os-187"));

    // Z = 76 Osmium
    v.push_back(stable(76, 184, 183.952489, 0.02, "0+"));
    v.push_back(radio (76, 186, 185.953835, 1.59, 2.0e15, TimeUnit::Years, "0+", DecayMode::Alpha, "W-182"));
    v.push_back(stable(76, 187, 186.955748, 1.96, "1/2-"));
    v.push_back(stable(76, 188, 187.955836, 13.24, "0+"));
    v.push_back(stable(76, 189, 188.958145, 16.15, "3/2-"));
    v.push_back(stable(76, 190, 189.958444, 26.26, "0+"));
    v.push_back(stable(76, 192, 191.961479, 40.78, "0+"));

    // Z = 77 Iridium
    v.push_back(stable(77, 191, 190.960589, 37.3, "3/2+"));
    v.push_back(stable(77, 193, 192.962922, 62.7, "3/2+"));

    // Z = 78 Platinum
    v.push_back(radio (78, 190, 189.959929, 0.012, 6.5e11, TimeUnit::Years, "0+", DecayMode::Alpha, "Os-186"));
    v.push_back(stable(78, 192, 191.961038, 0.782, "0+"));
    v.push_back(stable(78, 194, 193.962680, 32.864, "0+"));
    v.push_back(stable(78, 195, 194.964791, 33.775, "1/2-"));
    v.push_back(stable(78, 196, 195.964952, 25.211, "0+"));
    v.push_back(stable(78, 198, 197.967893, 7.356, "0+"));

    // Z = 79 Gold
    v.push_back(stable(79, 197, 196.966569, 100.0, "3/2+"));

    // Z = 80 Mercury
    v.push_back(stable(80, 196, 195.965833, 0.15, "0+"));
    v.push_back(stable(80, 198, 197.966769, 9.97, "0+"));
    v.push_back(stable(80, 199, 198.968281, 16.87, "1/2-"));
    v.push_back(stable(80, 200, 199.968327, 23.10, "0+"));
    v.push_back(stable(80, 201, 200.970303, 13.18, "3/2-"));
    v.push_back(stable(80, 202, 201.970644, 29.86, "0+"));
    v.push_back(stable(80, 204, 203.973494, 6.87, "0+"));

    // Z = 81 Thallium
    v.push_back(stable(81, 203, 202.972344, 29.52, "1/2+"));
    v.push_back(stable(81, 205, 204.974428, 70.48, "1/2+"));

    // Z = 82 Lead
    v.push_back(stable(82, 204, 203.973044, 1.4, "0+"));
    v.push_back(stable(82, 206, 205.974466, 24.1, "0+"));
    v.push_back(stable(82, 207, 206.975897, 22.1, "1/2-"));
    v.push_back(stable(82, 208, 207.976653, 52.4, "0+"));

    // Z = 83 Bismuth (primordial, quasi-stable t1/2 ~ 1.9e19 y)
    v.push_back(radio (83, 209, 208.980399, 100.0, 1.9e19, TimeUnit::Years, "9/2-", DecayMode::Alpha, "Tl-205"));

    // Z = 84..89 Representative radioactive nuclides
    v.push_back(radio (84, 210, 209.982874, 0.0, 138.376, TimeUnit::Days, "0+", DecayMode::Alpha, "Pb-206"));
    v.push_back(radio (85, 210, 209.987148, 0.0, 8.1, TimeUnit::Hours, "5+", DecayMode::ElectronCapture, "Po-210"));
    v.push_back(radio (86, 222, 222.017578, 0.0, 3.8235, TimeUnit::Days, "0+", DecayMode::Alpha, "Po-218"));
    v.push_back(radio (87, 223, 223.019736, 0.0, 22.0, TimeUnit::Minutes, "3/2+", DecayMode::BetaMinus, "Ra-223"));
    v.push_back(radio (88, 226, 226.025410, 0.0, 1600.0, TimeUnit::Years, "0+", DecayMode::Alpha, "Rn-222"));
    v.push_back(radio (89, 227, 227.027752, 0.0, 21.772, TimeUnit::Years, "3/2-", DecayMode::BetaMinus, "Th-227"));

    // Z = 90 Thorium
    v.push_back(radio (90, 232, 232.038055, 100.0, 1.405e10, TimeUnit::Years, "0+", DecayMode::Alpha, "Ra-228"));

    // Z = 91 Protactinium
    v.push_back(radio (91, 231, 231.035884, 0.0, 3.276e4, TimeUnit::Years, "3/2-", DecayMode::Alpha, "Ac-227"));

    // Z = 92 Uranium
    v.push_back(radio (92, 234, 234.040952, 0.0054, 2.455e5, TimeUnit::Years, "0+", DecayMode::Alpha, "Th-230"));
    v.push_back(radio (92, 235, 235.043930, 0.7204, 7.038e8, TimeUnit::Years, "7/2-", DecayMode::Alpha, "Th-231"));
    v.push_back(radio (92, 238, 238.050788, 99.2742, 4.468e9, TimeUnit::Years, "0+", DecayMode::Alpha, "Th-234"));

    // Z = 93..96 Transuranics
    v.push_back(radio (93, 237, 237.048173, 0.0, 2.144e6, TimeUnit::Years, "5/2+", DecayMode::Alpha, "Pa-233"));
    v.push_back(radio (94, 239, 239.052163, 0.0, 24110.0, TimeUnit::Years, "1/2+", DecayMode::Alpha, "U-235"));
    v.push_back(radio (95, 241, 241.056829, 0.0, 432.6, TimeUnit::Years, "5/2-", DecayMode::Alpha, "Np-237"));
    v.push_back(radio (96, 244, 244.062753, 0.0, 18.1, TimeUnit::Years, "0+", DecayMode::Alpha, "Pu-240"));

    return v;
}

} // namespace

const std::vector<IsotopeRecord>& IsotopeRepository::all_isotopes() {
    static const std::vector<IsotopeRecord> table = build_isotope_table();
    return table;
}

std::vector<IsotopeRecord> IsotopeRepository::isotopes_for_element(int atomic_number) {
    std::vector<IsotopeRecord> res;
    for (const auto& iso : all_isotopes()) {
        if (iso.atomic_number == atomic_number) {
            res.push_back(iso);
        }
    }
    return res;
}

const IsotopeRecord* IsotopeRepository::find_isotope(int atomic_number, int mass_number) noexcept {
    for (const auto& iso : all_isotopes()) {
        if (iso.atomic_number == atomic_number && iso.mass_number == mass_number) {
            return &iso;
        }
    }
    return nullptr;
}

double IsotopeRepository::natural_abundance_sum(int atomic_number) noexcept {
    double sum = 0.0;
    for (const auto& iso : all_isotopes()) {
        if (iso.atomic_number == atomic_number) {
            sum += iso.abundance_pct;
        }
    }
    return sum;
}

void IsotopeRepository::populate_isotopes(core::domain::PeriodicTableDatabase& db) {
    for (int z = 1; z <= 118; ++z) {
        const auto* existing = db.get_element(z);
        if (!existing) continue;
        auto isos = isotopes_for_element(z);
        if (isos.empty()) continue;

        core::domain::PeriodicElement updated = *existing;
        updated.isotopes.clear();
        updated.isotopes.reserve(isos.size());
        for (const auto& rec : isos) {
            core::domain::ElementIsotope iso;
            iso.mass_number = rec.mass_number;
            iso.atomic_mass = rec.exact_mass_amu;
            iso.abundance_pct = rec.abundance_pct;
            iso.is_stable = rec.is_stable;
            iso.half_life_seconds = rec.half_life_seconds;
            iso.spin_parity = rec.spin_parity;
            updated.isotopes.push_back(std::move(iso));
        }
        (void)db.add_element(std::move(updated));
    }
}

std::string IsotopeRepository::to_json() {
    std::ostringstream out;
    out << "[\n";
    const auto& tbl = all_isotopes();
    for (size_t i = 0; i < tbl.size(); ++i) {
        const auto& r = tbl[i];
        out << "  {\"atomic_number\":" << r.atomic_number
            << ",\"mass_number\":" << r.mass_number
            << ",\"exact_mass_amu\":" << r.exact_mass_amu
            << ",\"abundance_pct\":" << r.abundance_pct
            << ",\"is_stable\":" << (r.is_stable ? "true" : "false")
            << ",\"spin_parity\":\"" << r.spin_parity << "\"}";
        if (i + 1 < tbl.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

std::string IsotopeRepository::generate_sqlite_isotopes_ddl() {
    return "CREATE TABLE IF NOT EXISTS isotopes (\n"
           "  atomic_number INTEGER NOT NULL,\n"
           "  mass_number INTEGER NOT NULL,\n"
           "  exact_mass_amu REAL NOT NULL,\n"
           "  abundance_pct REAL NOT NULL DEFAULT 0.0,\n"
           "  is_stable INTEGER NOT NULL,\n"
           "  half_life_seconds REAL,\n"
           "  spin_parity TEXT NOT NULL,\n"
           "  PRIMARY KEY (atomic_number, mass_number)\n"
           ");\n";
}

core::base::ValidationReport IsotopeRepository::audit_abundances() {
    core::base::ValidationReport report;
    std::map<int, double> sums;
    for (const auto& iso : all_isotopes()) {
        std::string path = "Z=" + std::to_string(iso.atomic_number) + ",A=" + std::to_string(iso.mass_number);
        if (iso.abundance_pct < 0.0 || iso.abundance_pct > 100.0) {
            report.add_error(path, "Invalid abundance out of [0,100]");
        }
        if (iso.exact_mass_amu <= 0.0) {
            report.add_error(path, "Invalid exact mass");
        }
        sums[iso.atomic_number] += iso.abundance_pct;
    }

    for (const auto& [z, sum] : sums) {
        if (sum > 0.0 && std::abs(sum - 100.0) > 0.01) {
            report.add_error("Z=" + std::to_string(z),
                             "Natural abundance sum mismatch (sum=" + std::to_string(sum) + "%)");
        }
    }
    return report;
}

} // namespace shunya::matdb
