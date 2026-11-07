#include "tests/test_framework.hpp"
#include "shunya/core/domain/data_exchange.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace shunya::core;
using namespace shunya::core::domain;

// =============================================================================
// IO-01: Versioned Project and Component Schemas
// =============================================================================

TEST_CASE("IO-01: Versioned Project and Component Schemas") {
    CHECK(schema_type_to_string(SchemaType::ProjectSchema) == "ProjectSchema");
    CHECK(schema_type_to_string(SchemaType::ComponentSchema) == "ComponentSchema");
    CHECK(schema_type_to_string(SchemaType::ExperimentSchema) == "ExperimentSchema");
    CHECK(schema_type_to_string(SchemaType::ResultSchema) == "ResultSchema");
    CHECK(schema_type_to_string(SchemaType::MaterialSchema) == "MaterialSchema");

    CHECK(schema_type_from_string("ProjectSchema").value() == SchemaType::ProjectSchema);
    CHECK(schema_type_from_string("ResultSchema").value() == SchemaType::ResultSchema);
    CHECK_FALSE(schema_type_from_string("UnknownSchema").has_value());

    // Schema validation
    SchemaDefinition schema{
        .type = SchemaType::ProjectSchema,
        .version = base::Version{1, 0, 0, "final"},
        .required_fields = {"project_id", "name", "format_version"},
        .optional_fields = {"description"}
    };

    std::string valid_json = R"({"project_id": "abc-123", "name": "TestProj", "format_version": "1.0.0"})";
    CHECK(schema.validate_json(valid_json).is_ok());

    std::string missing_name = R"({"project_id": "abc-123", "format_version": "1.0.0"})";
    auto val_err = schema.validate_json(missing_name);
    CHECK(val_err.is_err());

    // Registry compatibility
    auto& reg = SchemaRegistry::instance();
    base::Version v1_0{1, 0, 0, "final"};
    base::Version v1_2{1, 2, 0, "final"};
    base::Version v2_0{2, 0, 0, "final"};

    CHECK(reg.is_compatible(SchemaType::ProjectSchema, v1_0, v1_2)); // v1.0 file opened by v1.2 app -> compatible
    CHECK_FALSE(reg.is_compatible(SchemaType::ProjectSchema, v1_2, v1_0)); // v1.2 file in v1.0 app -> incompatible minor
    CHECK_FALSE(reg.is_compatible(SchemaType::ProjectSchema, v1_0, v2_0)); // major bump -> incompatible
}

// =============================================================================
// IO-02: Human-Readable Metadata & Efficient Binary Storage
// =============================================================================

TEST_CASE("IO-02: Efficient Binary Chunk Storage and CRC32 Validation") {
    BinaryStorageWriter writer;
    std::vector<double> time_points = {0.0, 0.001, 0.002, 0.003, 0.004};
    std::vector<double> voltages = {0.0, 1.25, 2.50, 3.75, 5.0};

    writer.add_doubles("time", time_points);
    writer.add_doubles("voltage", voltages);

    auto serialized = writer.serialize();
    CHECK(!serialized.empty());

    // Read back
    auto chunks_res = BinaryStorageReader::deserialize(serialized);
    REQUIRE(chunks_res.is_ok());

    const auto& chunks = chunks_res.value();
    REQUIRE(chunks.size() == 2u);
    CHECK(chunks[0].name == "time");
    CHECK(chunks[1].name == "voltage");

    auto recovered_time = BinaryStorageReader::extract_doubles(chunks[0]);
    REQUIRE(recovered_time.size() == 5u);
    CHECK(recovered_time[4] == 0.004);

    auto recovered_v = BinaryStorageReader::extract_doubles(chunks[1]);
    REQUIRE(recovered_v.size() == 5u);
    CHECK(recovered_v[4] == 5.0);

    // Corrupted payload detection
    auto corrupted = serialized;
    corrupted[corrupted.size() - 2] ^= 0xFF; // Flip bit in payload
    auto bad_res = BinaryStorageReader::deserialize(corrupted);
    CHECK(bad_res.is_err()); // Checksum mismatch
}

