#include "shunya/physim/collision_pipeline.hpp"
#include "shunya/phy/gjk_epa.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stack>

namespace shunya::physim {

// =============================================================================
// ColliderShape Support Functions
// =============================================================================

Vector3d ColliderShape::support_local(const Vector3d& dir) const noexcept {
    const double len_sq = dir.length_squared();
    if (len_sq < 1e-16) {
        return {0.0, 0.0, 0.0};
    }
    const Vector3d d_norm = dir / std::sqrt(len_sq);

    switch (kind) {
        case ShapeKind::Sphere:
            return d_norm * radius;

        case ShapeKind::Box:
            return {
                (dir.x >= 0.0) ? half_extents.x : -half_extents.x,
                (dir.y >= 0.0) ? half_extents.y : -half_extents.y,
                (dir.z >= 0.0) ? half_extents.z : -half_extents.z
            };

        case ShapeKind::Capsule: {
            const double y_core = (dir.y >= 0.0) ? half_height : -half_height;
            return Vector3d{0.0, y_core, 0.0} + d_norm * radius;
        }

        case ShapeKind::Cylinder: {
            const double y_cap = (dir.y >= 0.0) ? half_height : -half_height;
            const double xz_len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            Vector3d radial{0.0, 0.0, 0.0};
            if (xz_len > 1e-12) {
                radial.x = (dir.x / xz_len) * radius;
                radial.z = (dir.z / xz_len) * radius;
            }
            return Vector3d{radial.x, y_cap, radial.z};
        }

        case ShapeKind::ConvexPolyhedron: {
            if (vertices.empty()) {
                return {0.0, 0.0, 0.0};
            }
            double max_dot = -std::numeric_limits<double>::infinity();
            size_t best_idx = 0;
            for (size_t i = 0; i < vertices.size(); ++i) {
                const double dot_val = vertices[i].dot(dir);
                if (dot_val > max_dot) {
                    max_dot = dot_val;
                    best_idx = i;
                }
            }
            return vertices[best_idx];
        }
    }

    return {0.0, 0.0, 0.0};
}

Vector3d ColliderShape::support_world(
    const Vector3d& dir_world,
    const Vector3d& pos,
    const Quaterniond& orient) const noexcept {
    const Quaterniond q_norm = orient.normalized();
    const Vector3d dir_local = q_norm.conjugate().rotate(dir_world);
    const Vector3d sup_local = support_local(dir_local);
    return pos + q_norm.rotate(sup_local);
}

AABB ColliderShape::compute_world_aabb(
    const Vector3d& pos,
    const Quaterniond& orient,
    double margin) const noexcept {
    const Vector3d sup_px = support_world({1.0, 0.0, 0.0}, pos, orient);
    const Vector3d sup_nx = support_world({-1.0, 0.0, 0.0}, pos, orient);
    const Vector3d sup_py = support_world({0.0, 1.0, 0.0}, pos, orient);
    const Vector3d sup_ny = support_world({0.0, -1.0, 0.0}, pos, orient);
    const Vector3d sup_pz = support_world({0.0, 0.0, 1.0}, pos, orient);
    const Vector3d sup_nz = support_world({0.0, 0.0, -1.0}, pos, orient);

    AABB box;
    box.min_point = {
        sup_nx.x - margin,
        sup_ny.y - margin,
        sup_nz.z - margin
    };
    box.max_point = {
        sup_px.x + margin,
        sup_py.y + margin,
        sup_pz.z + margin
    };
    return box;
}

double ColliderShape::bounding_radius() const noexcept {
    switch (kind) {
        case ShapeKind::Sphere:
            return radius;
        case ShapeKind::Box:
            return half_extents.length();
        case ShapeKind::Capsule:
            return half_height + radius;
        case ShapeKind::Cylinder:
            return std::sqrt(half_height * half_height + radius * radius);
        case ShapeKind::ConvexPolyhedron: {
            double max_r = 0.0;
            for (const auto& v : vertices) {
                max_r = std::max(max_r, v.length());
            }
            return max_r;
        }
    }
    return radius;
}

// =============================================================================
// DynamicAabbBvh (PH4-09.02.001)
// =============================================================================

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

} // namespace

