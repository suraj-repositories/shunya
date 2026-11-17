#include "shunya/cad3d/shader_pipeline_cache.hpp"

#include <cstring>
#include <fstream>

namespace shunya::cad3d {

namespace {

constexpr uint32_t PIPELINE_CACHE_MAGIC = 0x53485043; // "SHPC"
constexpr uint32_t PIPELINE_CACHE_VERSION = 1;

uint32_t compute_crc32(std::span<const uint8_t> data) noexcept {
    uint32_t crc = 0xFFFFFFFF;
    for (uint8_t byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

} // namespace

uint64_t PipelineCacheKey::compute_hash() const noexcept {
    uint64_t h = 14695981039346656037ULL;
    auto mix = [&](uint64_t v) {
        h ^= v;
        h *= 1099511628211ULL;
    };
    mix(vs_hash);
    mix(fs_hash);
    mix(cs_hash);
    mix(static_cast<uint64_t>(target_api));
    mix(pso_desc.depth_test ? 1 : 0);
    mix(pso_desc.depth_write ? 1 : 0);
    mix(static_cast<uint64_t>(pso_desc.depth_op));
    mix(static_cast<uint64_t>(pso_desc.blend_mode));
    mix(static_cast<uint64_t>(pso_desc.cull_mode));
    for (char c : pso_desc.name) mix(static_cast<uint8_t>(c));
    mix(is_compute ? 1 : 0);
    return h;
}

bool PipelineCacheKey::operator==(const PipelineCacheKey& other) const noexcept {
    return vs_hash == other.vs_hash &&
           fs_hash == other.fs_hash &&
           cs_hash == other.cs_hash &&
           target_api == other.target_api &&
           pso_desc.depth_test == other.pso_desc.depth_test &&
           pso_desc.depth_write == other.pso_desc.depth_write &&
           pso_desc.depth_op == other.pso_desc.depth_op &&
           pso_desc.blend_mode == other.pso_desc.blend_mode &&
           pso_desc.cull_mode == other.pso_desc.cull_mode &&
           pso_desc.name == other.pso_desc.name &&
           is_compute == other.is_compute;
}

const CachedPipelineBlob* ShaderPipelineCache::find(const PipelineCacheKey& key) {
    ++metrics_.lookups;
    const uint64_t h = key.compute_hash();
    auto it = entries_.find(h);
    if (it != entries_.end()) {
        ++metrics_.hits;
        return &it->second;
    }
    ++metrics_.misses;
    return nullptr;
}

void ShaderPipelineCache::store(
    const PipelineCacheKey& key,
    uint32_t pso_id,
    std::string_view name,
    std::span<const uint8_t> binary_data
) {
    const uint64_t h = key.compute_hash();
    CachedPipelineBlob blob;
    blob.key_hash = h;
    blob.pso_id = pso_id;
    blob.pipeline_name = std::string(name);
    if (!binary_data.empty()) {
        blob.binary_data.assign(binary_data.begin(), binary_data.end());
    } else {
        // Synthesize simulated driver PSO binary blob
        blob.binary_data.resize(64);
        std::memcpy(blob.binary_data.data(), &h, sizeof(h));
        std::memcpy(blob.binary_data.data() + 8, &pso_id, sizeof(pso_id));
    }
    blob.checksum = compute_crc32(blob.binary_data);

    entries_[h] = std::move(blob);
}

std::vector<uint8_t> ShaderPipelineCache::export_blob() const {
    std::vector<uint8_t> out;

    auto append_u32 = [&](uint32_t val) {
        for (size_t b = 0; b < sizeof(val); ++b) {
            out.push_back(static_cast<uint8_t>((val >> (b * 8)) & 0xFF));
        }
    };
    auto append_u64 = [&](uint64_t val) {
        for (size_t b = 0; b < sizeof(val); ++b) {
            out.push_back(static_cast<uint8_t>((val >> (b * 8)) & 0xFF));
        }
    };

    append_u32(PIPELINE_CACHE_MAGIC);
    append_u32(PIPELINE_CACHE_VERSION);
    append_u32(static_cast<uint32_t>(entries_.size()));

    for (const auto& [hash, blob] : entries_) {
        append_u64(blob.key_hash);
        append_u32(blob.pso_id);
        append_u32(static_cast<uint32_t>(blob.pipeline_name.size()));
        out.insert(out.end(), blob.pipeline_name.begin(), blob.pipeline_name.end());
        append_u32(static_cast<uint32_t>(blob.binary_data.size()));
        out.insert(out.end(), blob.binary_data.begin(), blob.binary_data.end());
        append_u32(blob.checksum);
    }

    return out;
}

bool ShaderPipelineCache::import_blob(std::span<const uint8_t> bytes) {
    if (bytes.size() < 12) return false;

    size_t offset = 0;
    auto read_u32 = [&]() -> uint32_t {
        uint32_t val = 0;
        if (offset + sizeof(val) <= bytes.size()) {
            for (size_t b = 0; b < sizeof(val); ++b) {
                val |= (static_cast<uint32_t>(bytes[offset + b]) << (b * 8));
            }
            offset += sizeof(val);
        }
        return val;
    };
    auto read_u64 = [&]() -> uint64_t {
        uint64_t val = 0;
        if (offset + sizeof(val) <= bytes.size()) {
            for (size_t b = 0; b < sizeof(val); ++b) {
                val |= (static_cast<uint64_t>(bytes[offset + b]) << (b * 8));
            }
            offset += sizeof(val);
        }
        return val;
    };

    const uint32_t magic = read_u32();
    const uint32_t version = read_u32();
    if (magic != PIPELINE_CACHE_MAGIC || version != PIPELINE_CACHE_VERSION) {
        return false;
    }

    const uint32_t count = read_u32();
    for (uint32_t i = 0; i < count; ++i) {
        if (offset + sizeof(uint64_t) + sizeof(uint32_t) + sizeof(uint32_t) > bytes.size()) {
            return false;
        }

        CachedPipelineBlob blob;
        blob.key_hash = read_u64();
        blob.pso_id = read_u32();

        const uint32_t name_len = read_u32();
        if (offset + name_len > bytes.size()) return false;
        blob.pipeline_name.assign(reinterpret_cast<const char*>(bytes.data() + offset), name_len);
        offset += name_len;

        const uint32_t data_len = read_u32();
        if (offset + data_len + sizeof(uint32_t) > bytes.size()) return false;
        blob.binary_data.assign(bytes.data() + offset, bytes.data() + offset + data_len);
        offset += data_len;

        blob.checksum = read_u32();
        if (blob.checksum != compute_crc32(blob.binary_data)) {
            return false; // Corrupted cache payload rejected
        }

        entries_[blob.key_hash] = std::move(blob);
    }

    return true;
}

bool ShaderPipelineCache::save_to_file(const std::filesystem::path& file_path) const {
    const auto blob = export_blob();
    std::ofstream ofs(file_path, std::ios::binary);
    if (!ofs.is_open()) return false;
    ofs.write(reinterpret_cast<const char*>(blob.data()), static_cast<std::streamsize>(blob.size()));
    return ofs.good();
}

bool ShaderPipelineCache::load_from_file(const std::filesystem::path& file_path) {
    std::ifstream ifs(file_path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) return false;
    const auto size = ifs.tellg();
    if (size <= 0) return false;
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    ifs.read(reinterpret_cast<char*>(buffer.data()), size);
    return import_blob(buffer);
}

void ShaderPipelineCache::prewarm_standard_pipelines(ShaderCompiler& compiler, ShaderTargetApi target) {
    struct ModeSpec {
        const char* name;
        PipelineStateDesc pso;
        bool is_compute;
    };

    const ModeSpec standard_modes[] = {
        {"Opaque_PBR", {"opaque_pbr", true, true, RhiCompareOp::Less, RhiBlendMode::Opaque, RhiCullMode::Back}, false},
        {"Transparent_WBOIT", {"transparent_wboit", true, false, RhiCompareOp::Less, RhiBlendMode::AlphaBlend, RhiCullMode::None}, false},
        {"Wireframe_Overlay", {"wireframe_overlay", true, true, RhiCompareOp::LessEqual, RhiBlendMode::Opaque, RhiCullMode::None}, false},
        {"Depth_Prepass", {"depth_prepass", true, true, RhiCompareOp::Less, RhiBlendMode::Opaque, RhiCullMode::Back}, false},
        {"Shadow_Cascade", {"shadow_cascade", true, true, RhiCompareOp::Less, RhiBlendMode::Opaque, RhiCullMode::Back}, false},
        {"Normals_Debug", {"normals_debug", true, true, RhiCompareOp::Less, RhiBlendMode::Opaque, RhiCullMode::Back}, false},
        {"Compute_LightCull", {"compute_light_cull", false, false, RhiCompareOp::Always, RhiBlendMode::Opaque, RhiCullMode::None}, true}
    };

    (void)compiler.compile(ShaderCompiler::builtin_wboit_accum_frag(), ShaderStage::Fragment, target);
    (void)compiler.compile(ShaderCompiler::builtin_light_culler_comp(), ShaderStage::Compute, target);

    uint32_t assigned_pso_id = 0;
    for (const auto& spec : standard_modes) {
        PipelineCacheKey key{};
        key.pso_desc = spec.pso;
        key.target_api = target;
        key.is_compute = spec.is_compute;
        key.vs_hash = 0x12345678ULL;
        key.fs_hash = spec.is_compute ? 0 : 0x87654321ULL;
        key.cs_hash = spec.is_compute ? 0xABCDEF01ULL : 0;

        store(key, assigned_pso_id++, spec.name);
        ++metrics_.prewarmed_count;
    }
}

void ShaderPipelineCache::clear() noexcept {
    entries_.clear();
    metrics_ = PipelineCacheMetrics{};
}

} // namespace shunya::cad3d
