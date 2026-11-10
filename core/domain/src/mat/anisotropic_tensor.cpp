#include "shunya/mat/anisotropic_tensor.hpp"

#include <algorithm>
#include <cmath>

namespace shunya::mat {

namespace {

core::base::Error make_tensor_error(std::string msg) {
    return core::base::Error{
        .code = 4245,
        .domain = core::base::ErrorDomain::Materials,
        .severity = core::base::Severity::Error,
        .message = std::move(msg),
        .details = ""
    };
}

bool invert_6x6(const Matrix6x6& a_in, Matrix6x6& a_out) noexcept {
    double aug[6][12]{};
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) aug[i][j] = a_in[i][j];
        aug[i][6 + i] = 1.0;
    }
    for (int col = 0; col < 6; ++col) {
        int piv = col;
        double max_v = std::abs(aug[col][col]);
        for (int r = col + 1; r < 6; ++r) {
            if (std::abs(aug[r][col]) > max_v) {
                max_v = std::abs(aug[r][col]);
                piv = r;
            }
        }
        if (max_v < 1e-14) return false;
        if (piv != col) {
            for (int k = 0; k < 12; ++k) std::swap(aug[col][k], aug[piv][k]);
        }
        const double diag = aug[col][col];
        for (int k = 0; k < 12; ++k) aug[col][k] /= diag;
        for (int r = 0; r < 6; ++r) {
            if (r == col) continue;
            const double f = aug[r][col];
            for (int k = 0; k < 12; ++k) aug[r][k] -= f * aug[col][k];
        }
    }
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) a_out[i][j] = aug[i][6 + j];
    }
    return true;
}

std::array<double, 6> jacobi_eigenvalues_6x6(Matrix6x6 a) noexcept {
    for (int iter = 0; iter < 80; ++iter) {
        int p = 0, q = 1;
        double max_off = 0.0;
        for (int i = 0; i < 6; ++i) {
            for (int j = i + 1; j < 6; ++j) {
                if (std::abs(a[i][j]) > max_off) {
                    max_off = std::abs(a[i][j]);
                    p = i;
                    q = j;
                }
            }
        }
        if (max_off < 1e-12) break;
        const double diff = a[q][q] - a[p][p];
        const double phi = (std::abs(diff) < 1e-15)
                               ? 0.7853981633974483
                               : 0.5 * std::atan2(2.0 * a[p][q], diff);
        const double c = std::cos(phi);
        const double s = std::sin(phi);

        const double app = a[p][p];
        const double aqq = a[q][q];
        const double apq = a[p][q];
        a[p][p] = c * c * app - 2.0 * s * c * apq + s * s * aqq;
        a[q][q] = s * s * app + 2.0 * s * c * apq + c * c * aqq;
        a[p][q] = a[q][p] = 0.0;
        for (int r = 0; r < 6; ++r) {
            if (r == p || r == q) continue;
            const double arp = a[r][p];
            const double arq = a[r][q];
            a[r][p] = a[p][r] = c * arp - s * arq;
            a[r][q] = a[q][r] = s * arp + c * arq;
        }
    }
    std::array<double, 6> evals{};
    for (int i = 0; i < 6; ++i) evals[static_cast<size_t>(i)] = a[i][i];
    std::sort(evals.begin(), evals.end());
    return evals;
}

