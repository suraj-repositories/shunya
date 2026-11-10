#include "shunya/integ/fmu_exporter.hpp"

#include <sstream>

namespace shunya::integ {

namespace {

const char* causality_str(FmiCausality c) noexcept {
    switch (c) {
        case FmiCausality::Parameter: return "parameter";
        case FmiCausality::CalculatedParameter: return "calculatedParameter";
        case FmiCausality::Input: return "input";
        case FmiCausality::Output: return "output";
        case FmiCausality::Local: return "local";
        case FmiCausality::Independent: return "independent";
    }
    return "local";
}

const char* variability_str(FmiVariability v) noexcept {
    switch (v) {
        case FmiVariability::Constant: return "constant";
        case FmiVariability::Fixed: return "fixed";
        case FmiVariability::Tunable: return "tunable";
        case FmiVariability::Discrete: return "discrete";
        case FmiVariability::Continuous: return "continuous";
    }
    return "continuous";
}

uint32_t crc32_ieee(const uint8_t* data, size_t len) noexcept {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= static_cast<uint32_t>(data[i]);
        for (int b = 0; b < 8; ++b) {
            const uint32_t mask = (crc & 1u) ? 0xEDB88320u : 0u;
            crc = (crc >> 1) ^ mask;
        }
    }
    return ~crc;
}

void write_u16_le(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
}

void write_u32_le(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFFu));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFFu));
}