void DynamicAabbBvh::clear() noexcept {
    nodes_.clear();
    root_ = -1;
}

void DynamicAabbBvh::build(const std::vector<AABB>& boxes) {
    clear();
    nodes_.reserve(boxes.size() * 2);
    for (size_t i = 0; i < boxes.size(); ++i) {
        insert_leaf(boxes[i], static_cast<int>(i));
    }
}

void DynamicAabbBvh::insert_leaf(const AABB& box, int body_index) {
    const int leaf_idx = static_cast<int>(nodes_.size());
    BvhNode leaf;
    leaf.aabb = box;
    leaf.body_index = body_index;
    nodes_.push_back(leaf);

    if (root_ == -1) {
        root_ = leaf_idx;
        return;
    }

    // Surface Area Heuristic (SAH) greedy traversal
    int current = root_;
    while (!nodes_[static_cast<size_t>(current)].is_leaf()) {
        const auto& curr_node = nodes_[static_cast<size_t>(current)];
        const int left = curr_node.left;
        const int right = curr_node.right;

        const double curr_sa = surface_area(curr_node.aabb);
        const double comb_sa = surface_area(combine_aabb(curr_node.aabb, box));
        const double cost_create = 2.0 * comb_sa;
        const double inherit_cost = 2.0 * (comb_sa - curr_sa);

        auto eval_cost = [&](int child_idx) -> double {
            const auto& child = nodes_[static_cast<size_t>(child_idx)];
            const double new_sa = surface_area(combine_aabb(child.aabb, box));
            if (child.is_leaf()) {
                return new_sa + inherit_cost;
            } else {
                return (new_sa - surface_area(child.aabb)) + inherit_cost;
            }
        };

        const double cost_l = eval_cost(left);
        const double cost_r = eval_cost(right);

        if (cost_create < cost_l && cost_create < cost_r) {
            break;
        }

        current = (cost_l < cost_r) ? left : right;
    }

    // Split sibling and introduce new internal node
    const int sibling = current;
    const int old_parent = nodes_[static_cast<size_t>(sibling)].parent;
    const int new_parent = static_cast<int>(nodes_.size());

    BvhNode parent_node;
    parent_node.parent = old_parent;
    parent_node.aabb = combine_aabb(box, nodes_[static_cast<size_t>(sibling)].aabb);
    parent_node.left = sibling;
    parent_node.right = leaf_idx;
    nodes_.push_back(parent_node);

    nodes_[static_cast<size_t>(sibling)].parent = new_parent;
    nodes_[static_cast<size_t>(leaf_idx)].parent = new_parent;

    if (old_parent != -1) {
        if (nodes_[static_cast<size_t>(old_parent)].left == sibling) {
            nodes_[static_cast<size_t>(old_parent)].left = new_parent;
        } else {
            nodes_[static_cast<size_t>(old_parent)].right = new_parent;
        }
    } else {
        root_ = new_parent;
    }

    // Refit ancestor AABBs
    int walk = new_parent;
    while (walk != -1) {
        const int l = nodes_[static_cast<size_t>(walk)].left;
        const int r = nodes_[static_cast<size_t>(walk)].right;
        nodes_[static_cast<size_t>(walk)].aabb =
            combine_aabb(nodes_[static_cast<size_t>(l)].aabb, nodes_[static_cast<size_t>(r)].aabb);
        walk = nodes_[static_cast<size_t>(walk)].parent;
    }
}

