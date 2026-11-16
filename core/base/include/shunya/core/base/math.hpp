#pragma once

#include <cmath>
#include <concepts>
#include <algorithm>
#include <array>
#include <numbers>
#include <stdexcept>

namespace shunya::core::base::math {

inline constexpr double PI = std::numbers::pi;
inline constexpr double DEFAULT_EPSILON = 1e-9;

template <std::floating_point T>
[[nodiscard]] constexpr bool approx_equal(T a, T b, T abs_tol = static_cast<T>(DEFAULT_EPSILON), T rel_tol = static_cast<T>(DEFAULT_EPSILON)) noexcept {
    T diff = std::abs(a - b);
    if (diff <= abs_tol) return true;
    return diff <= rel_tol * std::max(std::abs(a), std::abs(b));
}

template <typename T>
[[nodiscard]] constexpr T clamp(T val, T low, T high) noexcept {
    return std::clamp(val, low, high);
}

template <std::floating_point T>
[[nodiscard]] constexpr T lerp(T a, T b, T t) noexcept {
    return a + t * (b - a);
}

template <std::floating_point T>
[[nodiscard]] constexpr T to_radians(T degrees) noexcept {
    return degrees * (static_cast<T>(PI) / static_cast<T>(180.0));
}

template <std::floating_point T>
[[nodiscard]] constexpr T to_degrees(T radians) noexcept {
    return radians * (static_cast<T>(180.0) / static_cast<T>(PI));
}

// Forward declarations
template <std::floating_point T> struct Vector2;
template <std::floating_point T> struct Vector3;
template <std::floating_point T> struct alignas(sizeof(T) * 4) Vector4;
template <std::floating_point T> struct alignas(sizeof(T) * 4) Quaternion;
template <std::floating_point T> struct alignas(sizeof(T) * 4) Matrix4x4;

// -------------------------------------------------------------
// Vector2
// -------------------------------------------------------------
template <std::floating_point T = double>
struct Vector2 {
    T x = 0;
    T y = 0;

    constexpr Vector2() noexcept = default;
    constexpr Vector2(T x_, T y_) noexcept : x(x_), y(y_) {}

    constexpr Vector2 operator+(const Vector2& rhs) const noexcept { return {x + rhs.x, y + rhs.y}; }
    constexpr Vector2 operator-(const Vector2& rhs) const noexcept { return {x - rhs.x, y - rhs.y}; }
    constexpr Vector2 operator*(T scalar) const noexcept { return {x * scalar, y * scalar}; }
    constexpr Vector2 operator/(T scalar) const {
        if (scalar == 0) throw std::domain_error("Vector2 division by zero");
        return {x / scalar, y / scalar};
    }

    constexpr Vector2& operator+=(const Vector2& rhs) noexcept { x += rhs.x; y += rhs.y; return *this; }
    constexpr Vector2& operator-=(const Vector2& rhs) noexcept { x -= rhs.x; y -= rhs.y; return *this; }
    constexpr Vector2& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; return *this; }

    [[nodiscard]] constexpr T dot(const Vector2& rhs) const noexcept { return x * rhs.x + y * rhs.y; }
    [[nodiscard]] constexpr T length_squared() const noexcept { return x * x + y * y; }
    [[nodiscard]] T length() const noexcept { return std::sqrt(length_squared()); }

    [[nodiscard]] Vector2 normalized() const {
        T len = length();
        if (len < static_cast<T>(DEFAULT_EPSILON)) return {0, 0};
        return *this * (static_cast<T>(1) / len);
    }

    [[nodiscard]] bool approx_equals(const Vector2& rhs, T tol = static_cast<T>(DEFAULT_EPSILON)) const noexcept {
        return approx_equal(x, rhs.x, tol) && approx_equal(y, rhs.y, tol);
    }

    constexpr bool operator==(const Vector2& rhs) const noexcept = default;
};

// -------------------------------------------------------------
// Vector3
// -------------------------------------------------------------
template <std::floating_point T = double>
struct Vector3 {
    T x = 0;
    T y = 0;
    T z = 0;