uint16_t read_u16_le(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t read_u32_le(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

} // namespace

core::base::Result<std::string> FmuExporter::generate_model_description_xml(
    const FmiModelDescription& desc) {
    if (desc.model_name.empty() || desc.guid.empty()) {
        return core::base::Error{
            471, core::base::ErrorDomain::Validation, core::base::Severity::Error,
            "Cannot export FMU modelDescription.xml without modelName and guid", ""
        };
    }

    const std::string model_id = desc.model_identifier.empty() ? desc.model_name : desc.model_identifier;
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<fmiModelDescription fmiVersion=\"" << desc.fmi_version << "\" "
       << "modelName=\"" << desc.model_name << "\" "
       << "guid=\"" << desc.guid << "\" "
       << "generationTool=\"Shunya Engineering Studio\">\n"
       << "  <CoSimulation modelIdentifier=\"" << model_id << "\" "
       << "canHandleVariableCommunicationStepSize=\"true\"/>\n"
       << "  <DefaultExperiment startTime=\"" << desc.default_start_time << "\" "
       << "stopTime=\"" << desc.default_stop_time << "\" "
       << "stepSize=\"" << desc.default_step_size << "\"/>\n"
       << "  <ModelVariables>\n";

    std::vector<size_t> output_indices;
    for (size_t i = 0; i < desc.variables.size(); ++i) {
        const auto& v = desc.variables[i];
        if (v.causality == FmiCausality::Output) {
            output_indices.push_back(i + 1); // 1-based scalar variable index per FMI spec
        }
        os << "    <ScalarVariable name=\"" << v.name << "\" "
           << "valueReference=\"" << v.value_reference << "\" "
           << "causality=\"" << causality_str(v.causality) << "\" "
           << "variability=\"" << variability_str(v.variability) << "\"";
        if (!v.description.empty()) {
            os << " description=\"" << v.description << "\"";
        }
        os << ">\n"
           << "      <Real";
        if (v.has_start) {
            os << " start=\"" << v.start_value << "\"";
        }
        if (!v.unit.empty()) {
            os << " unit=\"" << v.unit << "\"";
        }
        os << "/>\n"
           << "    </ScalarVariable>\n";
    }

    os << "  </ModelVariables>\n"
       << "  <ModelStructure>\n";
    if (!output_indices.empty()) {
        os << "    <Outputs>\n";
        for (size_t idx : output_indices) {
            os << "      <Unknown index=\"" << idx << "\"/>\n";
        }
        os << "    </Outputs>\n";
    }
    os << "  </ModelStructure>\n"
       << "</fmiModelDescription>\n";

    return os.str();
}

std::string FmuExporter::generate_c_wrapper_source(const FmiModelDescription& desc) {
    const std::string model_id = desc.model_identifier.empty() ? desc.model_name : desc.model_identifier;
    std::ostringstream os;
    os << "/* Auto-generated FMI 2.0 Co-Simulation C Wrapper for " << model_id << " */\n"
       << "#define MODEL_IDENTIFIER " << model_id << "\n"
       << "#define MODEL_GUID \"" << desc.guid << "\"\n"
       << "#include <stddef.h>\n"
       << "#include <stdlib.h>\n\n"
       << "typedef struct {\n"
       << "  double real_vars[" << (desc.variables.empty() ? 1 : desc.variables.size()) << "];\n"
       << "} ShunyaFmuState;\n\n"
       << "void* fmi2Instantiate(const char* name, const char* guid, const char* res) {\n"
       << "  (void)name; (void)guid; (void)res;\n"
       << "  ShunyaFmuState* s = (ShunyaFmuState*)calloc(1, sizeof(ShunyaFmuState));\n"
       << "  return s;\n"
       << "}\n\n"
       << "int fmi2DoStep(void* c, double t, double dt, int no_set) {\n"
       << "  (void)c; (void)t; (void)dt; (void)no_set;\n"
       << "  return 0;\n"
       << "}\n\n"
       << "void fmi2FreeInstance(void* c) { free(c); }\n";
    return os.str();
}

core::base::Result<std::vector<uint8_t>> FmuExporter::build_stored_zip_archive(
    const std::vector<FmuArchiveEntry>& entries) {
    if (entries.empty()) {
        return core::base::Error{472, core::base::ErrorDomain::Validation, core::base::Severity::Error, "Cannot build empty FMU archive", ""};
    }

    struct CentralDirRecord {
        uint32_t offset;
        uint32_t crc;
        uint32_t size;
        std::string name;
    };

    std::vector<uint8_t> zip;
    std::vector<CentralDirRecord> dir;
    dir.reserve(entries.size());

    for (const auto& entry : entries) {
        const uint32_t local_offset = static_cast<uint32_t>(zip.size());
        const uint32_t size = static_cast<uint32_t>(entry.content.size());
        const uint32_t crc = entry.content.empty() ? 0u : crc32_ieee(entry.content.data(), size);
        const uint16_t name_len = static_cast<uint16_t>(entry.relative_path.size());

        // Local file header PK\x03\x04
        write_u32_le(zip, 0x04034B50u);
        write_u16_le(zip, 20);   // version needed
        write_u16_le(zip, 0);    // flags
        write_u16_le(zip, 0);    // compression = 0 (stored)
        write_u16_le(zip, 0);    // mod time
        write_u16_le(zip, 0x5421); // mod date
        write_u32_le(zip, crc);
        write_u32_le(zip, size);
        write_u32_le(zip, size);
        write_u16_le(zip, name_len);
        write_u16_le(zip, 0);    // extra length
        zip.insert(zip.end(), entry.relative_path.begin(), entry.relative_path.end());
        zip.insert(zip.end(), entry.content.begin(), entry.content.end());

        dir.push_back(CentralDirRecord{local_offset, crc, size, entry.relative_path});
    }

    const uint32_t cd_offset = static_cast<uint32_t>(zip.size());
    for (const auto& rec : dir) {
        const uint16_t name_len = static_cast<uint16_t>(rec.name.size());
        write_u32_le(zip, 0x02014B50u);
        write_u16_le(zip, 20);
        write_u16_le(zip, 20);
        write_u16_le(zip, 0);
        write_u16_le(zip, 0);
        write_u16_le(zip, 0);
        write_u16_le(zip, 0x5421);
        write_u32_le(zip, rec.crc);
        write_u32_le(zip, rec.size);
        write_u32_le(zip, rec.size);
        write_u16_le(zip, name_len);
        write_u16_le(zip, 0);
        write_u16_le(zip, 0);
        write_u16_le(zip, 0);
        write_u16_le(zip, 0);
        write_u32_le(zip, 0);
        write_u32_le(zip, rec.offset);
        zip.insert(zip.end(), rec.name.begin(), rec.name.end());
    }
    const uint32_t cd_size = static_cast<uint32_t>(zip.size()) - cd_offset;

    // End of Central Directory PK\x05\x06
    write_u32_le(zip, 0x06054B50u);
    write_u16_le(zip, 0);
    write_u16_le(zip, 0);
    write_u16_le(zip, static_cast<uint16_t>(dir.size()));
    write_u16_le(zip, static_cast<uint16_t>(dir.size()));
    write_u32_le(zip, cd_size);
    write_u32_le(zip, cd_offset);
    write_u16_le(zip, 0);

    return zip;
}

core::base::Result<std::vector<uint8_t>> FmuExporter::export_fmu_bytes(
    const FmiModelDescription& desc) {
    auto xml_res = generate_model_description_xml(desc);
    if (!xml_res.is_ok()) {
        return xml_res.error();
    }
    const std::string c_src = generate_c_wrapper_source(desc);

    const auto& xml = xml_res.value();
    const std::string model_id = desc.model_identifier.empty() ? desc.model_name : desc.model_identifier;

    std::vector<FmuArchiveEntry> entries;
    entries.push_back(FmuArchiveEntry{
        "modelDescription.xml",
        std::vector<uint8_t>(xml.begin(), xml.end())
    });
    entries.push_back(FmuArchiveEntry{
        "sources/" + model_id + ".c",
        std::vector<uint8_t>(c_src.begin(), c_src.end())
    });
    return build_stored_zip_archive(entries);
}

core::base::Result<std::string> FmuExporter::extract_entry_text_from_zip(
    const std::vector<uint8_t>& zip_bytes,
    const std::string& relative_path) {
    size_t pos = 0;
    while (pos + 30 <= zip_bytes.size()) {
        const uint32_t sig = read_u32_le(zip_bytes.data() + pos);
        if (sig != 0x04034B50u) {
            break;
        }
        const uint32_t size = read_u32_le(zip_bytes.data() + pos + 18);
        const uint16_t name_len = read_u16_le(zip_bytes.data() + pos + 26);
        const uint16_t extra_len = read_u16_le(zip_bytes.data() + pos + 28);
        const size_t data_start = pos + 30 + name_len + extra_len;
        if (data_start + size > zip_bytes.size()) {
            break;
        }
        const std::string name(
            reinterpret_cast<const char*>(zip_bytes.data() + pos + 30), name_len);
        if (name == relative_path) {
            return std::string(
                reinterpret_cast<const char*>(zip_bytes.data() + data_start), size);
        }
        pos = data_start + size;
    }
    return core::base::Error{
        473, core::base::ErrorDomain::IO, core::base::Severity::Error,
        "Archive entry not found in FMU zip", relative_path
    };
}

} // namespace shunya::integ