std::vector<std::pair<size_t, size_t>> DynamicAabbBvh::query_overlapping_pairs() const {
    std::vector<std::pair<size_t, size_t>> pairs;
    if (root_ == -1 || nodes_[static_cast<size_t>(root_)].is_leaf()) {
        return pairs;
    }

    std::vector<int> stack;
    stack.reserve(64);

    for (size_t i = 0; i < nodes_.size(); ++i) {
        if (!nodes_[i].is_leaf()) continue;
        const auto& leaf = nodes_[i];
        const int body_a = leaf.body_index;
        const AABB& box_a = leaf.aabb;

        stack.clear();
        stack.push_back(root_);

        while (!stack.empty()) {
            const int curr_idx = stack.back();
            stack.pop_back();
            if (curr_idx == -1) continue;

            const auto& curr = nodes_[static_cast<size_t>(curr_idx)];
            if (!curr.aabb.intersects(box_a)) continue;

            if (curr.is_leaf()) {
                const int body_b = curr.body_index;
                if (body_a < body_b) {
                    pairs.push_back({static_cast<size_t>(body_a), static_cast<size_t>(body_b)});
                }
            } else {
                stack.push_back(curr.left);
                stack.push_back(curr.right);
            }
        }
    }

    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    return pairs;
}

// =============================================================================
// SpatialHashGrid (PH4-09.02.001)
// =============================================================================

uint64_t SpatialHashGrid::hash_cell(int cx, int cy, int cz) noexcept {
    const uint64_t h1 = static_cast<uint64_t>(cx) * 73856093ULL;
    const uint64_t h2 = static_cast<uint64_t>(cy) * 19349663ULL;
    const uint64_t h3 = static_cast<uint64_t>(cz) * 83492791ULL;
    return h1 ^ h2 ^ h3;
}

void SpatialHashGrid::clear() noexcept {
    grid_.clear();
}

void SpatialHashGrid::insert(size_t body_index, const AABB& aabb) {
    const int min_x = static_cast<int>(std::floor(aabb.min_point.x / cell_size_));
    const int max_x = static_cast<int>(std::floor(aabb.max_point.x / cell_size_));
    const int min_y = static_cast<int>(std::floor(aabb.min_point.y / cell_size_));
    const int max_y = static_cast<int>(std::floor(aabb.max_point.y / cell_size_));
    const int min_z = static_cast<int>(std::floor(aabb.min_point.z / cell_size_));
    const int max_z = static_cast<int>(std::floor(aabb.max_point.z / cell_size_));

    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int z = min_z; z <= max_z; ++z) {
                const uint64_t key = hash_cell(x, y, z);
                grid_[key].push_back(body_index);
            }
        }
    }
}

std::vector<std::pair<size_t, size_t>> SpatialHashGrid::query_overlapping_pairs() const {
    std::vector<std::pair<size_t, size_t>> pairs;
    for (const auto& [_, body_list] : grid_) {
        if (body_list.size() < 2) continue;
        for (size_t i = 0; i < body_list.size(); ++i) {
            for (size_t j = i + 1; j < body_list.size(); ++j) {
                const size_t a = body_list[i];
                const size_t b = body_list[j];
                if (a != b) {
                    pairs.push_back({std::min(a, b), std::max(a, b)});
                }
            }
        }
    }
    std::sort(pairs.begin(), pairs.end());
    pairs.erase(std::unique(pairs.begin(), pairs.end()), pairs.end());
    return pairs;
}

// =============================================================================
// GJK & EPA Narrowphase (PH4-09.02.002)
// =============================================================================

namespace {

shunya::phy::ConvexCollider to_phy_collider(
    const ColliderShape& shape,
    const Vector3d& pos,
    const Quaterniond& orient) {
    shunya::phy::ConvexCollider c;
    c.position = pos;
    c.orientation = orient;
    c.radius = shape.radius;
    c.half_height = shape.half_height;
    c.half_extents = shape.half_extents;

    switch (shape.kind) {
        case ShapeKind::Sphere:
            c.kind = shunya::phy::ColliderKind::Sphere;
            break;
        case ShapeKind::Box:
            c.kind = shunya::phy::ColliderKind::Box;
            break;
        case ShapeKind::Capsule:
            c.kind = shunya::phy::ColliderKind::Capsule;
            break;
        case ShapeKind::Cylinder:
        case ShapeKind::ConvexPolyhedron:
            c.kind = shunya::phy::ColliderKind::ConvexHull;
            if (shape.kind == ShapeKind::Cylinder) {
                // Generate cylinder discretization hull points
                constexpr int kSegs = 16;
                c.hull_vertices.reserve(kSegs * 2);
                for (int s = 0; s < kSegs; ++s) {
                    const double theta = (2.0 * std::numbers::pi * s) / kSegs;
                    const double cx = std::cos(theta) * shape.radius;
                    const double cz = std::sin(theta) * shape.radius;
                    c.hull_vertices.push_back({cx, shape.half_height, cz});
                    c.hull_vertices.push_back({cx, -shape.half_height, cz});
                }
            } else {
                c.hull_vertices = shape.vertices;
            }
            break;
    }
    return c;
}

} // namespace