    constexpr Vector3() noexcept = default;
    constexpr Vector3(T x_, T y_, T z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr Vector3 operator+(const Vector3& rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z}; }
    constexpr Vector3 operator-(const Vector3& rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z}; }
    constexpr Vector3 operator*(T scalar) const noexcept { return {x * scalar, y * scalar, z * scalar}; }
    constexpr Vector3 operator-() const noexcept { return {-x, -y, -z}; }

    constexpr Vector3 operator/(T scalar) const {
        if (scalar == 0) throw std::domain_error("Vector3 division by zero");
        return {x / scalar, y / scalar, z / scalar};
    }

    constexpr Vector3& operator+=(const Vector3& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; return *this; }
    constexpr Vector3& operator-=(const Vector3& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; return *this; }
    constexpr Vector3& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }

    [[nodiscard]] constexpr T dot(const Vector3& rhs) const noexcept { return x * rhs.x + y * rhs.y + z * rhs.z; }

    [[nodiscard]] constexpr Vector3 cross(const Vector3& rhs) const noexcept {
        return {
            y * rhs.z - z * rhs.y,
            z * rhs.x - x * rhs.z,
            x * rhs.y - y * rhs.x
        };
    }

    [[nodiscard]] constexpr T length_squared() const noexcept { return x * x + y * y + z * z; }
    [[nodiscard]] T length() const noexcept { return std::sqrt(length_squared()); }

    [[nodiscard]] Vector3 normalized() const {
        T len = length();
        if (len < static_cast<T>(DEFAULT_EPSILON)) return {0, 0, 0};
        return *this * (static_cast<T>(1) / len);
    }

    [[nodiscard]] Vector3 reflect(const Vector3& normal) const noexcept {
        return *this - normal * (static_cast<T>(2) * dot(normal));
    }

    [[nodiscard]] Vector3 project(const Vector3& onto) const noexcept {
        T d = onto.length_squared();
        if (d < static_cast<T>(DEFAULT_EPSILON)) return {0, 0, 0};
        return onto * (dot(onto) / d);
    }

    [[nodiscard]] T angle_between(const Vector3& other) const {
        T denom = length() * other.length();
        if (denom < static_cast<T>(DEFAULT_EPSILON)) return 0;
        T cos_val = clamp(dot(other) / denom, static_cast<T>(-1.0), static_cast<T>(1.0));
        return std::acos(cos_val);
    }

    [[nodiscard]] bool approx_equals(const Vector3& rhs, T tol = static_cast<T>(DEFAULT_EPSILON)) const noexcept {
        return approx_equal(x, rhs.x, tol) && approx_equal(y, rhs.y, tol) && approx_equal(z, rhs.z, tol);
    }

    constexpr bool operator==(const Vector3& rhs) const noexcept = default;
};

// -------------------------------------------------------------
// Vector4 (SIMD-aligned 4-component vector)
// -------------------------------------------------------------
template <std::floating_point T = double>
struct alignas(sizeof(T) * 4) Vector4 {
    T x = 0;
    T y = 0;
    T z = 0;
    T w = 0;

