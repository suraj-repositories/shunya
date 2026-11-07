#include "tests/test_framework.hpp"

#include "shunya/core/domain/sparse_matrix.hpp"
#include "shunya/core/domain/numerical_solver.hpp"

#include <cmath>
#include <memory>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

TEST_CASE("SOL-01: Problem Types and SolverConfig Serialization") {
    // Problem type string conversions
    CHECK(problem_type_to_string(ProblemType::Linear) == "Linear");
    CHECK(problem_type_to_string(ProblemType::Nonlinear) == "Nonlinear");
    CHECK(problem_type_to_string(ProblemType::ODE) == "ODE");
    CHECK(problem_type_to_string(ProblemType::DAE) == "DAE");
    CHECK(problem_type_to_string(ProblemType::PDE) == "PDE");
    CHECK(problem_type_to_string(ProblemType::DiscreteEvent) == "DiscreteEvent");

    CHECK(problem_type_from_string("Linear").value() == ProblemType::Linear);
    CHECK(problem_type_from_string("ODE").value() == ProblemType::ODE);
    CHECK_FALSE(problem_type_from_string("UnknownType").has_value());

    // SolverConfig JSON roundtrip
    SolverConfig original;
    original.relative_tolerance = 1e-7;
    original.absolute_tolerance = 1e-9;
    original.max_iterations = 2500;
    original.damping_factor = 0.8;
    original.initial_step_size = 0.005;
    original.min_step_size = 1e-6;
    original.max_step_size = 0.5;
    original.use_preconditioning = true;
    original.enable_line_search = true;

    std::string json = original.to_json();
    auto restored_res = SolverConfig::from_json(json);
    REQUIRE(restored_res.is_ok());

    const auto& restored = restored_res.value();
    CHECK(std::abs(restored.relative_tolerance - 1e-7) < 1e-12);
    CHECK(std::abs(restored.absolute_tolerance - 1e-9) < 1e-12);
    CHECK(restored.max_iterations == 2500);
    CHECK(std::abs(restored.damping_factor - 0.8) < 1e-9);
    CHECK(std::abs(restored.initial_step_size - 0.005) < 1e-9);
    CHECK(restored.use_preconditioning);
    CHECK(restored.enable_line_search);
}

TEST_CASE("SOL-02: Sparse Matrix CSR Storage, Operations and Vector Helpers") {
    // Vector operations
    std::vector<double> u = {1.0, 2.0, 3.0};
    std::vector<double> v = {4.0, -5.0, 6.0};

    CHECK(std::abs(vector_ops::dot(u, v) - 12.0) < 1e-9); // 4 - 10 + 18 = 12
    CHECK(std::abs(vector_ops::norm_2(u) - std::sqrt(14.0)) < 1e-9);
    CHECK(std::abs(vector_ops::norm_inf(v) - 6.0) < 1e-9);

    std::vector<double> y = {1.0, 1.0, 1.0};
    vector_ops::axpy(2.0, u, y); // y = 2*u + y = {3, 5, 7}
    CHECK(std::abs(y[0] - 3.0) < 1e-9);
    CHECK(std::abs(y[1] - 5.0) < 1e-9);
    CHECK(std::abs(y[2] - 7.0) < 1e-9);

    // Sparse Matrix CSR assembly from triplets
    // 3x3 matrix:
    // [ 4  1  0 ]
    // [ 1  4  1 ]
    // [ 0  1  4 ]
    std::vector<Triplet> triplets = {
        {0, 0, 4.0}, {0, 1, 1.0},
        {1, 0, 1.0}, {1, 1, 4.0}, {1, 2, 1.0},
        {2, 1, 1.0}, {2, 2, 4.0}
    };

    auto A = SparseMatrixCSR::from_triplets(3, 3, triplets);
    CHECK(A.rows() == 3);
    CHECK(A.cols() == 3);
    CHECK(A.non_zeros() == 7);
    CHECK(A.coeff(0, 0) == 4.0);
    CHECK(A.coeff(0, 1) == 1.0);
    CHECK(A.coeff(0, 2) == 0.0);
    CHECK(A.coeff(1, 1) == 4.0);
    CHECK(A.coeff(2, 2) == 4.0);

    // Sparsity ratio: 7 / 9 non-zeros -> 1 - 7/9 = 2/9
    CHECK(std::abs(A.sparsity_ratio() - (2.0 / 9.0)) < 1e-6);

    // Matrix-vector multiplication: A * [1, 2, 3]^T = [6, 12, 14]^T
    std::vector<double> x = {1.0, 2.0, 3.0};
    auto Ax = A.multiply(x);
    REQUIRE(Ax.size() == 3);
    CHECK(std::abs(Ax[0] - 6.0) < 1e-9);
    CHECK(std::abs(Ax[1] - 12.0) < 1e-9);
    CHECK(std::abs(Ax[2] - 14.0) < 1e-9);

    // Transpose and Norms
    auto At = A.transpose();
    CHECK(At.coeff(1, 0) == 1.0);
    CHECK(A.trace() == 12.0); // 4 + 4 + 4
    CHECK(A.frobenius_norm() > 0.0);

    // ILU(0) Factorization and Triangular Solve
    auto ilu_res = A.compute_ilu0();
    REQUIRE(ilu_res.is_ok());
    const auto& lu = ilu_res.value();
    std::vector<double> b = {6.0, 12.0, 14.0};
    std::vector<double> sol(3, 0.0);
    lu.solve_lu(b, sol);
    // Since ILU0 is approximate for general matrices, check residual is small
    auto A_sol = A.multiply(sol);
    CHECK(std::abs(A_sol[0] - 6.0) < 0.5);
}

