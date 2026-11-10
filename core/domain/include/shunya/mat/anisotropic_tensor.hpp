#pragma once

#include "shunya/core/base/math.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/mat/tensor_validator.hpp"

namespace shunya::mat {

struct OrthotropicEngineeringConstants {
    double ex_gpa{140.0};
    double ey_gpa{10.0};
    double ez_gpa{10.0};
    double nu_xy{0.30};
    double nu_xz{0.30};
    double nu_yz{0.40};
    double g_xy_gpa{5.0};
    double g_xz_gpa{5.0};
    double g_yz_gpa{3.5};
};

/**
 * @brief 6x6 symmetric anisotropic elasticity stiffness/compliance tensor with Bond 3D rotation.
 */
class AnisotropicElasticityTensor {
public:
    AnisotropicElasticityTensor() = default;
    explicit AnisotropicElasticityTensor(Matrix6x6 c_gpa) : c_gpa_(c_gpa) {}

    [[nodiscard]] static core::base::Result<AnisotropicElasticityTensor> from_orthotropic(
        const OrthotropicEngineeringConstants& constants);

    [[nodiscard]] static core::base::Result<AnisotropicElasticityTensor> from_transversely_isotropic(
        double e_axial_gpa, double e_transverse_gpa, double nu_axial, double nu_transverse, double g_axial_gpa);

    [[nodiscard]] static AnisotropicElasticityTensor from_isotropic(double youngs_gpa, double poisson);

    [[nodiscard]] const Matrix6x6& stiffness_gpa() const noexcept { return c_gpa_; }
    [[nodiscard]] core::base::Result<Matrix6x6> compliance_inv_gpa() const;

    /**
     * @brief Evaluates 6D Voigt stress [MPa/GPa] = C * strain (engineering gamma_yz, gamma_xz, gamma_xy).
     */
    [[nodiscard]] std::array<double, 6> multiply_strain(const std::array<double, 6>& voigt_strain) const noexcept;

    /**
     * @brief Transforms the 6x6 stiffness matrix into rotated material frame via 3D Bond matrix M(R).
     */
    [[nodiscard]] AnisotropicElasticityTensor rotated_by(
        const core::base::math::Quaternion<double>& orientation) const noexcept;

    [[nodiscard]] AnisotropicElasticityTensor rotated_about_z(double angle_rad) const noexcept;

    [[nodiscard]] StiffnessStabilityResult analyze_stability() const;
    [[nodiscard]] std::array<double, 6> kelvin_eigenvalues_gpa() const noexcept;

private:
    Matrix6x6 c_gpa_{};
};

} // namespace shunya::mat
