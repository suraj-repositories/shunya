#pragma once

#include "shunya/core/base/uuid.hpp"
#include "shunya/core/base/result.hpp"
#include "shunya/core/base/error.hpp"
#include "shunya/core/base/version.hpp"
#include "shunya/core/base/math.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace shunya::core::domain {

// =============================================================================
// IO-01: Versioned Project and Component Schemas
// =============================================================================

enum class SchemaType {
    ProjectSchema,
    ComponentSchema,
    ExperimentSchema,
    ResultSchema,
    MaterialSchema
};

[[nodiscard]] constexpr std::string_view schema_type_to_string(SchemaType type) noexcept {
    switch (type) {
        case SchemaType::ProjectSchema: return "ProjectSchema";
        case SchemaType::ComponentSchema: return "ComponentSchema";
        case SchemaType::ExperimentSchema: return "ExperimentSchema";
        case SchemaType::ResultSchema: return "ResultSchema";
        case SchemaType::MaterialSchema: return "MaterialSchema";
    }
    return "ProjectSchema";
}

[[nodiscard]] std::optional<SchemaType> schema_type_from_string(std::string_view str) noexcept;

struct SchemaDefinition {
    SchemaType type{SchemaType::ProjectSchema};
    base::Version version{1, 0, 0, "final"};
    std::vector<std::string> required_fields;
    std::vector<std::string> optional_fields;

    [[nodiscard]] base::Result<void> validate_json(std::string_view json_str) const;
};

class SchemaRegistry {
public:
    static SchemaRegistry& instance();

    void register_schema(SchemaDefinition schema);
    [[nodiscard]] std::optional<SchemaDefinition> find_schema(SchemaType type, const base::Version& ver) const;
    [[nodiscard]] bool is_compatible(SchemaType type, const base::Version& file_ver, const base::Version& app_ver) const;

private:
    SchemaRegistry();
    std::vector<SchemaDefinition> schemas_;
};

// =============================================================================
// IO-02: Human-Readable Metadata & Efficient Binary Storage
// =============================================================================

enum class BinaryChunkType : uint32_t {
    Float64Array = 1,
    Float32Array = 2,
    Int32Array = 3,
    DenseMatrix = 4,
    MeshGeometry = 5,
    Custom = 99
};

#pragma pack(push, 1)
struct BinaryChunkHeader {
    char magic[8]{'S', 'H', 'N', 'B', 'I', 'N', '1', '\0'};
    uint32_t chunk_type{0};
    uint32_t element_count{0};
    uint32_t element_byte_size{0};
    uint32_t payload_bytes{0};
    uint32_t checksum_crc32{0};
};
#pragma pack(pop)

struct BinaryChunk {
    BinaryChunkHeader header;
    std::string name;
    std::vector<uint8_t> payload;
};

class BinaryStorageWriter {
public:
    void add_doubles(std::string_view chunk_name, const std::vector<double>& data);
    void add_raw_chunk(std::string_view chunk_name, BinaryChunkType type, uint32_t elem_count,
                       uint32_t elem_size, const void* data, size_t data_bytes);
    [[nodiscard]] std::vector<uint8_t> serialize() const;
    base::Result<void> write_to_file(const std::filesystem::path& path) const;

private:
    std::vector<BinaryChunk> chunks_;
};

class BinaryStorageReader {
public:
    [[nodiscard]] static base::Result<std::vector<BinaryChunk>> deserialize(const std::vector<uint8_t>& bytes);
    [[nodiscard]] static base::Result<std::vector<BinaryChunk>> read_from_file(const std::filesystem::path& path);
    [[nodiscard]] static std::vector<double> extract_doubles(const BinaryChunk& chunk);
};

// =============================================================================
// IO-03: Canonical Units & Numeric Precision Policy
// =============================================================================

class CanonicalPrecisionPolicy {
public:
    /// Format double using standard 17-digit decimal precision guaranteeing IEEE-754 round-trip
    [[nodiscard]] static std::string format_double(double value);

    /// Parse double from string
    [[nodiscard]] static base::Result<double> parse_double(std::string_view s);

    /// Check if two double-precision values are bitwise identical
    [[nodiscard]] static bool is_bitwise_equal(double a, double b) noexcept;

    /// Converts raw bit pattern to uint64_t for bit-exact serialization
    [[nodiscard]] static uint64_t to_bits(double value) noexcept;
    [[nodiscard]] static double from_bits(uint64_t bits) noexcept;
};

// =============================================================================
// IO-04: Atomic Save, Backup, Autosave, and Crash Recovery
// =============================================================================

struct AtomicFileWriterOptions {
    bool create_backup_copy{true};
    std::string backup_extension{".bak"};
};

class AtomicFileWriter {
public:
    using Options = AtomicFileWriterOptions;

    [[nodiscard]] static base::Result<void> write_file_atomic(
        const std::filesystem::path& target_path,
        std::string_view content,
        const Options& options);

    [[nodiscard]] static base::Result<void> write_file_atomic(
        const std::filesystem::path& target_path,
        std::string_view content) {
        return write_file_atomic(target_path, content, Options{});
    }
};

class AutosaveManager {
public:
    explicit AutosaveManager(std::filesystem::path autosave_dir);

    base::Result<void> save_snapshot(std::string_view project_id, std::string_view content);
    [[nodiscard]] bool has_crash_recovery(std::string_view project_id) const;
    [[nodiscard]] base::Result<std::string> recover_latest(std::string_view project_id) const;
    base::Result<void> clear_autosave(std::string_view project_id);

private:
    std::filesystem::path autosave_dir_;
};

