#include "shunya/cad3d/gpu_vertex_buffer.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace shunya::cad3d {

namespace {

inline float sign_not_zero(float v) noexcept {
    return (v >= 0.0f) ? 1.0f : -1.0f;
}

} // namespace

std::array<int16_t, 2> GpuVertexBufferBuilder::encode_octahedral(float nx, float ny, float nz) noexcept {
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len > 1e-6f) {
        nx /= len;
        ny /= len;
        nz /= len;
    } else {
        nx = 0.0f; ny = 0.0f; nz = 1.0f;
    }

    const float l1 = std::abs(nx) + std::abs(ny) + std::abs(nz);
    float ox = nx / l1;
    float oy = ny / l1;

    if (nz < 0.0f) {
        const float old_ox = ox;
        ox = (1.0f - std::abs(oy)) * sign_not_zero(old_ox);
        oy = (1.0f - std::abs(old_ox)) * sign_not_zero(oy);
    }

    const int16_t qx = static_cast<int16_t>(std::clamp(std::round(ox * 32767.0f), -32767.0f, 32767.0f));
    const int16_t qy = static_cast<int16_t>(std::clamp(std::round(oy * 32767.0f), -32767.0f, 32767.0f));
    return {qx, qy};
}

std::array<float, 3> GpuVertexBufferBuilder::decode_octahedral(int16_t ox, int16_t oy) noexcept {
    float x = static_cast<float>(ox) / 32767.0f;
    float y = static_cast<float>(oy) / 32767.0f;
    float z = 1.0f - std::abs(x) - std::abs(y);

    if (z < 0.0f) {
        const float old_x = x;
        x = (1.0f - std::abs(y)) * sign_not_zero(old_x);
        y = (1.0f - std::abs(old_x)) * sign_not_zero(y);
    }

    const float len = std::sqrt(x * x + y * y + z * z);
    if (len > 1e-6f) {
        x /= len;
        y /= len;
        z /= len;
    }
    return {x, y, z};
}

GpuMeshBuffer<GpuVertex32> GpuVertexBufferBuilder::build_interleaved_32(
    const SolidMesh3D& mesh,
    bool compute_smooth_normals
) {
    GpuMeshBuffer<GpuVertex32> buffer;
    if (mesh.vertices.empty()) return buffer;

    const size_t vcount = mesh.vertices.size();
    buffer.vertices.resize(vcount);

    // Compute normals
    std::vector<std::array<float, 3>> normals(vcount, {0.0f, 0.0f, 0.0f});
    for (const auto& tri : mesh.triangles) {
        if (tri[0] >= vcount || tri[1] >= vcount || tri[2] >= vcount) continue;
        const auto& p0 = mesh.vertices[tri[0]];
        const auto& p1 = mesh.vertices[tri[1]];
        const auto& p2 = mesh.vertices[tri[2]];

        const Vec3d e1 = p1 - p0;
        const Vec3d e2 = p2 - p0;
        const Vec3d fn = e1.cross(e2);
        for (int i = 0; i < 3; ++i) {
            normals[tri[i]][0] += static_cast<float>(fn.x);
            normals[tri[i]][1] += static_cast<float>(fn.y);
            normals[tri[i]][2] += static_cast<float>(fn.z);
        }
    }

    // Populate vertices
    for (size_t i = 0; i < vcount; ++i) {
        auto& v = buffer.vertices[i];
        v.pos[0] = static_cast<float>(mesh.vertices[i].x);
        v.pos[1] = static_cast<float>(mesh.vertices[i].y);
        v.pos[2] = static_cast<float>(mesh.vertices[i].z);

        if (compute_smooth_normals) {
            float nx = normals[i][0];
            float ny = normals[i][1];
            float nz = normals[i][2];
            const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (len > 1e-6f) {
                nx /= len; ny /= len; nz /= len;
            } else {
                nx = 0.0f; ny = 0.0f; nz = 1.0f;
            }
            v.normal[0] = nx;
            v.normal[1] = ny;
            v.normal[2] = nz;
        } else {
            v.normal[0] = 0.0f;
            v.normal[1] = 0.0f;
            v.normal[2] = 1.0f;
        }

        // Spherical/cylindrical UV unwrap
        const float theta = std::atan2(v.pos[2], v.pos[0]);
        v.uv[0] = (theta + static_cast<float>(std::numbers::pi)) / (2.0f * static_cast<float>(std::numbers::pi));
        v.uv[1] = 0.5f + v.pos[1] * 0.1f;
    }

    // Populate indices
    buffer.indices.reserve(mesh.triangles.size() * 3);
    for (const auto& tri : mesh.triangles) {
        buffer.indices.push_back(tri[0]);
        buffer.indices.push_back(tri[1]);
        buffer.indices.push_back(tri[2]);
    }

    return buffer;
}

