#include "tests/test_framework.hpp"

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/rig3d/armature.hpp"
#include "shunya/rig3d/kinematics.hpp"
#include "shunya/rig3d/organic_mesh.hpp"
#include "shunya/rig3d/sculpt_brush.hpp"
#include "shunya/rig3d/skinning.hpp"

#include <cmath>
#include <numbers>

using namespace shunya;
using namespace shunya::cad3d;
using namespace shunya::rig3d;
using core::base::math::Mat4;
using core::base::math::Quat;

// =============================================================================
// Task 1: PH4-11.02.004 - Dynamic Vertex Masking
// =============================================================================
TEST_CASE("PH4-11.02.004: Dynamic vertex masking protects surface regions from sculpt brush displacement") {
    QuadSphereBuilder::Params sp;
    sp.radius = 1.0;
    sp.subdivisions = 4;
    SolidMesh3D sphere = QuadSphereBuilder::build(sp);

    VertexMask mask(sphere.vertices.size(), 0.0);
    REQUIRE(mask.size() == sphere.vertices.size());

    // Paint full mask (1.0) on top pole (+Z region)
    StrokePoint mask_stroke;
    mask_stroke.position = Point3D(0.0, 0.0, 1.0);
    mask.paint(sphere, mask_stroke, 0.5, 1.0, false);

    size_t masked_idx = 0;
    bool found_masked = false;
    for (size_t i = 0; i < sphere.vertices.size(); ++i) {
        if ((sphere.vertices[i] - Point3D(0, 0, 1)).length() < 0.2) {
            CHECK(mask.get(i) > 0.5);
            masked_idx = i;
            found_masked = true;
            break;
        }
    }
    REQUIRE(found_masked);

    BrushConfig bcfg;
    bcfg.type = BrushType::Inflate;
    bcfg.radius = 0.5;
    bcfg.strength = 1.0;
    SculptEngine engine(bcfg);

    const Point3D orig_masked_pos = sphere.vertices[masked_idx];
    StrokePoint sculpt_stroke;
    sculpt_stroke.position = Point3D(0.0, 0.0, 1.0);
    sculpt_stroke.normal = Vec3d(0.0, 0.0, 1.0);

    engine.apply_stroke(sphere, sculpt_stroke, &mask);
    const double pole_disp = (sphere.vertices[masked_idx] - orig_masked_pos).length();
    CHECK(pole_disp < 0.02);

    mask.invert();
    CHECK(mask.get(masked_idx) < 0.5);
}

// =============================================================================
// Task 2: PH4-11.02.005 - Mesh Surface & Normal Smoothing Passes
// =============================================================================
TEST_CASE("PH4-11.02.005: Mesh surface smoothing (Laplacian, Taubin) and normal smoothing passes") {
    QuadSphereBuilder::Params sp;
    sp.radius = 2.0;
    sp.subdivisions = 4;
    SolidMesh3D mesh = QuadSphereBuilder::build(sp);

    mesh.vertices[5].z += 0.8;
    mesh.vertices[10].x += 0.8;
    const double initial_spike = mesh.vertices[5].z;

    // Laplacian Smoothing
    {
        SolidMesh3D lap_mesh = mesh;
        SurfaceSmoother::laplacian_smooth(lap_mesh, 3, 0.5);
        CHECK(lap_mesh.vertices[5].z < initial_spike - 0.2);
    }

    // Taubin Non-Shrinking Smoothing
    {
        SolidMesh3D tau_mesh = mesh;
        const double orig_vol = tau_mesh.compute_volume();
        SurfaceSmoother::taubin_smooth(tau_mesh, 5, 0.5, -0.53);
        const double smoothed_vol = tau_mesh.compute_volume();
        CHECK(std::abs(smoothed_vol - orig_vol) / orig_vol < 0.15);
    }

    // Normal Smoothing Pass
    {
        SolidMesh3D norm_mesh = mesh;
        SurfaceSmoother::normal_smooth_pass(norm_mesh, 0.785398);
        REQUIRE(norm_mesh.normals.size() == norm_mesh.vertices.size());
        for (const auto& n : norm_mesh.normals) {
            CHECK(std::abs(n.length() - 1.0) < 1e-4);
        }
    }
}

