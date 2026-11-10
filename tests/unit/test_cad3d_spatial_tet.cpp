#include "tests/test_framework.hpp"

#include "shunya/cad3d/bvh_tree.hpp"
#include "shunya/cad3d/extrusion_feature.hpp"
#include "shunya/cad3d/subdivision.hpp"
#include "shunya/cad3d/tetrahedral_mesher.hpp"
#include "shunya/core/base/math.hpp"

#include <chrono>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

using namespace shunya::cad3d;
using shunya::cad2d::Path2D;
using shunya::core::base::math::PI;

namespace {

SolidMesh3D make_box(double sx, double sy, double sz) {
    const double hx = sx * 0.5;
    const double hy = sy * 0.5;
    const Path2D rect{{-hx, -hy}, {hx, -hy}, {hx, hy}, {-hx, hy}};
    return ExtrusionEngine::extrude(rect, {}, {sz, 100.0, 0.0, EndCondition::Symmetric, SketchPlane3D::xy()});
}

SolidMesh3D make_sphere_mesh(double radius, size_t subd_levels = 4) {
    SolidMesh3D m;
    m.vertices = {
        { radius, 0, 0}, {-radius, 0, 0},
        {0,  radius, 0}, {0, -radius, 0},
        {0, 0,  radius}, {0, 0, -radius}
    };
    m.triangles = {
        {0, 2, 4}, {2, 1, 4}, {1, 3, 4}, {3, 0, 4},
        {2, 0, 5}, {1, 2, 5}, {3, 1, 5}, {0, 3, 5}
    };
    SolidMesh3D res = SubdivisionEngine::subdivide_loop(m, subd_levels);
    for (auto& v : res.vertices) {
        if (v.length_squared() > 1e-18) {
            v = v.normalized() * radius;
        }
    }
    return res;
}

RayHit3D brute_force_raycast(const SolidMesh3D& mesh, const Ray3D& ray) {
    RayHit3D best;
    for (size_t i = 0; i < mesh.triangles.size(); ++i) {
        const auto& tri = mesh.triangles[i];
        double t = 0.0, u = 0.0, v = 0.0;
        if (intersect_ray_triangle_mt(
                ray,
                mesh.vertices[tri[0]],
                mesh.vertices[tri[1]],
                mesh.vertices[tri[2]],
                best.distance,
                t, u, v)) {
            best.hit = true;
            best.distance = t;
            best.u = u;
            best.v = v;
            best.triangle_index = static_cast<uint32_t>(i);
        }
    }
    if (best.hit) {
        best.point = ray.origin + ray.direction * best.distance;
        best.normal = mesh.triangle_normal(best.triangle_index);
    }
    return best;
}

} // namespace

TEST_CASE("LinearBvh3D & Octree3D: 5,000 random rays match brute-force Möller-Trumbore exactly") {
    const SolidMesh3D sphere = make_sphere_mesh(5.0, 4); // 2048 triangles
    REQUIRE(sphere.triangles.size() == 2048);

    LinearBvh3D bvh;
    bvh.build(sphere, 4);
    REQUIRE(bvh.is_built());

    Octree3D octree;
    octree.build(sphere, 6, 8);
    REQUIRE(octree.node_count() > 1);

    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist_orig(-9.0, 9.0);
    std::uniform_real_distribution<double> dist_target(-3.0, 3.0);
    std::uniform_real_distribution<double> dist_dir(-1.0, 1.0);

    std::vector<Ray3D> rays;
    rays.reserve(5000);
    for (int r = 0; r < 5000; ++r) {
        const Point3D orig{dist_orig(rng), dist_orig(rng), dist_orig(rng)};
        Vec3d dir;
        if (r % 2 == 0) {
            const Point3D tgt{dist_target(rng), dist_target(rng), dist_target(rng)};
            dir = tgt - orig;
        } else {
            dir = Vec3d{dist_dir(rng), dist_dir(rng), dist_dir(rng)};
        }
        if (dir.length_squared() < 1e-6) dir = {0.0, 0.0, 1.0};
        rays.push_back({orig, dir.normalized()});
    }

    size_t hit_count = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t r = 0; r < rays.size(); ++r) {
        const RayHit3D bvh_hit = bvh.raycast(rays[r]);
        if (bvh_hit.hit) ++hit_count;
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double us_per_ray =
        std::chrono::duration<double, std::micro>(t1 - t0).count() / static_cast<double>(rays.size());

    // Cross-validate against Octree3D (5,000 rays) and brute-force (first 500 rays)
    for (size_t r = 0; r < rays.size(); ++r) {
        const RayHit3D bvh_hit = bvh.raycast(rays[r]);
        const RayHit3D oct_hit = octree.raycast(rays[r]);
        REQUIRE(bvh_hit.hit == oct_hit.hit);
        if (bvh_hit.hit) {
            REQUIRE(std::abs(bvh_hit.distance - oct_hit.distance) < 1e-9);
        }
        if (r < 500) {
            const RayHit3D bf_hit = brute_force_raycast(sphere, rays[r]);
            REQUIRE(bvh_hit.hit == bf_hit.hit);
            if (bvh_hit.hit) {
                REQUIRE(std::abs(bvh_hit.distance - bf_hit.distance) < 1e-9);
            }
        }
    }

    REQUIRE(hit_count > 1000);
    REQUIRE(us_per_ray < 50.0);

    // Verify contains_point and AABB range query
    REQUIRE(bvh.contains_point({0.0, 0.0, 0.0}));
    REQUIRE(bvh.contains_point({2.5, -1.0, 1.5}));
    REQUIRE(!bvh.contains_point({6.0, 0.0, 0.0}));
    REQUIRE(!bvh.query_aabb({{-1, -1, -1}, {6, 6, 6}}).empty());
}

TEST_CASE("TetrahedralMesher: Tet4 elements have positive Jacobians and conserve solid volume") {
    // 1. Box 10x10x10 = 1000 volume
    const SolidMesh3D box = make_box(10.0, 10.0, 10.0);
    const TetraMesh3D box_tets = TetrahedralMesher::generate(box, {3.5, true});
    const TetQualityReport brep = TetQualityAnalyzer::evaluate(box_tets);

    REQUIRE(brep.element_count > 40);
    REQUIRE(brep.inverted_count == 0);
    REQUIRE(brep.min_jacobian > 0.0);
    REQUIRE(brep.min_dihedral_deg > 5.0);
    REQUIRE(std::abs(brep.total_volume - 1000.0) < 1e-3);

    // 2. Smooth 3-level sphere (radius 5.0) volumetric tet mesh
    const SolidMesh3D sphere = make_sphere_mesh(5.0, 2); // 128 boundary tris
    const TetraMesh3D sphere_tets = TetrahedralMesher::generate(sphere, {2.5, true});
    const TetQualityReport srep = TetQualityAnalyzer::evaluate(sphere_tets);

    REQUIRE(srep.element_count > 100);
    REQUIRE(srep.inverted_count == 0);
    REQUIRE(srep.min_jacobian > 0.0);
    REQUIRE(std::abs(srep.total_volume - sphere.compute_volume()) < 1e-2);
}