GpuMeshBuffer<GpuVertexTangent32> GpuVertexBufferBuilder::build_packed_tangents_32(
    const SolidMesh3D& mesh
) {
    auto base_buf = build_interleaved_32(mesh, true);
    GpuMeshBuffer<GpuVertexTangent32> buffer;
    buffer.indices = std::move(base_buf.indices);
    buffer.vertices.resize(base_buf.vertices.size());

    const size_t vcount = base_buf.vertices.size();
    std::vector<std::array<float, 3>> tangents(vcount, {0.0f, 0.0f, 0.0f});

    for (size_t ti = 0; ti < buffer.indices.size(); ti += 3) {
        const uint32_t i0 = buffer.indices[ti];
        const uint32_t i1 = buffer.indices[ti + 1];
        const uint32_t i2 = buffer.indices[ti + 2];

        const auto& v0 = base_buf.vertices[i0];
        const auto& v1 = base_buf.vertices[i1];
        const auto& v2 = base_buf.vertices[i2];

        const float e1x = v1.pos[0] - v0.pos[0], e1y = v1.pos[1] - v0.pos[1], e1z = v1.pos[2] - v0.pos[2];
        const float e2x = v2.pos[0] - v0.pos[0], e2y = v2.pos[1] - v0.pos[1], e2z = v2.pos[2] - v0.pos[2];

        const float du1 = v1.uv[0] - v0.uv[0], dv1 = v1.uv[1] - v0.uv[1];
        const float du2 = v2.uv[0] - v0.uv[0], dv2 = v2.uv[1] - v0.uv[1];

        const float det = du1 * dv2 - dv1 * du2;
        const float inv_det = (std::abs(det) > 1e-7f) ? 1.0f / det : 0.0f;

        const float tx = (e1x * dv2 - e2x * dv1) * inv_det;
        const float ty = (e1y * dv2 - e2y * dv1) * inv_det;
        const float tz = (e1z * dv2 - e2z * dv1) * inv_det;

        for (uint32_t idx : {i0, i1, i2}) {
            tangents[idx][0] += tx;
            tangents[idx][1] += ty;
            tangents[idx][2] += tz;
        }
    }

    for (size_t i = 0; i < vcount; ++i) {
        const auto& src = base_buf.vertices[i];
        auto& dst = buffer.vertices[i];

        dst.pos[0] = src.pos[0];
        dst.pos[1] = src.pos[1];
        dst.pos[2] = src.pos[2];
        dst.uv[0] = src.uv[0];
        dst.uv[1] = src.uv[1];

        dst.normal_oct = encode_octahedral(src.normal[0], src.normal[1], src.normal[2]);

        // Gram-Schmidt orthogonalize tangent
        float tx = tangents[i][0], ty = tangents[i][1], tz = tangents[i][2];
        const float ndott = src.normal[0] * tx + src.normal[1] * ty + src.normal[2] * tz;
        tx -= src.normal[0] * ndott;
        ty -= src.normal[1] * ndott;
        tz -= src.normal[2] * ndott;
        const float tlen = std::sqrt(tx * tx + ty * ty + tz * tz);
        if (tlen > 1e-6f) {
            tx /= tlen; ty /= tlen; tz /= tlen;
        } else {
            tx = 1.0f; ty = 0.0f; tz = 0.0f;
        }

        dst.tangent_oct = encode_octahedral(tx, ty, tz);
        dst.tangent_sign = 1;
        dst.reserved[0] = 0;
        dst.reserved[1] = 0;
        dst.reserved[2] = 0;
    }

    return buffer;
}

GpuMeshBuffer<GpuVertexFull64> GpuVertexBufferBuilder::build_full_64(
    const SolidMesh3D& mesh
) {
    auto base_buf = build_interleaved_32(mesh, true);
    GpuMeshBuffer<GpuVertexFull64> buffer;
    buffer.indices = std::move(base_buf.indices);
    buffer.vertices.resize(base_buf.vertices.size());

    for (size_t i = 0; i < base_buf.vertices.size(); ++i) {
        const auto& src = base_buf.vertices[i];
        auto& dst = buffer.vertices[i];

        dst.pos[0] = src.pos[0];
        dst.pos[1] = src.pos[1];
        dst.pos[2] = src.pos[2];

        dst.normal[0] = src.normal[0];
        dst.normal[1] = src.normal[1];
        dst.normal[2] = src.normal[2];

        dst.tangent[0] = 1.0f;
        dst.tangent[1] = 0.0f;
        dst.tangent[2] = 0.0f;
        dst.tangent[3] = 1.0f;

        dst.uv[0] = src.uv[0];
        dst.uv[1] = src.uv[1];

        dst.color[0] = 1.0f;
        dst.color[1] = 1.0f;
        dst.color[2] = 1.0f;
        dst.color[3] = 1.0f;
    }

    return buffer;
}

bool GpuVertexBufferBuilder::verify_buffer_alignment(
    const void* ptr,
    size_t stride,
    size_t total_bytes
) noexcept {
    if (!ptr || total_bytes == 0) return true;
    if (reinterpret_cast<uintptr_t>(ptr) % 32 != 0) return false;
    if (stride % 32 != 0) return false;
    if (total_bytes % stride != 0) return false;
    return true;
}

} // namespace shunya::cad3d