Matrix6x6 bond_stress_matrix(const double l[3][3]) noexcept {
    // 6x6 Bond stress transformation matrix M for Voigt ordering: (11, 22, 33, 23, 13, 12)
    Matrix6x6 m{};
    auto sq = [](double x) { return x * x; };

    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            m[r][c] = sq(l[r][c]);
        }
        m[r][3] = 2.0 * l[r][1] * l[r][2];
        m[r][4] = 2.0 * l[r][2] * l[r][0];
        m[r][5] = 2.0 * l[r][0] * l[r][1];
    }

    const int pairs[3][2] = {{1, 2}, {2, 0}, {0, 1}};
    for (int p = 0; p < 3; ++p) {
        const int i = pairs[p][0];
        const int j = pairs[p][1];
        const int row = 3 + p;
        for (int c = 0; c < 3; ++c) {
            m[row][c] = l[i][c] * l[j][c];
        }
        m[row][3] = l[i][1] * l[j][2] + l[i][2] * l[j][1];
        m[row][4] = l[i][2] * l[j][0] + l[i][0] * l[j][2];
        m[row][5] = l[i][0] * l[j][1] + l[i][1] * l[j][0];
    }
    return m;
}

} // namespace

StiffnessStabilityResult TensorValidator::analyze_stability(const Matrix6x6& c_gpa) {
    StiffnessStabilityResult res;
    for (int i = 0; i < 6; ++i) {
        for (int j = i + 1; j < 6; ++j) {
            if (std::abs(c_gpa[i][j] - c_gpa[j][i]) > 1e-6) {
                res.is_symmetric = false;
            }
        }
    }
    res.eigenvalues_gpa = jacobi_eigenvalues_6x6(c_gpa);
    res.min_eigenvalue_gpa = res.eigenvalues_gpa[0];
    res.is_positive_definite = res.is_symmetric && (res.min_eigenvalue_gpa > 1e-9);
    return res;
}

core::base::ValidationReport TensorValidator::validate_stiffness(const Matrix6x6& c_gpa) {
    core::base::ValidationReport report;
    const auto st = analyze_stability(c_gpa);
    if (!st.is_symmetric) {
        report.add_error("StiffnessTensor", "Elasticity stiffness matrix is not symmetric (C_ij != C_ji)");
    }
    if (!st.is_positive_definite) {
        report.add_error("StiffnessTensor",
                         "Thermodynamically unstable stiffness matrix (min eigenvalue = " +
                         std::to_string(st.min_eigenvalue_gpa) + " GPa <= 0)");
    }
    return report;
}

core::base::Result<AnisotropicElasticityTensor> AnisotropicElasticityTensor::from_orthotropic(
    const OrthotropicEngineeringConstants& k) {
    if (k.ex_gpa <= 0.0 || k.ey_gpa <= 0.0 || k.ez_gpa <= 0.0 ||
        k.g_xy_gpa <= 0.0 || k.g_xz_gpa <= 0.0 || k.g_yz_gpa <= 0.0) {
        return make_tensor_error("All Young's and shear moduli must be strictly positive");
    }

    Matrix6x6 s{};
    s[0][0] = 1.0 / k.ex_gpa;
    s[1][1] = 1.0 / k.ey_gpa;
    s[2][2] = 1.0 / k.ez_gpa;
    s[0][1] = s[1][0] = -k.nu_xy / k.ex_gpa;
    s[0][2] = s[2][0] = -k.nu_xz / k.ex_gpa;
    s[1][2] = s[2][1] = -k.nu_yz / k.ey_gpa;
    s[3][3] = 1.0 / k.g_yz_gpa;
    s[4][4] = 1.0 / k.g_xz_gpa;
    s[5][5] = 1.0 / k.g_xy_gpa;

    Matrix6x6 c{};
    if (!invert_6x6(s, c)) {
        return make_tensor_error("Singular orthotropic compliance matrix");
    }
    const auto stab = TensorValidator::analyze_stability(c);
    if (!stab.is_positive_definite) {
        return make_tensor_error("Invalid orthotropic Poisson ratios violate positive definiteness");
    }
    return AnisotropicElasticityTensor{c};
}

