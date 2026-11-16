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


TEST_CASE("PH4-01.02.005: SIMD-Aligned Vector4 and Vector3 Primitives") {
    // 1. Verify SIMD memory alignment
    CHECK(alignof(Vec4) >= 32);
    CHECK(alignof(Quat) >= 32);
    CHECK(alignof(Mat4) >= 32);

    // 2. Vector4 operations
    Vec4 v4a{1.0, 2.0, 3.0, 4.0};
    Vec4 v4b{2.0, 3.0, 4.0, 5.0};
    Vec4 sum = v4a + v4b;
    CHECK(sum.x == 3.0);
    CHECK(sum.y == 5.0);
    CHECK(sum.z == 7.0);
    CHECK(sum.w == 9.0);

    CHECK(v4a.dot(v4b) == (1*2 + 2*3 + 3*4 + 4*5)); // 2 + 6 + 12 + 20 = 40.0
    CHECK(approx_equal(Vec4{0, 0, 3.0, 4.0}.length(), 5.0));

    Vec4 norm4 = Vec4{0, 0, 3.0, 4.0}.normalized();
    CHECK(approx_equal(norm4.length(), 1.0));
    CHECK(approx_equal(norm4.z, 0.6));
    CHECK(approx_equal(norm4.w, 0.8));

    // Perspective divide
    Vec4 clip_pt{10.0, 20.0, 30.0, 2.0};
    Vec3 div_pt = clip_pt.perspective_divide();
    CHECK(div_pt.approx_equals(Vec3{5.0, 10.0, 15.0}));

    // 3. Vector3 extensions (reflect, project, angle_between)
    Vec3 ray{1.0, -1.0, 0.0};
    Vec3 normal{0.0, 1.0, 0.0};
    Vec3 reflected = ray.reflect(normal);
    CHECK(reflected.approx_equals(Vec3{1.0, 1.0, 0.0}));

    Vec3 vec_a{3.0, 4.0, 0.0};
    Vec3 axis_x{1.0, 0.0, 0.0};
    Vec3 proj = vec_a.project(axis_x);
    CHECK(proj.approx_equals(Vec3{3.0, 0.0, 0.0}));

    Vec3 vec_b{0.0, 5.0, 0.0};
    double angle = vec_a.angle_between(vec_b);
    CHECK(approx_equal(angle, std::atan2(3.0, 4.0), 1e-6));
}

TEST_CASE("PH4-01.02.005: Matrix4x4 Inverse, Determinant, Transpose & Perspective/LookAt") {
    // 1. Determinant and Transpose of Identity
    Mat4 id = Mat4::identity();
    CHECK(approx_equal(id.determinant(), 1.0));
    CHECK(id.transpose() == id);

    // 2. Matrix Multiplication with Vector4
    Mat4 tr = Mat4::translation(Vec3{5, 10, 15});
    Vec4 pt4{1, 2, 3, 1};
    Vec4 transformed4 = tr * pt4;
    CHECK(transformed4.approx_equals(Vec4{6, 12, 18, 1}));

    // 3. Determinant and Inverse of non-trivial transform matrix
    Xform xf;
    xf.position = Vec3{1.5, -3.2, 4.8};
    xf.rotation = Quat::from_axis_angle(Vec3{0, 1, 0}, PI / 4.0);
    xf.scale = Vec3{2.0, 3.0, 0.5};
    Mat4 m = xf.to_matrix();

    double det = m.determinant();
    // Scaling det = 2 * 3 * 0.5 = 3.0 (rotation det = 1, translation det = 1)
    CHECK(approx_equal(det, 3.0, 1e-5));

    Mat4 inv = m.inverse();
    Mat4 identity_check = m * inv;
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            double expected = (r == c) ? 1.0 : 0.0;
            CHECK(approx_equal(identity_check.m[r * 4 + c], expected, 1e-6));
        }
    }

    // 4. LookAt camera matrix
    Vec3 eye{0, 0, 10};
    Vec3 target{0, 0, 0};
    Vec3 up{0, 1, 0};
    Mat4 view = Mat4::look_at(eye, target, up);
    Vec3 origin_in_view = view.transform_point(Vec3{0, 0, 0});
    CHECK(origin_in_view.approx_equals(Vec3{0, 0, -10}, 1e-6));

    // 5. Perspective projection
    Mat4 proj = Mat4::perspective(to_radians(60.0), 16.0 / 9.0, 0.1, 1000.0);
    CHECK(proj.m[14] == -1.0);
    CHECK(proj.m[0] > 0.0);
    CHECK(proj.m[5] > 0.0);
}

TEST_CASE("PH4-01.02.005: Quaternion Euler Angles & Conversions") {
    // Pitch 30 degrees (around Y)
    double pitch_rad = to_radians(30.0);
    Quat q_pitch = Quat::from_euler_angles(0.0, pitch_rad, 0.0);
    Vec3 euler = q_pitch.to_euler_angles();
    CHECK(approx_equal(euler.y, pitch_rad, 1e-6));
    CHECK(approx_equal(euler.x, 0.0, 1e-6));
    CHECK(approx_equal(euler.z, 0.0, 1e-6));

    // Combined Roll, Pitch, Yaw
    double roll = to_radians(15.0);
    double pitch = to_radians(-25.0);
    double yaw = to_radians(45.0);
    Quat q_combined = Quat::from_euler_angles(roll, pitch, yaw);
    Vec3 roundtrip = q_combined.to_euler_angles();
    CHECK(approx_equal(roundtrip.x, roll, 1e-6));
    CHECK(approx_equal(roundtrip.y, pitch, 1e-6));
    CHECK(approx_equal(roundtrip.z, yaw, 1e-6));
}
