#include "shunya/core/domain/data_exchange.hpp"

#include <bit>
#include <charconv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>

namespace shunya::core::domain {

namespace {

base::Error make_io_error(std::string message, int32_t code = 1001) {
    return base::Error{
        .code = code,
        .domain = base::ErrorDomain::IO,
        .severity = base::Severity::Error,
        .message = std::move(message),
        .details = ""
    };
}

// -----------------------------------------------------------------------------
// Pure C++20 SHA-256 Implementation (Zero External Dependencies)
// -----------------------------------------------------------------------------
struct Sha256Context {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
};

inline uint32_t rotr(uint32_t x, uint32_t n) noexcept {
    return (x >> n) | (x << (32 - n));
}

inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) noexcept { return (x & y) ^ (~x & z); }
inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) noexcept { return (x & y) ^ (x & z) ^ (y & z); }
inline uint32_t ep0(uint32_t x) noexcept { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
inline uint32_t ep1(uint32_t x) noexcept { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
inline uint32_t sig0(uint32_t x) noexcept { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
inline uint32_t sig1(uint32_t x) noexcept { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

constexpr uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void sha256_transform(uint32_t state[8], const uint8_t data[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    uint32_t m[64];

    for (size_t i = 0, j = 0; i < 16; ++i, j += 4) {
        m[i] = (static_cast<uint32_t>(data[j]) << 24) |
               (static_cast<uint32_t>(data[j + 1]) << 16) |
               (static_cast<uint32_t>(data[j + 2]) << 8) |
               (static_cast<uint32_t>(data[j + 3]));
    }
    for (size_t i = 16; i < 64; ++i) {
        m[i] = sig1(m[i - 2]) + m[i - 7] + sig0(m[i - 15]) + m[i - 16];
    }
    for (size_t i = 0; i < 64; ++i) {
        uint32_t t1 = h + ep1(e) + ch(e, f, g) + K256[i] + m[i];
        uint32_t t2 = ep0(a) + maj(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

void sha256_init(Sha256Context* ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

void sha256_update(Sha256Context* ctx, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->buffer[ctx->count % 64] = data[i];
        ctx->count++;
        if (ctx->count % 64 == 0) {
            sha256_transform(ctx->state, ctx->buffer);
        }
    }
}

void sha256_final(Sha256Context* ctx, uint8_t digest[32]) {
    uint64_t total_bits = ctx->count * 8;
    ctx->buffer[ctx->count % 64] = 0x80;
    ctx->count++;
    if (ctx->count % 64 > 56) {
        while (ctx->count % 64 != 0) {
            ctx->buffer[ctx->count % 64] = 0x00;
            ctx->count++;
        }
        sha256_transform(ctx->state, ctx->buffer);
    }
    while (ctx->count % 64 != 56) {
        ctx->buffer[ctx->count % 64] = 0x00;
        ctx->count++;
    }
    for (int i = 7; i >= 0; --i) {
        ctx->buffer[ctx->count % 64] = static_cast<uint8_t>(total_bits >> (i * 8));
        ctx->count++;
    }
    sha256_transform(ctx->state, ctx->buffer);
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 8; ++j) {
            digest[j * 4 + i] = static_cast<uint8_t>((ctx->state[j] >> ((3 - i) * 8)) & 0x000000ff);
        }
    }
}

} // namespace

// =============================================================================
// IO-01: Versioned Project and Component Schemas
// =============================================================================

std::optional<SchemaType> schema_type_from_string(std::string_view str) noexcept {
    if (str == "ProjectSchema") return SchemaType::ProjectSchema;
    if (str == "ComponentSchema") return SchemaType::ComponentSchema;
    if (str == "ExperimentSchema") return SchemaType::ExperimentSchema;
    if (str == "ResultSchema") return SchemaType::ResultSchema;
    if (str == "MaterialSchema") return SchemaType::MaterialSchema;
    return std::nullopt;
}

base::Result<void> SchemaDefinition::validate_json(std::string_view json_str) const {
    for (const auto& req : required_fields) {
        std::string needle = "\"" + req + "\"";
        if (json_str.find(needle) == std::string_view::npos) {
            return base::Result<void>::err(make_io_error(
                "Missing required schema field: '" + req + "' for " +
                std::string(schema_type_to_string(type)), 1002));
        }
    }
    return base::Result<void>::ok();
}

SchemaRegistry& SchemaRegistry::instance() {
    static SchemaRegistry reg;
    return reg;
}

SchemaRegistry::SchemaRegistry() {
    // Default system schemas
    register_schema(SchemaDefinition{
        .type = SchemaType::ProjectSchema,
        .version = base::Version{1, 0, 0, "final"},
        .required_fields = {"project_id", "name", "format_version"},
        .optional_fields = {"description", "created_at"}
    });

    register_schema(SchemaDefinition{
        .type = SchemaType::ComponentSchema,
        .version = base::Version{1, 0, 0, "final"},
        .required_fields = {"component_id", "name", "ports"},
        .optional_fields = {"parameters", "fidelity_tier"}
    });

    register_schema(SchemaDefinition{
        .type = SchemaType::ResultSchema,
        .version = base::Version{1, 0, 0, "final"},
        .required_fields = {"run_id", "timestamp_iso", "signals"},
        .optional_fields = {"solver_name", "git_commit"}
    });
}

void SchemaRegistry::register_schema(SchemaDefinition schema) {
    schemas_.push_back(std::move(schema));
}

std::optional<SchemaDefinition> SchemaRegistry::find_schema(SchemaType type, const base::Version& ver) const {
    for (const auto& s : schemas_) {
        if (s.type == type && s.version.major == ver.major && s.version.minor == ver.minor) {
            return s;
        }
    }
    return std::nullopt;
}

bool SchemaRegistry::is_compatible(SchemaType type, const base::Version& file_ver, const base::Version& app_ver) const {
    // Major version must match, file minor version must be <= application minor version
    (void)type;
    if (file_ver.major != app_ver.major) return false;
    return file_ver.minor <= app_ver.minor;
}

// =============================================================================
// IO-02: Binary Storage Implementation
// =============================================================================

void BinaryStorageWriter::add_doubles(std::string_view chunk_name, const std::vector<double>& data) {
    add_raw_chunk(
        chunk_name,
        BinaryChunkType::Float64Array,
        static_cast<uint32_t>(data.size()),
        sizeof(double),
        data.data(),
        data.size() * sizeof(double)
    );
}

void BinaryStorageWriter::add_raw_chunk(
    std::string_view chunk_name,
    BinaryChunkType type,
    uint32_t elem_count,
    uint32_t elem_size,
    const void* data,
    size_t data_bytes)
{
    BinaryChunk chunk;
    chunk.name = std::string(chunk_name);
    chunk.header.chunk_type = static_cast<uint32_t>(type);
    chunk.header.element_count = elem_count;
    chunk.header.element_byte_size = elem_size;
    chunk.header.payload_bytes = static_cast<uint32_t>(data_bytes);
    chunk.header.checksum_crc32 = ContentIntegrityChecker::compute_crc32(data, data_bytes);

    chunk.payload.resize(data_bytes);
    if (data_bytes > 0 && data != nullptr) {
        std::memcpy(chunk.payload.data(), data, data_bytes);
    }
    chunks_.push_back(std::move(chunk));
}

std::vector<uint8_t> BinaryStorageWriter::serialize() const {
    std::vector<uint8_t> buffer;
    for (const auto& chunk : chunks_) {
        uint32_t name_len = static_cast<uint32_t>(chunk.name.size());
        const auto* name_len_ptr = reinterpret_cast<const uint8_t*>(&name_len);
        buffer.insert(buffer.end(), name_len_ptr, name_len_ptr + sizeof(uint32_t));
        buffer.insert(buffer.end(), chunk.name.begin(), chunk.name.end());

        const auto* hdr_ptr = reinterpret_cast<const uint8_t*>(&chunk.header);
        buffer.insert(buffer.end(), hdr_ptr, hdr_ptr + sizeof(BinaryChunkHeader));

        buffer.insert(buffer.end(), chunk.payload.begin(), chunk.payload.end());
    }
    return buffer;
}

base::Result<void> BinaryStorageWriter::write_to_file(const std::filesystem::path& path) const {
    auto data = serialize();
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return base::Result<void>::err(make_io_error("Failed to open binary storage file for writing: " + path.string()));
    }
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return base::Result<void>::ok();
}

base::Result<std::vector<BinaryChunk>> BinaryStorageReader::deserialize(const std::vector<uint8_t>& bytes) {
    std::vector<BinaryChunk> chunks;
    size_t offset = 0;

    while (offset < bytes.size()) {
        if (offset + sizeof(uint32_t) > bytes.size()) {
            return base::Result<std::vector<BinaryChunk>>::err(make_io_error("Truncated chunk name length"));
        }

        uint32_t name_len = 0;
        std::memcpy(&name_len, bytes.data() + offset, sizeof(uint32_t));
        offset += sizeof(uint32_t);

        if (offset + name_len + sizeof(BinaryChunkHeader) > bytes.size()) {
            return base::Result<std::vector<BinaryChunk>>::err(make_io_error("Truncated chunk header"));
        }

        std::string name(reinterpret_cast<const char*>(bytes.data() + offset), name_len);
        offset += name_len;

        BinaryChunkHeader header;
        std::memcpy(&header, bytes.data() + offset, sizeof(BinaryChunkHeader));
        offset += sizeof(BinaryChunkHeader);

        // Verify magic
        if (std::memcmp(header.magic, "SHNBIN1\0", 8) != 0) {
            return base::Result<std::vector<BinaryChunk>>::err(make_io_error("Invalid SHNBIN1 magic header in chunk '" + name + "'"));
        }

        if (offset + header.payload_bytes > bytes.size()) {
            return base::Result<std::vector<BinaryChunk>>::err(make_io_error("Truncated chunk payload in '" + name + "'"));
        }

        // Verify CRC32
        uint32_t computed_crc = ContentIntegrityChecker::compute_crc32(bytes.data() + offset, header.payload_bytes);
        if (computed_crc != header.checksum_crc32) {
            return base::Result<std::vector<BinaryChunk>>::err(make_io_error("CRC32 mismatch in chunk '" + name + "'"));
        }

        BinaryChunk chunk;
        chunk.name = std::move(name);
        chunk.header = header;
        chunk.payload.assign(bytes.begin() + offset, bytes.begin() + offset + header.payload_bytes);
        offset += header.payload_bytes;

        chunks.push_back(std::move(chunk));
    }

    return base::Result<std::vector<BinaryChunk>>::ok(std::move(chunks));
}

base::Result<std::vector<BinaryChunk>> BinaryStorageReader::read_from_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        return base::Result<std::vector<BinaryChunk>>::err(make_io_error("Cannot open binary storage file: " + path.string()));
    }
    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return base::Result<std::vector<BinaryChunk>>::err(make_io_error("Failed to read binary storage file: " + path.string()));
    }
    return deserialize(buffer);
}

std::vector<double> BinaryStorageReader::extract_doubles(const BinaryChunk& chunk) {
    if (chunk.header.element_count == 0 || chunk.payload.empty()) {
        return {};
    }
    std::vector<double> result(chunk.header.element_count);
    size_t copy_bytes = std::min(chunk.payload.size(), chunk.header.element_count * sizeof(double));
    std::memcpy(result.data(), chunk.payload.data(), copy_bytes);
    return result;
}

// =============================================================================
// IO-03: Canonical Units & Numeric Precision Policy
// =============================================================================

std::string CanonicalPrecisionPolicy::format_double(double value) {
    std::ostringstream ss;
    ss << std::setprecision(17) << value;
    return ss.str();
}

base::Result<double> CanonicalPrecisionPolicy::parse_double(std::string_view s) {
    char* end_ptr = nullptr;
    std::string str(s);
    double val = std::strtod(str.c_str(), &end_ptr);
    if (end_ptr == str.c_str()) {
        return base::Result<double>::err(make_io_error("Failed to parse double string: '" + str + "'"));
    }
    return base::Result<double>::ok(val);
}

bool CanonicalPrecisionPolicy::is_bitwise_equal(double a, double b) noexcept {
    return std::bit_cast<uint64_t>(a) == std::bit_cast<uint64_t>(b);
}

uint64_t CanonicalPrecisionPolicy::to_bits(double value) noexcept {
    return std::bit_cast<uint64_t>(value);
}

double CanonicalPrecisionPolicy::from_bits(uint64_t bits) noexcept {
    return std::bit_cast<double>(bits);
}

// =============================================================================
// IO-04: Atomic Save, Backup, Autosave, and Crash Recovery
// =============================================================================

base::Result<void> AtomicFileWriter::write_file_atomic(
    const std::filesystem::path& target_path,
    std::string_view content,
    const Options& options)
{
    std::error_code ec;
    auto parent_dir = target_path.parent_path();
    if (!parent_dir.empty()) {
        std::filesystem::create_directories(parent_dir, ec);
    }

    auto tmp_path = target_path;
    tmp_path += ".tmp";

    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out) {
            return base::Result<void>::err(make_io_error("Failed to create temporary atomic file: " + tmp_path.string()));
        }
        out.write(content.data(), static_cast<std::streamsize>(content.size()));
        out.flush();
        if (!out.good()) {
            return base::Result<void>::err(make_io_error("Write error in temporary atomic file: " + tmp_path.string()));
        }
    }

    // Optional backup copy of existing file
    if (options.create_backup_copy && std::filesystem::exists(target_path, ec)) {
        auto bak_path = target_path;
        bak_path += options.backup_extension;
        std::filesystem::copy_file(target_path, bak_path, std::filesystem::copy_options::overwrite_existing, ec);
    }

    // Atomic rename
    std::filesystem::rename(tmp_path, target_path, ec);
    if (ec) {
        // Fallback for Windows if target exists and rename fails
        std::filesystem::remove(target_path, ec);
        std::filesystem::rename(tmp_path, target_path, ec);
        if (ec) {
            return base::Result<void>::err(make_io_error("Atomic rename failed: " + ec.message()));
        }
    }

    return base::Result<void>::ok();
}