    constexpr Vector4() noexcept = default;
    constexpr Vector4(T x_, T y_, T z_, T w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vector4(const Vector3<T>& v, T w_ = 0) noexcept : x(v.x), y(v.y), z(v.z), w(w_) {}

    constexpr Vector4 operator+(const Vector4& rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w}; }
    constexpr Vector4 operator-(const Vector4& rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w}; }
    constexpr Vector4 operator*(T scalar) const noexcept { return {x * scalar, y * scalar, z * scalar, w * scalar}; }
    constexpr Vector4 operator-() const noexcept { return {-x, -y, -z, -w}; }

    constexpr Vector4 operator/(T scalar) const {
        if (scalar == 0) throw std::domain_error("Vector4 division by zero");
        return {x / scalar, y / scalar, z / scalar, w / scalar};
    }

    constexpr Vector4& operator+=(const Vector4& rhs) noexcept { x += rhs.x; y += rhs.y; z += rhs.z; w += rhs.w; return *this; }
    constexpr Vector4& operator-=(const Vector4& rhs) noexcept { x -= rhs.x; y -= rhs.y; z -= rhs.z; w -= rhs.w; return *this; }
    constexpr Vector4& operator*=(T scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; w *= scalar; return *this; }

    [[nodiscard]] constexpr T dot(const Vector4& rhs) const noexcept {
        return x * rhs.x + y * rhs.y + z * rhs.z + w * rhs.w;
    }

    [[nodiscard]] constexpr T length_squared() const noexcept {
        return x * x + y * y + z * z + w * w;
    }

    [[nodiscard]] T length() const noexcept {
        return std::sqrt(length_squared());
    }

    [[nodiscard]] Vector4 normalized() const {
        T len = length();
        if (len < static_cast<T>(DEFAULT_EPSILON)) return {0, 0, 0, 0};
        return *this * (static_cast<T>(1) / len);
    }

    [[nodiscard]] Vector3<T> to_vector3() const noexcept {
        return {x, y, z};
    }

    [[nodiscard]] Vector3<T> perspective_divide() const {
        if (w != 0 && w != 1) {
            T inv_w = static_cast<T>(1) / w;
            return {x * inv_w, y * inv_w, z * inv_w};
        }
        return {x, y, z};
    }

    [[nodiscard]] bool approx_equals(const Vector4& rhs, T tol = static_cast<T>(DEFAULT_EPSILON)) const noexcept {
        return approx_equal(x, rhs.x, tol) && approx_equal(y, rhs.y, tol) &&
               approx_equal(z, rhs.z, tol) && approx_equal(w, rhs.w, tol);
    }

    constexpr bool operator==(const Vector4& rhs) const noexcept = default;
};

// -------------------------------------------------------------
// Quaternion (SIMD-aligned unit quaternion)
// -------------------------------------------------------------
template <std::floating_point T = double>
struct alignas(sizeof(T) * 4) Quaternion {
    T w = 1;
    T x = 0;
    T y = 0;
    T z = 0;

    constexpr Quaternion() noexcept = default;
    constexpr Quaternion(T w_, T x_, T y_, T z_) noexcept : w(w_), x(x_), y(y_), z(z_) {}

    [[nodiscard]] static Quaternion identity() noexcept { return {1, 0, 0, 0}; }

    [[nodiscard]] static Quaternion from_axis_angle(const Vector3<T>& axis, T angle_radians) {
        Vector3<T> n = axis.normalized();
        T half = angle_radians * static_cast<T>(0.5);
        T sin_half = std::sin(half);
        return {std::cos(half), n.x * sin_half, n.y * sin_half, n.z * sin_half};
    }

    [[nodiscard]] static Quaternion from_euler_angles(T roll_x, T pitch_y, T yaw_z) {
        T cr = std::cos(roll_x * static_cast<T>(0.5));
        T sr = std::sin(roll_x * static_cast<T>(0.5));
        T cp = std::cos(pitch_y * static_cast<T>(0.5));
        T sp = std::sin(pitch_y * static_cast<T>(0.5));
        T cy = std::cos(yaw_z * static_cast<T>(0.5));
        T sy = std::sin(yaw_z * static_cast<T>(0.5));

        return {
            cr * cp * cy + sr * sp * sy,
            sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy
        };
    }

    [[nodiscard]] Vector3<T> to_euler_angles() const noexcept {
        Vector3<T> angles;

        // roll (x-axis rotation)
        T sinr_cosp = static_cast<T>(2) * (w * x + y * z);
        T cosr_cosp = static_cast<T>(1) - static_cast<T>(2) * (x * x + y * y);
        angles.x = std::atan2(sinr_cosp, cosr_cosp);

        // pitch (y-axis rotation)
        T sinp = static_cast<T>(2) * (w * y - z * x);
        if (std::abs(sinp) >= static_cast<T>(1)) {
            angles.y = std::copysign(static_cast<T>(PI / 2.0), sinp);
        } else {
            angles.y = std::asin(sinp);
        }

        // yaw (z-axis rotation)
        T siny_cosp = static_cast<T>(2) * (w * z + x * y);
        T cosy_cosp = static_cast<T>(1) - static_cast<T>(2) * (y * y + z * z);
        angles.z = std::atan2(siny_cosp, cosy_cosp);

        return angles;
    }

