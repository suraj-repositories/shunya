#include "shunya/matdb/bohr_orbital_model.hpp"

#include "shunya/matdb/electron_config_parser.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"

#include <array>
#include <cmath>

namespace shunya::matdb {

namespace {

inline constexpr double kPi = 3.14159265358979323846;

core::base::Error make_bohr_error(std::string msg) {
    return core::base::Error{
        .code = 4130,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

char shell_letter_for_n(int n) noexcept {
    constexpr char letters[] = {'?', 'K', 'L', 'M', 'N', 'O', 'P', 'Q'};
    if (n >= 1 && n <= 7) return letters[n];
    return '?';
}

} // namespace

core::base::Result<BohrAtomModel> BohrOrbitalModel::compute_for_element(int atomic_number) {
    const auto* elem = Elements118Catalog::find_by_z(atomic_number);
    if (!elem) return make_bohr_error("Atomic number out of [1, 118]");

    auto parsed_res = ElectronConfigParser::parse(elem->electron_configuration);
    if (!parsed_res) return parsed_res.error();

    std::array<int, 8> per_n{};
    int max_n = 1;
    for (const auto& sub : parsed_res.value().subshells) {
        if (sub.n >= 1 && sub.n <= 7) {
            per_n[static_cast<size_t>(sub.n)] += sub.occupancy;
            if (sub.n > max_n) max_n = sub.n;
        }
    }

    BohrAtomModel model;
    model.atomic_number = atomic_number;
    model.symbol = elem->symbol;
    int total = 0;
    for (int n = 1; n <= max_n; ++n) {
        const int count = per_n[static_cast<size_t>(n)];
        model.shells.push_back(BohrShellCount{n, shell_letter_for_n(n), count, 2 * n * n});
        total += count;
    }
    model.total_electrons = total;
    return model;
}

double BohrOrbitalModel::spherical_harmonic_density(int l, int m,
                                                    double theta_rad,
                                                    double phi_rad) noexcept {
    const double st = std::sin(theta_rad);
    const double ct = std::cos(theta_rad);
    const double cp = std::cos(phi_rad);
    const double sp = std::sin(phi_rad);
    double y = 0.0;

    if (l == 0) {
        y = 0.5 * std::sqrt(1.0 / kPi);
    } else if (l == 1) {
        const double c = std::sqrt(3.0 / (4.0 * kPi));
        if (m == 0)       y = c * ct;
        else if (m == 1)  y = c * st * cp;
        else if (m == -1) y = c * st * sp;
    } else if (l == 2) {
        if (m == 0)       y = 0.25 * std::sqrt(5.0 / kPi) * (3.0 * ct * ct - 1.0);
        else if (m == 1)  y = 0.5 * std::sqrt(15.0 / kPi) * st * ct * cp;
        else if (m == -1) y = 0.5 * std::sqrt(15.0 / kPi) * st * ct * sp;
        else if (m == 2)  y = 0.25 * std::sqrt(15.0 / kPi) * st * st * (cp * cp - sp * sp);
        else if (m == -2) y = 0.5 * std::sqrt(15.0 / kPi) * st * st * cp * sp;
    } else if (l == 3) {
        if (m == 0) y = 0.25 * std::sqrt(7.0 / kPi) * (5.0 * ct * ct * ct - 3.0 * ct);
        else        y = 0.25 * std::sqrt(21.0 / (2.0 * kPi)) * st * (5.0 * ct * ct - 1.0) * cp;
    }
    return y * y;
}

std::string BohrOrbitalModel::angular_orbital_glsl_fragment() {
    return "#version 440\n"
           "layout(location = 0) in vec2 vUv;\n"
           "layout(location = 0) out vec4 fragColor;\n"
           "uniform int uL;\n"
           "uniform int uM;\n"
           "void main() {\n"
           "  vec2 p = vUv * 2.0 - 1.0;\n"
           "  float r2 = dot(p, p);\n"
           "  float theta = acos(clamp(p.y / max(sqrt(r2), 1e-4), -1.0, 1.0));\n"
           "  float amp = (uL == 0) ? 1.0 : ((uL == 1) ? abs(cos(theta)) : abs(3.0 * cos(theta) * cos(theta) - 1.0) * 0.5);\n"
           "  float prob = amp * amp * exp(-3.5 * length(p));\n"
           "  fragColor = vec4(0.2 * prob, 0.7 * prob, 1.0 * prob, 1.0);\n"
           "}\n";
}

core::base::ValidationReport BohrOrbitalModel::audit_all_118_bohr_counts() {
    core::base::ValidationReport report;
    for (int z = 1; z <= 118; ++z) {
        auto res = compute_for_element(z);
        if (!res) {
            report.add_error("Z=" + std::to_string(z), res.error().message);
            continue;
        }
        if (res.value().total_electrons != z) {
            report.add_error("Z=" + std::to_string(z), "Shell sum does not match atomic number Z");
        }
    }
    return report;
}

} // namespace shunya::matdb