AutosaveManager::AutosaveManager(std::filesystem::path autosave_dir)
    : autosave_dir_(std::move(autosave_dir))
{
    std::error_code ec;
    std::filesystem::create_directories(autosave_dir_, ec);
}

base::Result<void> AutosaveManager::save_snapshot(std::string_view project_id, std::string_view content) {
    auto path = autosave_dir_ / (std::string(project_id) + ".autosave.json");
    return AtomicFileWriter::write_file_atomic(path, content, {.create_backup_copy = false});
}

bool AutosaveManager::has_crash_recovery(std::string_view project_id) const {
    auto path = autosave_dir_ / (std::string(project_id) + ".autosave.json");
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

base::Result<std::string> AutosaveManager::recover_latest(std::string_view project_id) const {
    auto path = autosave_dir_ / (std::string(project_id) + ".autosave.json");
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return base::Result<std::string>::err(make_io_error("No autosave recovery file found for project: " + std::string(project_id)));
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return base::Result<std::string>::ok(ss.str());
}

base::Result<void> AutosaveManager::clear_autosave(std::string_view project_id) {
    auto path = autosave_dir_ / (std::string(project_id) + ".autosave.json");
    std::error_code ec;
    std::filesystem::remove(path, ec);
    return base::Result<void>::ok();
}

// =============================================================================
// IO-05: Mesh/CAD Format Import/Export (STL & OBJ)
// =============================================================================

base::math::Vector3<double> TriangleMesh::bounding_box_min() const {
    if (vertices.empty()) return base::math::Vector3<double>{0, 0, 0};
    base::math::Vector3<double> bmin = vertices[0];
    for (const auto& v : vertices) {
        bmin.x = std::min(bmin.x, v.x);
        bmin.y = std::min(bmin.y, v.y);
        bmin.z = std::min(bmin.z, v.z);
    }
    return bmin;
}

base::math::Vector3<double> TriangleMesh::bounding_box_max() const {
    if (vertices.empty()) return base::math::Vector3<double>{0, 0, 0};
    base::math::Vector3<double> bmax = vertices[0];
    for (const auto& v : vertices) {
        bmax.x = std::max(bmax.x, v.x);
        bmax.y = std::max(bmax.y, v.y);
        bmax.z = std::max(bmax.z, v.z);
    }
    return bmax;
}

double TriangleMesh::compute_surface_area() const {
    double total_area = 0.0;
    for (const auto& f : faces) {
        if (f.v0 < vertices.size() && f.v1 < vertices.size() && f.v2 < vertices.size()) {
            const auto& p0 = vertices[f.v0];
            const auto& p1 = vertices[f.v1];
            const auto& p2 = vertices[f.v2];
            base::math::Vector3<double> e1{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
            base::math::Vector3<double> e2{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
            base::math::Vector3<double> cross = e1.cross(e2);
            total_area += 0.5 * cross.length();
        }
    }
    return total_area;
}

double TriangleMesh::compute_volume() const {
    double total_vol = 0.0;
    for (const auto& f : faces) {
        if (f.v0 < vertices.size() && f.v1 < vertices.size() && f.v2 < vertices.size()) {
            const auto& p0 = vertices[f.v0];
            const auto& p1 = vertices[f.v1];
            const auto& p2 = vertices[f.v2];
            base::math::Vector3<double> cross = p1.cross(p2);
            total_vol += p0.dot(cross);
        }
    }
    return std::abs(total_vol) / 6.0;
}

std::string StlMeshIO::export_ascii(const TriangleMesh& mesh, std::string_view solid_name) {
    std::ostringstream ss;
    ss << "solid " << solid_name << "\n";
    for (const auto& f : mesh.faces) {
        if (f.v0 < mesh.vertices.size() && f.v1 < mesh.vertices.size() && f.v2 < mesh.vertices.size()) {
            const auto& p0 = mesh.vertices[f.v0];
            const auto& p1 = mesh.vertices[f.v1];
            const auto& p2 = mesh.vertices[f.v2];
            base::math::Vector3<double> n{0, 0, 1};
            if (!mesh.normals.empty() && f.v0 < mesh.normals.size()) {
                n = mesh.normals[f.v0];
            }
            ss << "  facet normal " << n.x << " " << n.y << " " << n.z << "\n";
            ss << "    outer loop\n";
            ss << "      vertex " << p0.x << " " << p0.y << " " << p0.z << "\n";
            ss << "      vertex " << p1.x << " " << p1.y << " " << p1.z << "\n";
            ss << "      vertex " << p2.x << " " << p2.y << " " << p2.z << "\n";
            ss << "    endloop\n";
            ss << "  endfacet\n";
        }
    }
    ss << "endsolid " << solid_name << "\n";
    return ss.str();
}

base::Result<TriangleMesh> StlMeshIO::import_ascii(std::string_view stl_content) {
    TriangleMesh mesh;
    std::istringstream stream{std::string(stl_content)};
    std::string line;
    std::vector<base::math::Vector3<double>> tri_verts;

    while (std::getline(stream, line)) {
        std::istringstream ls(line);
        std::string token;
        ls >> token;
        if (token == "vertex") {
            double x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            tri_verts.push_back({x, y, z});
            if (tri_verts.size() == 3) {
                uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertices.push_back(tri_verts[0]);
                mesh.vertices.push_back(tri_verts[1]);
                mesh.vertices.push_back(tri_verts[2]);
                mesh.faces.push_back({idx, idx + 1, idx + 2});
                tri_verts.clear();
            }
        }
    }

    if (mesh.faces.empty()) {
        return base::Result<TriangleMesh>::err(make_io_error("No valid triangular facets found in ASCII STL"));
    }
    return base::Result<TriangleMesh>::ok(std::move(mesh));
}

std::vector<uint8_t> StlMeshIO::export_binary(const TriangleMesh& mesh) {
    std::vector<uint8_t> bytes;
    bytes.resize(80 + sizeof(uint32_t) + mesh.faces.size() * 50);

    // 80-byte header
    std::string header = "Shunya Engineering Platform Binary STL Export";
    std::memcpy(bytes.data(), header.data(), std::min(header.size(), size_t(80)));

    uint32_t face_count = static_cast<uint32_t>(mesh.faces.size());
    std::memcpy(bytes.data() + 80, &face_count, sizeof(uint32_t));

    size_t offset = 84;
    for (const auto& f : mesh.faces) {
        float normal[3] = {0.0f, 0.0f, 1.0f};
        std::memcpy(bytes.data() + offset, normal, 12);
        offset += 12;

        const auto& p0 = mesh.vertices[f.v0];
        const auto& p1 = mesh.vertices[f.v1];
        const auto& p2 = mesh.vertices[f.v2];

        float v0[3] = {static_cast<float>(p0.x), static_cast<float>(p0.y), static_cast<float>(p0.z)};
        float v1[3] = {static_cast<float>(p1.x), static_cast<float>(p1.y), static_cast<float>(p1.z)};
        float v2[3] = {static_cast<float>(p2.x), static_cast<float>(p2.y), static_cast<float>(p2.z)};

        std::memcpy(bytes.data() + offset, v0, 12); offset += 12;
        std::memcpy(bytes.data() + offset, v1, 12); offset += 12;
        std::memcpy(bytes.data() + offset, v2, 12); offset += 12;

        uint16_t attr_byte_count = 0;
        std::memcpy(bytes.data() + offset, &attr_byte_count, sizeof(uint16_t));
        offset += sizeof(uint16_t);
    }

    return bytes;
}

base::Result<TriangleMesh> StlMeshIO::import_binary(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 84) {
        return base::Result<TriangleMesh>::err(make_io_error("Binary STL too short (< 84 bytes)"));
    }

    uint32_t num_faces = 0;
    std::memcpy(&num_faces, bytes.data() + 80, sizeof(uint32_t));

    if (bytes.size() < 84 + num_faces * 50) {
        return base::Result<TriangleMesh>::err(make_io_error("Binary STL payload truncated"));
    }

    TriangleMesh mesh;
    mesh.faces.reserve(num_faces);
    mesh.vertices.reserve(num_faces * 3);

    size_t offset = 84;
    for (uint32_t i = 0; i < num_faces; ++i) {
        offset += 12; // skip normal
        float v0[3], v1[3], v2[3];
        std::memcpy(v0, bytes.data() + offset, 12); offset += 12;
        std::memcpy(v1, bytes.data() + offset, 12); offset += 12;
        std::memcpy(v2, bytes.data() + offset, 12); offset += 12;
        offset += 2; // skip attr bytes

        uint32_t idx = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({v0[0], v0[1], v0[2]});
        mesh.vertices.push_back({v1[0], v1[1], v1[2]});
        mesh.vertices.push_back({v2[0], v2[1], v2[2]});
        mesh.faces.push_back({idx, idx + 1, idx + 2});
    }

    return base::Result<TriangleMesh>::ok(std::move(mesh));
}

std::string ObjMeshIO::export_obj(const TriangleMesh& mesh) {
    std::ostringstream ss;
    ss << "# Shunya Wavefront OBJ Export\n";
    for (const auto& v : mesh.vertices) {
        ss << "v " << v.x << " " << v.y << " " << v.z << "\n";
    }
    for (const auto& f : mesh.faces) {
        // OBJ is 1-indexed
        ss << "f " << (f.v0 + 1) << " " << (f.v1 + 1) << " " << (f.v2 + 1) << "\n";
    }
    return ss.str();
}

base::Result<TriangleMesh> ObjMeshIO::import_obj(std::string_view obj_content) {
    TriangleMesh mesh;
    std::istringstream stream{std::string(obj_content)};
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ls(line);
        std::string token;
        ls >> token;
        if (token == "v") {
            double x = 0, y = 0, z = 0;
            ls >> x >> y >> z;
            mesh.vertices.push_back({x, y, z});
        } else if (token == "f") {
            std::string s0, s1, s2;
            ls >> s0 >> s1 >> s2;
            auto parse_idx = [](const std::string& s) -> uint32_t {
                size_t slash = s.find('/');
                std::string idx_str = (slash == std::string::npos) ? s : s.substr(0, slash);
                return static_cast<uint32_t>(std::stoul(idx_str)) - 1; // 1-based to 0-based
            };
            mesh.faces.push_back({parse_idx(s0), parse_idx(s1), parse_idx(s2)});
        }
    }

    return base::Result<TriangleMesh>::ok(std::move(mesh));
}

// =============================================================================
// IO-06: Tabular Data Import
// =============================================================================

size_t DataTable::row_count() const noexcept {
    if (columns.empty()) return 0;
    return columns[0].values.size();
}

const DataColumn* DataTable::find_column(std::string_view header) const noexcept {
    for (const auto& col : columns) {
        if (col.header == header) return &col;
    }
    return nullptr;
}

base::Result<DataTable> TabularDataImporter::import_table(std::string_view text, const Options& options) {
    DataTable table;
    std::istringstream stream{std::string(text)};
    std::string line;

    char delim = options.delimiter;
    bool header_read = false;

    while (std::getline(stream, line)) {
        // Strip trailing \r if present
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Comments
        if (options.ignore_comment_lines) {
            std::string_view sv = line;
            size_t non_ws = sv.find_first_not_of(" \t");
            if (non_ws == std::string_view::npos) continue;
            if (sv[non_ws] == '#' || (non_ws + 1 < sv.size() && sv[non_ws] == '/' && sv[non_ws + 1] == '/')) {
                continue;
            }
        }

        if (!header_read) {
            // Auto detect delimiter
            if (options.auto_detect_delimiter) {
                if (line.find(',') != std::string::npos) delim = ',';
                else if (line.find('\t') != std::string::npos) delim = '\t';
                else if (line.find(';') != std::string::npos) delim = ';';
            }

            std::istringstream hs(line);
            std::string token;
            while (std::getline(hs, token, delim)) {
                // Trim token
                size_t start = token.find_first_not_of(" \t");
                size_t end = token.find_last_not_of(" \t");
                if (start != std::string::npos && end != std::string::npos) {
                    token = token.substr(start, end - start + 1);
                }

                DataColumn col;
                if (options.parse_header_units) {
                    // Check for "Name (unit)" or "Name [unit]"
                    std::regex re(R"re((.+?)\s*[\(\[]([^\)\]]+)[\)\]])re");
                    std::smatch m;
                    if (std::regex_match(token, m, re)) {
                        col.header = m[1].str();
                        col.unit = m[2].str();
                        size_t h_end = col.header.find_last_not_of(" \t");
                        if (h_end != std::string::npos) col.header = col.header.substr(0, h_end + 1);
                    } else {
                        col.header = token;
                    }
                } else {
                    col.header = token;
                }
                table.columns.push_back(std::move(col));
            }
            header_read = true;
            continue;
        }

        // Row values
        std::istringstream row_stream(line);
        std::string token;
        size_t col_idx = 0;

        while (std::getline(row_stream, token, delim) && col_idx < table.columns.size()) {
            char* end_ptr = nullptr;
            double val = std::strtod(token.c_str(), &end_ptr);
            table.columns[col_idx].values.push_back(val);
            col_idx++;
        }
    }

    if (table.columns.empty()) {
        return base::Result<DataTable>::err(make_io_error("Tabular data contains no columns"));
    }

    return base::Result<DataTable>::ok(std::move(table));
}

// =============================================================================
// IO-07: CSV/JSON Result Export and Metadata Sidecars
// =============================================================================

std::string ResultExportManager::export_csv(const DataTable& table) {
    std::ostringstream ss;
    // Header
    for (size_t c = 0; c < table.columns.size(); ++c) {
        ss << table.columns[c].header;
        if (!table.columns[c].unit.empty()) {
            ss << " (" << table.columns[c].unit << ")";
        }
        if (c + 1 < table.columns.size()) ss << ",";
    }
    ss << "\n";

    // Data
    const size_t rows = table.row_count();
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < table.columns.size(); ++c) {
            ss << std::setprecision(12) << table.columns[c].values[r];
            if (c + 1 < table.columns.size()) ss << ",";
        }
        ss << "\n";
    }
    return ss.str();
}

