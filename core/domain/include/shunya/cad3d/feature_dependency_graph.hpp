#pragma once

#include "shunya/cad2d/polygon_boolean.hpp"
#include "shunya/cad3d/csg_boolean.hpp"
#include "shunya/cad3d/extrusion_feature.hpp"
#include "shunya/cad3d/revolve_feature.hpp"
#include "shunya/cad3d/sweep_feature.hpp"
#include "shunya/cad3d/solid_mesh.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace shunya::cad3d {

class FeatureDependencyGraph;

enum class FeatureNodeType {
    Sketch2D,
    Extrude,
    Revolve,
    Sweep,
    Boolean
};

/**
 * @brief Base parametric feature node participating in CAD 2D-to-3D dependency graph.
 */
class FeatureNode {
public:
    FeatureNode(std::string id, FeatureNodeType type, std::string name = "")
        : id_(std::move(id)), type_(type), name_(std::move(name)) {}

    virtual ~FeatureNode() = default;

    [[nodiscard]] const std::string& id() const noexcept { return id_; }
    [[nodiscard]] FeatureNodeType type() const noexcept { return type_; }
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void set_name(std::string name) { name_ = std::move(name); }

    [[nodiscard]] bool is_dirty() const noexcept { return is_dirty_; }
    void mark_dirty() noexcept { is_dirty_ = true; }
    void clear_dirty() noexcept { is_dirty_ = false; }

    [[nodiscard]] const std::vector<std::string>& dependencies() const noexcept { return dependencies_; }
    void add_dependency(std::string dep_id) { dependencies_.push_back(std::move(dep_id)); }

    virtual void evaluate(FeatureDependencyGraph& graph) = 0;
    [[nodiscard]] virtual const SolidMesh3D* mesh() const noexcept { return nullptr; }

protected:
    std::string id_;
    FeatureNodeType type_;
    std::string name_;
    bool is_dirty_{true};
    std::vector<std::string> dependencies_;
};

/**
 * @brief 2D Sketch node providing planar profile inputs for downstream 3D solid operations.
 */
class Sketch2DNode : public FeatureNode {
public:
    Sketch2DNode(std::string id, std::string name = "")
        : FeatureNode(std::move(id), FeatureNodeType::Sketch2D, std::move(name)) {}

    [[nodiscard]] const cad2d::Path2D& outer() const noexcept { return outer_; }
    [[nodiscard]] const cad2d::Paths2D& holes() const noexcept { return holes_; }
    [[nodiscard]] const SketchPlane3D& plane() const noexcept { return plane_; }

    void set_outer(cad2d::Path2D outer, FeatureDependencyGraph* graph = nullptr);
    void set_holes(cad2d::Paths2D holes, FeatureDependencyGraph* graph = nullptr);
    void set_plane(SketchPlane3D plane, FeatureDependencyGraph* graph = nullptr);

    void evaluate(FeatureDependencyGraph& graph) override;

private:
    cad2d::Path2D outer_;
    cad2d::Paths2D holes_;
    SketchPlane3D plane_{SketchPlane3D::xy()};
};

/**
 * @brief Extrude 3D solid feature node depending on a Sketch2D profile.
 */
class ExtrudeFeatureNode : public FeatureNode {
public:
    ExtrudeFeatureNode(std::string id, std::string sketch_id, ExtrusionParams params, std::string name = "")
        : FeatureNode(std::move(id), FeatureNodeType::Extrude, std::move(name)),
          sketch_id_(sketch_id), params_(std::move(params)) {
        add_dependency(std::move(sketch_id));
    }

    [[nodiscard]] const ExtrusionParams& params() const noexcept { return params_; }
    void set_params(ExtrusionParams params, FeatureDependencyGraph* graph = nullptr);

    [[nodiscard]] const SolidMesh3D* mesh() const noexcept override { return &mesh_; }
    void evaluate(FeatureDependencyGraph& graph) override;

private:
    std::string sketch_id_;
    ExtrusionParams params_;
    SolidMesh3D mesh_;
};

/**
 * @brief Revolve 3D solid feature node depending on a Sketch2D profile.
 */