// =============================================================================
// IO-03: Canonical Units & Numeric Precision Policy
// =============================================================================

TEST_CASE("IO-03: Numeric Precision and Bit-Exact Serialization") {
    // 17-digit decimal round-trip
    const double val = 3.14159265358979323846;
    std::string formatted = CanonicalPrecisionPolicy::format_double(val);
    auto parsed_res = CanonicalPrecisionPolicy::parse_double(formatted);
    REQUIRE(parsed_res.is_ok());
    CHECK(parsed_res.value() == val);

    // Bit-exact serialization
    uint64_t bits = CanonicalPrecisionPolicy::to_bits(val);
    double from_bits = CanonicalPrecisionPolicy::from_bits(bits);
    CHECK(CanonicalPrecisionPolicy::is_bitwise_equal(val, from_bits));

    // Subnormal / edge values
    const double subnormal = 4.9406564584124654e-324;
    CHECK(CanonicalPrecisionPolicy::is_bitwise_equal(
        subnormal, CanonicalPrecisionPolicy::from_bits(CanonicalPrecisionPolicy::to_bits(subnormal))));
}

// =============================================================================
// IO-04: Atomic Save, Backup, Autosave, and Crash Recovery
// =============================================================================

TEST_CASE("IO-04: Atomic File Writer and Crash Recovery Snapshot") {
    auto test_dir = std::filesystem::current_path() / "test_io_temp";
    std::filesystem::create_directories(test_dir);

    auto file_path = test_dir / "project.json";
    std::string v1_content = "{\"version\": 1}";
    std::string v2_content = "{\"version\": 2}";

    // First atomic write
    auto ok1 = AtomicFileWriter::write_file_atomic(file_path, v1_content, {.create_backup_copy = true});
    CHECK(ok1.is_ok());
    CHECK(std::filesystem::exists(file_path));

    // Overwrite with backup
    auto ok2 = AtomicFileWriter::write_file_atomic(file_path, v2_content, {.create_backup_copy = true});
    CHECK(ok2.is_ok());

    auto bak_path = test_dir / "project.json.bak";
    CHECK(std::filesystem::exists(bak_path));

    // Autosave crash recovery
    AutosaveManager autosave(test_dir / "autosave");
    std::string proj_id = "test-project-123";
    std::string auto_content = "{\"autosave\": true, \"sim_time\": 42.5}";

    CHECK_FALSE(autosave.has_crash_recovery(proj_id));
    auto save_res = autosave.save_snapshot(proj_id, auto_content);
    CHECK(save_res.is_ok());
    CHECK(autosave.has_crash_recovery(proj_id));

    auto recover_res = autosave.recover_latest(proj_id);
    REQUIRE(recover_res.is_ok());
    CHECK(recover_res.value() == auto_content);

    auto clear_res = autosave.clear_autosave(proj_id);
    CHECK(clear_res.is_ok());
    CHECK_FALSE(autosave.has_crash_recovery(proj_id));

    // Clean up
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
}

// =============================================================================
// IO-05: Mesh/CAD Format Import/Export (STL & OBJ)
// =============================================================================