std::string ResultExportManager::export_json(const DataTable& table) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"columns\": [\n";
    for (size_t c = 0; c < table.columns.size(); ++c) {
        ss << "    {\"name\": \"" << table.columns[c].header << "\", \"unit\": \""
           << table.columns[c].unit << "\"}"
           << (c + 1 < table.columns.size() ? "," : "") << "\n";
    }
    ss << "  ],\n";
    ss << "  \"rows\": [\n";
    const size_t rows = table.row_count();
    for (size_t r = 0; r < rows; ++r) {
        ss << "    [";
        for (size_t c = 0; c < table.columns.size(); ++c) {
            ss << std::setprecision(12) << table.columns[c].values[r];
            if (c + 1 < table.columns.size()) ss << ", ";
        }
        ss << "]" << (r + 1 < rows ? "," : "") << "\n";
    }
    ss << "  ]\n";
    ss << "}";
    return ss.str();
}

std::string MetadataSidecar::to_json() const {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"run_id\": \"" << run_id.to_string() << "\",\n";
    ss << "  \"project_hash\": \"" << project_hash << "\",\n";
    ss << "  \"git_commit\": \"" << git_commit << "\",\n";
    ss << "  \"timestamp_iso\": \"" << timestamp_iso << "\",\n";
    ss << "  \"solver_name\": \"" << solver_name << "\",\n";
    ss << "  \"payload_checksum\": " << payload_checksum << "\n";
    ss << "}";
    return ss.str();
}

