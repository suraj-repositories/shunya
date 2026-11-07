#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/domain/sparse_matrix.hpp"

#include <chrono>
#include <cmath>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace shunya::core::domain {

// =============================================================================
// SOL-01: Problem Types & Common Configuration
// =============================================================================

enum class ProblemType {
    Linear,
    Nonlinear,
    ODE,
    DAE,
    PDE,
    DiscreteEvent
};

[[nodiscard]] constexpr std::string_view problem_type_to_string(ProblemType type) noexcept {
    switch (type) {
        case ProblemType::Linear: return "Linear";
        case ProblemType::Nonlinear: return "Nonlinear";
        case ProblemType::ODE: return "ODE";
        case ProblemType::DAE: return "DAE";
        case ProblemType::PDE: return "PDE";
        case ProblemType::DiscreteEvent: return "DiscreteEvent";
    }
    return "Linear";
}

[[nodiscard]] std::optional<ProblemType> problem_type_from_string(std::string_view str) noexcept;

struct SolverConfig {
    double relative_tolerance{1e-6};
    double absolute_tolerance{1e-8};
    size_t max_iterations{1000};
    double damping_factor{1.0};      // For nonlinear line search / damping
    double initial_step_size{0.01};   // For ODE / transient stepping
    double min_step_size{1e-7};
    double max_step_size{1.0};
    bool use_preconditioning{true};
    bool enable_line_search{true};

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<SolverConfig> from_json(std::string_view json_str);
};

// =============================================================================
// SOL-09: Solver Diagnostics
// =============================================================================

struct SolverDiagnostics {
    size_t total_iterations{0};
    size_t step_count{0};
    size_t accepted_steps{0};
    size_t rejected_steps{0};
    double initial_residual{0.0};
    double final_residual{0.0};
    double residual_reduction_ratio{0.0};
    double matrix_condition_estimate{1.0};
    double assembly_time_ms{0.0};
    double solve_time_ms{0.0};
    double total_runtime_ms{0.0};
    size_t peak_memory_bytes{0};
    bool stagnation_detected{false};

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<SolverDiagnostics> from_json(std::string_view json_str);
    void reset() noexcept;
};

// =============================================================================
// SOL-03: Preconditioner Interface & Implementations
// =============================================================================

class IPreconditioner {
public:
    virtual ~IPreconditioner() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    virtual base::Result<void> setup(const SparseMatrixCSR& A) = 0;
    virtual void apply(std::span<const double> r, std::span<double> z) const noexcept = 0;
};

class IdentityPreconditioner : public IPreconditioner {
public:
    [[nodiscard]] std::string name() const override { return "Identity"; }
    base::Result<void> setup(const SparseMatrixCSR&) override { return {}; }
    void apply(std::span<const double> r, std::span<double> z) const noexcept override;
};

class JacobiPreconditioner : public IPreconditioner {
public:
    [[nodiscard]] std::string name() const override { return "Jacobi"; }
    base::Result<void> setup(const SparseMatrixCSR& A) override;
    void apply(std::span<const double> r, std::span<double> z) const noexcept override;

private:
    std::vector<double> inv_diag_;
};

class ILU0Preconditioner : public IPreconditioner {
public:
    [[nodiscard]] std::string name() const override { return "ILU0"; }
    base::Result<void> setup(const SparseMatrixCSR& A) override;
    void apply(std::span<const double> r, std::span<double> z) const noexcept override;

private:
    SparseMatrixCSR lu_factored_;
};

// =============================================================================
// SOL-03: Linear Solvers (Ax = b)
// =============================================================================

struct LinearSolveReport {
    bool converged{false};
    size_t iterations{0};
    double initial_residual{0.0};
    double final_residual{0.0};
    double relative_residual{0.0};
    double condition_number_estimate{1.0};
    std::string message;
};

class ILinearSolver {
public:
    virtual ~ILinearSolver() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    virtual void set_preconditioner(std::shared_ptr<IPreconditioner> precond) { precond_ = std::move(precond); }
    [[nodiscard]] virtual base::Result<LinearSolveReport> solve(
        const SparseMatrixCSR& A,
        std::span<const double> b,
        std::span<double> x,
        const SolverConfig& config
    ) = 0;

protected:
    std::shared_ptr<IPreconditioner> precond_{nullptr};
};

class ConjugateGradientSolver : public ILinearSolver {
public:
    [[nodiscard]] std::string name() const override { return "ConjugateGradient"; }
    [[nodiscard]] base::Result<LinearSolveReport> solve(
        const SparseMatrixCSR& A,
        std::span<const double> b,
        std::span<double> x,
        const SolverConfig& config
    ) override;
};

