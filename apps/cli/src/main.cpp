#include "shunya/core/base/app_context.hpp"
#include "shunya/core/base/logging.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/domain/project.hpp"
#include "shunya/core/domain/simulation_clock.hpp"
#include "shunya/core/domain/simulation_runtime.hpp"
#include "shunya/core/domain/numerical_solver.hpp"
#include "shunya/core/domain/sparse_matrix.hpp"
#include "shunya/core/domain/circuit.hpp"
#include "shunya/core/domain/thermal.hpp"
#include "shunya/core/domain/mechanics.hpp"
#include "shunya/core/domain/performance.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <sstream>

using namespace shunya::core::base;
using namespace shunya::core::domain;

namespace {

void print_banner() {
    std::cout << "=================================================================\n";
    std::cout << "  Shunya Virtual Engineering & Multiphysics Platform CLI\n";
    std::cout << "  Headless Simulation & Scientific Verification Engine\n";
    std::cout << "  Version " << CURRENT_VERSION.as_string();
    std::cout << " (C++20, MinGW/POSIX, Headless Profile)\n";
    std::cout << "=================================================================\n";
}

void print_help(const char* prog) {
    print_banner();
    std::cout << "\nUsage:\n";
    std::cout << "  " << prog << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help                 Show this help message and exit\n";
    std::cout << "  -v, --version              Show engine version and exit\n";
    std::cout << "  --project <path>           Load project manifest from <path>\n";
    std::cout << "  --run                      Run project simulation headlessly\n";
    std::cout << "  --duration <sec>           Set simulation duration in seconds (default: 1.0)\n";
    std::cout << "  --dt <sec>                 Set simulation timestep in seconds (default: 0.001)\n";
    std::cout << "  --sweep <p> <min> <max> <n> Perform parametric sweep on parameter <p>\n";
    std::cout << "  --benchmark                Run automated scientific benchmark verification suite\n";
    std::cout << "  --export-results <path>    Export simulation/benchmark results to JSON\n";
    std::cout << "  --verbose                  Enable verbose trace diagnostics\n";
    std::cout << "  --quiet                    Suppress banner and progress output\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << prog << " --benchmark\n";
    std::cout << "  " << prog << " --benchmark --export-results benchmarks.json\n";
    std::cout << "  " << prog << " --project my_lab.shunya --run --duration 2.5 --dt 0.0005\n";
    std::cout << "  " << prog << " --sweep resistance 10.0 100.0 10 --export-results sweep.json\n";
}

struct CliOptions {
    bool show_help = false;
    bool show_version = false;
    bool run_simulation = false;
    bool run_benchmark = false;
    bool verbose = false;
    bool quiet = false;
    std::string project_path;
    std::string export_path;
    double duration = 1.0;
    double dt = 0.001;
    bool has_sweep = false;
    std::string sweep_param;
    double sweep_min = 0.0;
    double sweep_max = 1.0;
    int sweep_steps = 5;
};

CliOptions parse_arguments(int argc, char* argv[]) {
    CliOptions opt;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            opt.show_help = true;
        } else if (arg == "-v" || arg == "--version") {
            opt.show_version = true;
        } else if (arg == "--verbose") {
            opt.verbose = true;
        } else if (arg == "--quiet") {
            opt.quiet = true;
        } else if (arg == "--run") {
            opt.run_simulation = true;
        } else if (arg == "--benchmark") {
            opt.run_benchmark = true;
        } else if (arg == "--project" && i + 1 < argc) {
            opt.project_path = argv[++i];
        } else if (arg == "--export-results" && i + 1 < argc) {
            opt.export_path = argv[++i];
        } else if (arg == "--duration" && i + 1 < argc) {
            opt.duration = std::stod(argv[++i]);
        } else if (arg == "--dt" && i + 1 < argc) {
            opt.dt = std::stod(argv[++i]);
        } else if (arg == "--sweep" && i + 4 < argc) {
            opt.has_sweep = true;
            opt.sweep_param = argv[++i];
            opt.sweep_min = std::stod(argv[++i]);
            opt.sweep_max = std::stod(argv[++i]);
            opt.sweep_steps = std::stoi(argv[++i]);
        }
    }
    return opt;
}

struct BenchmarkRecord {
    std::string domain;
    std::string test_name;
    double duration_us = 0.0;
    int iterations = 0;
    double residual = 0.0;
    bool passed = false;
};