base::Result<MetadataSidecar> MetadataSidecar::from_json(std::string_view json_str) {
    MetadataSidecar sidecar;
    auto get_str_val = [](std::string_view json, std::string_view key) -> std::string {
        std::string pattern = "\"" + std::string(key) + "\": \"";
        auto pos = json.find(pattern);
        if (pos == std::string_view::npos) return "";
        pos += pattern.size();
        auto end = json.find('"', pos);
        if (end == std::string_view::npos) return "";
        return std::string(json.substr(pos, end - pos));
    };

    sidecar.project_hash = get_str_val(json_str, "project_hash");
    sidecar.git_commit = get_str_val(json_str, "git_commit");
    sidecar.timestamp_iso = get_str_val(json_str, "timestamp_iso");
    sidecar.solver_name = get_str_val(json_str, "solver_name");

    std::string id_str = get_str_val(json_str, "run_id");
    auto uuid_res = base::Uuid::from_string(id_str);
    if (uuid_res.has_value()) {
        sidecar.run_id = uuid_res.value();
    }

    auto ck_pos = json_str.find("\"payload_checksum\": ");
    if (ck_pos != std::string_view::npos) {
        ck_pos += 20;
        char* end_ptr = nullptr;
        sidecar.payload_checksum = static_cast<uint32_t>(std::strtoul(json_str.data() + ck_pos, &end_ptr, 10));
    }

    return base::Result<MetadataSidecar>::ok(std::move(sidecar));
}