core::base::Result<AnisotropicElasticityTensor> AnisotropicElasticityTensor::from_transversely_isotropic(
    double e_axial_gpa, double e_transverse_gpa, double nu_axial, double nu_transverse, double g_axial_gpa) {
    if (e_transverse_gpa <= 0.0 || (1.0 + nu_transverse) <= 0.0) {
        return make_tensor_error("Invalid transverse Young's modulus or Poisson ratio");
    }
    const double g_trans = e_transverse_gpa / (2.0 * (1.0 + nu_transverse));
    return from_orthotropic(OrthotropicEngineeringConstants{
        e_axial_gpa, e_transverse_gpa, e_transverse_gpa,
        nu_axial, nu_axial, nu_transverse,
        g_axial_gpa, g_axial_gpa, g_trans
    });
}

AnisotropicElasticityTensor AnisotropicElasticityTensor::from_isotropic(double youngs_gpa, double poisson) {
    const double g = youngs_gpa / (2.0 * (1.0 + poisson));
    return from_orthotropic({youngs_gpa, youngs_gpa, youngs_gpa, poisson, poisson, poisson, g, g, g}).value();
}

core::base::Result<Matrix6x6> AnisotropicElasticityTensor::compliance_inv_gpa() const {
    Matrix6x6 s{};
    if (!invert_6x6(c_gpa_, s)) {
        return make_tensor_error("Stiffness matrix is singular");
    }
    return s;
}

std::array<double, 6> AnisotropicElasticityTensor::multiply_strain(
    const std::array<double, 6>& voigt_strain) const noexcept {
    std::array<double, 6> stress{};
    for (int i = 0; i < 6; ++i) {
        double sum = 0.0;
        for (int j = 0; j < 6; ++j) sum += c_gpa_[i][j] * voigt_strain[static_cast<size_t>(j)];
        stress[static_cast<size_t>(i)] = sum;
    }
    return stress;
}

AnisotropicElasticityTensor AnisotropicElasticityTensor::rotated_by(
    const core::base::math::Quaternion<double>& q) const noexcept {
    // Active/frame rotation matrix from quaternion q
    const auto qn = q.normalized();
    const double w = qn.w, x = qn.x, y = qn.y, z = qn.z;
    const double l[3][3] = {
        {1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w),       2.0 * (x * z + y * w)},
        {2.0 * (x * y + z * w),       1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - x * w)},
        {2.0 * (x * z - y * w),       2.0 * (y * z + x * w),       1.0 - 2.0 * (x * x + y * y)}
    };
    const Matrix6x6 m = bond_stress_matrix(l);

    // C' = M * C * M^T
    Matrix6x6 tmp{};
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            double s = 0.0;
            for (int k = 0; k < 6; ++k) s += m[i][k] * c_gpa_[k][j];
            tmp[i][j] = s;
        }
    }
    Matrix6x6 out{};
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            double s = 0.0;
            for (int k = 0; k < 6; ++k) s += tmp[i][k] * m[j][k];
            out[i][j] = s;
        }
    }
    return AnisotropicElasticityTensor{out};
}

AnisotropicElasticityTensor AnisotropicElasticityTensor::rotated_about_z(double angle_rad) const noexcept {
    const auto q = core::base::math::Quaternion<double>::from_axis_angle({0.0, 0.0, 1.0}, angle_rad);
    return rotated_by(q);
}

StiffnessStabilityResult AnisotropicElasticityTensor::analyze_stability() const {
    return TensorValidator::analyze_stability(c_gpa_);
}

std::array<double, 6> AnisotropicElasticityTensor::kelvin_eigenvalues_gpa() const noexcept {
    // Kelvin/Mandel form C_M = W * C_voigt * W with W = diag(1, 1, 1, sqrt(2), sqrt(2), sqrt(2))
    constexpr double kSqrt2 = 1.4142135623730950488;
    const double w[6] = {1.0, 1.0, 1.0, kSqrt2, kSqrt2, kSqrt2};
    Matrix6x6 m{};
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            m[i][j] = w[i] * c_gpa_[i][j] * w[j];
        }
    }
    return jacobi_eigenvalues_6x6(m);
}

} // namespace shunya::mat