TEST_CASE("IO-05: Mesh Triangle Bounding, Volume and STL/OBJ Round-Trip") {
    // Construct regular tetrahedron
    TriangleMesh mesh;
    mesh.vertices = {
        base::math::Vector3<double>{0.0, 0.0, 0.0},
        base::math::Vector3<double>{1.0, 0.0, 0.0},
        base::math::Vector3<double>{0.5, 0.866025, 0.0},
        base::math::Vector3<double>{0.5, 0.288675, 0.816497}
    };
    mesh.faces = {
        MeshFace{0, 1, 2},
        MeshFace{0, 1, 3},
        MeshFace{1, 2, 3},
        MeshFace{2, 0, 3}
    };

    auto bmin = mesh.bounding_box_min();
    auto bmax = mesh.bounding_box_max();
    CHECK(bmin.x == 0.0);
    CHECK(bmax.x == 1.0);
    CHECK(mesh.compute_surface_area() > 0.0);
    CHECK(mesh.compute_volume() > 0.0);

    // ASCII STL Roundtrip
    std::string ascii_stl = StlMeshIO::export_ascii(mesh, "tetra");
    CHECK(ascii_stl.find("solid tetra") != std::string::npos);
    auto imp_stl = StlMeshIO::import_ascii(ascii_stl);
    REQUIRE(imp_stl.is_ok());
    CHECK(imp_stl.value().faces.size() == 4u);

    // Binary STL Roundtrip
    auto bin_stl = StlMeshIO::export_binary(mesh);
    CHECK(bin_stl.size() == 84 + 4 * 50);
    auto imp_bin = StlMeshIO::import_binary(bin_stl);
    REQUIRE(imp_bin.is_ok());
    CHECK(imp_bin.value().faces.size() == 4u);

    // Wavefront OBJ Roundtrip
    std::string obj_text = ObjMeshIO::export_obj(mesh);
    CHECK(obj_text.find("v 0 0 0") != std::string::npos);
    auto imp_obj = ObjMeshIO::import_obj(obj_text);
    REQUIRE(imp_obj.is_ok());
    CHECK(imp_obj.value().vertices.size() == 4u);
    CHECK(imp_obj.value().faces.size() == 4u);
}

// =============================================================================
// IO-06: Tabular Data Import (Material Curves & Waveforms)
// =============================================================================

TEST_CASE("IO-06: Tabular Data Import with Unit Detection and Comments") {
    std::string csv_data =
        "# Measurement log from experimental rig\n"
        "// Operator: Engineering Bench\n"
        "Time (s), Voltage [V], Temperature (K)\n"
        "0.0, 0.0, 293.15\n"
        "0.5, 2.5, 295.0\n"
        "1.0, 5.0, 298.2\n";

    auto table_res = TabularDataImporter::import_table(csv_data);
    REQUIRE(table_res.is_ok());

    const auto& table = table_res.value();
    REQUIRE(table.column_count() == 3u);
    CHECK(table.row_count() == 3u);

    const auto* time_col = table.find_column("Time");
    REQUIRE(time_col != nullptr);
    CHECK(time_col->unit == "s");
    CHECK(time_col->values[1] == 0.5);

    const auto* v_col = table.find_column("Voltage");
    REQUIRE(v_col != nullptr);
    CHECK(v_col->unit == "V");
    CHECK(v_col->values[2] == 5.0);

    const auto* temp_col = table.find_column("Temperature");
    REQUIRE(temp_col != nullptr);
    CHECK(temp_col->unit == "K");
    CHECK(temp_col->values[2] == 298.2);
}

// =============================================================================
// IO-07: CSV/JSON Result Export and Metadata Sidecars
// =============================================================================

TEST_CASE("IO-07: Result Export and Sidecar Provenance Generation") {
    DataTable table;
    table.columns = {
        DataColumn{.header = "Time", .unit = "s", .values = {0.0, 0.1, 0.2}},
        DataColumn{.header = "Current", .unit = "A", .values = {0.0, 1.5, 3.0}}
    };

    std::string csv = ResultExportManager::export_csv(table);
    CHECK(csv.find("Time (s),Current (A)") != std::string::npos);

    std::string json = ResultExportManager::export_json(table);
    CHECK(json.find("\"name\": \"Time\"") != std::string::npos);

    // Export with sidecar
    auto test_dir = std::filesystem::current_path() / "test_export_temp";
    std::filesystem::create_directories(test_dir);

    MetadataSidecar sidecar{
        .run_id = base::Uuid::generate_random(),
        .project_hash = "abcdef0123456789",
        .git_commit = "e238ea6",
        .timestamp_iso = "2026-11-07T12:00:00Z",
        .solver_name = "RungeKutta4",
        .column_names = {"Time", "Current"},
        .column_units = {"s", "A"},
        .payload_checksum = 0
    };

    auto ok = ResultExportManager::export_with_sidecar(test_dir / "sim_results", table, sidecar);
    CHECK(ok.is_ok());

    CHECK(std::filesystem::exists(test_dir / "sim_results.csv"));
    CHECK(std::filesystem::exists(test_dir / "sim_results.sidecar.json"));

    // Verify sidecar checksum round-trip
    std::ifstream in(test_dir / "sim_results.sidecar.json");
    std::string sidecar_text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto parsed_sidecar = MetadataSidecar::from_json(sidecar_text);
    REQUIRE(parsed_sidecar.is_ok());
    CHECK(parsed_sidecar.value().solver_name == "RungeKutta4");
    CHECK(parsed_sidecar.value().payload_checksum != 0);

    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
}

