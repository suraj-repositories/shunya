#include "tests/test_framework.hpp"
#include "shunya/core/base/math.hpp"

#include <cmath>

using namespace shunya::core::base::math;

TEST_CASE("FND-07: Scalar Numerical Utilities") {
    CHECK(approx_equal(1.0, 1.0 + 1e-11));
    CHECK(!approx_equal(1.0, 1.05, 1e-3, 1e-3));

    CHECK(clamp(5.0, 0.0, 10.0) == 5.0);
    CHECK(clamp(-2.0, 0.0, 10.0) == 0.0);
    CHECK(clamp(15.0, 0.0, 10.0) == 10.0);

    CHECK(approx_equal(lerp(0.0, 10.0, 0.5), 5.0));
    CHECK(approx_equal(lerp(2.0, 8.0, 0.25), 3.5));
}

TEST_CASE("FND-07: Vector2 and Vector3 Operations") {
    Vec2 v2a{1.0, 2.0};
    Vec2 v2b{3.0, 4.0};
    Vec2 v2sum = v2a + v2b;
    CHECK(v2sum.x == 4.0);
    CHECK(v2sum.y == 6.0);
    CHECK(v2a.dot(v2b) == 11.0);
    CHECK(approx_equal(Vec2{3.0, 4.0}.length(), 5.0));

    Vec3 v3a{1.0, 0.0, 0.0};
    Vec3 v3b{0.0, 1.0, 0.0};
    Vec3 v3cross = v3a.cross(v3b);
    CHECK(v3cross.approx_equals(Vec3{0.0, 0.0, 1.0}));
    CHECK(v3a.dot(v3b) == 0.0); // Orthogonal

    Vec3 diag{3.0, 0.0, 4.0};
    CHECK(approx_equal(diag.length(), 5.0));
    Vec3 norm = diag.normalized();
    CHECK(approx_equal(norm.length(), 1.0));
    CHECK(approx_equal(norm.x, 0.6));
    CHECK(approx_equal(norm.z, 0.8));
}

TEST_CASE("FND-07: Quaternion Rotation and Slerp") {
    Quat q_id = Quat::identity();
    CHECK(approx_equal(q_id.norm(), 1.0));

    // Rotate 90 degrees (pi/2 radians) around Z-axis: (1, 0, 0) should rotate to (0, 1, 0)
    Quat q_rot_z = Quat::from_axis_angle(Vec3{0, 0, 1}, PI / 2.0);
    Vec3 p{1, 0, 0};
    Vec3 p_rot = q_rot_z.rotate(p);
    CHECK(p_rot.approx_equals(Vec3{0, 1, 0}, 1e-7));

    // Rotate 180 degrees around Y-axis: (1, 0, 0) should rotate to (-1, 0, 0)
    Quat q_rot_y = Quat::from_axis_angle(Vec3{0, 1, 0}, PI);
    Vec3 p_rot_y = q_rot_y.rotate(p);
    CHECK(p_rot_y.approx_equals(Vec3{-1, 0, 0}, 1e-7));

    // Slerp halfway between 0 and 90 deg around Z: should give 45 deg
    Quat q_half = Quat::slerp(Quat::identity(), q_rot_z, 0.5);
    Vec3 p_half = q_half.rotate(p);
    double expected_coord = std::sqrt(0.5); // cos(45 deg)
    CHECK(p_half.approx_equals(Vec3{expected_coord, expected_coord, 0}, 1e-7));
}

TEST_CASE("FND-07: Matrix4x4 and Transform Composition") {
    Xform xform;
    xform.position = Vec3{10, 20, 30};
    xform.rotation = Quat::from_axis_angle(Vec3{0, 0, 1}, PI / 2.0); // 90 deg around Z
    xform.scale = Vec3{2, 2, 2};

    // Point (1, 0, 0):
    // 1. Scale -> (2, 0, 0)
    // 2. Rotate 90 deg Z -> (0, 2, 0)
    // 3. Translate -> (10, 22, 30)
    Vec3 pt{1, 0, 0};
    Vec3 pt_transformed = xform.transform_point(pt);
    CHECK(pt_transformed.approx_equals(Vec3{10, 22, 30}, 1e-7));

    // Matrix equivalence
    Mat4 m = xform.to_matrix();
    Vec3 pt_mat = m.transform_point(pt);
    CHECK(pt_mat.approx_equals(Vec3{10, 22, 30}, 1e-7));

    // Vector transformation (ignores translation):
    Vec3 v{1, 0, 0};
    Vec3 v_transformed = xform.transform_vector(v);
    CHECK(v_transformed.approx_equals(Vec3{0, 2, 0}, 1e-7));
}