TEST_CASE("SOL-03: Linear Solvers and Preconditioners") {
    // 4x4 Symmetric Positive Definite tridiagonal system:
    // [ 4 -1  0  0 ]   [ x0 ]   [  3 ]
    // [-1  4 -1  0 ] * [ x1 ] = [  2 ]
    // [ 0 -1  4 -1 ]   [ x2 ]   [  2 ]
    // [ 0  0 -1  4 ]   [ x3 ]   [  3 ]
    // Exact solution: x = [1, 1, 1, 1]
    std::vector<Triplet> triplets = {
        {0, 0, 4.0}, {0, 1, -1.0},
        {1, 0, -1.0}, {1, 1, 4.0}, {1, 2, -1.0},
        {2, 1, -1.0}, {2, 2, 4.0}, {2, 3, -1.0},
        {3, 2, -1.0}, {3, 3, 4.0}
    };
    auto A = SparseMatrixCSR::from_triplets(4, 4, triplets);
    std::vector<double> b = {3.0, 2.0, 2.0, 3.0};

    SolverConfig cfg;
    cfg.relative_tolerance = 1e-9;
    cfg.absolute_tolerance = 1e-12;
    cfg.max_iterations = 100;

    // 1. Direct LU Solver
    {
        DirectLUSolver direct_solver;
        std::vector<double> x(4, 0.0);
        auto res = direct_solver.solve(A, b, x, cfg);
        REQUIRE(res.is_ok());
        CHECK(res.value().converged);
        CHECK(std::abs(x[0] - 1.0) < 1e-9);
        CHECK(std::abs(x[1] - 1.0) < 1e-9);
        CHECK(std::abs(x[2] - 1.0) < 1e-9);
        CHECK(std::abs(x[3] - 1.0) < 1e-9);
    }

    // 2. Conjugate Gradient Solver with Jacobi Preconditioner
    {
        ConjugateGradientSolver cg_solver;
        auto jacobi = std::make_shared<JacobiPreconditioner>();
        REQUIRE(jacobi->setup(A).is_ok());
        cg_solver.set_preconditioner(jacobi);

        std::vector<double> x(4, 0.0);
        auto res = cg_solver.solve(A, b, x, cfg);
        REQUIRE(res.is_ok());
        CHECK(res.value().converged);
        CHECK(std::abs(x[0] - 1.0) < 1e-6);
        CHECK(std::abs(x[1] - 1.0) < 1e-6);
        CHECK(std::abs(x[2] - 1.0) < 1e-6);
        CHECK(std::abs(x[3] - 1.0) < 1e-6);
        CHECK(res.value().iterations <= 4);
    }

    // 3. BiCGSTAB Solver with ILU0 Preconditioner
    {
        BiCGSTABSolver bicgstab;
        auto ilu0 = std::make_shared<ILU0Preconditioner>();
        REQUIRE(ilu0->setup(A).is_ok());
        bicgstab.set_preconditioner(ilu0);

        std::vector<double> x(4, 0.0);
        auto res = bicgstab.solve(A, b, x, cfg);
        REQUIRE(res.is_ok());
        CHECK(res.value().converged);
        CHECK(std::abs(x[0] - 1.0) < 1e-6);
        CHECK(std::abs(x[1] - 1.0) < 1e-6);
        CHECK(std::abs(x[2] - 1.0) < 1e-6);
        CHECK(std::abs(x[3] - 1.0) < 1e-6);
    }
}