base::Result<void> ResultExportManager::export_with_sidecar(
    const std::filesystem::path& base_path,
    const DataTable& table,
    MetadataSidecar sidecar,
    bool use_json)
{
    std::string payload = use_json ? export_json(table) : export_csv(table);
    sidecar.payload_checksum = ContentIntegrityChecker::compute_crc32_string(payload);

    auto target_file = base_path;
    target_file += (use_json ? ".json" : ".csv");
    auto sidecar_file = base_path;
    sidecar_file += ".sidecar.json";

    auto res1 = AtomicFileWriter::write_file_atomic(target_file, payload);
    if (!res1.is_ok()) return res1;

    auto res2 = AtomicFileWriter::write_file_atomic(sidecar_file, sidecar.to_json());
    if (!res2.is_ok()) return res2;

    return base::Result<void>::ok();
}

// =============================================================================
// IO-08: Content Integrity Checks and Hashes
// =============================================================================

uint32_t ContentIntegrityChecker::compute_crc32(const void* data, size_t length) noexcept {
    const auto* bytes = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (int k = 0; k < 8; ++k) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

uint32_t ContentIntegrityChecker::compute_crc32_string(std::string_view data) noexcept {
    return compute_crc32(data.data(), data.size());
}

std::string ContentIntegrityChecker::compute_sha256_hex(std::string_view data) {
    Sha256Context ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, reinterpret_cast<const uint8_t*>(data.data()), data.size());
    uint8_t digest[32];
    sha256_final(&ctx, digest);

    std::ostringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(digest[i]);
    }
    return ss.str();
}

