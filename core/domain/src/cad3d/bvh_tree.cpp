#include "shunya/cad3d/bvh_tree.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <unordered_set>

namespace shunya::cad3d {

namespace {

inline double bbox_surface_area(const BBox3D& b) noexcept {
    const Vec3d d = b.size();
    return 2.0 * (d.x * d.y + d.y * d.z + d.z * d.x);
}

inline Vec3d safe_inv_dir(const Vec3d& d) noexcept {
    constexpr double INF = 1e30;
    return {
        std::abs(d.x) > 1e-18 ? (1.0 / d.x) : (d.x >= 0.0 ? INF : -INF),
        std::abs(d.y) > 1e-18 ? (1.0 / d.y) : (d.y >= 0.0 ? INF : -INF),
        std::abs(d.z) > 1e-18 ? (1.0 / d.z) : (d.z >= 0.0 ? INF : -INF)
    };
}

struct PrimInfo {
    BBox3D box;
    Point3D centroid;
};

struct SahBucket {
    uint32_t count{0};
    BBox3D bounds;
};

uint32_t build_bvh_recursive(
    std::vector<LinearBvhNode>& nodes,
    std::vector<uint32_t>& prim_indices,
    const std::vector<PrimInfo>& prims,
    uint32_t start,
    uint32_t end,
    uint32_t max_leaf_tris
) {
    const uint32_t node_idx = static_cast<uint32_t>(nodes.size());
    nodes.emplace_back();

    BBox3D node_box;
    BBox3D centroid_box;
    for (uint32_t i = start; i < end; ++i) {
        const auto& info = prims[prim_indices[i]];
        node_box.expand(info.box);
        centroid_box.expand(info.centroid);
    }
    nodes[node_idx].bounds = node_box;

    const uint32_t count = end - start;
    if (count <= max_leaf_tris) {
        nodes[node_idx].second_child_or_prim = start;
        nodes[node_idx].prim_count = static_cast<uint16_t>(count);
        return node_idx;
    }

    const Vec3d c_ext = centroid_box.size();
    uint8_t best_axis = 0;
    if (c_ext.y > c_ext.x && c_ext.y >= c_ext.z) best_axis = 1;
    else if (c_ext.z > c_ext.x && c_ext.z > c_ext.y) best_axis = 2;

    const auto coord_at = [](const Point3D& p, uint8_t ax) noexcept {
        return (ax == 0) ? p.x : ((ax == 1) ? p.y : p.z);
    };

    const double c_min = coord_at(centroid_box.min, best_axis);
    const double c_max = coord_at(centroid_box.max, best_axis);
    const double c_span = c_max - c_min;

    if (c_span <= 1e-12) {
        // Fallback: split equally by index if all centroids coincide
        const uint32_t mid = start + count / 2;
        nodes[node_idx].axis = best_axis;
        nodes[node_idx].prim_count = 0;
        build_bvh_recursive(nodes, prim_indices, prims, start, mid, max_leaf_tris);
        const uint32_t right_idx = build_bvh_recursive(nodes, prim_indices, prims, mid, end, max_leaf_tris);
        nodes[node_idx].second_child_or_prim = right_idx;
        return node_idx;
    }

    // 12-bucket Surface Area Heuristic (SAH) evaluation across dominant axis
    constexpr int NUM_BUCKETS = 12;
    SahBucket buckets[NUM_BUCKETS];
    for (uint32_t i = start; i < end; ++i) {
        const auto& info = prims[prim_indices[i]];
        int b = static_cast<int>(NUM_BUCKETS * ((coord_at(info.centroid, best_axis) - c_min) / c_span));
        if (b >= NUM_BUCKETS) b = NUM_BUCKETS - 1;
        if (b < 0) b = 0;
        ++buckets[b].count;
        buckets[b].bounds.expand(info.box);
    }

    double costs[NUM_BUCKETS - 1]{};
    const double parent_sa = std::max(1e-12, bbox_surface_area(node_box));
    for (int s = 0; s < NUM_BUCKETS - 1; ++s) {
        BBox3D b0, b1;
        uint32_t c0 = 0, c1 = 0;
        for (int j = 0; j <= s; ++j) {
            b0.expand(buckets[j].bounds);
            c0 += buckets[j].count;
        }
        for (int j = s + 1; j < NUM_BUCKETS; ++j) {
            b1.expand(buckets[j].bounds);
            c1 += buckets[j].count;
        }
        if (c0 == 0 || c1 == 0) {
            costs[s] = std::numeric_limits<double>::infinity();
        } else {
            costs[s] = 0.125 + (c0 * bbox_surface_area(b0) + c1 * bbox_surface_area(b1)) / parent_sa;
        }
    }

    int min_bucket = 0;
    double min_cost = costs[0];
    for (int s = 1; s < NUM_BUCKETS - 1; ++s) {
        if (costs[s] < min_cost) {
            min_cost = costs[s];
            min_bucket = s;
        }
    }

    const double leaf_cost = static_cast<double>(count);
    uint32_t mid = start;
    if (count > max_leaf_tris || min_cost < leaf_cost) {
        auto it = std::partition(
            prim_indices.begin() + static_cast<std::ptrdiff_t>(start),
            prim_indices.begin() + static_cast<std::ptrdiff_t>(end),
            [&](uint32_t pid) {
                int b = static_cast<int>(NUM_BUCKETS * ((coord_at(prims[pid].centroid, best_axis) - c_min) / c_span));
                if (b >= NUM_BUCKETS) b = NUM_BUCKETS - 1;
                if (b < 0) b = 0;
                return b <= min_bucket;
            }
        );
        mid = static_cast<uint32_t>(std::distance(prim_indices.begin(), it));
        if (mid == start || mid == end) {
            mid = start + count / 2;
        }
    } else {
        nodes[node_idx].second_child_or_prim = start;
        nodes[node_idx].prim_count = static_cast<uint16_t>(count);
        return node_idx;
    }

    nodes[node_idx].axis = best_axis;
    nodes[node_idx].prim_count = 0;
    build_bvh_recursive(nodes, prim_indices, prims, start, mid, max_leaf_tris);
    const uint32_t right_idx = build_bvh_recursive(nodes, prim_indices, prims, mid, end, max_leaf_tris);
    nodes[node_idx].second_child_or_prim = right_idx;
    return node_idx;
}

} // namespace

void LinearBvh3D::clear() noexcept {
    mesh_ = nullptr;
    nodes_.clear();
    prim_indices_.clear();
}

void LinearBvh3D::build(const SolidMesh3D& mesh, uint32_t max_leaf_tris) {
    clear();
    mesh_ = &mesh;
    const size_t n_tris = mesh.triangles.size();
    if (n_tris == 0 || mesh.vertices.empty()) return;

    std::vector<PrimInfo> prims(n_tris);
    prim_indices_.resize(n_tris);
    for (size_t i = 0; i < n_tris; ++i) {
        const auto& t = mesh.triangles[i];
        BBox3D b;
        b.expand(mesh.vertices[t[0]]);
        b.expand(mesh.vertices[t[1]]);
        b.expand(mesh.vertices[t[2]]);
        prims[i].box = b;
        prims[i].centroid = (mesh.vertices[t[0]] + mesh.vertices[t[1]] + mesh.vertices[t[2]]) * (1.0 / 3.0);
        prim_indices_[i] = static_cast<uint32_t>(i);
    }

    nodes_.reserve(2 * n_tris);
    build_bvh_recursive(nodes_, prim_indices_, prims, 0, static_cast<uint32_t>(n_tris), std::max<uint32_t>(1, max_leaf_tris));
}

RayHit3D LinearBvh3D::raycast(const Ray3D& ray, double max_distance) const noexcept {
    RayHit3D best;
    if (!mesh_ || nodes_.empty()) return best;
    best.distance = max_distance;

    const Vec3d inv_dir = safe_inv_dir(ray.direction);
    const bool dir_is_neg[3] = {inv_dir.x < 0.0, inv_dir.y < 0.0, inv_dir.z < 0.0};

    uint32_t stack[64];
    int stack_ptr = 0;
    uint32_t cur_idx = 0;

    while (true) {
        const LinearBvhNode& node = nodes_[cur_idx];
        if (intersect_ray_aabb_fast(ray, inv_dir, node.bounds, best.distance)) {
            if (node.prim_count > 0) {
                for (uint32_t i = 0; i < node.prim_count; ++i) {
                    const uint32_t tid = prim_indices_[node.second_child_or_prim + i];
                    const auto& tri = mesh_->triangles[tid];
                    double t = 0.0, u = 0.0, v = 0.0;
                    if (intersect_ray_triangle_mt(
                            ray,
                            mesh_->vertices[tri[0]],
                            mesh_->vertices[tri[1]],
                            mesh_->vertices[tri[2]],
                            best.distance,
                            t, u, v)) {
                        best.hit = true;
                        best.distance = t;
                        best.u = u;
                        best.v = v;
                        best.triangle_index = tid;
                    }
                }
                if (stack_ptr == 0) break;
                cur_idx = stack[--stack_ptr];
            } else {
                if (dir_is_neg[node.axis]) {
                    stack[stack_ptr++] = cur_idx + 1;
                    cur_idx = node.second_child_or_prim;
                } else {
                    stack[stack_ptr++] = node.second_child_or_prim;
                    cur_idx = cur_idx + 1;
                }
            }
        } else {
            if (stack_ptr == 0) break;
            cur_idx = stack[--stack_ptr];
        }
    }

    if (best.hit) {
        best.point = ray.origin + ray.direction * best.distance;
        best.normal = mesh_->triangle_normal(best.triangle_index);
    } else {
        best.distance = std::numeric_limits<double>::infinity();
    }
    return best;
}

bool LinearBvh3D::any_hit(const Ray3D& ray, double max_distance) const noexcept {
    if (!mesh_ || nodes_.empty()) return false;
    const Vec3d inv_dir = safe_inv_dir(ray.direction);

    uint32_t stack[64];
    int stack_ptr = 0;
    uint32_t cur_idx = 0;

    while (true) {
        const LinearBvhNode& node = nodes_[cur_idx];
        if (intersect_ray_aabb_fast(ray, inv_dir, node.bounds, max_distance)) {
            if (node.prim_count > 0) {
                for (uint32_t i = 0; i < node.prim_count; ++i) {
                    const uint32_t tid = prim_indices_[node.second_child_or_prim + i];
                    const auto& tri = mesh_->triangles[tid];
                    double t = 0.0, u = 0.0, v = 0.0;
                    if (intersect_ray_triangle_mt(
                            ray,
                            mesh_->vertices[tri[0]],
                            mesh_->vertices[tri[1]],
                            mesh_->vertices[tri[2]],
                            max_distance,
                            t, u, v)) {
                        return true;
                    }
                }
                if (stack_ptr == 0) break;
                cur_idx = stack[--stack_ptr];
            } else {
                stack[stack_ptr++] = node.second_child_or_prim;
                cur_idx = cur_idx + 1;
            }
        } else {
            if (stack_ptr == 0) break;
            cur_idx = stack[--stack_ptr];
        }
    }
    return false;
}

bool LinearBvh3D::contains_point(const Point3D& pt) const noexcept {
    if (!mesh_ || nodes_.empty()) return false;

    const Vec3d dirs[3] = {
        Vec3d{0.7131, 0.5217, 0.4682}.normalized(),
        Vec3d{-0.3917, 0.8412, -0.3729}.normalized(),
        Vec3d{0.2891, -0.4519, 0.8439}.normalized()
    };

    int inside_votes = 0;
    for (int d_idx = 0; d_idx < 3; ++d_idx) {
        const Ray3D ray{pt, dirs[d_idx]};
        const Vec3d inv_dir = safe_inv_dir(ray.direction);

        uint32_t stack[64];
        int stack_ptr = 0;
        uint32_t cur_idx = 0;
        double hit_ts[64];
        int hit_count = 0;

        while (true) {
            const LinearBvhNode& node = nodes_[cur_idx];
            if (intersect_ray_aabb_fast(ray, inv_dir, node.bounds, 1e30)) {
                if (node.prim_count > 0) {
                    for (uint32_t i = 0; i < node.prim_count; ++i) {
                        const uint32_t tid = prim_indices_[node.second_child_or_prim + i];
                        const auto& tri = mesh_->triangles[tid];
                        double t = 0.0, u = 0.0, v = 0.0;
                        if (intersect_ray_triangle_mt(
                                ray,
                                mesh_->vertices[tri[0]],
                                mesh_->vertices[tri[1]],
                                mesh_->vertices[tri[2]],
                                1e30,
                                t, u, v)) {
                            // Deduplicate shared-edge/vertex double hits at identical distance t
                            bool dup = false;
                            for (int h = 0; h < hit_count; ++h) {
                                if (std::abs(hit_ts[h] - t) <= 1e-6) {
                                    dup = true;
                                    break;
                                }
                            }
                            if (!dup && hit_count < 64) {
                                hit_ts[hit_count++] = t;
                            }
                        }
                    }
                    if (stack_ptr == 0) break;
                    cur_idx = stack[--stack_ptr];
                } else {
                    stack[stack_ptr++] = node.second_child_or_prim;
                    cur_idx = cur_idx + 1;
                }
            } else {
                if (stack_ptr == 0) break;
                cur_idx = stack[--stack_ptr];
            }
        }
        if ((hit_count % 2) == 1) ++inside_votes;
    }
    return inside_votes >= 2;
}

std::vector<uint32_t> LinearBvh3D::query_aabb(const BBox3D& query_box) const {
    std::vector<uint32_t> result;
    if (!mesh_ || nodes_.empty() || !query_box.is_valid()) return result;

    auto boxes_overlap = [](const BBox3D& a, const BBox3D& b) noexcept {
        return !(a.max.x < b.min.x || a.min.x > b.max.x ||
                 a.max.y < b.min.y || a.min.y > b.max.y ||
                 a.max.z < b.min.z || a.min.z > b.max.z);
    };

    std::vector<uint32_t> stack{0};
    while (!stack.empty()) {
        const uint32_t idx = stack.back();
        stack.pop_back();
        const LinearBvhNode& node = nodes_[idx];
        if (!boxes_overlap(node.bounds, query_box)) continue;
        if (node.prim_count > 0) {
            for (uint32_t i = 0; i < node.prim_count; ++i) {
                result.push_back(prim_indices_[node.second_child_or_prim + i]);
            }
        } else {
            stack.push_back(idx + 1);
            stack.push_back(node.second_child_or_prim);
        }
    }
    return result;
}

void Octree3D::build(const SolidMesh3D& mesh, uint32_t max_depth, uint32_t max_leaf_tris) {
    nodes_.clear();
    mesh_ = &mesh;
    if (mesh.triangles.empty() || mesh.vertices.empty()) return;

    std::vector<BBox3D> tri_boxes(mesh.triangles.size());
    BBox3D root_box;
    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& t = mesh.triangles[i];
        tri_boxes[i].expand(mesh.vertices[t[0]]);
        tri_boxes[i].expand(mesh.vertices[t[1]]);
        tri_boxes[i].expand(mesh.vertices[t[2]]);
        root_box.expand(tri_boxes[i]);
    }

