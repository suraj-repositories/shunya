#pragma once

#include "shunya/cad3d/solid_mesh.hpp"
#include "shunya/core/base/math.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace shunya::cad3d {

/**
 * @brief 32-byte cacheline-aligned interleaved GPU vertex layout (Position, Normal, UV).
 * Optimized for peak GPU vertex fetch throughput in Vulkan, Direct3D 12, and Metal.
 */
struct alignas(32) GpuVertex32 {
    float pos[3]{0.0f, 0.0f, 0.0f};       // 12 bytes, offset 0
    float normal[3]{0.0f, 0.0f, 1.0f};    // 12 bytes, offset 12
    float uv[2]{0.0f, 0.0f};              // 8 bytes, offset 24
};
static_assert(sizeof(GpuVertex32) == 32, "GpuVertex32 must be exactly 32 bytes");
static_assert(alignof(GpuVertex32) == 32, "GpuVertex32 must have 32-byte alignment");

/**
 * @brief 32-byte cacheline-aligned interleaved GPU vertex layout with packed octahedral Tangent & Normal.
 * Packed for full PBR normal mapping within a strict 32-byte memory budget.
 */
struct alignas(32) GpuVertexTangent32 {
    float pos[3]{0.0f, 0.0f, 0.0f};                 // 12 bytes, offset 0
    float uv[2]{0.0f, 0.0f};                        // 8 bytes, offset 12
    std::array<int16_t, 2> normal_oct{0, 32767};    // 4 bytes, offset 20 (SNORM16 Octahedral encoding)
    std::array<int16_t, 2> tangent_oct{32767, 0};   // 4 bytes, offset 24 (SNORM16 Octahedral encoding)
    int8_t tangent_sign{1};                         // 1 byte, offset 28 (+1 or -1 bitangent sign)
    uint8_t reserved[3]{0, 0, 0};                   // 3 bytes, offset 29 (padding)
};
static_assert(sizeof(GpuVertexTangent32) == 32, "GpuVertexTangent32 must be exactly 32 bytes");
static_assert(alignof(GpuVertexTangent32) == 32, "GpuVertexTangent32 must have 32-byte alignment");

/**
 * @brief 64-byte interleaved GPU vertex layout with uncompressed 3D tangents & vertex color (32-byte aligned).
 */
struct alignas(32) GpuVertexFull64 {
    float pos[3]{0.0f, 0.0f, 0.0f};           // 12 bytes, offset 0
    float normal[3]{0.0f, 0.0f, 1.0f};        // 12 bytes, offset 12
    float tangent[4]{1.0f, 0.0f, 0.0f, 1.0f}; // 16 bytes, offset 24 (xyz + handedness)
    float uv[2]{0.0f, 0.0f};                  // 8 bytes, offset 40
    float color[4]{1.0f, 1.0f, 1.0f, 1.0f};   // 16 bytes, offset 48
};
static_assert(sizeof(GpuVertexFull64) == 64, "GpuVertexFull64 must be exactly 64 bytes");
static_assert(alignof(GpuVertexFull64) == 32, "GpuVertexFull64 must have 32-byte alignment");

/**
 * @brief GPU Mesh Buffer Bundle containing interleaved vertex buffer and index buffer.
 */
template <typename TVertex>
struct GpuMeshBuffer {
    std::vector<TVertex> vertices;
    std::vector<uint32_t> indices;

    [[nodiscard]] size_t vertex_count() const noexcept { return vertices.size(); }
    [[nodiscard]] size_t index_count() const noexcept { return indices.size(); }
    [[nodiscard]] size_t vertex_buffer_bytes() const noexcept { return vertices.size() * sizeof(TVertex); }
    [[nodiscard]] size_t index_buffer_bytes() const noexcept { return indices.size() * sizeof(uint32_t); }

    [[nodiscard]] bool is_32byte_aligned() const noexcept {
        if (vertices.empty()) return true;
        return (reinterpret_cast<uintptr_t>(vertices.data()) % 32 == 0);
    }
};

/**
 * @brief Generator and compressor for GPU-optimized interleaved vertex buffers.
 */
class GpuVertexBufferBuilder {
public:
    /// Octahedral unit vector quantization to 2x int16_t (SNORM16)
    [[nodiscard]] static std::array<int16_t, 2> encode_octahedral(float nx, float ny, float nz) noexcept;
    /// Octahedral decoding back to normalized unit vector
    [[nodiscard]] static std::array<float, 3> decode_octahedral(int16_t ox, int16_t oy) noexcept;

    /// Build standard 32-byte interleaved vertex buffer from SolidMesh3D
    [[nodiscard]] static GpuMeshBuffer<GpuVertex32> build_interleaved_32(
        const SolidMesh3D& mesh,
        bool compute_smooth_normals = true
    );

    /// Build packed 32-byte interleaved vertex buffer with tangents from SolidMesh3D
    [[nodiscard]] static GpuMeshBuffer<GpuVertexTangent32> build_packed_tangents_32(
        const SolidMesh3D& mesh
    );

    /// Build full 64-byte interleaved vertex buffer from SolidMesh3D
    [[nodiscard]] static GpuMeshBuffer<GpuVertexFull64> build_full_64(
        const SolidMesh3D& mesh
    );

    /// Verifies whether a contiguous memory buffer satisfies strict 32-byte alignment constraints
    [[nodiscard]] static bool verify_buffer_alignment(const void* ptr, size_t stride, size_t total_bytes) noexcept;
};

} // namespace shunya::cad3d
