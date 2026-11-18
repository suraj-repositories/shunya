#include "tests/test_framework.hpp"

#include "shunya/cad3d/half_edge_mesh.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/rig3d/modifier_stack.hpp"
#include "shunya/rig3d/organic_mesh.hpp"
#include "shunya/rig3d/sculpt_brush.hpp"

#include <cmath>
#include <numbers>

using namespace shunya;
using namespace shunya::cad3d;
using namespace shunya::rig3d;

namespace {

SolidMesh3D make_test_cube(double half_size = 1.0) {
    SolidMesh3D mesh;
    const double h = half_size;

    const std::vector<Point3D> pts = {
        {-h, -h, -h}, { h, -h, -h}, { h,  h, -h}, {-h,  h, -h},
        {-h, -h,  h}, { h, -h,  h}, { h,  h,  h}, {-h,  h,  h}
    };
    mesh.vertices = pts;

    const std::vector<std::array<uint32_t, 3>> tris = {
        {0, 2, 1}, {0, 3, 2}, // -Z
        {4, 5, 6}, {4, 6, 7}, // +Z
        {0, 1, 5}, {0, 5, 4}, // -Y
        {2, 3, 7}, {2, 7, 6}, // +Y
        {0, 4, 7}, {0, 7, 3}, // -X
        {1, 2, 6}, {1, 6, 5}  // +X
    };
    mesh.triangles = tris;
    return mesh;
}

SolidMesh3D make_single_quad_sheet(double width = 1.0, double height = 1.0) {
    SolidMesh3D mesh;
    const double hw = width * 0.5;
    const double hh = height * 0.5;

    mesh.vertices = {
        Point3D(-hw, -hh, 0.0),
        Point3D( hw, -hh, 0.0),
        Point3D( hw,  hh, 0.0),
        Point3D(-hw,  hh, 0.0)
    };
    mesh.triangles = {
        {0, 1, 2},
        {0, 2, 3}
    };
    mesh.normals = {
        Vec3d(0, 0, 1), Vec3d(0, 0, 1), Vec3d(0, 0, 1), Vec3d(0, 0, 1)
    };
    return mesh;
}

} // namespace

// =============================================================================
// Task 1: PH4-11.01.001 - Modifier Stack Pipeline Architecture
// =============================================================================
TEST_CASE("PH4-11.01.001: Modifier stack pipeline dynamically evaluates ordered non-destructive stages") {
    SolidMesh3D cube = make_test_cube(1.0);
    const size_t orig_vcount = cube.vertices.size();
    const size_t orig_tcount = cube.triangles.size();

    ModifierStack stack;
    REQUIRE(stack.empty());
    REQUIRE(stack.size() == 0);

    auto mir = std::make_shared<MirrorModifier>();
    auto arr = std::make_shared<ArrayModifier>();
    auto sol = std::make_shared<SolidifyModifier>();

    stack.add_modifier(mir);
    stack.add_modifier(arr);
    stack.add_modifier(sol);

    REQUIRE(stack.size() == 3);
    REQUIRE(stack.get(0)->name() == "Mirror");
    REQUIRE(stack.get(1)->name() == "Array");
    REQUIRE(stack.get(2)->name() == "Solidify");

    // Reorder stages
    REQUIRE(stack.move_up(1));
    REQUIRE(stack.get(0)->name() == "Array");
    REQUIRE(stack.get(1)->name() == "Mirror");

    REQUIRE(stack.move_down(0));
    REQUIRE(stack.get(0)->name() == "Mirror");
    REQUIRE(stack.get(1)->name() == "Array");

    // Dynamic evaluation does not mutate input mesh
    SolidMesh3D eval1 = stack.evaluate(cube);
    REQUIRE(cube.vertices.size() == orig_vcount);
    REQUIRE(cube.triangles.size() == orig_tcount);
    REQUIRE(eval1.vertices.size() > orig_vcount);

    // Disable a stage
    mir->set_enabled(false);
    SolidMesh3D eval2 = stack.evaluate(cube);
    REQUIRE(eval2.vertices.size() != eval1.vertices.size());

    // Remove stage
    REQUIRE(stack.remove_modifier(0));
    REQUIRE(stack.size() == 2);
    REQUIRE(stack.get(0)->name() == "Array");
}