// =============================================================================
// Task 3: PH4-11.03.001 - Armature Entity & Skeletal Hierarchy
// =============================================================================
TEST_CASE("PH4-11.03.001: Armature entity represents a hierarchical tree of skeletal bones") {
    Armature armature;
    REQUIRE(armature.empty());

    // Root bone: Pelvis (Z: 0 to 1)
    uint32_t b0 = armature.add_bone("Pelvis", Point3D(0, 0, 0), Point3D(0, 0, 1));
    REQUIRE(b0 == 0);
    REQUIRE(armature.bone_count() == 1);
    REQUIRE(armature.bone(0).name == "Pelvis");
    CHECK(std::abs(armature.bone(0).length() - 1.0) < 1e-6);

    // Child bone: Spine (Z: 1 to 2, connected to Pelvis)
    uint32_t b1 = armature.add_bone("Spine", Point3D(0, 0, 1), Point3D(0, 0, 2), 0, true);
    REQUIRE(b1 == 1);
    REQUIRE(armature.bone(0).children_indices.size() == 1);
    REQUIRE(armature.bone(1).parent_index == 0);

    // Child bone: Head (Z: 2 to 2.5, connected to Spine)
    uint32_t b2 = armature.add_bone("Head", Point3D(0, 0, 2), Point3D(0, 0, 2.5), 1, true);
    REQUIRE(b2 == 2);
    REQUIRE(armature.bone(1).children_indices.size() == 1);

    CHECK(armature.find_bone_index("Pelvis") == 0);
    CHECK(armature.find_bone_index("Spine") == 1);
    CHECK(armature.find_bone_index("Head") == 2);
    CHECK(armature.find_bone_index("NonExistent") == -1);
}

// =============================================================================
// Task 4: PH4-11.03.002 - Bone Placement & Octahedron Visualization
// =============================================================================
TEST_CASE("PH4-11.03.002: Bone placement in 3D with interactive octahedron bone mesh generation") {
    Armature armature;
    uint32_t b_idx = armature.add_bone("Arm", Point3D(0, 1, 0), Point3D(0, 3, 0), -1, false, 0.0);
    REQUIRE(b_idx == 0);

    SolidMesh3D octa_mesh = armature.generate_octahedron_mesh(b_idx, 0.2);
    REQUIRE(octa_mesh.vertices.size() == 6);
    REQUIRE(octa_mesh.triangles.size() == 8);

    SolidMesh3D arm_mesh = armature.generate_armature_mesh();
    REQUIRE(arm_mesh.vertices.size() == 6);
    REQUIRE(arm_mesh.triangles.size() == 8);
}

// =============================================================================
// Task 5: PH4-11.03.003 - Connected vs Offset Bone Relationships
// =============================================================================
TEST_CASE("PH4-11.03.003: Parent-child bone relationships with connected vs offset heads") {
    Armature armature;
    uint32_t parent = armature.add_bone("UpperArm", Point3D(0, 0, 0), Point3D(0, 2, 0));
    uint32_t child = armature.add_bone("Forearm", Point3D(0, 2, 0), Point3D(0, 4, 0), parent, true);

    REQUIRE(armature.bone(child).connected);
    CHECK((armature.bone(child).head - armature.bone(parent).tail).length() < 1e-6);

    armature.update_bone_edit(parent, Point3D(0, 0, 0), Point3D(1, 3, 0));
    CHECK((armature.bone(child).head - Point3D(1, 3, 0)).length() < 1e-6);
    CHECK((armature.bone(child).tail - Point3D(1, 5, 0)).length() < 1e-6);
}

// =============================================================================
// Task 6: PH4-11.03.004 - Edit Mode vs Pose Mode & Transformation Matrices
// =============================================================================
TEST_CASE("PH4-11.03.004: Edit Mode rest matrices vs Pose Mode transformation matrices") {
    Armature armature;
    uint32_t b0 = armature.add_bone("Bone0", Point3D(0, 0, 0), Point3D(0, 1, 0));

    Mat4 skin0 = armature.get_skinning_matrix(b0);
    CHECK(std::abs(skin0.m[0] - 1.0) < 1e-5);
    CHECK(std::abs(skin0.m[5] - 1.0) < 1e-5);
    CHECK(std::abs(skin0.m[10] - 1.0) < 1e-5);

    armature.set_mode(ArmatureMode::Pose);
    BonePose p;
    p.rotation = Quat::from_axis_angle(Vec3d(0, 0, 1), std::numbers::pi / 2.0);
    armature.set_bone_pose(b0, p);

    Mat4 skin_posed = armature.get_skinning_matrix(b0);
    CHECK(std::abs(skin_posed.m[0] - 1.0) > 0.5);

    armature.reset_pose();
    Mat4 reset_skin = armature.get_skinning_matrix(b0);
    CHECK(std::abs(reset_skin.m[0] - 1.0) < 1e-5);
}

// =============================================================================
// Task 7: PH4-11.04.001 - Automated Linear Blend Skinning (LBS)
// =============================================================================
TEST_CASE("PH4-11.04.001: Automated Linear Blend Skinning (LBS) with distance weight calculation") {
    Armature armature;
    uint32_t b0 = armature.add_bone("Base", Point3D(0, 0, 0), Point3D(0, 1, 0));
    uint32_t b1 = armature.add_bone("Top",  Point3D(0, 1, 0), Point3D(0, 2, 0), b0, true);

    SolidMesh3D mesh;
    mesh.vertices = {
        Point3D(0.1, 0.2, 0.0),
        Point3D(0.1, 1.8, 0.0)
    };

    SkinningManager skinning;
    skinning.compute_distance_weights(mesh, armature);

    REQUIRE(skinning.vertex_count() == 2);
    CHECK(skinning.get_vertex_data(0).get_bone_weight(b0) > 0.8);
    CHECK(skinning.get_vertex_data(1).get_bone_weight(b1) > 0.8);

    BonePose p1;
    p1.rotation = Quat::from_axis_angle(Vec3d(0, 0, 1), std::numbers::pi / 2.0);
    armature.set_bone_pose(b1, p1);

    SolidMesh3D skinned = skinning.evaluate_skinning(mesh, armature);
    REQUIRE(skinned.vertices.size() == 2);

    CHECK(std::abs(skinned.vertices[0].x - 0.1) < 0.05);
    CHECK(skinned.vertices[1].x < 0.0);
}

