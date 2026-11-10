#include "tests/test_framework.hpp"

#include "shunya/cad3d/half_edge_mesh.hpp"
#include "shunya/cad3d/mesh_circulator.hpp"
#include "shunya/cad3d/mesh_decimator.hpp"
#include "shunya/cad3d/revolve_feature.hpp"
#include "shunya/cad3d/subdivision.hpp"
#include "shunya/core/base/math.hpp"

#include <chrono>
#include <cmath>
#include <vector>

using namespace shunya::cad3d;
using shunya::cad2d::Path2D;
using shunya::core::base::math::PI;

namespace {

HalfEdgeMesh make_cube_quad_cage(double half_size = 1.0) {
    const double s = half_size;
    std::vector<Point3D> verts{
        {-s, -s, -s}, { s, -s, -s}, { s,  s, -s}, {-s,  s, -s},
        {-s, -s,  s}, { s, -s,  s}, { s,  s,  s}, {-s,  s,  s}
    };
    std::vector<std::vector<uint32_t>> quads{
        {0, 3, 2, 1}, // -Z
        {4, 5, 6, 7}, // +Z
        {0, 1, 5, 4}, // -Y
        {1, 2, 6, 5}, // +X
        {2, 3, 7, 6}, // +Y
        {3, 0, 4, 7}  // -X
    };
    HalfEdgeMesh he;
    he.build_from_polygons(verts, quads);
    return he;
}

SolidMesh3D make_octahedron(double radius = 1.0) {
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
    return m;
}

double vertex_radius_cv(const HalfEdgeMesh& he) {
    std::vector<double> radii;
    radii.reserve(he.vertices.size());
    double sum = 0.0;
    for (const auto& v : he.vertices) {
        const double r = v.position.length();
        radii.push_back(r);
        sum += r;
    }
    const double mean = sum / static_cast<double>(radii.size());
    double var = 0.0;
    for (double r : radii) var += (r - mean) * (r - mean);
    return std::sqrt(var / static_cast<double>(radii.size())) / mean;
}

} // namespace

TEST_CASE("HalfEdgeMesh: twin consistency, Euler V-E+F, 1-ring circulators, and non-manifold detection") {
    // 1. Quad cube cage
    const HalfEdgeMesh cube_he = make_cube_quad_cage(1.0);
    REQUIRE(!cube_he.has_non_manifold_edges());
    REQUIRE(cube_he.verify_twin_consistency());
    const ManifoldReport cube_rep = cube_he.verify_topology();
    REQUIRE(cube_rep.is_2_manifold);
    REQUIRE(cube_rep.euler_characteristic == 2); // V=8, E=12, F=6
    REQUIRE(cube_he.vertex_valency(0) == 3);
    REQUIRE(!cube_he.is_boundary_vertex(0));

    // 2. Torus manifold topology (genus 1 -> Euler V - E + F == 0)
    Path2D torus_prof;
    for (int i = 0; i < 16; ++i) {
        const double th = 2.0 * PI * (static_cast<double>(i) / 16.0);
        torus_prof.push_back({5.0 + std::cos(th), std::sin(th)});
    }
    const SolidMesh3D torus = RevolveEngine::revolve(torus_prof, {}, {{}, 2.0 * PI, 24});
    HalfEdgeMesh torus_he;
    REQUIRE(torus_he.build_from_solid(torus));
    REQUIRE(torus_he.verify_twin_consistency());
    const ManifoldReport tor_rep = torus_he.verify_topology();
    REQUIRE(tor_rep.is_2_manifold);
    REQUIRE(tor_rep.euler_characteristic == 0);

    // 3. Open boundary grid 1-ring query
    std::vector<Point3D> grid_v{{0, 0, 0}, {1, 0, 0}, {2, 0, 0}, {0, 1, 0}, {1, 1, 0}, {2, 1, 0}, {0, 2, 0}, {1, 2, 0}, {2, 2, 0}};
    std::vector<std::vector<uint32_t>> grid_f{
        {0, 1, 4, 3}, {1, 2, 5, 4},
        {3, 4, 7, 6}, {4, 5, 8, 7}
    };
    HalfEdgeMesh grid_he;
    REQUIRE(grid_he.build_from_polygons(grid_v, grid_f));
    REQUIRE(grid_he.verify_twin_consistency());
    REQUIRE(!grid_he.is_boundary_vertex(4)); // center vertex
    REQUIRE(grid_he.vertex_valency(4) == 4);
    REQUIRE(grid_he.is_boundary_vertex(0));  // corner vertex
    REQUIRE(grid_he.vertex_valency(0) == 2);

    // 4. Non-manifold 3-face shared edge detection
    std::vector<Point3D> nm_v{{0, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}};
    std::vector<std::array<uint32_t, 3>> nm_t{{0, 1, 2}, {0, 3, 1}, {0, 1, 4}}; // edge (0,1) used twice in same direction / 3 faces
    HalfEdgeMesh nm_he;
    REQUIRE(!nm_he.build_from_triangles(nm_v, nm_t));
    REQUIRE(nm_he.has_non_manifold_edges());
}