// =============================================================================
// Task 2: PH4-11.01.002 - Mirror Modifier
// =============================================================================
TEST_CASE("PH4-11.01.002: Mirror Modifier across X/Y/Z axes with clipping, merge, and normal preservation") {
    SolidMesh3D half_cube;
    half_cube.vertices = {
        Point3D(0.0, 0.0, 0.0),
        Point3D(2.0, 0.0, 0.0),
        Point3D(2.0, 2.0, 0.0),
        Point3D(0.0, 2.0, 0.0)
    };
    half_cube.triangles = {
        {0, 1, 2},
        {0, 2, 3}
    };
    half_cube.normals = {
        Vec3d(0, 0, 1), Vec3d(0, 0, 1), Vec3d(0, 0, 1), Vec3d(0, 0, 1)
    };

    MirrorModifierConfig cfg;
    cfg.mirror_x = true;
    cfg.mirror_y = false;
    cfg.mirror_z = false;
    cfg.merge = true;
    cfg.merge_threshold = 0.01;
    cfg.mirror_plane_offset = 0.0;

    MirrorModifier mirror(cfg);
    SolidMesh3D mirrored = mirror.evaluate(half_cube);

    // 2 boundary vertices at x=0 merged, 2 vertices mirrored -> 6 total vertices
    REQUIRE(mirrored.vertices.size() == 6);
    // 2 triangles * 2 = 4 triangles
    REQUIRE(mirrored.triangles.size() == 4);

    double min_x = 100.0, max_x = -100.0;
    for (const auto& v : mirrored.vertices) {
        min_x = std::min(min_x, v.x);
        max_x = std::max(max_x, v.x);
    }
    CHECK(std::abs(min_x - (-2.0)) < 1e-4);
    CHECK(std::abs(max_x - ( 2.0)) < 1e-4);
}

// =============================================================================
// Task 3: PH4-11.01.003 - Array Modifier
// =============================================================================
TEST_CASE("PH4-11.01.003: Array Modifier linear and circular repetition") {
    SolidMesh3D cube = make_test_cube(0.5); // 1.0 x 1.0 x 1.0 cube

    // Linear repetition
    {
        ArrayModifierConfig cfg;
        cfg.count = 4;
        cfg.use_relative_offset = true;
        cfg.relative_offset = Vec3d(1.5, 0.0, 0.0);
        ArrayModifier array_mod(cfg);

        SolidMesh3D result = array_mod.evaluate(cube);
        REQUIRE(result.vertices.size() == cube.vertices.size() * 4);
        REQUIRE(result.triangles.size() == cube.triangles.size() * 4);

        double min_x = 100.0, max_x = -100.0;
        for (const auto& v : result.vertices) {
            min_x = std::min(min_x, v.x);
            max_x = std::max(max_x, v.x);
        }
        CHECK(std::abs(min_x - (-0.5)) < 1e-3);
        CHECK(std::abs(max_x - ( 5.0)) < 1e-3);
    }

    // Circular radial array
    {
        ArrayModifierConfig cfg;
        cfg.count = 6;
        cfg.use_circular = true;
        cfg.circular_angle_rad = (2.0 * std::numbers::pi) / 6.0;
        cfg.circular_center = Point3D(0.0, 0.0, 0.0);
        ArrayModifier array_mod(cfg);

        SolidMesh3D result = array_mod.evaluate(cube);
        REQUIRE(result.vertices.size() == cube.vertices.size() * 6);
        REQUIRE(result.triangles.size() == cube.triangles.size() * 6);
    }
}

// =============================================================================
// Task 4: PH4-11.01.004 - Solidify Modifier
// =============================================================================
TEST_CASE("PH4-11.01.004: Solidify Modifier extrudes open sheet into watertight solid skin") {
    SolidMesh3D sheet = make_single_quad_sheet(2.0, 2.0);

    SolidifyModifierConfig cfg;
    cfg.thickness = 0.2;
    cfg.offset = -1.0;
    cfg.fill_rim = true;

    SolidifyModifier solidify(cfg);
    SolidMesh3D solid = solidify.evaluate(sheet);

    // 4 outer + 4 inner = 8 vertices
    REQUIRE(solid.vertices.size() == 8);
    // 2 outer + 2 inner + 8 rim = 12 triangles
    REQUIRE(solid.triangles.size() == 12);

    double min_z = 10.0, max_z = -10.0;
    for (const auto& v : solid.vertices) {
        min_z = std::min(min_z, v.z);
        max_z = std::max(max_z, v.z);
    }
    CHECK(std::abs(min_z - (-0.2)) < 1e-4);
    CHECK(std::abs(max_z - ( 0.0)) < 1e-4);
}

