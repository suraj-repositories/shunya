#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace shunya::cad3d {

struct ExtrudeOptions {
    double distance{1.0};
    bool individual{false}; // true = each face separate; false = region
    Vec3d direction{0.0, 0.0, 0.0}; // if non-zero, overrides face normal
};

struct InsetOptions {
    double thickness{0.2}; // inset distance inwards from boundary
    double depth{0.0};     // optional displacement along normal (extrude + inset)
    bool individual{false};
};

struct BevelOptions {
    double offset{0.1};
    uint32_t segments{1}; // 1 = chamfer/flat, >1 = round fillet
    double profile{0.5};  // 0.5 = round, 0.0 = inward/straight, 1.0 = sharp
    bool vertex_only{false};
};

struct LoopCutOptions {
    uint32_t cuts{1};
    double slide{0.0}; // -1.0 to +1.0 relative slide factor
};

enum class SubdivideMode {
    Linear,
    CatmullClark
};

enum class VertexMergeMode {
    AtFirst,
    AtLast,
    AtCenter,
    ByDistance
};

class MeshTopologyOps {
public:
    // ========================================================================
    // Task 4 (PH4-05.02.003): Face Extrude
    // ========================================================================
    static bool extrude_faces(
        SolidMesh3D& mesh,
        const std::vector<uint32_t>& face_indices,
        const ExtrudeOptions& options = {}
    );

    // ========================================================================
    // Task 5 (PH4-05.02.004): Inset Faces
    // ========================================================================
    static bool inset_faces(
        SolidMesh3D& mesh,
        const std::vector<uint32_t>& face_indices,
        const InsetOptions& options = {}
    );

    // ========================================================================
    // Task 6 (PH4-05.02.005): Edge & Vertex Beveling
    // ========================================================================
    static bool bevel(
        SolidMesh3D& mesh,
        const std::vector<std::pair<uint32_t, uint32_t>>& edges,
        const std::vector<uint32_t>& vertices,
        const BevelOptions& options = {}
    );

    // ========================================================================
    // Task 7 (PH4-05.02.006): Loop Cut & Slide
    // ========================================================================
    static bool loop_cut(
        SolidMesh3D& mesh,
        uint32_t edge_u,
        uint32_t edge_v,
        const LoopCutOptions& options = {}
    );

    // ========================================================================
    // Task 8 (PH4-05.03.001): Subdivide Face
    // ========================================================================
    static bool subdivide_faces(
        SolidMesh3D& mesh,
        const std::vector<uint32_t>& face_indices,
        uint32_t cuts = 1,
        SubdivideMode mode = SubdivideMode::Linear
    );

    // ========================================================================
    // Task 9 (PH4-05.03.002): Bridge Edge Loops
    // ========================================================================
    static bool bridge_edge_loops(
        SolidMesh3D& mesh,
        const std::vector<uint32_t>& loop_a,
        const std::vector<uint32_t>& loop_b,
        uint32_t segments = 1,
        double twist = 0.0
    );

    // ========================================================================
    // Task 10 (PH4-05.03.003): Vertex Merge
    // ========================================================================
    static size_t merge_vertices(
        SolidMesh3D& mesh,
        const std::vector<uint32_t>& vertex_indices,
        VertexMergeMode mode = VertexMergeMode::AtCenter,
        double distance_threshold = 1e-4
    );

    // ========================================================================
    // Task 1 (PH4-05.03.004): Edge and Face Dissolve
    // ========================================================================
    static bool dissolve_edges(
        SolidMesh3D& mesh,
        const std::vector<std::pair<uint32_t, uint32_t>>& edges
    );

    static bool dissolve_faces(
        SolidMesh3D& mesh,
        const std::vector<uint32_t>& face_indices
    );
};

} // namespace shunya::cad3d