    OctNode root;
    root.bounds = root_box;
    root.tri_indices.resize(mesh.triangles.size());
    std::iota(root.tri_indices.begin(), root.tri_indices.end(), 0U);
    nodes_.push_back(std::move(root));

    insert_recursive(0, tri_boxes, 0, max_depth, max_leaf_tris);
}

void Octree3D::insert_recursive(
    uint32_t node_idx,
    const std::vector<BBox3D>& tri_boxes,
    uint32_t depth,
    uint32_t max_depth,
    uint32_t max_leaf_tris
) {
    if (depth >= max_depth || nodes_[node_idx].tri_indices.size() <= max_leaf_tris) {
        return;
    }

    const BBox3D pbox = nodes_[node_idx].bounds;
    const Point3D mid = (pbox.min + pbox.max) * 0.5;
    std::vector<uint32_t> tris = std::move(nodes_[node_idx].tri_indices);
    nodes_[node_idx].tri_indices.clear();
    nodes_[node_idx].is_leaf = false;

    const uint32_t base_child = static_cast<uint32_t>(nodes_.size());
    nodes_.resize(base_child + 8);

    auto boxes_overlap = [](const BBox3D& a, const BBox3D& b) noexcept {
        return !(a.max.x < b.min.x || a.min.x > b.max.x ||
                 a.max.y < b.min.y || a.min.y > b.max.y ||
                 a.max.z < b.min.z || a.min.z > b.max.z);
    };

    for (uint32_t c = 0; c < 8; ++c) {
        nodes_[node_idx].children[c] = base_child + c;
        BBox3D cb;
        cb.min.x = (c & 1) ? mid.x : pbox.min.x;
        cb.max.x = (c & 1) ? pbox.max.x : mid.x;
        cb.min.y = (c & 2) ? mid.y : pbox.min.y;
        cb.max.y = (c & 2) ? pbox.max.y : mid.y;
        cb.min.z = (c & 4) ? mid.z : pbox.min.z;
        cb.max.z = (c & 4) ? pbox.max.z : mid.z;
        nodes_[base_child + c].bounds = cb;

        for (const uint32_t tid : tris) {
            if (boxes_overlap(cb, tri_boxes[tid])) {
                nodes_[base_child + c].tri_indices.push_back(tid);
            }
        }
    }

    for (uint32_t c = 0; c < 8; ++c) {
        insert_recursive(base_child + c, tri_boxes, depth + 1, max_depth, max_leaf_tris);
    }
}

