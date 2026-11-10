#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::matdb {

inline constexpr double kRydbergInfinityPerM = 10973731.568160;
inline constexpr double kRydbergHydrogenPerM = 10967758.34;

struct SpectralLine {
    int atomic_number{1};
    std::string element_symbol;
    std::string transition_label;
    double wavelength_vacuum_nm{656.469};
    double wavelength_air_nm{656.280};
    double einstein_a_s_inv{4.41e7};
    double relative_intensity{100.0};
    int n_upper{0};
    int n_lower{0};
};

/**
 * @brief Converts a vacuum wavelength (nm) to standard dry air wavelength (nm) via Edlen's equation.
 */
[[nodiscard]] double vacuum_to_air_nm(double lambda_vac_nm) noexcept;

/**
 * @brief Computes hydrogenic vacuum transition wavelength (nm) using the Rydberg formula.
 */
[[nodiscard]] std::optional<double> rydberg_wavelength_vacuum_nm(int atomic_number,
                                                                 int n_upper,
                                                                 int n_lower) noexcept;

/**
 * @brief Catalog of NIST ASD atomic emission/absorption lines and analytical Rydberg transitions.
 */
class SpectralLineCatalog {
public:
    static const std::vector<SpectralLine>& all_lines();
    [[nodiscard]] static std::vector<SpectralLine> query_by_element(int atomic_number);
    [[nodiscard]] static std::vector<SpectralLine> query_by_symbol(std::string_view symbol);
    [[nodiscard]] static std::vector<SpectralLine> query_wavelength_range(double min_vac_nm,
                                                                          double max_vac_nm);
    [[nodiscard]] static std::string to_json();
};

} // namespace shunya::matdb