TEST_CASE("MeshDecimator: QEM simplification preserves manifold outward normals and speed on dense sphere") {
    // 5-level subdivided octahedron = 8 * 4^5 = 8192 triangles (smooth sphere)
    SolidMesh3D dense_sphere = SubdivisionEngine::subdivide_loop(make_octahedron(10.0), 5);
    REQUIRE(dense_sphere.triangles.size() == 8192);

    DecimationStats stats;
    DecimationParams params;
    params.target_triangle_count = 1024;
    const auto t0 = std::chrono::steady_clock::now();
    const SolidMesh3D simplified = MeshDecimator::simplify(dense_sphere, params, &stats);
    const auto t1 = std::chrono::steady_clock::now();
    const double elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    REQUIRE(simplified.triangles.size() <= 1024);
    REQUIRE(stats.collapses_performed > 3000);
    REQUIRE(elapsed_ms < 200.0);

    const ManifoldReport rep = simplified.analyze_manifold();
    REQUIRE(rep.is_2_manifold);
    REQUIRE(rep.euler_characteristic == 2);

    // Verify ZERO flipped normals across all simplified triangles
    for (size_t ti = 0; ti < simplified.triangles.size(); ++ti) {
        const auto& tri = simplified.triangles[ti];
        const Point3D centroid = (simplified.vertices[tri[0]] + simplified.vertices[tri[1]] + simplified.vertices[tri[2]]) * (1.0 / 3.0);
        const Vec3d n = simplified.triangle_normal(ti);
        REQUIRE(n.dot(centroid.normalized()) > 0.5);
    }
}

TEST_CASE("SubdivisionEngine: Catmull-Clark and Loop watertight convergence and sharp crease preservation") {
    // 1. Catmull-Clark on coarse cube -> 3 levels produces watertight sphere-like smooth surface
    const HalfEdgeMesh cube = make_cube_quad_cage(1.0);
    const HalfEdgeMesh cc1 = SubdivisionEngine::subdivide_catmull_clark(cube, 1);
    const HalfEdgeMesh cc3 = SubdivisionEngine::subdivide_catmull_clark(cc1, 2);
    REQUIRE(cc3.faces.size() == 6 * 64); // 6 * 4^3 = 384 quads
    REQUIRE(cc3.verify_twin_consistency());
    const ManifoldReport cc_rep = cc3.verify_topology();
    REQUIRE(cc_rep.is_2_manifold);
    REQUIRE(cc_rep.euler_characteristic == 2);
    REQUIRE(cc3.to_solid_mesh().analyze_manifold().is_2_manifold);

    const double cv1 = vertex_radius_cv(cc1);
    const double cv3 = vertex_radius_cv(cc3);
    REQUIRE(cv1 > 0.01);
    REQUIRE(cv3 < cv1 * 0.35); // Converges toward spherical uniformity after 3 levels
    REQUIRE(cv3 < 0.03);

    // 2. Loop subdivision across 3 levels on octahedron -> 8 * 64 = 512 triangles, watertight
    const SolidMesh3D loop3 = SubdivisionEngine::subdivide_loop(make_octahedron(5.0), 3);
    REQUIRE(loop3.triangles.size() == 512);
    const ManifoldReport lp_rep = loop3.analyze_manifold();
    REQUIRE(lp_rep.is_2_manifold);
    REQUIRE(lp_rep.euler_characteristic == 2);

    // 3. Crease angle preservation: subdividing a cube triangulated with 60-deg sharp threshold keeps corners at 1.0
    const SolidMesh3D sharp_box = cube.to_solid_mesh();
    const SolidMesh3D creased = SubdivisionEngine::subdivide_loop(sharp_box, 2, PI * 0.33);
    REQUIRE(creased.analyze_manifold().is_2_manifold);
    const BBox3D bbox = creased.compute_bbox();
    REQUIRE(std::abs(bbox.max.x - 1.0) < 1e-6);
    REQUIRE(std::abs(bbox.min.x + 1.0) < 1e-6);
}
