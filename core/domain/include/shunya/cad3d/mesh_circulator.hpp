#pragma once

#include "shunya/cad3d/half_edge_mesh.hpp"

#include <cstdint>
#include <vector>

namespace shunya::cad3d {

/**
 * @brief O(deg(v)) topological circulators over index-based HalfEdgeMesh.
 */
class MeshCirculator {
public:
    /**
     * @brief Returns ordered outgoing half-edge indices originating from vertex v in O(deg(v)) time.
     */
    [[nodiscard]] static std::vector<uint32_t> outgoing_half_edges(
        const HalfEdgeMesh& mesh,
        uint32_t v_idx
    );

    /**
     * @brief Returns ordered 1-ring neighbor vertex indices around vertex v in O(deg(v)) time.
     */
    [[nodiscard]] static std::vector<uint32_t> vertex_neighbors(
        const HalfEdgeMesh& mesh,
        uint32_t v_idx
    );

    /**
     * @brief Returns ordered incident face indices around vertex v in O(deg(v)) time.
     */
    [[nodiscard]] static std::vector<uint32_t> incident_faces(
        const HalfEdgeMesh& mesh,
        uint32_t v_idx
    );

    /**
     * @brief Returns ordered vertex indices around face f.
     */
    [[nodiscard]] static std::vector<uint32_t> face_vertices(
        const HalfEdgeMesh& mesh,
        uint32_t f_idx
    );
};

} // namespace shunya::cad3d
