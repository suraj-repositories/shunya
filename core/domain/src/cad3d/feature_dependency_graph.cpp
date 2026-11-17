#include "shunya/cad3d/feature_dependency_graph.hpp"

#include <queue>
#include <stdexcept>

namespace shunya::cad3d {

// ============================================================================
// Sketch2DNode Implementation
// ============================================================================

void Sketch2DNode::set_outer(cad2d::Path2D outer, FeatureDependencyGraph* graph) {
    outer_ = std::move(outer);
    mark_dirty();
    if (graph) {
        graph->mark_dirty(id_);
    }
}

void Sketch2DNode::set_holes(cad2d::Paths2D holes, FeatureDependencyGraph* graph) {
    holes_ = std::move(holes);
    mark_dirty();
    if (graph) {
        graph->mark_dirty(id_);
    }
}

void Sketch2DNode::set_plane(SketchPlane3D plane, FeatureDependencyGraph* graph) {
    plane_ = plane;
    mark_dirty();
    if (graph) {
        graph->mark_dirty(id_);
    }
}

void Sketch2DNode::evaluate(FeatureDependencyGraph& /*graph*/) {
    clear_dirty();
}

// ============================================================================
// ExtrudeFeatureNode Implementation
// ============================================================================

void ExtrudeFeatureNode::set_params(ExtrusionParams params, FeatureDependencyGraph* graph) {
    params_ = std::move(params);
    mark_dirty();
    if (graph) {
        graph->mark_dirty(id_);
    }
}

void ExtrudeFeatureNode::evaluate(FeatureDependencyGraph& graph) {
    auto base_node = graph.get_node(sketch_id_);
    auto sketch = std::dynamic_pointer_cast<Sketch2DNode>(base_node);
    if (!sketch) {
        mesh_.clear();
        clear_dirty();
        return;
    }

    mesh_ = ExtrusionEngine::extrude(sketch->outer(), sketch->holes(), params_);
    clear_dirty();
    graph.notify_regenerated(id_, mesh_);
}

// ============================================================================
// RevolveFeatureNode Implementation
// ============================================================================

void RevolveFeatureNode::set_params(RevolveParams params, FeatureDependencyGraph* graph) {
    params_ = std::move(params);
    mark_dirty();
    if (graph) {
        graph->mark_dirty(id_);
    }
}

void RevolveFeatureNode::evaluate(FeatureDependencyGraph& graph) {
    auto base_node = graph.get_node(sketch_id_);
    auto sketch = std::dynamic_pointer_cast<Sketch2DNode>(base_node);
    if (!sketch) {
        mesh_.clear();
        clear_dirty();
        return;
    }

    mesh_ = RevolveEngine::revolve(sketch->outer(), sketch->holes(), params_);
    clear_dirty();
    graph.notify_regenerated(id_, mesh_);
}

// ============================================================================
// SweepFeatureNode Implementation
// ============================================================================

void SweepFeatureNode::evaluate(FeatureDependencyGraph& graph) {
    auto base_node = graph.get_node(sketch_id_);
    auto sketch = std::dynamic_pointer_cast<Sketch2DNode>(base_node);
    if (!sketch) {
        mesh_.clear();
        clear_dirty();
        return;
    }

    mesh_ = SweepEngine::sweep(sketch->outer(), sketch->holes(), trajectory_, params_);
    clear_dirty();
    graph.notify_regenerated(id_, mesh_);
}

// ============================================================================
// BooleanFeatureNode Implementation
// ============================================================================

void BooleanFeatureNode::evaluate(FeatureDependencyGraph& graph) {
    auto node_a = graph.get_node(solid_a_id_);
    auto node_b = graph.get_node(solid_b_id_);
    if (!node_a || !node_b || !node_a->mesh() || !node_b->mesh()) {
        mesh_.clear();
        clear_dirty();
        return;
    }

    mesh_ = CsgBooleanEngine::execute(op_, *node_a->mesh(), *node_b->mesh());
    clear_dirty();
    graph.notify_regenerated(id_, mesh_);
}

// ============================================================================
// FeatureDependencyGraph Implementation
// ============================================================================

bool FeatureDependencyGraph::add_node(std::shared_ptr<FeatureNode> node) {
    if (!node || nodes_.find(node->id()) != nodes_.end()) {
        return false;
    }

    nodes_[node->id()] = node;
    for (const auto& dep : node->dependencies()) {
        downstream_dependents_[dep].push_back(node->id());
    }

    return true;
}

std::shared_ptr<FeatureNode> FeatureDependencyGraph::get_node(const std::string& id) const {
    auto it = nodes_.find(id);
    if (it != nodes_.end()) {
        return it->second;
    }
    return nullptr;
}

void FeatureDependencyGraph::mark_dirty(const std::string& node_id) {
    auto node = get_node(node_id);
    if (!node) return;

    node->mark_dirty();

    // Recursively propagate dirty flag downstream
    std::queue<std::string> q;
    q.push(node_id);

    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();

        auto it = downstream_dependents_.find(curr);
        if (it != downstream_dependents_.end()) {
            for (const auto& down_id : it->second) {
                auto down_node = get_node(down_id);
                if (down_node && !down_node->is_dirty()) {
                    down_node->mark_dirty();
                    q.push(down_id);
                }
            }
        }
    }
}

size_t FeatureDependencyGraph::dirty_count() const noexcept {
    size_t count = 0;
    for (const auto& [_, node] : nodes_) {
        if (node && node->is_dirty()) {
            ++count;
        }
    }
    return count;
}

std::vector<std::string> FeatureDependencyGraph::topological_sort() const {
    std::unordered_map<std::string, size_t> in_degree;
    for (const auto& [id, _] : nodes_) {
        in_degree[id] = 0;
    }

    for (const auto& [_, dependents] : downstream_dependents_) {
        for (const auto& dep : dependents) {
            in_degree[dep]++;
        }
    }

    std::queue<std::string> q;
    for (const auto& [id, deg] : in_degree) {
        if (deg == 0) {
            q.push(id);
        }
    }

    std::vector<std::string> order;
    while (!q.empty()) {
        std::string curr = q.front();
        q.pop();
        order.push_back(curr);

        auto it = downstream_dependents_.find(curr);
        if (it != downstream_dependents_.end()) {
            for (const auto& down_id : it->second) {
                if (--in_degree[down_id] == 0) {
                    q.push(down_id);
                }
            }
        }
    }

    return order;
}

bool FeatureDependencyGraph::has_cycle() const {
    return topological_sort().size() < nodes_.size();
}

void FeatureDependencyGraph::evaluate() {
    auto order = topological_sort();
    for (const auto& id : order) {
        auto node = get_node(id);
        if (node && node->is_dirty()) {
            node->evaluate(*this);
        }
    }
}

void FeatureDependencyGraph::evaluate_node(const std::string& id) {
    auto target = get_node(id);
    if (!target) return;

    // Evaluate prerequisites
    for (const auto& dep_id : target->dependencies()) {
        evaluate_node(dep_id);
    }

    if (target->is_dirty()) {
        target->evaluate(*this);
    }
}

} // namespace shunya::cad3d