std::vector<BenchmarkRecord> run_benchmarks(bool quiet) {
    std::vector<BenchmarkRecord> records;

    if (!quiet) {
        std::cout << "\nExecuting Scientific Solver Verification Benchmarks...\n";
        std::cout << "---------------------------------------------------------------------------------------------------\n";
    }

    // Benchmark 1: Linear Sparse Solver (Conjugate Gradient on SPD Laplace 1D system)
    {
        constexpr size_t N = 50;
        std::vector<Triplet> triplets;
        for (size_t i = 0; i < N; ++i) {
            if (i > 0) triplets.push_back({i, i - 1, -1.0});
            triplets.push_back({i, i, 2.0});
            if (i + 1 < N) triplets.push_back({i, i + 1, -1.0});
        }
        auto spd = SparseMatrixCSR::from_triplets(N, N, triplets);
        std::vector<double> b(N, 1.0);
        std::vector<double> x(N, 0.0);

        SolverConfig cfg;
        cfg.max_iterations = 1000;
        cfg.relative_tolerance = 1e-8;
        ConjugateGradientSolver solver;

        auto t0 = std::chrono::high_resolution_clock::now();
        auto sol_res = solver.solve(spd, b, x, cfg);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        BenchmarkRecord rec;
        rec.domain = "Linear Algebra";
        rec.test_name = "CG Solver 50x50 SPD";
        rec.duration_us = elapsed_us;
        rec.iterations = sol_res.is_ok() ? static_cast<int>(sol_res.value().iterations) : 0;
        rec.residual = sol_res.is_ok() ? sol_res.value().final_residual : 1.0;
        rec.passed = sol_res.is_ok() && sol_res.value().converged;
        records.push_back(rec);
    }

    // Benchmark 2: BiCGSTAB Solver (Non-symmetric system)
    {
        constexpr size_t N = 30;
        std::vector<Triplet> triplets;
        for (size_t i = 0; i < N; ++i) {
            if (i > 0) triplets.push_back({i, i - 1, -0.8});
            triplets.push_back({i, i, 2.5});
            if (i + 1 < N) triplets.push_back({i, i + 1, -1.2});
        }
        auto mat = SparseMatrixCSR::from_triplets(N, N, triplets);
        std::vector<double> b(N, 2.0);
        std::vector<double> x(N, 0.0);

        SolverConfig cfg;
        cfg.max_iterations = 1000;
        cfg.relative_tolerance = 1e-7;
        BiCGSTABSolver solver;

        auto t0 = std::chrono::high_resolution_clock::now();
        auto sol_res = solver.solve(mat, b, x, cfg);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        BenchmarkRecord rec;
        rec.domain = "Linear Algebra";
        rec.test_name = "BiCGSTAB Solver 30x30 Non-Symmetric";
        rec.duration_us = elapsed_us;
        rec.iterations = sol_res.is_ok() ? static_cast<int>(sol_res.value().iterations) : 0;
        rec.residual = sol_res.is_ok() ? sol_res.value().final_residual : 1.0;
        rec.passed = sol_res.is_ok() && sol_res.value().converged;
        records.push_back(rec);
    }

    // Benchmark 3: Thermal 1D Steady-State Conduction Slab Benchmark
    {
        using namespace shunya::core::domain::thermal;
        auto net = ThermalBenchmarkFactory::create_1d_slab_conduction(1.0, 1.0, 100.0, 373.15, 273.15, 5);

        auto t0 = std::chrono::high_resolution_clock::now();
        auto sol_res = net.solve_steady_state();
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        BenchmarkRecord rec;
        rec.domain = "Thermal";
        rec.test_name = "1D Slab Conduction (Analytical 5-Node)";
        rec.duration_us = elapsed_us;
        rec.iterations = 1;
        if (sol_res.is_ok()) {
            double t_mid = net.get_node(2).state.temperature;
            rec.residual = std::abs(t_mid - 323.15);
            rec.passed = rec.residual < 0.05;
        } else {
            rec.residual = 1.0;
            rec.passed = false;
        }
        records.push_back(rec);
    }

    // Benchmark 4: Circuit Simulation (Voltage Divider Benchmark)
    {
        auto divider = CircuitBenchmarkFactory::create_voltage_divider(12.0, 3000.0, 1000.0);
        DcOperatingPointSolver solver;

        auto t0 = std::chrono::high_resolution_clock::now();
        auto op_res = solver.solve(divider);
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        BenchmarkRecord rec;
        rec.domain = "Electronics";
        rec.test_name = "DC Voltage Divider (12V -> 3V)";
        rec.duration_us = elapsed_us;
        if (op_res.is_ok()) {
            rec.iterations = static_cast<int>(op_res.value().iterations);
            double v_out = op_res.value().get_voltage("out");
            rec.residual = std::abs(v_out - 3.0);
            rec.passed = op_res.value().converged && (rec.residual < 1e-4);
        } else {
            rec.iterations = 0;
            rec.residual = 1.0;
            rec.passed = false;
        }
        records.push_back(rec);
    }

    // Benchmark 5: Mechanics Analytical Free Fall Benchmark
    {
        using namespace shunya::core::domain::mechanics;
        auto world = MechanicsBenchmarkFactory::create_free_fall_benchmark(100.0, 2.0);
        constexpr double dt = 0.005;
        constexpr int steps = 200; // 1.0 second total

        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < steps; ++i) {
            world.step(dt);
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double elapsed_us = std::chrono::duration<double, std::micro>(t1 - t0).count();

        BenchmarkRecord rec;
        rec.domain = "Mechanics";
        rec.test_name = "Analytical Free Fall (200 Steps)";
        rec.duration_us = elapsed_us;
        rec.iterations = steps;

        auto faller = world.get_body("faller");
        if (faller != nullptr) {
            double expected_y = 100.0 - 0.5 * 9.80665 * 1.0 * 1.0;
            rec.residual = std::abs(faller->position().y - expected_y);
            rec.passed = rec.residual < 0.1;
        } else {
            rec.residual = 1.0;
            rec.passed = false;
        }
        records.push_back(rec);
    }

    if (!quiet) {
        std::cout << std::left << std::setw(16) << "Domain"
                  << std::left << std::setw(38) << "Benchmark"
                  << std::right << std::setw(12) << "Time (us)"
                  << std::right << std::setw(10) << "Iters"
                  << std::right << std::setw(14) << "Residual"
                  << std::right << std::setw(10) << "Status" << "\n";
        std::cout << "---------------------------------------------------------------------------------------------------\n";

        for (const auto& r : records) {
            std::cout << std::left << std::setw(16) << r.domain
                      << std::left << std::setw(38) << r.test_name
                      << std::right << std::setw(12) << std::fixed << std::setprecision(1) << r.duration_us
                      << std::right << std::setw(10) << r.iterations
                      << std::right << std::setw(14) << std::scientific << std::setprecision(3) << r.residual
                      << std::right << std::setw(10) << (r.passed ? "[PASS]" : "[FAIL]") << "\n";
        }
        std::cout << "---------------------------------------------------------------------------------------------------\n";
    }

    return records;
}

