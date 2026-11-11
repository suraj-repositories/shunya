#pragma once

#include <cstddef>
#include <vector>

namespace shunya::perf {

struct SubdomainSlab {
    size_t subdomain_id{0};
    size_t nx{0}, ny{0}, nz{0};       // Full grid dims
    size_t slice_begin{0};            // Inclusive interior range along split axis
    size_t slice_end{0};              // Exclusive interior range along split axis
    size_t split_axis{0};             // 0=X, 1=Y, 2=Z
    std::vector<double> current;      // Local slab + 1 ghost slice on each end (local_len = interior + 2)
    std::vector<double> next;
};

/**
 * @brief Synchronizes 1-cell overlapping boundary ghost planes between adjacent decomposed subdomains.
 */
class GhostCellSynchronizer {
public:
    static void exchange_ghost_layers(std::vector<SubdomainSlab>& slabs);
};

} // namespace shunya::perf
