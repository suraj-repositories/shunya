#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/rig3d/armature.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace shunya::rig3d {

struct VertexWeight {
    uint32_t bone_index{0};
    double weight{0.0};
};

struct VertexSkinningData {
    std::vector<VertexWeight> weights;

    void normalize() noexcept {
        double sum = 0.0;
        for (const auto& w : weights) {
            sum += w.weight;
        }
        if (sum > 1e-12) {
            for (auto& w : weights) {
                w.weight /= sum;
            }
        } else if (!weights.empty()) {
            weights[0].weight = 1.0;
        }
    }

    [[nodiscard]] double get_bone_weight(uint32_t bone_idx) const noexcept {
        for (const auto& w : weights) {
            if (w.bone_index == bone_idx) return w.weight;
        }
        return 0.0;
    }

    void set_bone_weight(uint32_t bone_idx, double val) {
        for (auto& w : weights) {
            if (w.bone_index == bone_idx) {
                w.weight = val;
                return;
            }
        }
        weights.push_back({bone_idx, val});
    }
};

/**
 * @brief Linear Blend Skinning (LBS) and Weight Painting Manager.
 */
class SkinningManager {
public:
    SkinningManager() = default;

    void resize(size_t vertex_count) {
        vertex_data_.resize(vertex_count);
    }

    [[nodiscard]] size_t vertex_count() const noexcept { return vertex_data_.size(); }
    [[nodiscard]] const VertexSkinningData& get_vertex_data(size_t idx) const { return vertex_data_[idx]; }
    [[nodiscard]] VertexSkinningData& get_vertex_data(size_t idx) { return vertex_data_[idx]; }

    /// PH4-11.04.001: Automated distance/heat-map bone weight calculation
    void compute_distance_weights(const SolidMesh3D& mesh, const Armature& armature, double falloff_power = 2.0);

    /// Linear Blend Skinning evaluation: v' = sum_j (w_j * M_j * v)
    [[nodiscard]] SolidMesh3D evaluate_skinning(const SolidMesh3D& rest_mesh, const Armature& armature) const;

    /// PH4-11.04.002: Interactive 3D Weight Painting tool
    void paint_weight(
        const SolidMesh3D& mesh,
        uint32_t target_bone,
        const Point3D& stroke_pos,
        double radius,
        double strength,
        bool subtract = false
    );

    /// Weight to RGB heat-map color: Blue (0.0) -> Cyan (0.25) -> Green (0.5) -> Yellow (0.75) -> Red (1.0)
    [[nodiscard]] static Vec3d weight_to_color(double weight) noexcept;

private:
    std::vector<VertexSkinningData> vertex_data_;
};

} // namespace shunya::rig3d