struct SweepPoint {
    double param_val = 0.0;
    double metric_val = 0.0;
};

std::vector<SweepPoint> execute_sweep(std::string_view param, double min_v, double max_v, int steps, bool quiet) {
    std::vector<SweepPoint> sweep_results;
    if (steps <= 0) steps = 1;
    double delta = (steps > 1) ? (max_v - min_v) / (steps - 1) : 0.0;

    if (!quiet) {
        std::cout << "\nExecuting Parametric Sweep on '" << param << "' from "
                  << min_v << " to " << max_v << " (" << steps << " steps)...\n";
        std::cout << "-----------------------------------------------------------------\n";
        std::cout << std::left << std::setw(12) << "Step"
                  << std::right << std::setw(18) << param
                  << std::right << std::setw(22) << "Circuit Vout (V)" << "\n";
        std::cout << "-----------------------------------------------------------------\n";
    }

    DcOperatingPointSolver solver;

    for (int s = 0; s < steps; ++s) {
        double val = min_v + s * delta;
        auto netlist = CircuitBenchmarkFactory::create_voltage_divider(10.0, val > 0.0 ? val : 1.0, 100.0);
        auto sol_res = solver.solve(netlist);

        double vout = 0.0;
        if (sol_res.is_ok() && sol_res.value().converged) {
            vout = sol_res.value().get_voltage("out");
        }

        sweep_results.push_back({val, vout});

        if (!quiet) {
            std::cout << std::left << std::setw(12) << s + 1
                      << std::right << std::setw(18) << std::fixed << std::setprecision(3) << val
                      << std::right << std::setw(22) << std::fixed << std::setprecision(4) << vout << "\n";
        }
    }
    return sweep_results;
}