base::Result<std::string> ContentIntegrityChecker::compute_file_sha256(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return base::Result<std::string>::err(make_io_error("Failed to open file for SHA-256 calculation: " + path.string()));
    }
    Sha256Context ctx;
    sha256_init(&ctx);
    char buffer[4096];
    while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0) {
        sha256_update(&ctx, reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(in.gcount()));
    }
    uint8_t digest[32];
    sha256_final(&ctx, digest);

    std::ostringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(digest[i]);
    }
    return base::Result<std::string>::ok(ss.str());
}

bool ContentIntegrityChecker::verify_file_checksum(const std::filesystem::path& path, std::string_view expected_sha256) {
    auto res = compute_file_sha256(path);
    if (!res.is_ok()) return false;
    return res.value() == expected_sha256;
}

// =============================================================================
// IO-09: Schema Migration and Backwards Compatibility
// =============================================================================

void SchemaMigrationEngine::register_migration(const base::Version& from_ver, const base::Version& to_ver, MigrationFunc func) {
    steps_.push_back({from_ver, to_ver, std::move(func)});
}

bool SchemaMigrationEngine::can_migrate(const base::Version& from_ver, const base::Version& to_ver) const {
    if (from_ver == to_ver) return true;
    for (const auto& s : steps_) {
        if (s.from == from_ver) {
            if (s.to == to_ver || can_migrate(s.to, to_ver)) {
                return true;
            }
        }
    }
    return false;
}