GjkEpaOutput GjkEpaNarrowphase::detect(
    const ColliderShape& shape_a, const Vector3d& pos_a, const Quaterniond& orient_a,
    const ColliderShape& shape_b, const Vector3d& pos_b, const Quaterniond& orient_b) noexcept {
    const auto ca = to_phy_collider(shape_a, pos_a, orient_a);
    const auto cb = to_phy_collider(shape_b, pos_b, orient_b);

    const auto phy_res = shunya::phy::GjkEpaSolver::detect(ca, cb);

    GjkEpaOutput out;
    out.intersecting = phy_res.intersecting;
    out.distance = phy_res.distance;
    out.penetration_depth = phy_res.penetration_depth;
    out.normal = phy_res.normal;
    out.point_a = phy_res.point_a;
    out.point_b = phy_res.point_b;
    return out;
}

// =============================================================================
// Continuous Collision Detection (CCD) (PH4-09.02.003)
// =============================================================================

CcdOutput ContinuousCollisionDetector::compute_toi(
    const ColliderShape& shape_a, const MotionTransformSweep& sweep_a,
    const ColliderShape& shape_b, const MotionTransformSweep& sweep_b,
    double tolerance) noexcept {

    CcdOutput out;
    out.hit = false;
    out.toi = 1.0;

    double t = 0.0;
    const double r_a = shape_a.bounding_radius();
    const double r_b = shape_b.bounding_radius();

    const Vector3d v_rel = (sweep_a.end_position - sweep_a.start_position) -
                           (sweep_b.end_position - sweep_b.start_position);
    const double v_rel_mag = v_rel.length();
    const double omega_max = sweep_a.angular_velocity.length() * r_a +
                             sweep_b.angular_velocity.length() * r_b;
    const double max_approach_speed = v_rel_mag + omega_max;

    if (max_approach_speed < 1e-6) {
        // Static relative geometry: run instantaneous test
        const auto res = GjkEpaNarrowphase::detect(
            shape_a, sweep_a.start_position, sweep_a.start_orientation,
            shape_b, sweep_b.start_position, sweep_b.start_orientation);
        if (res.intersecting) {
            out.hit = true;
            out.toi = 0.0;
            out.hit_normal = res.normal;
            out.hit_point = (res.point_a + res.point_b) * 0.5;
        }
        return out;
    }

    for (int step = 0; step < kMaxCcdSteps && t <= 1.0; ++step) {
        const Vector3d pos_a = sweep_a.start_position +
            (sweep_a.end_position - sweep_a.start_position) * t;
        const Quaterniond ori_a = Quaterniond::slerp(sweep_a.start_orientation, sweep_a.end_orientation, t);

        const Vector3d pos_b = sweep_b.start_position +
            (sweep_b.end_position - sweep_b.start_position) * t;
        const Quaterniond ori_b = Quaterniond::slerp(sweep_b.start_orientation, sweep_b.end_orientation, t);

        const auto res = GjkEpaNarrowphase::detect(shape_a, pos_a, ori_a, shape_b, pos_b, ori_b);

        if (res.intersecting || res.distance <= tolerance) {
            out.hit = true;
            out.toi = t;
            out.hit_normal = res.normal;
            out.hit_point = (res.point_a + res.point_b) * 0.5;
            return out;
        }

        // Conservative advancement step delta_t = dist / max_speed
        const double dt_adv = std::max(res.distance / max_approach_speed, 1e-4);
        t += dt_adv;
    }

    return out;
}

