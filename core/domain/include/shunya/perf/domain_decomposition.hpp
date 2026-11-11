#pragma once

#include "shunya/core/base/result.hpp"
#include "shunya/perf/ghost_cell_sync.hpp"
#include "shunya/perf/work_stealing_pool.hpp"

#include <cstddef>
#include <vector>

namespace shunya::perf {

/**
 * @brief Spatial 3D domain decomposition along longest axis with parallel ghost-cell explicit diffusion verification.
 */
class DomainDecompositionManager {
public:
    [[nodiscard]] static size_t choose_longest_axis(size_t nx, size_t ny, size_t nz) noexcept;

    [[nodiscard]] static core::base::Result<std::vector<SubdomainSlab>> partition_grid(
        size_t nx, size_t ny, size_t nz,
        size_t num_subdomains,
        const std::vector<double>& initial_field);

    [[nodiscard]] static std::vector<double> gather_field(const std::vector<SubdomainSlab>& slabs);

    /**
     * @brief Runs explicit 3D heat diffusion across decomposed subdomains using WorkStealingPool.
     */
    [[nodiscard]] static core::base::Result<std::vector<double>> run_parallel_diffusion_3d(
        size_t nx, size_t ny, size_t nz,
        size_t num_subdomains,
        double fourier_alpha,
        int num_steps,
        const std::vector<double>& initial_field,
        WorkStealingPool& pool);

    /**
     * @brief Exact monolithic single-threaded 3D diffusion reference on identical indexing & summation order.
     */
    [[nodiscard]] static std::vector<double> run_monolithic_diffusion_3d(
        size_t nx, size_t ny, size_t nz,
        double fourier_alpha,
        int num_steps,
        const std::vector<double>& initial_field);
};

} // namespace shunya::perf