base::Result<std::string> SchemaMigrationEngine::migrate(
    std::string_view json_content,
    const base::Version& from_ver,
    const base::Version& to_ver) const
{
    if (from_ver == to_ver) {
        return base::Result<std::string>::ok(std::string(json_content));
    }

    base::Version current_ver = from_ver;
    std::string current_content(json_content);

    while (current_ver != to_ver) {
        bool found_step = false;
        for (const auto& s : steps_) {
            if (s.from == current_ver) {
                auto res = s.func(current_content);
                if (!res.is_ok()) return res;
                current_content = res.value();
                current_ver = s.to;
                found_step = true;
                break;
            }
        }
        if (!found_step) {
            return base::Result<std::string>::err(make_io_error(
                "No migration path from version " + current_ver.as_string() + " to " + to_ver.as_string()));
        }
    }

    return base::Result<std::string>::ok(std::move(current_content));
}

// =============================================================================
// IO-10: Format Documentation, Limitations, and Round-Trip Guarantees
// =============================================================================

FormatRegistry& FormatRegistry::instance() {
    static FormatRegistry reg;
    return reg;
}

FormatRegistry::FormatRegistry() {
    formats_ = {
        FormatCapability{
            .format_id = "SHUNYA_PROJECT",
            .name = "Shunya Native Project Manifest",
            .extension = ".shunya",
            .mime_type = "application/x-shunya-project",
            .supports_lossless_roundtrip = true,
            .max_recommended_size_bytes = 100 * 1024 * 1024,
            .precision_guarantee = "17-digit decimal (IEEE-754 binary64 conformant)",
            .description = "Primary atomic JSON manifest storing project metadata, element hierarchy, and configurations."
        },
        FormatCapability{
            .format_id = "SHUNYA_MODEL",
            .name = "Shunya Component Definition Package",
            .extension = ".shunyamodel",
            .mime_type = "application/x-shunya-model",
            .supports_lossless_roundtrip = true,
            .max_recommended_size_bytes = 50 * 1024 * 1024,
            .precision_guarantee = "17-digit decimal (IEEE-754 binary64 conformant)",
            .description = "Reusable component package storing ports, equations, parameters, and internal sub-circuits."
        },
        FormatCapability{
            .format_id = "SHUNYA_BINARY",
            .name = "Shunya Binary Chunks Storage",
            .extension = ".shunyabin",
            .mime_type = "application/x-shunya-binary",
            .supports_lossless_roundtrip = true,
            .max_recommended_size_bytes = 10ULL * 1024 * 1024 * 1024,
            .precision_guarantee = "IEEE-754 raw 64-bit bit-exact representation",
            .description = "High-throughput binary chunk storage for large time-series and spatial discretization meshes."
        },
        FormatCapability{
            .format_id = "STL_MESH",
            .name = "Standard Stereolithography Mesh (ASCII / Binary)",
            .extension = ".stl",
            .mime_type = "model/stl",
            .supports_lossless_roundtrip = false,
            .max_recommended_size_bytes = 500 * 1024 * 1024,
            .precision_guarantee = "32-bit single precision floats",
            .description = "Triangular boundary representation mesh without topology or connectivity metadata."
        },
        FormatCapability{
            .format_id = "OBJ_MESH",
            .name = "Wavefront OBJ 3D Model",
            .extension = ".obj",
            .mime_type = "model/obj",
            .supports_lossless_roundtrip = false,
            .max_recommended_size_bytes = 500 * 1024 * 1024,
            .precision_guarantee = "Variable ASCII float precision",
            .description = "Standard polygonal mesh format supporting shared vertex pools and face normals."
        },
        FormatCapability{
            .format_id = "CSV_TABLE",
            .name = "Comma-Separated Tabular Values",
            .extension = ".csv",
            .mime_type = "text/csv",
            .supports_lossless_roundtrip = false,
            .max_recommended_size_bytes = 2ULL * 1024 * 1024 * 1024,
            .precision_guarantee = "12-digit decimal text formatting",
            .description = "Interoperable tabular export format for simulation probes, sweeps, and experimental data."
        }
    };
}

const FormatCapability* FormatRegistry::find_by_extension(std::string_view ext) const noexcept {
    for (const auto& f : formats_) {
        if (f.extension == ext) return &f;
    }
    return nullptr;
}

const FormatCapability* FormatRegistry::find_by_id(std::string_view format_id) const noexcept {
    for (const auto& f : formats_) {
        if (f.format_id == format_id) return &f;
    }
    return nullptr;
}

} // namespace shunya::core::domain