// =============================================================================
// IO-05: Mesh/CAD Format Import/Export (STL & OBJ)
// =============================================================================

struct MeshFace {
    uint32_t v0{0};
    uint32_t v1{0};
    uint32_t v2{0};
};

struct TriangleMesh {
    std::vector<base::math::Vector3<double>> vertices;
    std::vector<base::math::Vector3<double>> normals;
    std::vector<MeshFace> faces;

    [[nodiscard]] base::math::Vector3<double> bounding_box_min() const;
    [[nodiscard]] base::math::Vector3<double> bounding_box_max() const;
    [[nodiscard]] double compute_surface_area() const;
    [[nodiscard]] double compute_volume() const;
};

class StlMeshIO {
public:
    [[nodiscard]] static std::string export_ascii(const TriangleMesh& mesh, std::string_view solid_name = "shunya");
    [[nodiscard]] static base::Result<TriangleMesh> import_ascii(std::string_view stl_content);
    [[nodiscard]] static std::vector<uint8_t> export_binary(const TriangleMesh& mesh);
    [[nodiscard]] static base::Result<TriangleMesh> import_binary(const std::vector<uint8_t>& bytes);
};

class ObjMeshIO {
public:
    [[nodiscard]] static std::string export_obj(const TriangleMesh& mesh);
    [[nodiscard]] static base::Result<TriangleMesh> import_obj(std::string_view obj_content);
};

// =============================================================================
// IO-06: Tabular Data Import (Material Curves, Waveforms, Experiments)
// =============================================================================

struct DataColumn {
    std::string header;
    std::string unit;
    std::vector<double> values;
};

struct DataTable {
    std::vector<DataColumn> columns;

    [[nodiscard]] size_t row_count() const noexcept;
    [[nodiscard]] size_t column_count() const noexcept { return columns.size(); }
    [[nodiscard]] const DataColumn* find_column(std::string_view header) const noexcept;
};

struct TabularImportOptions {
    char delimiter{','};
    bool auto_detect_delimiter{true};
    bool parse_header_units{true}; // e.g., "Time (s)" -> header "Time", unit "s"
    bool ignore_comment_lines{true}; // lines starting with # or //
};

class TabularDataImporter {
public:
    using Options = TabularImportOptions;

    [[nodiscard]] static base::Result<DataTable> import_table(
        std::string_view text,
        const Options& options);

    [[nodiscard]] static base::Result<DataTable> import_table(std::string_view text) {
        return import_table(text, Options{});
    }
};

// =============================================================================
// IO-07: CSV/JSON Result Export and Metadata Sidecars
// =============================================================================

struct MetadataSidecar {
    base::Uuid run_id;
    std::string project_hash;
    std::string git_commit;
    std::string timestamp_iso;
    std::string solver_name;
    std::vector<std::string> column_names;
    std::vector<std::string> column_units;
    uint32_t payload_checksum{0};

    [[nodiscard]] std::string to_json() const;
    [[nodiscard]] static base::Result<MetadataSidecar> from_json(std::string_view json_str);
};

class ResultExportManager {
public:
    [[nodiscard]] static std::string export_csv(const DataTable& table);
    [[nodiscard]] static std::string export_json(const DataTable& table);

    [[nodiscard]] static base::Result<void> export_with_sidecar(
        const std::filesystem::path& base_path,
        const DataTable& table,
        MetadataSidecar sidecar,
        bool use_json = false);
};

// =============================================================================
// IO-08: Content Integrity Checks and Hashes
// =============================================================================

class ContentIntegrityChecker {
public:
    [[nodiscard]] static uint32_t compute_crc32(const void* data, size_t length) noexcept;
    [[nodiscard]] static uint32_t compute_crc32_string(std::string_view data) noexcept;
    [[nodiscard]] static std::string compute_sha256_hex(std::string_view data);
    [[nodiscard]] static base::Result<std::string> compute_file_sha256(const std::filesystem::path& path);
    [[nodiscard]] static bool verify_file_checksum(const std::filesystem::path& path, std::string_view expected_sha256);
};

// =============================================================================
// IO-09: Schema Migration and Backwards Compatibility
// =============================================================================

class SchemaMigrationEngine {
public:
    using MigrationFunc = std::function<base::Result<std::string>(std::string_view)>;

    void register_migration(const base::Version& from_ver, const base::Version& to_ver, MigrationFunc func);

    [[nodiscard]] bool can_migrate(const base::Version& from_ver, const base::Version& to_ver) const;

    [[nodiscard]] base::Result<std::string> migrate(
        std::string_view json_content,
        const base::Version& from_ver,
        const base::Version& to_ver) const;

private:
    struct Step {
        base::Version from;
        base::Version to;
        MigrationFunc func;
    };
    std::vector<Step> steps_;
};

// =============================================================================
// IO-10: Format Documentation, Limitations, and Round-Trip Guarantees
// =============================================================================

struct FormatCapability {
    std::string format_id;
    std::string name;
    std::string extension;
    std::string mime_type;
    bool supports_lossless_roundtrip{false};
    uint64_t max_recommended_size_bytes{0};
    std::string precision_guarantee;
    std::string description;
};

class FormatRegistry {
public:
    static FormatRegistry& instance();

    [[nodiscard]] const std::vector<FormatCapability>& supported_formats() const noexcept { return formats_; }
    [[nodiscard]] const FormatCapability* find_by_extension(std::string_view ext) const noexcept;
    [[nodiscard]] const FormatCapability* find_by_id(std::string_view format_id) const noexcept;

private:
    FormatRegistry();
    std::vector<FormatCapability> formats_;
};

} // namespace shunya::core::domain