// =============================================================================
// Task 5: PH4-11.01.005 - Subdivision Surface (Subsurf) Modifier
// =============================================================================
TEST_CASE("PH4-11.01.005: Subdivision Surface (Subsurf) Catmull-Clark iterative refinement") {
    SolidMesh3D cube = make_test_cube(1.0);
    const size_t initial_tris = cube.triangles.size();

    SubsurfModifierConfig cfg;
    cfg.levels = 1;
    cfg.edge_crease = 0.0;

    SubsurfModifier subsurf(cfg);
    SolidMesh3D sub1 = subsurf.evaluate(cube);
    REQUIRE(sub1.triangles.size() == initial_tris * 4);

    cfg.levels = 2;
    subsurf.set_config(cfg);
    SolidMesh3D sub2 = subsurf.evaluate(cube);
    REQUIRE(sub2.triangles.size() == initial_tris * 16);
}

// =============================================================================
// Task 6: PH4-11.01.006 - Decimate Modifier
// =============================================================================
TEST_CASE("PH4-11.01.006: Decimate Modifier simplifies mesh via QEM edge collapse") {
    IcosphereBuilder::Params ic_params;
    ic_params.resolution_level = 2; // 320 triangles
    SolidMesh3D sphere = IcosphereBuilder::build(ic_params);
    const size_t orig_tris = sphere.triangles.size();
    REQUIRE(orig_tris == 320);

    DecimationParams dparams;
    dparams.target_ratio = 0.5;
    DecimateModifier decimator(dparams);

    SolidMesh3D simplified = decimator.evaluate(sphere);
    const size_t new_tris = simplified.triangles.size();

    REQUIRE(new_tris < orig_tris);
    REQUIRE(new_tris <= static_cast<size_t>(orig_tris * 0.6));
    REQUIRE(new_tris >= static_cast<size_t>(orig_tris * 0.4));
}

// =============================================================================
// Task 7: PH4-11.01.007 - Apply Modifier Operation
// =============================================================================
TEST_CASE("PH4-11.01.007: Apply Modifier permanently bakes modifier into base HalfEdgeMesh") {
    SolidMesh3D cube = make_test_cube(1.0);
    HalfEdgeMesh he_mesh;
    REQUIRE(he_mesh.build_from_solid(cube));

    ModifierStack stack;
    auto mir = std::make_shared<MirrorModifier>();
    auto arr = std::make_shared<ArrayModifier>();
    ArrayModifierConfig acfg;
    acfg.count = 2;
    arr->set_config(acfg);

    stack.add_modifier(mir);
    stack.add_modifier(arr);
    REQUIRE(stack.size() == 2);

    REQUIRE(stack.apply_modifier(0, he_mesh));
    REQUIRE(stack.size() == 1);
    REQUIRE(he_mesh.vertices.size() > 8);

    stack.apply_all(he_mesh);
    REQUIRE(stack.empty());
    REQUIRE(he_mesh.vertices.size() >= 16);

    SolidMesh3D baked_solid = he_mesh.to_solid_mesh();
    REQUIRE(!baked_solid.vertices.empty());
    REQUIRE(!baked_solid.triangles.empty());
}

// =============================================================================
// Task 8: PH4-11.02.001 - Quad Sphere and Icosphere Organic Mesh Generation
// =============================================================================
TEST_CASE("PH4-11.02.001: QuadSphere and Icosphere generate polar-singularity-free organic spheres") {
    // QuadSphereBuilder
    {
        QuadSphereBuilder::Params params;
        params.radius = 2.5;
        params.subdivisions = 4;
        params.center = Point3D(1.0, 2.0, 3.0);

        SolidMesh3D sphere = QuadSphereBuilder::build(params);
        REQUIRE(!sphere.vertices.empty());
        REQUIRE(!sphere.triangles.empty());

        for (const auto& v : sphere.vertices) {
            const double dist = (v - params.center).length();
            CHECK(std::abs(dist - params.radius) < 1e-4);
        }
    }

    // IcosphereBuilder
    {
        IcosphereBuilder::Params params;
        params.radius = 1.5;
        params.resolution_level = 1; // 80 triangles, 42 vertices
        params.center = Point3D(0.0, 0.0, 0.0);

        SolidMesh3D sphere = IcosphereBuilder::build(params);
        REQUIRE(sphere.triangles.size() == 80);
        REQUIRE(sphere.vertices.size() == 42);

        // Euler characteristic: V - E + F = 2
        const size_t v = sphere.vertices.size();
        const size_t f = sphere.triangles.size();
        const size_t e = (3 * f) / 2;
        const int64_t chi = static_cast<int64_t>(v) - static_cast<int64_t>(e) + static_cast<int64_t>(f);
        REQUIRE(chi == 2);
    }
}

