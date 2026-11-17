#pragma once

#include "shunya/cad3d/render_pass.hpp"
#include "shunya/cad3d/shader_compiler.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::cad3d {

struct PipelineCacheKey {
    uint64_t vs_hash{0};
    uint64_t fs_hash{0};
    uint64_t cs_hash{0};
    PipelineStateDesc pso_desc{};
    ShaderTargetApi target_api{ShaderTargetApi::SpirV_Vulkan};
    bool is_compute{false};

    [[nodiscard]] uint64_t compute_hash() const noexcept;
    bool operator==(const PipelineCacheKey& other) const noexcept;
};

struct CachedPipelineBlob {
    uint64_t key_hash{0};
    uint32_t pso_id{0};
    std::string pipeline_name;
    std::vector<uint8_t> binary_data;
    uint32_t checksum{0};
};

struct PipelineCacheMetrics {
    uint64_t lookups{0};
    uint64_t hits{0};
    uint64_t misses{0};
    uint64_t prewarmed_count{0};

    [[nodiscard]] double hit_ratio() const noexcept {
        return lookups > 0 ? static_cast<double>(hits) / static_cast<double>(lookups) : 0.0;
    }
};

/**
 * @brief High-performance pipeline state and shader bytecode cache.
 * Eliminates runtime compilation hitches across graphics rendering modes and compute dispatches.
 */
class ShaderPipelineCache {
public:
    ShaderPipelineCache() = default;

    [[nodiscard]] size_t size() const noexcept { return entries_.size(); }
    [[nodiscard]] const PipelineCacheMetrics& metrics() const noexcept { return metrics_; }
    void reset_metrics() noexcept { metrics_ = PipelineCacheMetrics{}; }

    [[nodiscard]] const CachedPipelineBlob* find(const PipelineCacheKey& key);

    void store(
        const PipelineCacheKey& key,
        uint32_t pso_id,
        std::string_view name,
        std::span<const uint8_t> binary_data = {}
    );

    bool save_to_file(const std::filesystem::path& file_path) const;
    bool load_from_file(const std::filesystem::path& file_path);

    [[nodiscard]] std::vector<uint8_t> export_blob() const;
    bool import_blob(std::span<const uint8_t> bytes);

    void prewarm_standard_pipelines(ShaderCompiler& compiler, ShaderTargetApi target);

    void clear() noexcept;

private:
    std::unordered_map<uint64_t, CachedPipelineBlob> entries_;
    PipelineCacheMetrics metrics_{};
};

} // namespace shunya::cad3d