// =============================================================================
// IO-08: Content Integrity Checks and Hashes
// =============================================================================

TEST_CASE("IO-08: CRC32 and SHA-256 Digest Verification") {
    // NIST Standard SHA-256 Vector: "abc"
    // sha256("abc") = ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
    std::string sha = ContentIntegrityChecker::compute_sha256_hex("abc");
    CHECK(sha == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    // Known CRC-32 Vector: "123456789" -> 0xcbf43926
    uint32_t crc = ContentIntegrityChecker::compute_crc32_string("123456789");
    CHECK(crc == 0xcbf43926u);
}

// =============================================================================
// IO-09: Schema Migration and Backwards Compatibility
// =============================================================================

TEST_CASE("IO-09: Schema Migration Pipeline and Backwards Compatibility") {
    SchemaMigrationEngine engine;
    base::Version v1_0{1, 0, 0, "final"};
    base::Version v1_1{1, 1, 0, "final"};
    base::Version v2_0{2, 0, 0, "final"};

    // Step 1: 1.0 -> 1.1 (field rename)
    engine.register_migration(v1_0, v1_1, [](std::string_view json) -> base::Result<std::string> {
        std::string s(json);
        size_t pos = s.find("\"proj_title\":");
        if (pos != std::string::npos) {
            s.replace(pos, 13, "\"title\":");
        }
        return base::Result<std::string>::ok(std::move(s));
    });

    // Step 2: 1.1 -> 2.0 (title -> name, inject format_version)
    engine.register_migration(v1_1, v2_0, [](std::string_view json) -> base::Result<std::string> {
        std::string s(json);
        size_t pos = s.find("\"title\":");
        if (pos != std::string::npos) {
            s.replace(pos, 8, "\"name\":");
        }
        s.insert(1, "\"format_version\": \"2.0.0\", ");
        return base::Result<std::string>::ok(std::move(s));
    });

    CHECK(engine.can_migrate(v1_0, v2_0));

    std::string legacy_json = "{\"proj_title\": \"LegacyProject\"}";
    auto migrated = engine.migrate(legacy_json, v1_0, v2_0);
    REQUIRE(migrated.is_ok());
    CHECK(migrated.value().find("\"name\": \"LegacyProject\"") != std::string::npos);
    CHECK(migrated.value().find("\"format_version\": \"2.0.0\"") != std::string::npos);
}

// =============================================================================
// IO-10: Format Documentation, Limitations, and Round-Trip Guarantees
// =============================================================================

TEST_CASE("IO-10: Format Registry Capabilities and Metadata") {
    auto& reg = FormatRegistry::instance();
    CHECK(reg.supported_formats().size() >= 5u);

    const auto* proj_fmt = reg.find_by_extension(".shunya");
    REQUIRE(proj_fmt != nullptr);
    CHECK(proj_fmt->format_id == "SHUNYA_PROJECT");
    CHECK(proj_fmt->supports_lossless_roundtrip);

    const auto* bin_fmt = reg.find_by_id("SHUNYA_BINARY");
    REQUIRE(bin_fmt != nullptr);
    CHECK(bin_fmt->supports_lossless_roundtrip);
    CHECK(bin_fmt->precision_guarantee.find("bit-exact") != std::string::npos);

    const auto* stl_fmt = reg.find_by_extension(".stl");
    REQUIRE(stl_fmt != nullptr);
    CHECK_FALSE(stl_fmt->supports_lossless_roundtrip); // Mesh approximations are non-lossless
}