class BiCGSTABSolver : public ILinearSolver {
public:
    [[nodiscard]] std::string name() const override { return "BiCGSTAB"; }
    [[nodiscard]] base::Result<LinearSolveReport> solve(
        const SparseMatrixCSR& A,
        std::span<const double> b,
        std::span<double> x,
        const SolverConfig& config
    ) override;
};

class DirectLUSolver : public ILinearSolver {
public:
    [[nodiscard]] std::string name() const override { return "DirectLU"; }
    [[nodiscard]] base::Result<LinearSolveReport> solve(
        const SparseMatrixCSR& A,
        std::span<const double> b,
        std::span<double> x,
        const SolverConfig& config
    ) override;
};

// =============================================================================
// SOL-04: Nonlinear Solver Interface (F(x) = 0)
// =============================================================================

using NonlinearResidualFn = std::function<std::vector<double>(std::span<const double> x)>;
using NonlinearJacobianFn = std::function<SparseMatrixCSR(std::span<const double> x)>;

struct NonlinearSolveReport {
    bool converged{false};
    size_t iterations{0};
    double initial_residual_norm{0.0};
    double final_residual_norm{0.0};
    std::vector<double> residual_history;
    bool stagnation_detected{false};
    std::string message;
};

class INonlinearSolver {
public:
    virtual ~INonlinearSolver() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual base::Result<NonlinearSolveReport> solve(
        NonlinearResidualFn residual_fn,
        std::optional<NonlinearJacobianFn> jacobian_fn,
        std::span<double> x,
        const SolverConfig& config
    ) = 0;
};

class NewtonRaphsonSolver : public INonlinearSolver {
public:
    explicit NewtonRaphsonSolver(std::shared_ptr<ILinearSolver> linear_solver = nullptr);
    [[nodiscard]] std::string name() const override { return "NewtonRaphson"; }

    [[nodiscard]] base::Result<NonlinearSolveReport> solve(
        NonlinearResidualFn residual_fn,
        std::optional<NonlinearJacobianFn> jacobian_fn,
        std::span<double> x,
        const SolverConfig& config
    ) override;

private:
    std::shared_ptr<ILinearSolver> linear_solver_;
};

// =============================================================================
// SOL-05: ODE Integration Methods
// =============================================================================

using ODERhsFn = std::function<std::vector<double>(double t, std::span<const double> y)>;

struct ODEStepResult {
    bool step_accepted{true};
    double next_dt{0.0};
    double estimated_error{0.0};
    std::string message;
};

class IODESolver {
public:
    virtual ~IODESolver() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual int order() const = 0;
    [[nodiscard]] virtual bool is_adaptive() const = 0;
    [[nodiscard]] virtual bool is_stiff_stable() const = 0;

    [[nodiscard]] virtual base::Result<ODEStepResult> step(
        ODERhsFn rhs,
        double t,
        double dt,
        std::span<double> y,
        const SolverConfig& config
    ) = 0;
};

class ExplicitEulerSolver : public IODESolver {
public:
    [[nodiscard]] std::string name() const override { return "ExplicitEuler"; }
    [[nodiscard]] int order() const override { return 1; }
    [[nodiscard]] bool is_adaptive() const override { return false; }
    [[nodiscard]] bool is_stiff_stable() const override { return false; }

    [[nodiscard]] base::Result<ODEStepResult> step(
        ODERhsFn rhs, double t, double dt, std::span<double> y, const SolverConfig& config
    ) override;
};

class RungeKutta4Solver : public IODESolver {
public:
    [[nodiscard]] std::string name() const override { return "RungeKutta4"; }
    [[nodiscard]] int order() const override { return 4; }
    [[nodiscard]] bool is_adaptive() const override { return false; }
    [[nodiscard]] bool is_stiff_stable() const override { return false; }

    [[nodiscard]] base::Result<ODEStepResult> step(
        ODERhsFn rhs, double t, double dt, std::span<double> y, const SolverConfig& config
    ) override;
};

class AdaptiveRK45Solver : public IODESolver {
public:
    [[nodiscard]] std::string name() const override { return "AdaptiveRK45"; }
    [[nodiscard]] int order() const override { return 5; }
    [[nodiscard]] bool is_adaptive() const override { return true; }
    [[nodiscard]] bool is_stiff_stable() const override { return false; }

    [[nodiscard]] base::Result<ODEStepResult> step(
        ODERhsFn rhs, double t, double dt, std::span<double> y, const SolverConfig& config
    ) override;
};

class ImplicitEulerSolver : public IODESolver {
public:
    explicit ImplicitEulerSolver(std::shared_ptr<INonlinearSolver> nonlinear_solver = nullptr);
    [[nodiscard]] std::string name() const override { return "ImplicitEuler"; }
    [[nodiscard]] int order() const override { return 1; }
    [[nodiscard]] bool is_adaptive() const override { return false; }
    [[nodiscard]] bool is_stiff_stable() const override { return true; }

