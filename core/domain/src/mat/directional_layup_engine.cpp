#include "shunya/mat/directional_layup_engine.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>

namespace shunya::mat {

namespace {

struct TransformedQ {
    double q11{0.0};
    double q22{0.0};
    double q12{0.0};
    double q66{0.0};
    double q16{0.0};
    double q26{0.0};
};

[[nodiscard]] TransformedQ compute_transformed_q(const LaminatePly& ply) noexcept {
    const double e1 = std::max(1e-3, ply.e1_gpa);
    const double e2 = std::max(1e-3, ply.e2_gpa);
    const double nu12 = std::clamp(ply.nu12, 0.01, 0.99);
    const double g12 = std::max(1e-3, ply.g12_gpa);

    // Minor Poisson's ratio: nu21 = nu12 * E2 / E1
    const double nu21 = nu12 * (e2 / e1);
    const double denom = std::max(1e-6, 1.0 - nu12 * nu21);

    const double q11 = e1 / denom;
    const double q22 = e2 / denom;
    const double q12 = (nu12 * e2) / denom;
    const double q66 = g12;

    const double theta_rad = ply.angle_deg * DirectionalLayupEngine::DEG_TO_RAD;
    const double c = std::cos(theta_rad);
    const double s = std::sin(theta_rad);
    const double c2 = c * c;
    const double s2 = s * s;
    const double c4 = c2 * c2;
    const double s4 = s2 * s2;
    const double c2s2 = c2 * s2;

    TransformedQ qbar;
    qbar.q11 = q11 * c4 + 2.0 * (q12 + 2.0 * q66) * c2s2 + q22 * s4;
    qbar.q22 = q11 * s4 + 2.0 * (q12 + 2.0 * q66) * c2s2 + q22 * c4;
    qbar.q12 = (q11 + q22 - 4.0 * q66) * c2s2 + q12 * (c4 + s4);
    qbar.q66 = (q11 + q22 - 2.0 * q12 - 2.0 * q66) * c2s2 + q66 * (c4 + s4);
    qbar.q16 = (q11 - q12 - 2.0 * q66) * s * (c2 * c) - (q22 - q12 - 2.0 * q66) * (s2 * s) * c;
    qbar.q26 = (q11 - q12 - 2.0 * q66) * (s2 * s) * c - (q22 - q12 - 2.0 * q66) * s * (c2 * c);

    return qbar;
}

} // namespace

std::array<std::array<double, 3>, 3> DirectionalLayupEngine::compute_second_order_orientation_tensor(
    const std::vector<LaminatePly>& plies) noexcept {
    std::array<std::array<double, 3>, 3> a{};

    double total_t = 0.0;
    for (const auto& ply : plies) {
        total_t += std::max(1e-9, ply.thickness_m);
    }
    if (total_t <= 0.0) return a;

    for (const auto& ply : plies) {
        const double w = std::max(1e-9, ply.thickness_m) / total_t;
        const double theta = ply.angle_deg * DEG_TO_RAD;
        const double nx = std::cos(theta);
        const double ny = std::sin(theta);

        a[0][0] += w * nx * nx;
        a[0][1] += w * nx * ny;
        a[1][0] += w * nx * ny;
        a[1][1] += w * ny * ny;
        // a_zz remains 0 for in-plane planar laminates
    }

    return a;
}

LaminateAbdResult DirectionalLayupEngine::evaluate_laminate(
    const std::vector<LaminatePly>& plies) {
    LaminateAbdResult res;
    if (plies.empty()) return res;

    // 1. Total thickness and symmetry check
    for (const auto& ply : plies) {
        res.total_thickness_m += std::max(1e-9, ply.thickness_m);
    }

    const size_t n = plies.size();
    res.is_symmetric = true;
    for (size_t i = 0; i < n / 2; ++i) {
        const auto& p1 = plies[i];
        const auto& p2 = plies[n - 1 - i];
        if (std::abs(p1.thickness_m - p2.thickness_m) > 1e-9 ||
            std::abs(p1.angle_deg - p2.angle_deg) > 1e-4 ||
            p1.name != p2.name) {
            res.is_symmetric = false;
            break;
        }
    }

    // Balance check: +theta plies match -theta plies
    std::map<double, double> angle_thickness;
    for (const auto& ply : plies) {
        angle_thickness[ply.angle_deg] += ply.thickness_m;
    }
    res.is_balanced = true;
    for (const auto& [ang, thick] : angle_thickness) {
        if (std::abs(ang) > 1e-4 && std::abs(std::abs(ang) - 90.0) > 1e-4) {
            double neg_thick = angle_thickness[-ang];
            if (std::abs(thick - neg_thick) > 1e-7) {
                res.is_balanced = false;
                break;
            }
        }
    }

    // 2. Classical Laminate Theory ABD Matrix integration
    double z_curr = -0.5 * res.total_thickness_m;

    for (const auto& ply : plies) {
        const double t = std::max(1e-9, ply.thickness_m);
        const double z_next = z_curr + t;

        const double delta_z = z_next - z_curr;
        const double delta_z2 = (z_next * z_next) - (z_curr * z_curr);
        const double delta_z3 = (z_next * z_next * z_next) - (z_curr * z_curr * z_curr);

        const auto qbar = compute_transformed_q(ply);

        // A matrix [GPa * m]
        res.a_matrix_gpa_m[0][0] += qbar.q11 * delta_z;
        res.a_matrix_gpa_m[0][1] += qbar.q12 * delta_z;
        res.a_matrix_gpa_m[1][0] += qbar.q12 * delta_z;
        res.a_matrix_gpa_m[1][1] += qbar.q22 * delta_z;
        res.a_matrix_gpa_m[2][2] += qbar.q66 * delta_z;
        res.a_matrix_gpa_m[0][2] += qbar.q16 * delta_z;
        res.a_matrix_gpa_m[2][0] += qbar.q16 * delta_z;
        res.a_matrix_gpa_m[1][2] += qbar.q26 * delta_z;
        res.a_matrix_gpa_m[2][1] += qbar.q26 * delta_z;

        // B matrix [GPa * m^2]
        res.b_matrix_gpa_m2[0][0] += 0.5 * qbar.q11 * delta_z2;
        res.b_matrix_gpa_m2[0][1] += 0.5 * qbar.q12 * delta_z2;
        res.b_matrix_gpa_m2[1][0] += 0.5 * qbar.q12 * delta_z2;
        res.b_matrix_gpa_m2[1][1] += 0.5 * qbar.q22 * delta_z2;
        res.b_matrix_gpa_m2[2][2] += 0.5 * qbar.q66 * delta_z2;
        res.b_matrix_gpa_m2[0][2] += 0.5 * qbar.q16 * delta_z2;
        res.b_matrix_gpa_m2[2][0] += 0.5 * qbar.q16 * delta_z2;
        res.b_matrix_gpa_m2[1][2] += 0.5 * qbar.q26 * delta_z2;
        res.b_matrix_gpa_m2[2][1] += 0.5 * qbar.q26 * delta_z2;

        // D matrix [GPa * m^3]
        const double d_factor = (1.0 / 3.0) * delta_z3;
        res.d_matrix_gpa_m3[0][0] += qbar.q11 * d_factor;
        res.d_matrix_gpa_m3[0][1] += qbar.q12 * d_factor;
        res.d_matrix_gpa_m3[1][0] += qbar.q12 * d_factor;
        res.d_matrix_gpa_m3[1][1] += qbar.q22 * d_factor;
        res.d_matrix_gpa_m3[2][2] += qbar.q66 * d_factor;
        res.d_matrix_gpa_m3[0][2] += qbar.q16 * d_factor;
        res.d_matrix_gpa_m3[2][0] += qbar.q16 * d_factor;
        res.d_matrix_gpa_m3[1][2] += qbar.q26 * d_factor;
        res.d_matrix_gpa_m3[2][1] += qbar.q26 * d_factor;

        z_curr = z_next;
    }

    // If symmetric, B matrix should strictly be zero
    if (res.is_symmetric) {
        for (auto& row : res.b_matrix_gpa_m2) {
            row.fill(0.0);
        }
    }

    // 3. Equivalent In-Plane Engineering Constants
    const double a11 = res.a_matrix_gpa_m[0][0];
    const double a22 = res.a_matrix_gpa_m[1][1];
    const double a12 = res.a_matrix_gpa_m[0][1];
    const double a66 = res.a_matrix_gpa_m[2][2];
    const double h = std::max(1e-9, res.total_thickness_m);

    const double det_a = (a11 * a22) - (a12 * a12);
    if (det_a > 1e-9) {
        res.effective_ex_gpa = det_a / (h * std::max(1e-9, a22));
        res.effective_ey_gpa = det_a / (h * std::max(1e-9, a11));
        res.effective_gxy_gpa = a66 / h;
        res.effective_nu_xy = a12 / std::max(1e-9, a22);
    }

    // 4. Orientation Tensor
    res.orientation_tensor = compute_second_order_orientation_tensor(plies);

    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "CLT Layup (" << plies.size() << " plies, H=" << (res.total_thickness_m * 1e3) << " mm): "
       << (res.is_symmetric ? "Symmetric" : "Unsymmetric") << ", "
       << (res.is_balanced ? "Balanced" : "Unbalanced") << ". "
       << "Effective Ex=" << res.effective_ex_gpa << " GPa, Ey=" << res.effective_ey_gpa << " GPa, "
       << "Gxy=" << res.effective_gxy_gpa << " GPa, nu_xy=" << res.effective_nu_xy << ".";
    res.layup_notes.push_back(ss.str());

    std::ostringstream s_tens;
    s_tens << std::fixed << std::setprecision(3);
    s_tens << "Orientation tensor a_ij: [axx=" << res.orientation_tensor[0][0]
           << ", ayy=" << res.orientation_tensor[1][1]
           << ", axy=" << res.orientation_tensor[0][1]
           << ", Tr(a)=" << (res.orientation_tensor[0][0] + res.orientation_tensor[1][1]) << "].";
    res.layup_notes.push_back(s_tens.str());

    return res;
}

} // namespace shunya::mat