std::vector<uint32_t> Octree3D::query_aabb(const BBox3D& query_box) const {
    std::vector<uint32_t> out;
    if (!mesh_ || nodes_.empty() || !query_box.is_valid()) return out;

    auto boxes_overlap = [](const BBox3D& a, const BBox3D& b) noexcept {
        return !(a.max.x < b.min.x || a.min.x > b.max.x ||
                 a.max.y < b.min.y || a.min.y > b.max.y ||
                 a.max.z < b.min.z || a.min.z > b.max.z);
    };

    std::vector<uint32_t> stack{0};
    std::unordered_set<uint32_t> seen;
    while (!stack.empty()) {
        const uint32_t idx = stack.back();
        stack.pop_back();
        const OctNode& node = nodes_[idx];
        if (!boxes_overlap(node.bounds, query_box)) continue;
        if (node.is_leaf) {
            for (const uint32_t tid : node.tri_indices) {
                if (seen.insert(tid).second) out.push_back(tid);
            }
        } else {
            for (uint32_t c = 0; c < 8; ++c) stack.push_back(node.children[c]);
        }
    }
    return out;
}

RayHit3D Octree3D::raycast(const Ray3D& ray, double max_distance) const {
    RayHit3D best;
    if (!mesh_ || nodes_.empty()) return best;
    best.distance = max_distance;
    const Vec3d inv_dir = safe_inv_dir(ray.direction);

    std::vector<uint32_t> stack{0};
    while (!stack.empty()) {
        const uint32_t idx = stack.back();
        stack.pop_back();
        const OctNode& node = nodes_[idx];
        if (!intersect_ray_aabb_fast(ray, inv_dir, node.bounds, best.distance)) continue;
        if (node.is_leaf) {
            for (const uint32_t tid : node.tri_indices) {
                const auto& tri = mesh_->triangles[tid];
                double t = 0.0, u = 0.0, v = 0.0;
                if (intersect_ray_triangle_mt(
                        ray,
                        mesh_->vertices[tri[0]],
                        mesh_->vertices[tri[1]],
                        mesh_->vertices[tri[2]],
                        best.distance,
                        t, u, v)) {
                    best.hit = true;
                    best.distance = t;
                    best.u = u;
                    best.v = v;
                    best.triangle_index = tid;
                }
            }
        } else {
            for (uint32_t c = 0; c < 8; ++c) stack.push_back(node.children[c]);
        }
    }
    if (best.hit) {
        best.point = ray.origin + ray.direction * best.distance;
        best.normal = mesh_->triangle_normal(best.triangle_index);
    } else {
        best.distance = std::numeric_limits<double>::infinity();
    }
    return best;
}

} // namespace shunya::cad3d