// =============================================================================
// Task 8: PH4-11.04.002 - Interactive 3D Weight Painting Tool
// =============================================================================
TEST_CASE("PH4-11.04.002: Interactive 3D Weight Painting tool with color-mapped heat map") {
    SolidMesh3D mesh;
    mesh.vertices = {
        Point3D(0.0, 0.0, 0.0),
        Point3D(1.0, 0.0, 0.0)
    };

    SkinningManager skinning;
    skinning.resize(mesh.vertices.size());

    skinning.paint_weight(mesh, 2, Point3D(0, 0, 0), 0.5, 0.8);
    CHECK(skinning.get_vertex_data(0).get_bone_weight(2) > 0.5);
    CHECK(skinning.get_vertex_data(1).get_bone_weight(2) < 1e-4);

    Vec3d blue_c = SkinningManager::weight_to_color(0.0);
    CHECK(blue_c.z > 0.9);
    CHECK(blue_c.x < 0.1);

    Vec3d red_c = SkinningManager::weight_to_color(1.0);
    CHECK(red_c.x > 0.9);
    CHECK(red_c.z < 0.1);
}

// =============================================================================
// Task 9: PH4-11.04.003 - Forward Kinematics (FK)
// =============================================================================
TEST_CASE("PH4-11.04.003: Forward Kinematics (FK) joint rotation hierarchy propagation") {
    Armature armature;
    uint32_t b0 = armature.add_bone("UpperArm", Point3D(0, 0, 0), Point3D(0, 2, 0));
    uint32_t b1 = armature.add_bone("Forearm",  Point3D(0, 2, 0), Point3D(0, 4, 0), b0, true);

    Point3D tip0 = ForwardKinematics::get_bone_tip_position(armature, b1);
    CHECK(std::abs(tip0.y - 4.0) < 1e-5);

    Quat q90 = Quat::from_axis_angle(Vec3d(0, 0, 1), std::numbers::pi / 2.0);
    ForwardKinematics::set_bone_rotation(armature, b0, q90);

    Point3D tip_posed = ForwardKinematics::get_bone_tip_position(armature, b1);
    CHECK(std::abs(tip_posed.x - (-4.0)) < 1e-4);
    CHECK(std::abs(tip_posed.y - 0.0) < 1e-4);
}

// =============================================================================
// Task 10: PH4-11.04.004 - 2-Bone and Multi-Bone Inverse Kinematics (IK)
// =============================================================================
TEST_CASE("PH4-11.04.004: 2-bone and multi-bone Inverse Kinematics (IK) solvers") {
    // Analytical 2-Bone IK
    {
        Armature armature;
        uint32_t b0 = armature.add_bone("Thigh", Point3D(0, 0, 0), Point3D(0, 2, 0));
        uint32_t b1 = armature.add_bone("Calf",  Point3D(0, 2, 0), Point3D(0, 4, 0), b0, true);

        Point3D target(2.0, 2.0, 0.0);
        Point3D pole(0.0, 0.0, 1.0);

        IKSolveResult res = InverseKinematics::solve_2bone(armature, b0, b1, target, &pole);
        REQUIRE(res.converged);
        CHECK(res.final_error < 0.05);

        Point3D tip = ForwardKinematics::get_bone_tip_position(armature, b1);
        CHECK((tip - target).length() < 0.05);
    }

    // Multi-Bone CCD IK
    {
        Armature armature;
        uint32_t b0 = armature.add_bone("Link0", Point3D(0, 0, 0), Point3D(0, 1, 0));
        uint32_t b1 = armature.add_bone("Link1", Point3D(0, 1, 0), Point3D(0, 2, 0), b0, true);
        uint32_t b2 = armature.add_bone("Link2", Point3D(0, 2, 0), Point3D(0, 3, 0), b1, true);

        IKGoal goal;
        goal.end_effector_bone = b2;
        goal.target_position = Point3D(1.5, 1.5, 0.0);
        goal.chain_length = 3;
        goal.max_iterations = 40;
        goal.tolerance = 0.01;

        IKSolveResult res = InverseKinematics::solve_ccd(armature, goal);
        REQUIRE(res.converged);
        CHECK(res.final_error < 0.02);
    }
}
