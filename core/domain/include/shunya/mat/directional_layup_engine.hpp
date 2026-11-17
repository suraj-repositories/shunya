#pragma once

#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace shunya::mat {

struct LaminatePly {
    std::string name{"Carbon-Epoxy"};
    double angle_deg{0.0};
    double thickness_m{0.000125}; // 0.125 mm typical prepreg ply
    double e1_gpa{140.0};         // Longitudinal Young's modulus
    double e2_gpa{10.0};          // Transverse Young's modulus
    double nu12{0.30};            // Major Poisson's ratio
    double g12_gpa{5.0};          // In-plane shear modulus
};

struct LaminateAbdResult {
    double total_thickness_m{0.0};
    bool is_symmetric{false};
    bool is_balanced{false};

    // Classical Laminate Theory (CLT) matrices (indices 0=x, 1=y, 2=xy/s)
    std::array<std::array<double, 3>, 3> a_matrix_gpa_m{}; // In-plane extensional stiffness [GPa * m = GN/m]
    std::array<std::array<double, 3>, 3> b_matrix_gpa_m2{};// Extension-bending coupling stiffness [GPa * m^2]
    std::array<std::array<double, 3>, 3> d_matrix_gpa_m3{};// Bending stiffness [GPa * m^3]

    // Equivalent homogenized engineering constants
    double effective_ex_gpa{0.0};
    double effective_ey_gpa{0.0};
    double effective_gxy_gpa{0.0};
    double effective_nu_xy{0.0};

    // 2nd-order fiber/ply orientation tensor a_ij (Advani-Tucker orientation tensor with Tr(a) = 1.0)
    std::array<std::array<double, 3>, 3> orientation_tensor{};
    std::vector<std::string> layup_notes;
};

/**
 * @brief Classical Laminate Theory (CLT) engine for multi-axial directional layup modeling
 *        and anisotropic 2nd-order orientation tensors for layered and composite materials.
 * (Roadmap task PH4-03.01.006)
 */
class DirectionalLayupEngine {
public:
    static constexpr double DEG_TO_RAD = 3.14159265358979323846 / 180.0;

    /**
     * @brief Computes 3x3 CLT extensional (A), coupling (B), and bending (D) stiffness matrices
     *        along with effective in-plane engineering constants.
     */
    [[nodiscard]] static LaminateAbdResult evaluate_laminate(
        const std::vector<LaminatePly>& plies);

    /**
     * @brief Computes 2nd-order Advani-Tucker orientation tensor a_ij = sum(w_k * n_i * n_j).
     *        Guarantees Tr(a) = 1.0 and symmetry a_ij = a_ji.
     */
    [[nodiscard]] static std::array<std::array<double, 3>, 3> compute_second_order_orientation_tensor(
        const std::vector<LaminatePly>& plies) noexcept;
};

} // namespace shunya::mat