TEST_CASE("SOL-04: Nonlinear Solver with Line Search and Residual Monitoring") {
    // Coupled nonlinear system:
    // F0(x, y) = x^2 + y^2 - 4 = 0   (circle radius 2)
    // F1(x, y) = x - y - 0 = 0       (line x = y)
    // Positive root: x = sqrt(2) ~ 1.41421356, y = sqrt(2)
    NewtonRaphsonSolver solver;

    auto F = [](std::span<const double> v) -> std::vector<double> {
        return {
            v[0] * v[0] + v[1] * v[1] - 4.0,
            v[0] - v[1]
        };
    };

    std::vector<double> x = {2.0, 1.0}; // Initial guess
    SolverConfig cfg;
    cfg.absolute_tolerance = 1e-9;
    cfg.max_iterations = 50;
    cfg.enable_line_search = true;

    auto res = solver.solve(F, std::nullopt, x, cfg);
    REQUIRE(res.is_ok());
    CHECK(res.value().converged);
    CHECK(std::abs(x[0] - std::sqrt(2.0)) < 1e-8);
    CHECK(std::abs(x[1] - std::sqrt(2.0)) < 1e-8);
    CHECK(res.value().residual_history.size() > 1);
    CHECK(res.value().final_residual_norm <= 1e-9);
}

TEST_CASE("SOL-05: ODE Integration Methods and Adaptive Error Control") {
    // Test system: dy/dt = -y, y(0) = 1. Exact: y(t) = exp(-t)
    auto rhs = [](double, std::span<const double> y) -> std::vector<double> {
        return {-y[0]};
    };

    SolverConfig cfg;
    cfg.absolute_tolerance = 1e-5;
    cfg.relative_tolerance = 1e-4;

    // 1. Explicit Euler (O(h))
    {
        ExplicitEulerSolver euler;
        std::vector<double> y = {1.0};
        auto step_res = euler.step(rhs, 0.0, 0.1, y, cfg);
        REQUIRE(step_res.is_ok());
        CHECK(std::abs(y[0] - 0.9) < 1e-9); // 1 + 0.1*(-1) = 0.9
    }

    // 2. Runge-Kutta 4 (O(h^4))
    {
        RungeKutta4Solver rk4;
        std::vector<double> y = {1.0};
        auto step_res = rk4.step(rhs, 0.0, 0.1, y, cfg);
        REQUIRE(step_res.is_ok());
        const double exact = std::exp(-0.1);
        CHECK(std::abs(y[0] - exact) < 1e-6);
    }

    // 3. Adaptive RK45 (Dormand-Prince embedded error estimation)
    {
        AdaptiveRK45Solver rk45;
        std::vector<double> y = {1.0};
        auto step_res = rk45.step(rhs, 0.0, 0.1, y, cfg);
        REQUIRE(step_res.is_ok());
        CHECK(step_res.value().step_accepted);
        CHECK(step_res.value().next_dt > 0.0);
        const double exact = std::exp(-0.1);
        CHECK(std::abs(y[0] - exact) < 1e-8);
    }

    // 4. Implicit Euler (A-stable for stiff dynamics)
    {
        ImplicitEulerSolver implicit_euler;
        std::vector<double> y = {1.0};
        auto step_res = implicit_euler.step(rhs, 0.0, 0.1, y, cfg);
        REQUIRE(step_res.is_ok());
        // y_{n+1} = y_n / (1 + dt) = 1.0 / 1.1 ~ 0.9090909
        CHECK(std::abs(y[0] - (1.0 / 1.1)) < 1e-6);
    }
}