    constexpr Quaternion operator*(const Quaternion& rhs) const noexcept {
        return {
            w * rhs.w - x * rhs.x - y * rhs.y - z * rhs.z,
            w * rhs.x + x * rhs.w + y * rhs.z - z * rhs.y,
            w * rhs.y - x * rhs.z + y * rhs.w + z * rhs.x,
            w * rhs.z + x * rhs.y - y * rhs.x + z * rhs.w
        };
    }

    [[nodiscard]] constexpr Quaternion conjugate() const noexcept {
        return {w, -x, -y, -z};
    }

    [[nodiscard]] constexpr T norm_squared() const noexcept {
        return w * w + x * x + y * y + z * z;
    }

    [[nodiscard]] T norm() const noexcept {
        return std::sqrt(norm_squared());
    }

    [[nodiscard]] Quaternion normalized() const {
        T n = norm();
        if (n < static_cast<T>(DEFAULT_EPSILON)) return identity();
        T inv = static_cast<T>(1) / n;
        return {w * inv, x * inv, y * inv, z * inv};
    }

    [[nodiscard]] Vector3<T> rotate(const Vector3<T>& v) const noexcept {
        Quaternion qv{0, v.x, v.y, v.z};
        Quaternion res = (*this) * qv * conjugate();
        return {res.x, res.y, res.z};
    }

    [[nodiscard]] static Quaternion slerp(const Quaternion& q0, const Quaternion& q1, T t) {
        Quaternion q1_adj = q1;
        T dot_val = q0.w * q1.w + q0.x * q1.x + q0.y * q1.y + q0.z * q1.z;

        if (dot_val < 0) {
            dot_val = -dot_val;
            q1_adj = {-q1.w, -q1.x, -q1.y, -q1.z};
        }

        if (dot_val > static_cast<T>(0.9995)) {
            Quaternion res{
                lerp(q0.w, q1_adj.w, t),
                lerp(q0.x, q1_adj.x, t),
                lerp(q0.y, q1_adj.y, t),
                lerp(q0.z, q1_adj.z, t)
            };
            return res.normalized();
        }

        T theta_0 = std::acos(dot_val);
        T theta = theta_0 * t;
        T sin_theta = std::sin(theta);
        T sin_theta_0 = std::sin(theta_0);

        T s0 = std::cos(theta) - dot_val * sin_theta / sin_theta_0;
        T s1 = sin_theta / sin_theta_0;

        return {
            s0 * q0.w + s1 * q1_adj.w,
            s0 * q0.x + s1 * q1_adj.x,
            s0 * q0.y + s1 * q1_adj.y,
            s0 * q0.z + s1 * q1_adj.z
        };
    }

    [[nodiscard]] bool approx_equals(const Quaternion& rhs, T tol = static_cast<T>(DEFAULT_EPSILON)) const noexcept {
        return (approx_equal(w, rhs.w, tol) && approx_equal(x, rhs.x, tol) &&
                approx_equal(y, rhs.y, tol) && approx_equal(z, rhs.z, tol)) ||
               (approx_equal(w, -rhs.w, tol) && approx_equal(x, -rhs.x, tol) &&
                approx_equal(y, -rhs.y, tol) && approx_equal(z, -rhs.z, tol));
    }

    constexpr bool operator==(const Quaternion& rhs) const noexcept = default;
};

