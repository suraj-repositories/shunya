#include "shunya/perf/domain_decomposition.hpp"

#include <algorithm>
#include <cstring>

namespace shunya::perf {

namespace {

size_t plane_size_along_axis(size_t nx, size_t ny, size_t nz, size_t axis) noexcept {
    if (axis == 0) return ny * nz;
    if (axis == 1) return nx * nz;
    return nx * ny;
}

void write_slice(
    SubdomainSlab& slab,
    size_t local_s,
    const std::vector<double>& global_field,
    size_t global_s) {
    const size_t nx = slab.nx, ny = slab.ny, nz = slab.nz;
    if (slab.split_axis == 2) {
        const size_t slice_elems = nx * ny;
        std::memcpy(
            &slab.current[local_s * slice_elems],
            &global_field[global_s * slice_elems],
            slice_elems * sizeof(double));
    } else if (slab.split_axis == 1) {
        for (size_t k = 0; k < nz; ++k) {
            for (size_t i = 0; i < nx; ++i) {
                slab.current[(local_s * nz + k) * nx + i] =
                    global_field[(k * ny + global_s) * nx + i];
            }
        }
    } else {
        for (size_t k = 0; k < nz; ++k) {
            for (size_t j = 0; j < ny; ++j) {
                slab.current[(local_s * nz + k) * ny + j] =
                    global_field[(k * ny + j) * nx + global_s];
            }
        }
    }
}

double read_cell_global_layout(
    const SubdomainSlab& slab,
    size_t local_s,
    size_t u,
    size_t v) noexcept {
    const size_t nx = slab.nx, ny = slab.ny, nz = slab.nz;
    if (slab.split_axis == 2) {
        return slab.current[(local_s * ny + v) * nx + u];
    }
    if (slab.split_axis == 1) {
        return slab.current[(local_s * nz + v) * nx + u];
    }
    return slab.current[(local_s * nz + v) * ny + u];
}

double step_point(double c, double xm, double xp, double ym, double yp, double zm, double zp, double alpha) noexcept {
    return c + alpha * ((xm + xp) + (ym + yp) + (zm + zp) - 6.0 * c);
}

} // namespace

void GhostCellSynchronizer::exchange_ghost_layers(std::vector<SubdomainSlab>& slabs) {
    if (slabs.size() <= 1) return;
    const size_t slice_elems = plane_size_along_axis(
        slabs[0].nx, slabs[0].ny, slabs[0].nz, slabs[0].split_axis);

    for (size_t s = 0; s + 1 < slabs.size(); ++s) {
        auto& left = slabs[s];
        auto& right = slabs[s + 1];
        const size_t left_interior = left.slice_end - left.slice_begin;
        // Left's last interior slice -> Right's left ghost (local index 0)
        std::memcpy(
            &right.current[0],
            &left.current[left_interior * slice_elems],
            slice_elems * sizeof(double));
        // Right's first interior slice (local index 1) -> Left's right ghost (local index left_interior + 1)
        std::memcpy(
            &left.current[(left_interior + 1) * slice_elems],
            &right.current[1 * slice_elems],
            slice_elems * sizeof(double));
    }
}

size_t DomainDecompositionManager::choose_longest_axis(size_t nx, size_t ny, size_t nz) noexcept {
    if (nx >= ny && nx >= nz) return 0;
    if (ny >= nx && ny >= nz) return 1;
    return 2;
}

core::base::Result<std::vector<SubdomainSlab>> DomainDecompositionManager::partition_grid(
    size_t nx, size_t ny, size_t nz,
    size_t num_subdomains,
    const std::vector<double>& initial_field) {
    if (nx < 3 || ny < 3 || nz < 3 || num_subdomains == 0 ||
        initial_field.size() != nx * ny * nz) {
        return core::base::Error{
            531, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Invalid grid dimensions or subdomain count for domain decomposition", ""
        };
    }

    const size_t axis = choose_longest_axis(nx, ny, nz);
    const size_t axis_len = (axis == 0) ? nx : ((axis == 1) ? ny : nz);
    const size_t actual_parts = std::min(num_subdomains, axis_len);
    const size_t slice_elems = plane_size_along_axis(nx, ny, nz, axis);

    std::vector<SubdomainSlab> slabs;
    slabs.reserve(actual_parts);

    for (size_t p = 0; p < actual_parts; ++p) {
        const size_t b = (p * axis_len) / actual_parts;
        const size_t e = ((p + 1) * axis_len) / actual_parts;
        SubdomainSlab slab;
        slab.subdomain_id = p;
        slab.nx = nx;
        slab.ny = ny;
        slab.nz = nz;
        slab.slice_begin = b;
        slab.slice_end = e;
        slab.split_axis = axis;
        const size_t total_local_slices = (e - b) + 2;
        slab.current.assign(total_local_slices * slice_elems, 0.0);
        slab.next.assign(total_local_slices * slice_elems, 0.0);

        for (size_t gs = b; gs < e; ++gs) {
            write_slice(slab, 1 + (gs - b), initial_field, gs);
        }
        slab.next = slab.current;
        slabs.push_back(std::move(slab));
    }

    GhostCellSynchronizer::exchange_ghost_layers(slabs);
    return slabs;
}

std::vector<double> DomainDecompositionManager::gather_field(const std::vector<SubdomainSlab>& slabs) {
    if (slabs.empty()) return {};
    const size_t nx = slabs[0].nx, ny = slabs[0].ny, nz = slabs[0].nz;
    std::vector<double> out(nx * ny * nz, 0.0);

    for (const auto& slab : slabs) {
        for (size_t gs = slab.slice_begin; gs < slab.slice_end; ++gs) {
            const size_t ls = 1 + (gs - slab.slice_begin);
            if (slab.split_axis == 2) {
                std::memcpy(
                    &out[gs * nx * ny],
                    &slab.current[ls * nx * ny],
                    nx * ny * sizeof(double));
            } else if (slab.split_axis == 1) {
                for (size_t k = 0; k < nz; ++k) {
                    for (size_t i = 0; i < nx; ++i) {
                        out[(k * ny + gs) * nx + i] = read_cell_global_layout(slab, ls, i, k);
                    }
                }
            } else {
                for (size_t k = 0; k < nz; ++k) {
                    for (size_t j = 0; j < ny; ++j) {
                        out[(k * ny + j) * nx + gs] = read_cell_global_layout(slab, ls, j, k);
                    }
                }
            }
        }
    }
    return out;
}

core::base::Result<std::vector<double>> DomainDecompositionManager::run_parallel_diffusion_3d(
    size_t nx, size_t ny, size_t nz,
    size_t num_subdomains,
    double fourier_alpha,
    int num_steps,
    const std::vector<double>& initial_field,
    WorkStealingPool& pool) {
    auto slabs_res = partition_grid(nx, ny, nz, num_subdomains, initial_field);
    if (!slabs_res.is_ok()) return slabs_res.error();
    auto& slabs = slabs_res.value();

    for (int s = 0; s < num_steps; ++s) {
        GhostCellSynchronizer::exchange_ghost_layers(slabs);
        pool.parallel_for(0, slabs.size(), 1, [&](size_t p) {
            auto& slab = slabs[p];
            const size_t axis = slab.split_axis;
            const size_t slice_count = slab.slice_end - slab.slice_begin;
            for (size_t ls = 1; ls <= slice_count; ++ls) {
                const size_t gs = slab.slice_begin + (ls - 1);
                if (axis == 2) {
                    if (gs == 0 || gs + 1 == nz) continue;
                    for (size_t j = 1; j + 1 < ny; ++j) {
                        for (size_t i = 1; i + 1 < nx; ++i) {
                            const size_t off = (ls * ny + j) * nx + i;
                            slab.next[off] = step_point(
                                slab.current[off],
                                slab.current[off - 1], slab.current[off + 1],
                                slab.current[off - nx], slab.current[off + nx],
                                slab.current[off - nx * ny], slab.current[off + nx * ny],
                                fourier_alpha);
                        }
                    }
                } else if (axis == 1) {
                    if (gs == 0 || gs + 1 == ny) continue;
                    for (size_t k = 1; k + 1 < nz; ++k) {
                        for (size_t i = 1; i + 1 < nx; ++i) {
                            const size_t off = (ls * nz + k) * nx + i;
                            const size_t stride = nx * nz;
                            slab.next[off] = step_point(
                                slab.current[off],
                                slab.current[off - 1], slab.current[off + 1],
                                slab.current[off - stride], slab.current[off + stride],
                                slab.current[off - nx], slab.current[off + nx],
                                fourier_alpha);
                        }
                    }
                } else {
                    if (gs == 0 || gs + 1 == nx) continue;
                    for (size_t k = 1; k + 1 < nz; ++k) {
                        for (size_t j = 1; j + 1 < ny; ++j) {
                            const size_t off = (ls * nz + k) * ny + j;
                            const size_t stride = ny * nz;
                            slab.next[off] = step_point(
                                slab.current[off],
                                slab.current[off - stride], slab.current[off + stride],
                                slab.current[off - 1], slab.current[off + 1],
                                slab.current[off - ny], slab.current[off + ny],
                                fourier_alpha);
                        }
                    }
                }
            }
            std::swap(slab.current, slab.next);
        });
    }

    return gather_field(slabs);
}

std::vector<double> DomainDecompositionManager::run_monolithic_diffusion_3d(
    size_t nx, size_t ny, size_t nz,
    double fourier_alpha,
    int num_steps,
    const std::vector<double>& initial_field) {
    std::vector<double> cur = initial_field;
    std::vector<double> nxt = initial_field;

    for (int s = 0; s < num_steps; ++s) {
        for (size_t k = 1; k + 1 < nz; ++k) {
            for (size_t j = 1; j + 1 < ny; ++j) {
                for (size_t i = 1; i + 1 < nx; ++i) {
                    const size_t c = (k * ny + j) * nx + i;
                    nxt[c] = step_point(
                        cur[c],
                        cur[c - 1], cur[c + 1],
                        cur[c - nx], cur[c + nx],
                        cur[c - nx * ny], cur[c + nx * ny],
                        fourier_alpha);
                }
            }
        }
        std::swap(cur, nxt);
    }
    return cur;
}

} // namespace shunya::perf
