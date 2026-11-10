#pragma once

#include "shunya/cad3d/solid_mesh.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace shunya::cad3d {

inline constexpr uint32_t HE_INVALID = std::numeric_limits<uint32_t>::max();

struct HEVertex {
    Point3D position{0.0, 0.0, 0.0};
    Vec3d normal{0.0, 0.0, 0.0};
    uint32_t half_edge{HE_INVALID}; // one outgoing half-edge (origin == this vertex)
};

struct HEHalfEdge {
    uint32_t vertex{HE_INVALID}; // origin vertex index
    uint32_t next{HE_INVALID};
    uint32_t prev{HE_INVALID};
    uint32_t twin{HE_INVALID};
    uint32_t face{HE_INVALID};
};

struct HEFace {
    uint32_t half_edge{HE_INVALID};
    Vec3d normal{0.0, 0.0, 0.0};
    uint32_t degree{0};
};

/**
 * @brief Pointer-free index-based Half-Edge mesh supporting triangles, quads, and arbitrary manifold faces.
 */
class HalfEdgeMesh {
public:
    std::vector<HEVertex> vertices;
    std::vector<HEHalfEdge> half_edges;
    std::vector<HEFace> faces;

    void clear() noexcept;

    /**
     * @brief Builds topology from vertices and arbitrary polygonal face vertex lists.
     *
     * Returns true iff input forms a valid 2-manifold (possibly with boundary). Sets non_manifold() if
     * duplicate directed edges or >2 faces per undirected edge are encountered.
     */
    bool build_from_polygons(
        const std::vector<Point3D>& verts,
        const std::vector<std::vector<uint32_t>>& poly_faces
    );

    bool build_from_triangles(
        const std::vector<Point3D>& verts,
        const std::vector<std::array<uint32_t, 3>>& tris
    );

    bool build_from_solid(const SolidMesh3D& solid);

    /**
     * @brief Converts polygonal half-edge faces into an indexed SolidMesh3D via fan triangulation.
     */
    [[nodiscard]] SolidMesh3D to_solid_mesh() const;

    void compute_normals();

    [[nodiscard]] bool has_non_manifold_edges() const noexcept { return non_manifold_detected_; }

    [[nodiscard]] uint32_t target_vertex(uint32_t he_idx) const noexcept {
        if (he_idx >= half_edges.size()) return HE_INVALID;
        const uint32_t nxt = half_edges[he_idx].next;
        return (nxt < half_edges.size()) ? half_edges[nxt].vertex : HE_INVALID;
    }

    [[nodiscard]] bool is_boundary_half_edge(uint32_t he_idx) const noexcept {
        return he_idx < half_edges.size() && half_edges[he_idx].twin == HE_INVALID;
    }

    [[nodiscard]] bool is_boundary_vertex(uint32_t v_idx) const noexcept;
    [[nodiscard]] uint32_t vertex_valency(uint32_t v_idx) const noexcept;

    [[nodiscard]] bool verify_twin_consistency() const noexcept;
    [[nodiscard]] ManifoldReport verify_topology() const;

private:
    bool non_manifold_detected_{false};
};

} // namespace shunya::cad3d