// =============================================================================
// Task 9: PH4-11.02.002 - Interactive Brush Deformation Tools
// =============================================================================
TEST_CASE("PH4-11.02.002: Interactive brush deformation tools (Clay, Smooth, Grab, Inflate)") {
    QuadSphereBuilder::Params sp;
    sp.radius = 1.0;
    sp.subdivisions = 6;
    SolidMesh3D sphere = QuadSphereBuilder::build(sp);

    BrushConfig cfg;
    cfg.radius = 0.5;
    cfg.strength = 0.8;
    cfg.symmetry_x = false;

    SculptEngine engine(cfg);

    // Clay Brush
    {
        cfg.type = BrushType::Clay;
        engine.set_config(cfg);

        StrokePoint stroke;
        stroke.position = Point3D(0.0, 0.0, 1.0);
        stroke.normal = Vec3d(0.0, 0.0, 1.0);

        SculptResult res = engine.apply_stroke(sphere, stroke);
        REQUIRE(res.vertices_modified > 0);
        REQUIRE(res.max_displacement > 0.0);
    }

    // Inflate Brush
    {
        cfg.type = BrushType::Inflate;
        engine.set_config(cfg);

        StrokePoint stroke;
        stroke.position = Point3D(0.0, 1.0, 0.0);
        stroke.normal = Vec3d(0.0, 1.0, 0.0);

        SculptResult res = engine.apply_stroke(sphere, stroke);
        REQUIRE(res.vertices_modified > 0);
        REQUIRE(res.max_displacement > 0.0);
    }

    // Grab Brush
    {
        cfg.type = BrushType::Grab;
        engine.set_config(cfg);

        StrokePoint stroke;
        stroke.position = Point3D(1.0, 0.0, 0.0);
        stroke.motion_vector = Vec3d(0.2, 0.1, 0.0);

        SculptResult res = engine.apply_stroke(sphere, stroke);
        REQUIRE(res.vertices_modified > 0);
        REQUIRE(res.max_displacement > 0.0);
    }

    // Smooth Brush
    {
        sphere.vertices[0].z += 0.5;
        cfg.type = BrushType::Smooth;
        engine.set_config(cfg);

        StrokePoint stroke;
        stroke.position = sphere.vertices[0];
        stroke.normal = Vec3d(0, 0, 1);

        SculptResult res = engine.apply_stroke(sphere, stroke);
        REQUIRE(res.vertices_modified > 0);
    }
}

// =============================================================================
// Task 10: PH4-11.02.003 - Dynamic Topological Bilateral Symmetry
// =============================================================================
TEST_CASE("PH4-11.02.003: Dynamic topological symmetry enforces bilateral X-axis deformation") {
    QuadSphereBuilder::Params sp;
    sp.radius = 2.0;
    sp.subdivisions = 8;
    SolidMesh3D sphere = QuadSphereBuilder::build(sp);

    BrushConfig cfg;
    cfg.type = BrushType::Inflate;
    cfg.radius = 0.6;
    cfg.strength = 1.0;
    cfg.symmetry_x = true;
    cfg.symmetry_tolerance = 0.01;

    SculptEngine engine(cfg);

    StrokePoint stroke;
    stroke.position = Point3D(1.2, 0.0, 1.6);
    stroke.normal = Vec3d(1.2, 0.0, 1.6).normalized();

    SculptResult res = engine.apply_stroke(sphere, stroke);
    REQUIRE(res.vertices_modified > 0);

    double max_pos_x = 0.0, max_neg_x = 0.0;
    for (const auto& v : sphere.vertices) {
        if (v.x > 0.5) {
            max_pos_x = std::max(max_pos_x, v.length());
        } else if (v.x < -0.5) {
            max_neg_x = std::max(max_neg_x, v.length());
        }
    }

    REQUIRE(max_pos_x > 2.0);
    REQUIRE(max_neg_x > 2.0);
    CHECK(std::abs(max_pos_x - max_neg_x) < 1e-3);
}