    [[nodiscard]] base::Result<ODEStepResult> step(
        ODERhsFn rhs, double t, double dt, std::span<double> y, const SolverConfig& config
    ) override;

private:
    std::shared_ptr<INonlinearSolver> nonlinear_solver_;
};

// =============================================================================
// SOL-06: Mesh and Discretization Abstractions for PDE Domains
// =============================================================================

enum class ElementType {
    Line1D,
    Triangle2D,
    Quad2D,
    Tetrahedron3D
};

struct MeshNode {
    size_t id{0};
    double x{0.0};
    double y{0.0};
    double z{0.0};
    bool is_boundary{false};
};

struct MeshElement {
    size_t id{0};
    ElementType type{ElementType::Line1D};
    std::vector<size_t> node_ids;
};

enum class BoundaryConditionType {
    Dirichlet,
    Neumann,
    Robin
};

struct BoundaryCondition {
    size_t node_id{0};
    BoundaryConditionType type{BoundaryConditionType::Dirichlet};
    double value{0.0}; // g for Dirichlet, h for Neumann
    double alpha{1.0}; // Robin parameter: alpha * u + beta * du/dn = value
    double beta{0.0};
};

class DiscretizationMesh {
public:
    DiscretizationMesh() = default;

    static DiscretizationMesh generate_1d_uniform(double x0, double x1, size_t num_cells);
    static DiscretizationMesh generate_2d_uniform_grid(
        double x0, double x1, double y0, double y1, size_t nx, size_t ny
    );

    [[nodiscard]] const std::vector<MeshNode>& nodes() const noexcept { return nodes_; }
    [[nodiscard]] const std::vector<MeshElement>& elements() const noexcept { return elements_; }
    [[nodiscard]] size_t node_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] size_t element_count() const noexcept { return elements_.size(); }

    void add_boundary_condition(BoundaryCondition bc);
    [[nodiscard]] const std::vector<BoundaryCondition>& boundary_conditions() const noexcept {
        return boundary_conditions_;
    }

    // Assemble 1D Poisson system: -u'' = f with Dirichlet boundaries
    [[nodiscard]] static std::pair<SparseMatrixCSR, std::vector<double>> assemble_1d_poisson(
        size_t n, double L, double left_val, double right_val, std::function<double(double)> source = nullptr
    );

private:
    std::vector<MeshNode> nodes_;
    std::vector<MeshElement> elements_;
    std::vector<BoundaryCondition> boundary_conditions_;
};

// =============================================================================
// SOL-07: Solver Capability Registry & Compatibility Checking
// =============================================================================

struct SolverDescriptor {
    std::string name;
    base::Version version{1, 0, 0, "final"};
    std::vector<ProblemType> supported_problems;
    bool supports_adaptive_stepping{false};
    bool supports_sparse_matrices{false};
    bool is_stiff_stable{false};
    int accuracy_order{1};
};

class SolverRegistry {
public:
    static SolverRegistry& instance();

    void register_solver(SolverDescriptor descriptor);
    [[nodiscard]] std::vector<SolverDescriptor> get_available_solvers(ProblemType problem) const;
    [[nodiscard]] std::optional<SolverDescriptor> find_best_solver(
        ProblemType problem, bool stiff_required = false, bool adaptive_required = false
    ) const;
    [[nodiscard]] base::Result<void> validate_compatibility(
        const SolverDescriptor& solver, ProblemType problem, bool stiff, size_t dim
    ) const;

    void clear() noexcept;

private:
    SolverRegistry();
    std::vector<SolverDescriptor> solvers_;
};

// =============================================================================
// SOL-08: Numerical Benchmark Suite
// =============================================================================

struct BenchmarkResult {
    std::string benchmark_name;
    double l2_error{0.0};
    double linf_error{0.0};
    bool tolerance_passed{false};
    double runtime_us{0.0};
    std::string message;
};

class NumericalBenchmarkRunner {
public:
    [[nodiscard]] static BenchmarkResult run_harmonic_oscillator_benchmark(
        double omega, double t_end, double dt, IODESolver& solver, double tol = 1e-3
    );

    [[nodiscard]] static BenchmarkResult run_stiff_decay_benchmark(
        double lambda, double t_end, double dt, IODESolver& solver, double tol = 1e-3
    );

    [[nodiscard]] static BenchmarkResult run_nonlinear_root_benchmark(
        INonlinearSolver& solver, double tol = 1e-6
    );

    [[nodiscard]] static BenchmarkResult run_poisson_1d_benchmark(
        size_t n, ILinearSolver& solver, double tol = 1e-4
    );

    [[nodiscard]] static std::vector<BenchmarkResult> run_all_reference_benchmarks();
};

} // namespace shunya::core::domain
