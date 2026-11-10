#include "shunya/matdb/spectral_catalog.hpp"

#include <cmath>
#include <sstream>

namespace shunya::matdb {

double vacuum_to_air_nm(double lambda_vac_nm) noexcept {
    if (lambda_vac_nm < 200.0) {
        return lambda_vac_nm; // Vacuum UV; air opaque below 200 nm
    }
    const double sigma_um_inv = 1000.0 / lambda_vac_nm;
    const double s2 = sigma_um_inv * sigma_um_inv;
    const double n_minus_1 = 1e-8 * (8342.54 + 2406147.0 / (130.0 - s2) + 15998.0 / (38.9 - s2));
    return lambda_vac_nm / (1.0 + n_minus_1);
}

std::optional<double> rydberg_wavelength_vacuum_nm(int atomic_number,
                                                   int n_upper,
                                                   int n_lower) noexcept {
    if (atomic_number < 1 || n_lower < 1 || n_upper <= n_lower) {
        return std::nullopt;
    }
    const double r_m = (atomic_number == 1)
                           ? kRydbergHydrogenPerM
                           : (kRydbergInfinityPerM / (1.0 + 1.0 / (1836.15267 * static_cast<double>(2 * atomic_number))));
    const double inv_n2_diff =
        1.0 / (static_cast<double>(n_lower) * static_cast<double>(n_lower)) -
        1.0 / (static_cast<double>(n_upper) * static_cast<double>(n_upper));
    const double z2 = static_cast<double>(atomic_number) * static_cast<double>(atomic_number);
    const double inv_lambda_m = r_m * z2 * inv_n2_diff;
    if (inv_lambda_m <= 0.0) return std::nullopt;
    return 1e9 / inv_lambda_m;
}

namespace {

SpectralLine make_rydberg_h(std::string label, int n_upper, int n_lower, double einstein_a, double rel_int) {
    const double vac = rydberg_wavelength_vacuum_nm(1, n_upper, n_lower).value_or(0.0);
    return SpectralLine{
        1, "H", std::move(label), vac, vacuum_to_air_nm(vac),
        einstein_a, rel_int, n_upper, n_lower
    };
}

SpectralLine make_line(int z, std::string sym, std::string label, double vac_nm,
                       double einstein_a, double rel_int) {
    return SpectralLine{
        z, std::move(sym), std::move(label), vac_nm, vacuum_to_air_nm(vac_nm),
        einstein_a, rel_int, 0, 0
    };
}

std::vector<SpectralLine> build_spectral_catalog() {
    std::vector<SpectralLine> lines;
    lines.reserve(40);

    // Hydrogen Lyman, Balmer, and Paschen series
    lines.push_back(make_rydberg_h("Lyman-alpha (2->1)",   2, 1, 4.699e8, 1000.0));
    lines.push_back(make_rydberg_h("Lyman-beta (3->1)",    3, 1, 5.575e7,  400.0));
    lines.push_back(make_rydberg_h("Balmer H-alpha (3->2)", 3, 2, 4.410e7,  900.0));
    lines.push_back(make_rydberg_h("Balmer H-beta (4->2)",  4, 2, 8.419e6,  450.0));
    lines.push_back(make_rydberg_h("Balmer H-gamma (5->2)", 5, 2, 2.530e6,  250.0));
    lines.push_back(make_rydberg_h("Balmer H-delta (6->2)", 6, 2, 9.732e5,  140.0));
    lines.push_back(make_rydberg_h("Paschen-alpha (4->3)",  4, 3, 8.986e6,  200.0));

    // Helium
    lines.push_back(make_line(2,  "He", "He I Resonance",  58.433,  1.80e9, 900.0));
    lines.push_back(make_line(2,  "He", "He I D3 Yellow", 587.725,  7.07e7, 500.0));
    lines.push_back(make_line(2,  "He", "He I Green",     501.708,  1.34e7, 250.0));
    lines.push_back(make_line(2,  "He", "He I IR 1083",  1083.331,  1.02e7, 800.0));

    // Lithium & Sodium & Potassium (Alkali D doublets)
    lines.push_back(make_line(3,  "Li", "Li I Red 670.8", 670.961,  3.69e7, 950.0));
    lines.push_back(make_line(11, "Na", "Na I D2 (3p3/2->3s)", 589.158, 6.16e7, 1000.0));
    lines.push_back(make_line(11, "Na", "Na I D1 (3p1/2->3s)", 589.756, 6.14e7,  500.0));
    lines.push_back(make_line(19, "K",  "K I NIR 766.5",  766.701,  3.80e7,  900.0));
    lines.push_back(make_line(19, "K",  "K I NIR 769.9",  770.108,  3.75e7,  500.0));

    // Calcium & Neon & Argon & Iron & Copper
    lines.push_back(make_line(10, "Ne", "Ne I Red 640.2", 640.402,  5.14e7, 1000.0));
    lines.push_back(make_line(10, "Ne", "HeNe Laser 632.8", 632.991, 3.39e6, 300.0));
    lines.push_back(make_line(18, "Ar", "Ar I Blue 415.9", 415.976, 1.40e6,  400.0));
    lines.push_back(make_line(18, "Ar", "Ar I IR 763.5",   763.721, 2.45e7,  900.0));
    lines.push_back(make_line(20, "Ca", "Ca II K (393.4)", 393.477, 1.47e8, 1000.0));
    lines.push_back(make_line(20, "Ca", "Ca II H (396.8)", 396.959, 1.40e8,  700.0));
    lines.push_back(make_line(26, "Fe", "Fe I UV 248.3",   248.402, 4.90e8,  850.0));
    lines.push_back(make_line(26, "Fe", "Fe I Blue 438.4", 438.477, 5.00e7,  600.0));
    lines.push_back(make_line(29, "Cu", "Cu I UV 324.8",   324.848, 1.39e8, 1000.0));
    lines.push_back(make_line(29, "Cu", "Cu I Green 510.6", 510.696, 2.00e6, 400.0));

    // Mercury calibration lines (Hg I)
    lines.push_back(make_line(80, "Hg", "Hg I UVC 253.7",   253.728, 8.00e6, 1000.0));
    lines.push_back(make_line(80, "Hg", "Hg I i-line 365.0", 365.120, 1.30e8, 700.0));
    lines.push_back(make_line(80, "Hg", "Hg I h-line 404.7", 404.770, 2.10e7, 650.0));
    lines.push_back(make_line(80, "Hg", "Hg I g-line 435.8", 435.956, 5.57e7, 850.0));
    lines.push_back(make_line(80, "Hg", "Hg I e-line 546.1", 546.227, 4.87e7, 900.0));
    lines.push_back(make_line(80, "Hg", "Hg I Yellow 577.0", 577.120, 2.38e7, 550.0));
    lines.push_back(make_line(80, "Hg", "Hg I Yellow 579.1", 579.226, 2.05e7, 520.0));

    return lines;
}

} // namespace

const std::vector<SpectralLine>& SpectralLineCatalog::all_lines() {
    static const std::vector<SpectralLine> catalog = build_spectral_catalog();
    return catalog;
}

std::vector<SpectralLine> SpectralLineCatalog::query_by_element(int atomic_number) {
    std::vector<SpectralLine> out;
    for (const auto& l : all_lines()) {
        if (l.atomic_number == atomic_number) out.push_back(l);
    }
    return out;
}

std::vector<SpectralLine> SpectralLineCatalog::query_by_symbol(std::string_view symbol) {
    std::vector<SpectralLine> out;
    for (const auto& l : all_lines()) {
        if (l.element_symbol == symbol) out.push_back(l);
    }
    return out;
}

std::vector<SpectralLine> SpectralLineCatalog::query_wavelength_range(double min_vac_nm,
                                                                        double max_vac_nm) {
    std::vector<SpectralLine> out;
    for (const auto& l : all_lines()) {
        if (l.wavelength_vacuum_nm >= min_vac_nm && l.wavelength_vacuum_nm <= max_vac_nm) {
            out.push_back(l);
        }
    }
    return out;
}

std::string SpectralLineCatalog::to_json() {
    std::ostringstream out;
    out << "[\n";
    const auto& tbl = all_lines();
    for (size_t i = 0; i < tbl.size(); ++i) {
        const auto& l = tbl[i];
        out << "  {\"atomic_number\":" << l.atomic_number
            << ",\"symbol\":\"" << l.element_symbol << "\""
            << ",\"label\":\"" << l.transition_label << "\""
            << ",\"wavelength_vacuum_nm\":" << l.wavelength_vacuum_nm
            << ",\"wavelength_air_nm\":" << l.wavelength_air_nm
            << ",\"einstein_a_s_inv\":" << l.einstein_a_s_inv
            << "}";
        if (i + 1 < tbl.size()) out << ",";
        out << "\n";
    }
    out << "]\n";
    return out.str();
}

} // namespace shunya::matdb
