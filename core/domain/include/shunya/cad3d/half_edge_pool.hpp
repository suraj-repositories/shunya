#pragma once

#include "shunya/cad3d/half_edge_mesh.hpp"
#include "shunya/perf/fixed_size_pool.hpp"

#include <vector>
#include <cstdint>
#include <limits>

namespace shunya::cad3d {

/**
 * @brief Fixed-size block memory pooled storage for HalfEdgeMesh.
 *        Eliminates dynamic heap allocation overhead and fragmentation
 *        during dynamic mesh editing, subdivision, edge collapses, and face splits.
 */
class PooledHalfEdgeMesh {
public:
    explicit PooledHalfEdgeMesh(size_t initial_chunk_size = 1024);

    // Allocation & Free with O(1) slot recycling
    uint32_t allocate_vertex(Point3D pos = {0.0, 0.0, 0.0}, Vec3d norm = {0.0, 0.0, 0.0});
    void free_vertex(uint32_t v_idx);

    uint32_t allocate_half_edge(uint32_t origin_v = HE_INVALID);
    void free_half_edge(uint32_t he_idx);

    uint32_t allocate_face(uint32_t he_idx = HE_INVALID, uint32_t degree = 3);
    void free_face(uint32_t f_idx);

    // Element Access
    [[nodiscard]] HEVertex& vertex(uint32_t idx);
    [[nodiscard]] const HEVertex& vertex(uint32_t idx) const;

    [[nodiscard]] HEHalfEdge& half_edge(uint32_t idx);
    [[nodiscard]] const HEHalfEdge& half_edge(uint32_t idx) const;

    [[nodiscard]] HEFace& face(uint32_t idx);
    [[nodiscard]] const HEFace& face(uint32_t idx) const;

    [[nodiscard]] bool is_vertex_active(uint32_t idx) const noexcept;
    [[nodiscard]] bool is_half_edge_active(uint32_t idx) const noexcept;
    [[nodiscard]] bool is_face_active(uint32_t idx) const noexcept;

    // Topology Migration
    void load_from_mesh(const HalfEdgeMesh& mesh);
    [[nodiscard]] HalfEdgeMesh to_compact_mesh() const;

    // Metrics & Capacity
    [[nodiscard]] size_t active_vertex_count() const noexcept { return active_vertices_; }
    [[nodiscard]] size_t active_half_edge_count() const noexcept { return active_half_edges_; }
    [[nodiscard]] size_t active_face_count() const noexcept { return active_faces_; }

    [[nodiscard]] size_t vertex_capacity() const noexcept { return vertices_.size(); }
    [[nodiscard]] size_t half_edge_capacity() const noexcept { return half_edges_.size(); }
    [[nodiscard]] size_t face_capacity() const noexcept { return faces_.size(); }

    [[nodiscard]] size_t memory_bytes_allocated() const noexcept;

    void clear() noexcept;

private:
    struct SlotFlags {
        bool active{false};
    };

    std::vector<HEVertex> vertices_;
    std::vector<SlotFlags> vertex_active_;
    std::vector<uint32_t> vertex_free_list_;
    size_t active_vertices_{0};

    std::vector<HEHalfEdge> half_edges_;
    std::vector<SlotFlags> half_edge_active_;
    std::vector<uint32_t> half_edge_free_list_;
    size_t active_half_edges_{0};

    std::vector<HEFace> faces_;
    std::vector<SlotFlags> face_active_;
    std::vector<uint32_t> face_free_list_;
    size_t active_faces_{0};

    perf::FixedSizePool<HEVertex, 64> vertex_block_pool_;
    perf::FixedSizePool<HEHalfEdge, 64> half_edge_block_pool_;
    perf::FixedSizePool<HEFace, 64> face_block_pool_;
};

} // namespace shunya::cad3d
