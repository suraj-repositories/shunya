#include "tests/test_framework.hpp"

#include "shunya/integ/fmu_exporter.hpp"
#include "shunya/integ/fmu_importer.hpp"

#include <cmath>
#include <cstring>

using namespace shunya::integ;

namespace {

// Reference FMI 2.0 Co-Simulation BouncingBall slave instance (h'' = -g, bounce restitution e)
struct BouncingBallInstance {
    double h{1.0};
    double v{0.0};
    double g{9.81};
    double e{0.7};
};

Fmi2Component bb_instantiate(const char*, const char*, const char*) {
    return new BouncingBallInstance();
}
int bb_setup(Fmi2Component, int, double, double, int, double) { return 0; }
int bb_noop(Fmi2Component) { return 0; }
int bb_set_real(Fmi2Component c, const Fmi2ValueReference* vr, size_t nvr, const double* val) {
    auto* bb = static_cast<BouncingBallInstance*>(c);
    for (size_t i = 0; i < nvr; ++i) {
        if (vr[i] == 0) bb->h = val[i];
        else if (vr[i] == 1) bb->v = val[i];
        else if (vr[i] == 2) bb->g = val[i];
        else if (vr[i] == 3) bb->e = val[i];
    }
    return 0;
}
int bb_get_real(Fmi2Component c, const Fmi2ValueReference* vr, size_t nvr, double* val) {
    auto* bb = static_cast<BouncingBallInstance*>(c);
    for (size_t i = 0; i < nvr; ++i) {
        if (vr[i] == 0) val[i] = bb->h;
        else if (vr[i] == 1) val[i] = bb->v;
        else if (vr[i] == 2) val[i] = bb->g;
        else if (vr[i] == 3) val[i] = bb->e;
    }
    return 0;
}
int bb_do_step(Fmi2Component c, double, double dt, int) {
    auto* bb = static_cast<BouncingBallInstance*>(c);
    const int sub = 10;
    const double sdt = dt / sub;
    for (int k = 0; k < sub; ++k) {
        bb->v -= bb->g * sdt;
        bb->h += bb->v * sdt;
        if (bb->h <= 0.0 && bb->v < 0.0) {
            bb->h = 0.0;
            bb->v = -bb->e * bb->v;
        }
    }
    return 0;
}
void bb_free(Fmi2Component c) {
    delete static_cast<BouncingBallInstance*>(c);
}

} // namespace

TEST_CASE("INT-02-001 FMI 2.0 modelDescription.xml parsing and BouncingBall co-simulation") {
    const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<fmiModelDescription fmiVersion="2.0" modelName="BouncingBall" guid="{1234-5678}">
  <CoSimulation modelIdentifier="BouncingBall"/>
  <DefaultExperiment startTime="0.0" stopTime="3.0" stepSize="0.01"/>
  <ModelVariables>
    <ScalarVariable name="h" valueReference="0" causality="output" variability="continuous">
      <Real start="1.0" unit="m"/>
    </ScalarVariable>
    <ScalarVariable name="v" valueReference="1" causality="local" variability="continuous">
      <Real start="0.0" unit="m/s"/>
    </ScalarVariable>
    <ScalarVariable name="g" valueReference="2" causality="parameter" variability="fixed">
      <Real start="9.81" unit="m/s2"/>
    </ScalarVariable>
    <ScalarVariable name="e" valueReference="3" causality="parameter" variability="fixed">
      <Real start="0.7"/>
    </ScalarVariable>
  </ModelVariables>
</fmiModelDescription>)";

    auto desc = FmuImporter::parse_model_description_xml(xml);
    REQUIRE(desc.is_ok());
    CHECK(desc.value().model_name == "BouncingBall");
    CHECK(desc.value().guid == "{1234-5678}");
    CHECK(desc.value().variables.size() == 4);

    Fmi2CoSimulationApi api{
        bb_instantiate, bb_setup, bb_noop, bb_noop,
        bb_set_real, bb_get_real, bb_do_step, bb_noop, bb_free
    };

    FmuImporter importer;
    REQUIRE(importer.load_with_api(desc.value(), api).is_ok());
    REQUIRE(importer.initialize(0.0, 3.0).is_ok());

    double max_h_after_bounce = 0.0;
    bool hit_floor = false;
    for (int step = 0; step < 120; ++step) {
        const double t = step * 0.01;
        REQUIRE(importer.do_step(t, 0.01).is_ok());
        auto h = importer.get_real("h");
        auto v = importer.get_real("v");
        REQUIRE(h.is_ok());
        REQUIRE(v.is_ok());
        if (h.value() < 0.02) hit_floor = true;
        if (hit_floor) max_h_after_bounce = std::max(max_h_after_bounce, h.value());
    }

    CHECK(hit_floor);
    // Theoretical first rebound apex h_1 = e^2 * h_0 = 0.49 * 1.0 = 0.49 m
    CHECK(std::abs(max_h_after_bounce - 0.49) < 0.03);
}

TEST_CASE("INT-02-001 Shunya sub-model FMU export to PKZIP archive and round-trip XML re-import") {
    FmiModelDescription desc;
    desc.model_name = "ShunyaBatteryThermalPack";
    desc.guid = "{ABCD-18650-PACK}";
    desc.model_identifier = "ShunyaBatteryThermalPack";
    desc.default_stop_time = 600.0;
    desc.default_step_size = 0.5;
    desc.variables.push_back(FmiScalarVariable{
        "discharge_current_a", 0, FmiCausality::Input, FmiVariability::Continuous, 2.5, true, "A", "Load current"
    });
    desc.variables.push_back(FmiScalarVariable{
        "terminal_voltage_v", 1, FmiCausality::Output, FmiVariability::Continuous, 4.2, false, "V", "Terminal voltage"
    });
    desc.variables.push_back(FmiScalarVariable{
        "cell_temp_k", 2, FmiCausality::Output, FmiVariability::Continuous, 298.15, false, "K", "Cell temperature"
    });

    auto fmu_bytes = FmuExporter::export_fmu_bytes(desc);
    REQUIRE(fmu_bytes.is_ok());
    CHECK(fmu_bytes.value().size() > 200);
    // Valid PKZIP magic header "PK\x03\x04"
    CHECK(fmu_bytes.value()[0] == 'P');
    CHECK(fmu_bytes.value()[1] == 'K');

    auto xml_extracted = FmuExporter::extract_entry_text_from_zip(fmu_bytes.value(), "modelDescription.xml");
    auto c_extracted = FmuExporter::extract_entry_text_from_zip(fmu_bytes.value(), "sources/ShunyaBatteryThermalPack.c");
    REQUIRE(xml_extracted.is_ok());
    REQUIRE(c_extracted.is_ok());
    CHECK(c_extracted.value().find("fmi2DoStep") != std::string::npos);

    auto reimported = FmuImporter::parse_model_description_xml(xml_extracted.value());
    REQUIRE(reimported.is_ok());
    CHECK(reimported.value().model_name == "ShunyaBatteryThermalPack");
    CHECK(reimported.value().variables.size() == 3);
    CHECK(reimported.value().variables[1].causality == FmiCausality::Output);
}
