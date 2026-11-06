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

    [[nodiscard]] bool approx_equals(const Vector3& rhs, T tol = static_cast<T>(DEFAULT_EPSILON)) const noexcept {
        return approx_equal(x, rhs.x, tol) && approx_equal(y, rhs.y, tol) && approx_equal(z, rhs.z, tol);
    }
};

// -------------------------------------------------------------
// Quaternion
// -------------------------------------------------------------
template <std::floating_point T = double>
struct Quaternion {
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
        // q * v * q^-1 for unit quaternion
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
            // Linear interpolation for small angles
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
};

// -------------------------------------------------------------
// Matrix4x4 (Row-major 4x4 matrix)
// -------------------------------------------------------------
template <std::floating_point T = double>
struct Matrix4x4 {
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
        // T * R * S
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
using Quat = Quaternion<double>;
using Mat4 = Matrix4x4<double>;
using Xform = Transform<double>;

} // namespace shunya::core::base::math