class RevolveFeatureNode : public FeatureNode {
public:
    RevolveFeatureNode(std::string id, std::string sketch_id, RevolveParams params, std::string name = "")
        : FeatureNode(std::move(id), FeatureNodeType::Revolve, std::move(name)),
          sketch_id_(sketch_id), params_(std::move(params)) {
        add_dependency(std::move(sketch_id));
    }

    [[nodiscard]] const RevolveParams& params() const noexcept { return params_; }
    void set_params(RevolveParams params, FeatureDependencyGraph* graph = nullptr);

    [[nodiscard]] const SolidMesh3D* mesh() const noexcept override { return &mesh_; }
    void evaluate(FeatureDependencyGraph& graph) override;

private:
    std::string sketch_id_;
    RevolveParams params_;
    SolidMesh3D mesh_;
};

/**
 * @brief Sweep 3D solid feature node depending on a Sketch2D profile and 3D guide curve.
 */
class SweepFeatureNode : public FeatureNode {
public:
    SweepFeatureNode(
        std::string id,
        std::string sketch_id,
        std::vector<Point3D> trajectory,
        SweepParams params = {},
        std::string name = ""
    ) : FeatureNode(std::move(id), FeatureNodeType::Sweep, std::move(name)),
        sketch_id_(sketch_id), trajectory_(std::move(trajectory)), params_(params) {
        add_dependency(std::move(sketch_id));
    }

    [[nodiscard]] const SolidMesh3D* mesh() const noexcept override { return &mesh_; }
    void evaluate(FeatureDependencyGraph& graph) override;

private:
    std::string sketch_id_;
    std::vector<Point3D> trajectory_;
    SweepParams params_;
    SolidMesh3D mesh_;
};

/**
 * @brief Boolean CSG feature node combining two upstream 3D solid meshes.
 */
class BooleanFeatureNode : public FeatureNode {
public:
    BooleanFeatureNode(
        std::string id,
        std::string solid_a_id,
        std::string solid_b_id,
        CsgOp op,
        std::string name = ""
    ) : FeatureNode(std::move(id), FeatureNodeType::Boolean, std::move(name)),
        solid_a_id_(solid_a_id), solid_b_id_(solid_b_id), op_(op) {
        add_dependency(std::move(solid_a_id));
        add_dependency(std::move(solid_b_id));
    }

    [[nodiscard]] const SolidMesh3D* mesh() const noexcept override { return &mesh_; }
    void evaluate(FeatureDependencyGraph& graph) override;

private:
    std::string solid_a_id_;
    std::string solid_b_id_;
    CsgOp op_;
    SolidMesh3D mesh_;
};

/**
 * @brief Directed Acyclic Graph (DAG) manager for parametric feature dependencies and reactive updates.
 */
class FeatureDependencyGraph {
public:
    using RegenerationCallback = std::function<void(const std::string& node_id, const SolidMesh3D& mesh)>;

    FeatureDependencyGraph() = default;

    bool add_node(std::shared_ptr<FeatureNode> node);
    [[nodiscard]] std::shared_ptr<FeatureNode> get_node(const std::string& id) const;

    void mark_dirty(const std::string& node_id);
    void evaluate();
    void evaluate_node(const std::string& id);

    [[nodiscard]] size_t node_count() const noexcept { return nodes_.size(); }
    [[nodiscard]] size_t dirty_count() const noexcept;
    [[nodiscard]] bool has_cycle() const;

    void set_on_regenerated(RegenerationCallback callback) {
        on_regenerated_ = std::move(callback);
    }

    void notify_regenerated(const std::string& id, const SolidMesh3D& mesh) {
        if (on_regenerated_) {
            on_regenerated_(id, mesh);
        }
    }

private:
    [[nodiscard]] std::vector<std::string> topological_sort() const;

    std::unordered_map<std::string, std::shared_ptr<FeatureNode>> nodes_;
    std::unordered_map<std::string, std::vector<std::string>> downstream_dependents_;
    RegenerationCallback on_regenerated_;
};

} // namespace shunya::cad3d