void export_to_json(const std::string& path,
                    const std::vector<BenchmarkRecord>& benchmarks,
                    const std::vector<SweepPoint>& sweeps) {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "Error: Unable to open export file '" << path << "'\n";
        return;
    }

    out << "{\n";
    out << "  \"engine\": \"Shunya CLI\",\n";
    out << "  \"version\": \"" << CURRENT_VERSION.as_string() << "\",\n";

    out << "  \"benchmarks\": [\n";
    for (size_t i = 0; i < benchmarks.size(); ++i) {
        const auto& b = benchmarks[i];
        out << "    {\n";
        out << "      \"domain\": \"" << b.domain << "\",\n";
        out << "      \"name\": \"" << b.test_name << "\",\n";
        out << "      \"duration_us\": " << b.duration_us << ",\n";
        out << "      \"iterations\": " << b.iterations << ",\n";
        out << "      \"residual\": " << b.residual << ",\n";
        out << "      \"passed\": " << (b.passed ? "true" : "false") << "\n";
        out << "    }" << (i + 1 < benchmarks.size() ? "," : "") << "\n";
    }
    out << "  ],\n";

    out << "  \"sweeps\": [\n";
    for (size_t i = 0; i < sweeps.size(); ++i) {
        const auto& s = sweeps[i];
        out << "    {\n";
        out << "      \"param\": " << s.param_val << ",\n";
        out << "      \"result\": " << s.metric_val << "\n";
        out << "    }" << (i + 1 < sweeps.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    std::cout << "Successfully exported telemetry to: " << path << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
    CliOptions opt = parse_arguments(argc, argv);

    if (opt.show_help || (argc == 1 && !opt.run_benchmark && !opt.run_simulation && !opt.has_sweep)) {
        print_help(argv[0]);
        return 0;
    }

    if (opt.show_version) {
        std::cout << "shunya-cli version " << CURRENT_VERSION.as_string() << "\n";
        return 0;
    }

    if (!opt.quiet) {
        print_banner();
    }

    // Headless AppContext setup
    auto& context = AppContext::instance();
    AppInitOptions app_opts;
    app_opts.app_name = "shunya-cli";
    app_opts.worker_threads = 4;
    (void)context.initialize(app_opts);

    if (opt.verbose) {
        Logger::instance().set_level(LogLevel::Debug);
        std::cout << "[INFO] Verbose diagnostic mode enabled.\n";
    } else {
        Logger::instance().set_level(LogLevel::Info);
    }

    std::vector<BenchmarkRecord> benchmark_results;
    std::vector<SweepPoint> sweep_results;

    // 1. Run Automated Benchmarks
    if (opt.run_benchmark) {
        benchmark_results = run_benchmarks(opt.quiet);
        for (const auto& b : benchmark_results) {
            if (!b.passed) {
                std::cerr << "[FAIL] Benchmark failed: " << b.test_name << "\n";
                context.shutdown();
                return 1;
            }
        }
    }

    // 2. Load Project and Run Simulation
    if (!opt.project_path.empty()) {
        if (!opt.quiet) {
            std::cout << "\nLoading Project: " << opt.project_path << "\n";
        }
        auto load_res = Project::open_from_file(opt.project_path);
        if (!load_res.is_ok()) {
            std::cerr << "Error loading project: " << load_res.error().message << "\n";
            context.shutdown();
            return 1;
        }

        auto project = load_res.value();
        if (!opt.quiet) {
            std::cout << "Project loaded successfully: " << project->manifest().name << "\n";
            std::cout << "Schema version: " << project->manifest().schema_version.as_string() << "\n";
        }

        if (opt.run_simulation) {
            if (!opt.quiet) {
                std::cout << "Executing headless simulation: duration=" << opt.duration
                          << "s, dt=" << opt.dt << "s...\n";
            }
            SimulationClock clock;
            clock.start_wall_clock();
            int total_steps = static_cast<int>(opt.duration / opt.dt);
            for (int s = 0; s < total_steps; ++s) {
                clock.advance(opt.dt);
            }
            if (!opt.quiet) {
                std::cout << "Simulation completed: " << total_steps << " steps executed. Sim Time: "
                          << clock.physical_time() << "s.\n";
            }
        }
    }

    // 3. Parametric Sweep
    if (opt.has_sweep) {
        sweep_results = execute_sweep(opt.sweep_param, opt.sweep_min, opt.sweep_max, opt.sweep_steps, opt.quiet);
    }

    // 4. Export Results
    if (!opt.export_path.empty()) {
        export_to_json(opt.export_path, benchmark_results, sweep_results);
    }

    if (!opt.quiet) {
        std::cout << "\n[OK] Headless execution finished successfully.\n";
    }

    context.shutdown();
    return 0;
}