// =============================================================================
// Contact Manifold Generation (PH4-09.02.004)
// =============================================================================

PhysimContactManifold ContactManifoldGenerator::generate_manifold(
    const std::string& id_a, const ColliderShape& shape_a, const RigidBodyState& state_a,
    const std::string& id_b, const ColliderShape& shape_b, const RigidBodyState& state_b,
    double restitution,
    double static_friction,
    double dynamic_friction) noexcept {

    PhysimContactManifold manifold;
    manifold.body_a_id = id_a;
    manifold.body_b_id = id_b;
    manifold.restitution = restitution;
    manifold.static_friction = static_friction;
    manifold.dynamic_friction = dynamic_friction;

    const auto res = GjkEpaNarrowphase::detect(
        shape_a, state_a.position, state_a.orientation,
        shape_b, state_b.position, state_b.orientation);

    if (!res.intersecting && res.distance > 0.005) {
        return manifold;
    }

    const Vector3d primary_pt = (res.point_a + res.point_b) * 0.5;

    PhysimContactPoint cp;
    cp.world_position = primary_pt;
    cp.normal = res.normal;
    cp.penetration_depth = res.penetration_depth;
    cp.local_anchor_a = state_a.world_to_local_point(primary_pt);
    cp.local_anchor_b = state_b.world_to_local_point(primary_pt);
    manifold.contacts.push_back(cp);

    // If both shapes are boxes, generate a 4-point contact manifold patch
    if (shape_a.kind == ShapeKind::Box && shape_b.kind == ShapeKind::Box && res.penetration_depth > 1e-5) {
        Vector3d tangent1{0.0, 0.0, 0.0};
        if (std::abs(res.normal.x) < 0.8) {
            tangent1 = res.normal.cross({1.0, 0.0, 0.0}).normalized();
        } else {
            tangent1 = res.normal.cross({0.0, 1.0, 0.0}).normalized();
        }
        const Vector3d tangent2 = res.normal.cross(tangent1).normalized();

        const double patch_offset = 0.5 * std::min({
            shape_a.half_extents.x, shape_a.half_extents.y, shape_a.half_extents.z,
            shape_b.half_extents.x, shape_b.half_extents.y, shape_b.half_extents.z
        });

        if (patch_offset > 0.01) {
            manifold.contacts.clear();
            const std::array<std::pair<double, double>, 4> offsets = {
                std::make_pair( patch_offset,  patch_offset),
                std::make_pair( patch_offset, -patch_offset),
                std::make_pair(-patch_offset,  patch_offset),
                std::make_pair(-patch_offset, -patch_offset)
            };

            for (const auto& [o1, o2] : offsets) {
                PhysimContactPoint patch_cp;
                patch_cp.world_position = primary_pt + tangent1 * o1 + tangent2 * o2;
                patch_cp.normal = res.normal;
                patch_cp.penetration_depth = res.penetration_depth;
                patch_cp.local_anchor_a = state_a.world_to_local_point(patch_cp.world_position);
                patch_cp.local_anchor_b = state_b.world_to_local_point(patch_cp.world_position);
                manifold.contacts.push_back(patch_cp);
            }
        }
    }

    return manifold;
}

void ContactManifoldGenerator::update_manifold_coherence(
    PhysimContactManifold& manifold,
    const RigidBodyState& state_a,
    const RigidBodyState& state_b,
    double max_separation_drift) noexcept {

    std::vector<PhysimContactPoint> kept_contacts;
    kept_contacts.reserve(manifold.contacts.size());

    for (auto& cp : manifold.contacts) {
        const Vector3d cur_pt_a = state_a.local_to_world_point(cp.local_anchor_a);
        const Vector3d cur_pt_b = state_b.local_to_world_point(cp.local_anchor_b);
        const double separation = (cur_pt_a - cur_pt_b).length();

        if (separation <= max_separation_drift) {
            cp.world_position = (cur_pt_a + cur_pt_b) * 0.5;
            kept_contacts.push_back(cp);
        }
    }

    manifold.contacts = std::move(kept_contacts);
}

} // namespace shunya::physim
