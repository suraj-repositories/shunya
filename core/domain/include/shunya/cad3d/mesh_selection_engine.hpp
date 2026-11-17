#pragma once

#include "shunya/cad3d/raycast_query.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace shunya::cad3d {

enum class MeshSelectionMode {
    Vertex,
    Edge,
    Face,
    Object
};

struct PickResult {
    bool hit{false};
    MeshSelectionMode mode{MeshSelectionMode::Object};
    uint32_t element_index{0};
    std::pair<uint32_t, uint32_t> edge{0, 0};
    Point3D hit_point{0.0, 0.0, 0.0};
    double distance{std::numeric_limits<double>::infinity()};
};

class MeshSelection {
public:
    MeshSelectionMode mode{MeshSelectionMode::Object};
    bool object_selected{false};
    std::vector<uint32_t> selected_vertices;
    std::vector<std::pair<uint32_t, uint32_t>> selected_edges; // normalized u < v
    std::vector<uint32_t> selected_faces;

    void clear() noexcept;
    [[nodiscard]] bool empty() const noexcept;

    void set_mode(MeshSelectionMode new_mode) noexcept { mode = new_mode; }

    void select_vertex(uint32_t v_idx, bool toggle = false);
    void select_edge(uint32_t u, uint32_t v, bool toggle = false);
    void select_face(uint32_t f_idx, bool toggle = false);
    void select_object(bool selected = true) noexcept { object_selected = selected; }

    void select_all(const SolidMesh3D& mesh);
    void deselect_all() noexcept { clear(); }

    [[nodiscard]] bool is_vertex_selected(uint32_t v_idx) const noexcept;
    [[nodiscard]] bool is_edge_selected(uint32_t u, uint32_t v) const noexcept;
    [[nodiscard]] bool is_face_selected(uint32_t f_idx) const noexcept;
};

class MeshSelectionEngine {
public:
    /**
     * @brief Performs raycast picking on a SolidMesh3D according to the active selection mode.
     */
    [[nodiscard]] static PickResult pick(
        const SolidMesh3D& mesh,
        const Ray3D& ray,
        MeshSelectionMode mode,
        double max_distance = 1e6
    );

    /**
     * @brief Performs pick and applies the result directly to the MeshSelection state.
     */
    static bool pick_and_select(
        const SolidMesh3D& mesh,
        const Ray3D& ray,
        MeshSelection& selection,
        bool toggle = false,
        double max_distance = 1e6
    );

    /**
     * @brief Box/Frustum selection picking all elements inside a 3D bounding box.
     */
    static void select_in_box(
        const SolidMesh3D& mesh,
        const BBox3D& box,
        MeshSelection& selection,
        bool additive = false
    );
};

} // namespace shunya::cad3d