TEST_CASE("SOL-06: Mesh and Discretization Abstractions for PDE Domains") {
    // 1D Uniform Mesh
    auto mesh1d = DiscretizationMesh::generate_1d_uniform(0.0, 1.0, 10);
    CHECK(mesh1d.node_count() == 11);
    CHECK(mesh1d.element_count() == 10);
    CHECK(mesh1d.nodes().front().is_boundary);
    CHECK(mesh1d.nodes().back().is_boundary);
    CHECK_FALSE(mesh1d.nodes()[5].is_boundary);

    // 2D Uniform Grid
    auto mesh2d = DiscretizationMesh::generate_2d_uniform_grid(0.0, 1.0, 0.0, 1.0, 5, 5);
    CHECK(mesh2d.node_count() == 36); // (5+1)*(5+1)
    CHECK(mesh2d.element_count() == 25); // 5*5

    // 1D Poisson matrix assembly: -u'' = 0, u(0)=10, u(1)=20 -> linear u(x) = 10 + 10*x
    auto [A, b] = DiscretizationMesh::assemble_1d_poisson(9, 1.0, 10.0, 20.0, nullptr);
    CHECK(A.rows() == 9);
    CHECK(A.cols() == 9);

    DirectLUSolver direct_solver;
    std::vector<double> u(9, 0.0);
    SolverConfig cfg;
    auto res = direct_solver.solve(A, b, u, cfg);
    REQUIRE(res.is_ok());

    // Midpoint x = 0.5 (index 4) should be exactly 15.0
    CHECK(std::abs(u[4] - 15.0) < 1e-6);
}

TEST_CASE("SOL-07: Solver Capability Registry and Compatibility Checks") {
    auto& reg = SolverRegistry::instance();

    // Query solvers for ProblemType::ODE
    auto ode_solvers = reg.get_available_solvers(ProblemType::ODE);
    CHECK(ode_solvers.size() >= 4);

    // Find best adaptive solver for ODE
    auto best_adaptive = reg.find_best_solver(ProblemType::ODE, false, true);
    REQUIRE(best_adaptive.has_value());
    CHECK(best_adaptive->name == "AdaptiveRK45");
    CHECK(best_adaptive->accuracy_order == 5);

    // Find best stiff solver for ODE
    auto best_stiff = reg.find_best_solver(ProblemType::ODE, true, false);
    REQUIRE(best_stiff.has_value());
    CHECK(best_stiff->name == "ImplicitEuler");
    CHECK(best_stiff->is_stiff_stable);

    // Compatibility validation
    auto comp_res = reg.validate_compatibility(*best_adaptive, ProblemType::Linear, false, 10);
    CHECK_FALSE(comp_res.is_ok()); // RK45 doesn't solve pure linear algebra

    auto comp_ok = reg.validate_compatibility(*best_adaptive, ProblemType::ODE, false, 10);
    CHECK(comp_ok.is_ok());
}

TEST_CASE("SOL-08: Numerical Benchmark Suite Execution") {
    auto benchmarks = NumericalBenchmarkRunner::run_all_reference_benchmarks();
    REQUIRE(benchmarks.size() == 4);

    for (const auto& b : benchmarks) {
        CHECK(b.tolerance_passed);
        CHECK(b.linf_error < 0.05);
        CHECK(b.runtime_us > 0.0);
    }
}

TEST_CASE("SOL-09: Solver Diagnostics Serialization") {
    SolverDiagnostics diag;
    diag.total_iterations = 42;
    diag.step_count = 100;
    diag.accepted_steps = 95;
    diag.rejected_steps = 5;
    diag.initial_residual = 1.5;
    diag.final_residual = 1e-8;
    diag.residual_reduction_ratio = 1e-8 / 1.5;
    diag.matrix_condition_estimate = 12.5;
    diag.solve_time_ms = 4.2;
    diag.total_runtime_ms = 8.7;
    diag.stagnation_detected = false;

    std::string json = diag.to_json();
    auto restored_res = SolverDiagnostics::from_json(json);
    REQUIRE(restored_res.is_ok());

    const auto& restored = restored_res.value();
    CHECK(restored.total_iterations == 42);
    CHECK(restored.step_count == 100);
    CHECK(restored.accepted_steps == 95);
    CHECK(restored.rejected_steps == 5);
    CHECK(std::abs(restored.matrix_condition_estimate - 12.5) < 1e-9);
    CHECK_FALSE(restored.stagnation_detected);
}

TEST_CASE("SOL-10: Numerical Regression Across Solvers and Problems") {
    // Stiff Decay Regression with ImplicitEuler
    ImplicitEulerSolver implicit_solver;
    auto stiff_bench = NumericalBenchmarkRunner::run_stiff_decay_benchmark(
        500.0, 0.05, 0.0005, implicit_solver, 0.05
    );
    CHECK(stiff_bench.tolerance_passed);

    // Poisson 1D High Resolution (N=100) with Conjugate Gradient
    ConjugateGradientSolver cg_solver;
    auto poisson_bench = NumericalBenchmarkRunner::run_poisson_1d_benchmark(
        100, cg_solver, 1e-3
    );
    CHECK(poisson_bench.tolerance_passed);
}