// -------------------------------------------------------------
// Matrix4x4 (Row-major 4x4 matrix, SIMD-aligned)
// -------------------------------------------------------------
template <std::floating_point T = double>
struct alignas(sizeof(T) * 4) Matrix4x4 {
    std::array<T, 16> m{
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    [[nodiscard]] static Matrix4x4 identity() noexcept {
        return Matrix4x4{};
    }

    [[nodiscard]] static Matrix4x4 translation(const Vector3<T>& t) noexcept {
        Matrix4x4 res;
        res.m[3] = t.x;
        res.m[7] = t.y;
        res.m[11] = t.z;
        return res;
    }

    [[nodiscard]] static Matrix4x4 scaling(const Vector3<T>& s) noexcept {
        Matrix4x4 res;
        res.m[0] = s.x;
        res.m[5] = s.y;
        res.m[10] = s.z;
        return res;
    }

    [[nodiscard]] static Matrix4x4 from_quaternion(const Quaternion<T>& q) noexcept {
        Matrix4x4 res;
        T qx = q.x, qy = q.y, qz = q.z, qw = q.w;
        T x2 = qx + qx, y2 = qy + qy, z2 = qz + qz;
        T xx = qx * x2, xy = qx * y2, xz = qx * z2;
        T yy = qy * y2, yz = qy * z2, zz = qz * z2;
        T wx = qw * x2, wy = qw * y2, wz = qw * z2;

        res.m[0] = 1 - (yy + zz); res.m[1] = xy - wz;        res.m[2] = xz + wy;        res.m[3] = 0;
        res.m[4] = xy + wz;        res.m[5] = 1 - (xx + zz); res.m[6] = yz - wx;        res.m[7] = 0;
        res.m[8] = xz - wy;        res.m[9] = yz + wx;        res.m[10] = 1 - (xx + yy); res.m[11] = 0;
        res.m[12] = 0;             res.m[13] = 0;            res.m[14] = 0;             res.m[15] = 1;

        return res;
    }

    [[nodiscard]] static Matrix4x4 perspective(T fov_y_rad, T aspect, T z_near, T z_far) {
        if (aspect == 0 || z_near == z_far) {
            throw std::invalid_argument("Invalid perspective projection parameters");
        }
        T f = static_cast<T>(1) / std::tan(fov_y_rad * static_cast<T>(0.5));
        Matrix4x4 res;
        res.m[0] = f / aspect; res.m[1] = 0; res.m[2] = 0;                                          res.m[3] = 0;
        res.m[4] = 0;          res.m[5] = f; res.m[6] = 0;                                          res.m[7] = 0;
        res.m[8] = 0;          res.m[9] = 0; res.m[10] = (z_far + z_near) / (z_near - z_far);       res.m[11] = (static_cast<T>(2) * z_far * z_near) / (z_near - z_far);
        res.m[12] = 0;         res.m[13] = 0; res.m[14] = -1;                                       res.m[15] = 0;
        return res;
    }

    [[nodiscard]] static Matrix4x4 orthographic(T left, T right, T bottom, T top, T z_near, T z_far) {
        Matrix4x4 res;
        res.m[0] = static_cast<T>(2) / (right - left);
        res.m[1] = 0;
        res.m[2] = 0;
        res.m[3] = -(right + left) / (right - left);

        res.m[4] = 0;
        res.m[5] = static_cast<T>(2) / (top - bottom);
        res.m[6] = 0;
        res.m[7] = -(top + bottom) / (top - bottom);

        res.m[8] = 0;
        res.m[9] = 0;
        res.m[10] = -static_cast<T>(2) / (z_far - z_near);
        res.m[11] = -(z_far + z_near) / (z_far - z_near);

        res.m[12] = 0;
        res.m[13] = 0;
        res.m[14] = 0;
        res.m[15] = 1;
        return res;
    }

    [[nodiscard]] static Matrix4x4 look_at(const Vector3<T>& eye, const Vector3<T>& target, const Vector3<T>& up) {
        Vector3<T> f = (target - eye).normalized();
        Vector3<T> r = f.cross(up).normalized();
        Vector3<T> u = r.cross(f);

        Matrix4x4 res;
        res.m[0] = r.x;  res.m[1] = r.y;  res.m[2] = r.z;  res.m[3] = -r.dot(eye);
        res.m[4] = u.x;  res.m[5] = u.y;  res.m[6] = u.z;  res.m[7] = -u.dot(eye);
        res.m[8] = -f.x; res.m[9] = -f.y; res.m[10] = -f.z; res.m[11] = f.dot(eye);
        res.m[12] = 0;   res.m[13] = 0;   res.m[14] = 0;   res.m[15] = 1;
        return res;
    }

    constexpr Matrix4x4 operator*(const Matrix4x4& rhs) const noexcept {
        Matrix4x4 out;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                T sum = 0;
                for (int k = 0; k < 4; ++k) {
                    sum += m[r * 4 + k] * rhs.m[k * 4 + c];
                }
                out.m[r * 4 + c] = sum;
            }
        }
        return out;
    }

    [[nodiscard]] constexpr Vector4<T> operator*(const Vector4<T>& v) const noexcept {
        return {
            m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3] * v.w,
            m[4] * v.x + m[5] * v.y + m[6] * v.z + m[7] * v.w,
            m[8] * v.x + m[9] * v.y + m[10] * v.z + m[11] * v.w,
            m[12] * v.x + m[13] * v.y + m[14] * v.z + m[15] * v.w
        };
    }

    [[nodiscard]] constexpr Matrix4x4 transpose() const noexcept {
        Matrix4x4 res;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                res.m[c * 4 + r] = m[r * 4 + c];
            }
        }
        return res;
    }

    [[nodiscard]] T determinant() const noexcept {
        T a00 = m[0],  a01 = m[1],  a02 = m[2],  a03 = m[3];
        T a10 = m[4],  a11 = m[5],  a12 = m[6],  a13 = m[7];
        T a20 = m[8],  a21 = m[9],  a22 = m[10], a23 = m[11];
        T a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15];

        T b00 = a00 * a11 - a01 * a10;
        T b01 = a00 * a12 - a02 * a10;
        T b02 = a00 * a13 - a03 * a10;
        T b03 = a01 * a12 - a02 * a11;
        T b04 = a01 * a13 - a03 * a11;
        T b05 = a02 * a13 - a03 * a12;
        T b06 = a20 * a31 - a21 * a30;
        T b07 = a20 * a32 - a22 * a30;
        T b08 = a20 * a33 - a23 * a30;
        T b09 = a21 * a32 - a22 * a31;
        T b10 = a21 * a33 - a23 * a31;
        T b11 = a22 * a33 - a23 * a32;

        return b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
    }

    [[nodiscard]] Matrix4x4 inverse() const {
        T a00 = m[0],  a01 = m[1],  a02 = m[2],  a03 = m[3];
        T a10 = m[4],  a11 = m[5],  a12 = m[6],  a13 = m[7];
        T a20 = m[8],  a21 = m[9],  a22 = m[10], a23 = m[11];
        T a30 = m[12], a31 = m[13], a32 = m[14], a33 = m[15];

        T b00 = a00 * a11 - a01 * a10;
        T b01 = a00 * a12 - a02 * a10;
        T b02 = a00 * a13 - a03 * a10;
        T b03 = a01 * a12 - a02 * a11;
        T b04 = a01 * a13 - a03 * a11;
        T b05 = a02 * a13 - a03 * a12;
        T b06 = a20 * a31 - a21 * a30;
        T b07 = a20 * a32 - a22 * a30;
        T b08 = a20 * a33 - a23 * a30;
        T b09 = a21 * a32 - a22 * a31;
        T b10 = a21 * a33 - a23 * a31;
        T b11 = a22 * a33 - a23 * a32;

        T det = b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;
        if (std::abs(det) < static_cast<T>(DEFAULT_EPSILON)) {
            throw std::domain_error("Matrix4x4 is singular and non-invertible");
        }

        T inv_det = static_cast<T>(1) / det;
        Matrix4x4 res;

        res.m[0]  = (a11 * b11 - a12 * b10 + a13 * b09) * inv_det;
        res.m[1]  = (-a01 * b11 + a02 * b10 - a03 * b09) * inv_det;
        res.m[2]  = (a31 * b05 - a32 * b04 + a33 * b03) * inv_det;
        res.m[3]  = (-a21 * b05 + a22 * b04 - a23 * b03) * inv_det;

        res.m[4]  = (-a10 * b11 + a12 * b08 - a13 * b07) * inv_det;
        res.m[5]  = (a00 * b11 - a02 * b08 + a03 * b07) * inv_det;
        res.m[6]  = (-a30 * b05 + a32 * b02 - a33 * b01) * inv_det;
        res.m[7]  = (a20 * b05 - a22 * b02 + a23 * b01) * inv_det;

        res.m[8]  = (a10 * b10 - a11 * b08 + a13 * b06) * inv_det;
        res.m[9]  = (-a00 * b10 + a01 * b08 - a03 * b06) * inv_det;
        res.m[10] = (a30 * b04 - a31 * b02 + a33 * b00) * inv_det;
        res.m[11] = (-a20 * b04 + a21 * b02 - a23 * b00) * inv_det;

        res.m[12] = (-a10 * b09 + a11 * b07 - a12 * b06) * inv_det;
        res.m[13] = (a00 * b09 - a01 * b07 + a02 * b06) * inv_det;
        res.m[14] = (-a30 * b03 + a31 * b01 - a32 * b00) * inv_det;
        res.m[15] = (a20 * b03 - a21 * b01 + a22 * b00) * inv_det;

        return res;
    }

    [[nodiscard]] Vector3<T> transform_point(const Vector3<T>& p) const noexcept {
        T x = m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3];
        T y = m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7];
        T z = m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11];
        T w = m[12] * p.x + m[13] * p.y + m[14] * p.z + m[15];
        if (w != 1 && w != 0) {
            T inv_w = static_cast<T>(1) / w;
            return {x * inv_w, y * inv_w, z * inv_w};
        }
        return {x, y, z};
    }

    [[nodiscard]] Vector3<T> transform_vector(const Vector3<T>& v) const noexcept {
        return {
            m[0] * v.x + m[1] * v.y + m[2] * v.z,
            m[4] * v.x + m[5] * v.y + m[6] * v.z,
            m[8] * v.x + m[9] * v.y + m[10] * v.z
        };
    }

    constexpr bool operator==(const Matrix4x4& rhs) const noexcept = default;
};

