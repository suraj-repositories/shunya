#include "tests/test_framework.hpp"
#include "shunya/core/domain/viewport_interaction.hpp"
#include "shunya/cad3d/mesh_selection_engine.hpp"
#include "shunya/cad3d/mesh_topology_ops.hpp"
#include "shunya/cad3d/subdivision.hpp"
#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/eng/physical_object.hpp"
#include "shunya/eng/engineering_pipeline.hpp"

#include <cmath>
#include <vector>

using namespace shunya;

TEST_CASE("PH5-25.01: Viewport Camera Projection, Orbit-Pan-Zoom & Ortho Snap Views") {
    // 1. Perspective Camera Projection
    core::domain::ViewportCamera cam;
    cam.position = {0.0, 0.0, 5.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.up = {0.0, 1.0, 0.0};
    cam.aspect_ratio = 16.0 / 9.0;
    cam.fov_degrees = 45.0;
    cam.projection_type = core::domain::CameraProjectionType::Perspective;

    auto view_mat = cam.view_matrix();
    auto proj_mat = cam.projection_matrix();
    auto vp_mat = proj_mat * view_mat;

    // Center target (0,0,0) should project to origin (0,0) in NDC
    core::base::math::Vector4<double> target_h{0.0, 0.0, 0.0, 1.0};
    auto clip_target = vp_mat * target_h;
    CHECK(clip_target.w > 0.0);
    double ndc_x = clip_target.x / clip_target.w;
    double ndc_y = clip_target.y / clip_target.w;
    CHECK(std::abs(ndc_x) < 1e-6);
    CHECK(std::abs(ndc_y) < 1e-6);

    // 2. Orbit navigation math
    const double initial_dist = (cam.position - cam.target).length();
    cam.orbit(core::base::math::PI * 0.25, core::base::math::PI * 0.1);
    const double post_orbit_dist = (cam.position - cam.target).length();
    CHECK(std::abs(initial_dist - post_orbit_dist) < 1e-5);

    // 3. Pan navigation math
    cam.pan(1.5, -0.8);
    CHECK(std::abs(cam.target.x) > 0.1 || std::abs(cam.target.y) > 0.1);

    // 4. Zoom navigation math
    const double pre_zoom_dist = (cam.position - cam.target).length();
    cam.zoom(0.8); // zoom in
    const double post_zoom_dist = (cam.position - cam.target).length();
    CHECK(post_zoom_dist < pre_zoom_dist);

    // 5. Standard Numpad Orthogonal Views
    auto front_cam = core::domain::ViewCubeManager::create_camera_for_face(
        core::domain::CubeFace::Front, 10.0, {0.0, 0.0, 0.0}, true);
    CHECK(front_cam.projection_type == core::domain::CameraProjectionType::Orthographic);
    CHECK(std::abs(front_cam.position.z - 10.0) < 1e-5);
    CHECK(std::abs(front_cam.position.x) < 1e-5);

    auto top_cam = core::domain::ViewCubeManager::create_camera_for_face(
        core::domain::CubeFace::Top, 10.0, {0.0, 0.0, 0.0}, true);
    CHECK(std::abs(top_cam.position.y - 10.0) < 1e-5);
    CHECK(std::abs(top_cam.position.z) < 1e-5);

    auto right_cam = core::domain::ViewCubeManager::create_camera_for_face(
        core::domain::CubeFace::Right, 10.0, {0.0, 0.0, 0.0}, true);
    CHECK(std::abs(right_cam.position.x - 10.0) < 1e-5);
    CHECK(std::abs(right_cam.position.y) < 1e-5);
}

TEST_CASE("PH5-25.02: 3D Transform Gizmos, Hit Testing & Precision Snapping") {
    core::domain::ViewportCamera cam;
    cam.position = {5.0, 5.0, 5.0};
    cam.target = {0.0, 0.0, 0.0};
    cam.up = {0.0, 1.0, 0.0};
    cam.aspect_ratio = 1.0;
    cam.fov_degrees = 45.0;

    core::base::math::Vector3<double> origin{0.0, 0.0, 0.0};
    auto proj = core::domain::GizmoInteractionEngine::project_gizmo(cam, origin, 1.0, 800.0, 800.0);
    CHECK(proj.is_behind_camera == false);
    CHECK(proj.origin_2d.x > 300.0 && proj.origin_2d.x < 500.0);
    CHECK(proj.origin_2d.y > 300.0 && proj.origin_2d.y < 500.0);

    // Hit-testing near X axis tip
    auto hit_x = core::domain::GizmoInteractionEngine::pick_axis_2d(
        proj, proj.x_tip_2d.x, proj.x_tip_2d.y, 15.0);
    CHECK(hit_x == core::domain::GizmoAxis::X);

    // Hit-testing near origin
    auto hit_origin = core::domain::GizmoInteractionEngine::pick_axis_2d(
        proj, proj.origin_2d.x, proj.origin_2d.y, 10.0);
    CHECK(hit_origin == core::domain::GizmoAxis::All);

    // Precision Snapping
    core::domain::SnappingConfig snap;
    snap.enabled = true;
    snap.grid_step = 0.01; // 10 mm
    snap.angle_step_deg = 15.0; // 15 deg

    double raw_pos = 0.0384;
    double snapped_pos = snap.snap_value(raw_pos, snap.grid_step);
    CHECK(std::abs(snapped_pos - 0.04) < 1e-5);

    double raw_rad = 0.81; // ~46.4 deg
    double snapped_rad = snap.snap_angle_rad(raw_rad);
    // 45 degrees is PI / 4 = 0.785398 rad
    CHECK(std::abs(snapped_rad - (core::base::math::PI / 4.0)) < 0.05);
}

TEST_CASE("PH5-25.03: Edit Mode Selection Masks & Polyhedral Modeling Tools") {
    // Start with a clean canonical unit box
    cad3d::SolidMesh3D box = eng::ObjectGeometrySpec::make_box(1.0, 1.0, 1.0).generate_solid_mesh();
    CHECK(box.vertices.size() == 8);
    CHECK(box.triangles.size() == 12);

    // 1. Selection Engine State
    cad3d::MeshSelection sel;
    sel.set_mode(cad3d::MeshSelectionMode::Face);
    sel.select_face(0, false);
    CHECK(sel.is_face_selected(0) == true);
    CHECK(sel.is_face_selected(1) == false);

    // Switch mask to Vertex
    sel.set_mode(cad3d::MeshSelectionMode::Vertex);
    sel.select_vertex(2, false);
    CHECK(sel.is_vertex_selected(2) == true);

    // 2. Extrude Face Tool
    cad3d::ExtrudeOptions ext_opt;
    ext_opt.distance = 0.5;
    std::vector<uint32_t> faces_to_extrude = {0, 1}; // top face of cube
    bool ext_ok = cad3d::MeshTopologyOps::extrude_faces(box, faces_to_extrude, ext_opt);
    CHECK(ext_ok == true);
    // Extrusion adds side skirt triangles and shifts top face
    CHECK(box.vertices.size() > 8);
    CHECK(box.triangles.size() > 12);

    // 3. Inset Face Tool
    cad3d::SolidMesh3D box2 = eng::ObjectGeometrySpec::make_box(1.0, 1.0, 1.0).generate_solid_mesh();
    cad3d::InsetOptions inset_opt;
    inset_opt.thickness = 0.1;
    bool inset_ok = cad3d::MeshTopologyOps::inset_faces(box2, {0, 1}, inset_opt);
    CHECK(inset_ok == true);
    CHECK(box2.vertices.size() > 8);

    // 4. Bevel Edge Tool
    cad3d::SolidMesh3D box3 = eng::ObjectGeometrySpec::make_box(1.0, 1.0, 1.0).generate_solid_mesh();
    cad3d::BevelOptions bevel_opt;
    bevel_opt.offset = 0.05;
    bevel_opt.segments = 1;
    std::vector<std::pair<uint32_t, uint32_t>> bevel_edges = {{box3.triangles[0][0], box3.triangles[0][1]}};
    bool bevel_ok = cad3d::MeshTopologyOps::bevel(box3, bevel_edges, {}, bevel_opt);
    CHECK(bevel_ok == true);

    // 5. Subdivision Surface Tool (Loop Subdivision)
    cad3d::SolidMesh3D box4 = eng::ObjectGeometrySpec::make_box(1.0, 1.0, 1.0).generate_solid_mesh();
    const size_t pre_subdiv_triangles = box4.triangles.size();
    auto refined = cad3d::SubdivisionEngine::subdivide_loop(box4, 1);
    // Loop subdivision divides each triangle into 4 smaller triangles
    CHECK(refined.triangles.size() == pre_subdiv_triangles * 4);
}

TEST_CASE("PH5-25.04: Interactive Measurement & Section Slicing Analysis") {
    // 1. Euclidean Distance Measurement
    cad3d::Point3D p1{0.0, 0.0, 0.0};
    cad3d::Point3D p2{3.0, 4.0, 0.0};
    const double dist = (p2 - p1).length();
    CHECK(std::abs(dist - 5.0) < 1e-6);

    // 2. Dihedral / Corner Angle Measurement
    // Points forming right angle at B: A(1, 0, 0), B(0, 0, 0), C(0, 1, 0)
    cad3d::Point3D a{1.0, 0.0, 0.0};
    cad3d::Point3D b{0.0, 0.0, 0.0};
    cad3d::Point3D c{0.0, 1.0, 0.0};
    cad3d::Vec3d ba = a - b;
    cad3d::Vec3d bc = c - b;
    const double dot = ba.dot(bc) / (ba.length() * bc.length());
    const double angle_deg = std::acos(dot) * (180.0 / core::base::math::PI);
    CHECK(std::abs(angle_deg - 90.0) < 1e-6);

    // 3. Section Slicing Plane
    cad3d::Plane3D section_plane{cad3d::Point3D{0.0, 0.0, 0.0}, cad3d::Vec3d{0.0, 0.0, 1.0}};
    cad3d::Point3D above{0.0, 0.0, 0.5};
    cad3d::Point3D below{0.0, 0.0, -0.5};
    CHECK(section_plane.signed_distance(above) > 0.0);
    CHECK(section_plane.signed_distance(below) < 0.0);
}
