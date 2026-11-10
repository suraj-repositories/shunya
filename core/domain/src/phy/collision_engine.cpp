#include "shunya/phy/collision_engine.hpp"

#include <algorithm>
#include <stack>

namespace shunya::phy {

using namespace core::domain::mechanics;

namespace {

double surface_area(const AABB& b) noexcept {
    const Vector3d d = b.max_point - b.min_point;
    return 2.0 * (d.x * d.y + d.y * d.z + d.z * d.x);
}

AABB combine_aabb(const AABB& a, const AABB& b) noexcept {
    AABB out = a;
    out.expand(b.min_point);
    out.expand(b.max_point);
    return out;
}

class DynamicBvhBuilder {
public:
    std::vector<BroadphaseNode> nodes;
    int root{-1};

    void build(const std::vector<AABB>& boxes) {
        nodes.clear();
        nodes.reserve(boxes.size() * 2);
        root = -1;
        for (int i = 0; i < static_cast<int>(boxes.size()); ++i) {
            insert_leaf(boxes[static_cast<size_t>(i)], i);
        }
    }

    void query_box(const AABB& query, int self_body, std::vector<int>& out) const {
        if (root == -1) return;
        std::vector<int> stack;
        stack.reserve(64);
        stack.push_back(root);
        while (!stack.empty()) {
            const int nid = stack.back();
            stack.pop_back();
            const auto& node = nodes[static_cast<size_t>(nid)];
            if (!node.aabb.intersects(query)) continue;
            if (node.is_leaf()) {
                if (node.body_index > self_body) {
                    out.push_back(node.body_index);
                }
            } else {
                stack.push_back(node.left);
                stack.push_back(node.right);
            }
        }
    }

private:
    void insert_leaf(const AABB& box, int body_idx) {
        const int leaf_idx = static_cast<int>(nodes.size());
        nodes.push_back({box, -1, -1, -1, body_idx});
        if (root == -1) {
            root = leaf_idx;
            return;
        }

        // Greedy SAH traversal to pick best sibling
        int index = root;
        while (!nodes[static_cast<size_t>(index)].is_leaf()) {
            const int left = nodes[static_cast<size_t>(index)].left;
            const int right = nodes[static_cast<size_t>(index)].right;
            const double area = surface_area(nodes[static_cast<size_t>(index)].aabb);
            const AABB combined = combine_aabb(nodes[static_cast<size_t>(index)].aabb, box);
            const double cost = 2.0 * surface_area(combined);
            const double inherit_cost = 2.0 * (surface_area(combined) - area);

            auto child_cost = [&](int c) {
                const AABB merged = combine_aabb(box, nodes[static_cast<size_t>(c)].aabb);
                if (nodes[static_cast<size_t>(c)].is_leaf()) {
                    return surface_area(merged) + inherit_cost;
                }
                return (surface_area(merged) - surface_area(nodes[static_cast<size_t>(c)].aabb)) + inherit_cost;
            };

            const double cl = child_cost(left);
            const double cr = child_cost(right);
            if (cost < cl && cost < cr) break;
            index = (cl < cr) ? left : right;
        }

        const int old_parent = nodes[static_cast<size_t>(index)].parent;
        const int new_parent = static_cast<int>(nodes.size());
        nodes.push_back({combine_aabb(box, nodes[static_cast<size_t>(index)].aabb), old_parent, index, leaf_idx, -1});
        nodes[static_cast<size_t>(index)].parent = new_parent;
        nodes[static_cast<size_t>(leaf_idx)].parent = new_parent;

        if (old_parent != -1) {
            if (nodes[static_cast<size_t>(old_parent)].left == index) {
                nodes[static_cast<size_t>(old_parent)].left = new_parent;
            } else {
                nodes[static_cast<size_t>(old_parent)].right = new_parent;
            }
        } else {
            root = new_parent;
        }

        // Refit ancestor bounds
        int p = new_parent;
        while (p != -1) {
            const int l = nodes[static_cast<size_t>(p)].left;
            const int r = nodes[static_cast<size_t>(p)].right;
            nodes[static_cast<size_t>(p)].aabb = combine_aabb(
                nodes[static_cast<size_t>(l)].aabb,
                nodes[static_cast<size_t>(r)].aabb);
            p = nodes[static_cast<size_t>(p)].parent;
        }
    }
};

} // namespace

std::vector<std::pair<size_t, size_t>> CollisionEngine::find_broadphase_pairs(
    const std::vector<ConvexCollider>& colliders,
    double aabb_margin) {
    std::vector<AABB> aabbs;
    aabbs.reserve(colliders.size());
    for (const auto& c : colliders) {
        aabbs.push_back(c.world_aabb(aabb_margin));
    }

    DynamicBvhBuilder tree;
    tree.build(aabbs);

    std::vector<std::pair<size_t, size_t>> pairs;
    std::vector<int> hits;
    for (size_t i = 0; i < aabbs.size(); ++i) {
        hits.clear();
        tree.query_box(aabbs[i], static_cast<int>(i), hits);
        for (int j : hits) {
            pairs.emplace_back(i, static_cast<size_t>(j));
        }
    }
    return pairs;
}

std::vector<ContactManifold> CollisionEngine::detect_all_contacts(
    const std::vector<ConvexCollider>& colliders,
    double aabb_margin) {
    const auto pairs = find_broadphase_pairs(colliders, aabb_margin);
    std::vector<ContactManifold> manifolds;

    for (const auto& [i, j] : pairs) {
        const auto res = GjkEpaSolver::detect(colliders[i], colliders[j]);
        if (res.intersecting && res.penetration_depth > 0.0) {
            ContactManifold m;
            m.body_a_id = std::to_string(i);
            m.body_b_id = std::to_string(j);
            ContactPoint pt;
            pt.world_position = (res.point_a + res.point_b) * 0.5;
            pt.normal = res.normal;
            pt.penetration_depth = res.penetration_depth;
            m.contacts.push_back(pt);
            manifolds.push_back(std::move(m));
        }
    }
    return manifolds;
}

} // namespace shunya::phy