// -------------------------------------------------------------
// Transform
// -------------------------------------------------------------
template <std::floating_point T = double>
struct Transform {
    Vector3<T> position{0, 0, 0};
    Quaternion<T> rotation{Quaternion<T>::identity()};
    Vector3<T> scale{1, 1, 1};

    [[nodiscard]] Matrix4x4<T> to_matrix() const noexcept {
        Matrix4x4<T> t = Matrix4x4<T>::translation(position);
        Matrix4x4<T> r = Matrix4x4<T>::from_quaternion(rotation);
        Matrix4x4<T> s = Matrix4x4<T>::scaling(scale);
        return t * r * s;
    }

    [[nodiscard]] Vector3<T> transform_point(const Vector3<T>& p) const noexcept {
        Vector3<T> scaled{p.x * scale.x, p.y * scale.y, p.z * scale.z};
        Vector3<T> rotated = rotation.rotate(scaled);
        return rotated + position;
    }

    [[nodiscard]] Vector3<T> transform_vector(const Vector3<T>& v) const noexcept {
        Vector3<T> scaled{v.x * scale.x, v.y * scale.y, v.z * scale.z};
        return rotation.rotate(scaled);
    }
};

using Vec2 = Vector2<double>;
using Vec3 = Vector3<double>;
using Vec4 = Vector4<double>;
using Quat = Quaternion<double>;
using Mat4 = Matrix4x4<double>;
using Xform = Transform<double>;

} // namespace shunya::core::base::math
