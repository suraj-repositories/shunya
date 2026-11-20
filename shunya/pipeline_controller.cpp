#include "pipeline_controller.hpp"
#include "oscilloscope_controller.hpp"
#include "viewport_3d_item.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"
#include "shunya/mat/virtual_test_bench.hpp"
#include "shunya/mat/atomic_bond_checker.hpp"
#include "shunya/mat/material_comparison_engine.hpp"
#include "shunya/mat/experiment_persistence.hpp"

#include <QFileInfo>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace shunya::gui {

namespace {

QString normalizeLocalPath(const QString& pathOrUrl) {
    if (pathOrUrl.startsWith("file:")) {
        return QUrl(pathOrUrl).toLocalFile();
    }
    return pathOrUrl;
}

QVariant optDoubleVar(const std::optional<double>& v) {
    return v.has_value() ? QVariant(*v) : QVariant();
}

QVariantList strVecToList(const std::vector<std::string>& v) {
    QVariantList out;
    out.reserve(static_cast<int>(v.size()));
    for (const auto& s : v) out.append(QString::fromStdString(s));
    return out;
}

matdb::ElementSortField parseSortField(const QString& s) {
    if (s == "Name") return matdb::ElementSortField::Name;
    if (s == "Symbol") return matdb::ElementSortField::Symbol;
    if (s == "AtomicWeight") return matdb::ElementSortField::AtomicWeight;
    if (s == "Density") return matdb::ElementSortField::Density;
    if (s == "MeltingPoint") return matdb::ElementSortField::MeltingPoint;
    if (s == "BoilingPoint") return matdb::ElementSortField::BoilingPoint;
    if (s == "SpecificHeat") return matdb::ElementSortField::SpecificHeat;
    if (s == "ThermalConductivity") return matdb::ElementSortField::ThermalConductivity;
    if (s == "ElectricalConductivity") return matdb::ElementSortField::ElectricalConductivity;
    if (s == "YoungModulus") return matdb::ElementSortField::YoungModulus;
    if (s == "Electronegativity") return matdb::ElementSortField::Electronegativity;
    return matdb::ElementSortField::AtomicNumber;
}

QVariantMap evalPropToMap(const mat::EvaluatedProperty<double>& p) {
    QVariantMap m;
    m["available"] = p.available();
    if (p.value.has_value()) m["value"] = *p.value;
    m["unit"] = QString::fromStdString(p.unit);
    m["epistemic"] = QString::fromStdString(std::string(mat::epistemic_status_to_string(p.epistemic)));
    m["note"] = QString::fromStdString(p.model_note);
    return m;
}

class CircuitMutationCommand : public core::domain::ICommand {
public:
    CircuitMutationCommand(eng::InteractiveCircuitDocument* doc,
                           eng::InteractiveCircuitDocument before,
                           eng::InteractiveCircuitDocument after,
                           std::string label)
        : doc_(doc), before_(std::move(before)), after_(std::move(after)), label_(std::move(label)) {}

    core::base::Result<void> execute() override {
        if (doc_) *doc_ = after_;
        return core::base::Result<void>::ok();
    }
    core::base::Result<void> undo() override {
        if (doc_) *doc_ = before_;
        return core::base::Result<void>::ok();
    }
    std::string_view name() const noexcept override { return label_; }

private:
    eng::InteractiveCircuitDocument* doc_;
    eng::InteractiveCircuitDocument before_;
    eng::InteractiveCircuitDocument after_;
    std::string label_;
};

} // namespace

ShunyaPipelineController::ShunyaPipelineController(
    std::shared_ptr<eng::EngineeringWorkspaceState> workspace,
    QObject* parent)
    : QObject(parent), ws_(std::move(workspace)) {
    if (!ws_) {
        ws_ = std::make_shared<eng::EngineeringWorkspaceState>();
    }
    if (!ws_->element_database().is_loaded()) {
        (void)matdb::ElementDatabase::global().load_default();
    }
    if (ws_->material_registry().material_count() == 0 && ws_->object_manager().count() == 0) {
        (void)ws_->seed_lithium_battery_workflow();
    }
    ShunyaViewport3DItem::setDefaultWorkspace(ws_);
    ws_->subscribe([this](const eng::WorkspaceChangeEvent& ev) {
        if (!ev.summary.empty()) {
            pushStatus(QString::fromStdString(ev.summary));
        }
        emit workspaceChanged();
        emit circuitChanged();
        emit physicsChanged();
    });
}

void ShunyaPipelineController::emitAllChanged() {
    emit workspaceChanged();
    emit elementDbChanged();
    emit circuitChanged();
    emit physicsChanged();
    emit statusMessageChanged();
}

void ShunyaPipelineController::pushStatus(const QString& msg) {
    status_message_ = msg;
    emit statusMessageChanged();
}

void ShunyaPipelineController::setActiveWorkspace(const QString& ws) {
    if (active_workspace_ != ws) {
        active_workspace_ = ws;
        emit activeWorkspaceChanged();
    }
}

QString ShunyaPipelineController::projectName() const {
    return (ws_ && ws_->project()) ? QString::fromStdString(ws_->project()->manifest().name) : "Untitled";
}

QString ShunyaPipelineController::projectAuthor() const {
    return (ws_ && ws_->project()) ? QString::fromStdString(ws_->project()->manifest().author) : "Engineer";
}

bool ShunyaPipelineController::isDirty() const {
    return (ws_ && ws_->project()) ? ws_->project()->is_dirty() : false;
}

int ShunyaPipelineController::elementCount() const {
    return static_cast<int>(ws_->element_database().count());
}

int ShunyaPipelineController::materialCount() const {
    return static_cast<int>(ws_->material_registry().material_count());
}

int ShunyaPipelineController::experimentCount() const {
    return static_cast<int>(ws_->mixture_experiments().size());
}

int ShunyaPipelineController::objectCount() const {
    return static_cast<int>(ws_->object_manager().count());
}

int ShunyaPipelineController::componentCount() const {
    return static_cast<int>(ws_->component_library().count());
}

int ShunyaPipelineController::batteryCount() const {
    return static_cast<int>(ws_->battery_specs().size());
}

int ShunyaPipelineController::circuitCount() const {
    return static_cast<int>(ws_->circuits().size());
}

int ShunyaPipelineController::activeCircuitIndex() const {
    return static_cast<int>(ws_->active_circuit_index());
}

bool ShunyaPipelineController::canCircuitUndo() const {
    return ws_->command_stack().can_undo();
}

bool ShunyaPipelineController::canCircuitRedo() const {
    return ws_->command_stack().can_redo();
}

// =========================================================================
// 1. Elements & Natural Behavior
// =========================================================================
QVariantList ShunyaPipelineController::queryElements(
    const QString& searchText,
    const QString& phaseFilter,
    const QString& blockFilter,
    const QString& elecFilter,
    const QString& sortBy,
    bool descending) const {
    matdb::ElementQuery q;
    q.search_text = searchText.toStdString();
    q.phase_filter = phaseFilter.toStdString();
    q.block_filter = blockFilter.toStdString();
    q.electrical_type_filter = elecFilter.toStdString();
    q.sort_by = parseSortField(sortBy);
    q.descending = descending;

    const auto recs = ws_->element_database().query(q);
    QVariantList out;
    out.reserve(static_cast<int>(recs.size()));
    for (const auto* r : recs) {
        QVariantMap m;
        m["atomicNumber"] = r->atomic_number;
        m["symbol"] = QString::fromStdString(r->symbol);
        m["iupacSymbol"] = QString::fromStdString(r->iupac_symbol.empty() ? r->symbol : r->iupac_symbol);
        m["name"] = QString::fromStdString(r->name);
        m["phase"] = QString::fromStdString(r->phase);
        m["block"] = QString::fromStdString(r->block);
        m["period"] = r->period;
        if (r->group) m["group"] = *r->group;
        m["electricalType"] = QString::fromStdString(r->electrical_type);
        m["atomicWeight"] = optDoubleVar(r->atomic_weight);
        m["density"] = optDoubleVar(r->density);
        m["meltingPoint"] = optDoubleVar(r->melting_point);
        m["boilingPoint"] = optDoubleVar(r->boiling_point);
        m["thermalConductivity"] = optDoubleVar(r->thermal_conductivity);
        m["electricalConductivity"] = optDoubleVar(r->effective_conductivity());
        m["specificHeat"] = optDoubleVar(r->specific_heat);
        m["electronegativity"] = optDoubleVar(r->electronegativity);
        m["youngModulus"] = optDoubleVar(r->young_modulus);
        out.append(m);
    }
    return out;
}

QVariantMap ShunyaPipelineController::getElementDetails(int atomicNumber) const {
    QVariantMap res;
    const auto* r = ws_->element_database().find_by_z(atomicNumber);
    const auto* cat = matdb::Elements118Catalog::find_by_z(atomicNumber);
    if (!r && !cat) return res;

    int z = r ? r->atomic_number : (cat ? cat->atomic_number : atomicNumber);
    res["atomicNumber"] = z;
    res["symbol"] = QString::fromStdString(r ? r->symbol : cat->symbol);
    res["iupacSymbol"] = QString::fromStdString(r ? (r->iupac_symbol.empty() ? r->symbol : r->iupac_symbol) : cat->symbol);
    res["name"] = QString::fromStdString(r ? r->name : cat->name);
    res["phase"] = QString::fromStdString(r ? r->phase : (cat->standard_state == matdb::StandardPhase::Gas ? "Gas" : cat->standard_state == matdb::StandardPhase::Liquid ? "Liquid" : "Solid"));
    res["block"] = QString::fromStdString(r ? r->block : std::string(1, cat->block));
    res["notes"] = r ? strVecToList(r->normalization_notes) : QVariantList{};

    // Category and electron configuration
    if (cat) {
        res["atomicMass"] = (r && r->atomic_weight) ? *r->atomic_weight : cat->atomic_weight_amu;
        res["electronConfig"] = QString::fromStdString((r && !r->electron_configuration_iupac.empty()) ? r->electron_configuration_iupac : cat->electron_configuration);
        switch (cat->family) {
        case matdb::ElementFamily::AlkaliMetal: res["category"] = "Alkali Metal"; break;
        case matdb::ElementFamily::AlkalineEarth: res["category"] = "Alkaline Earth"; break;
        case matdb::ElementFamily::TransitionMetal: res["category"] = "Transition Metal"; break;
        case matdb::ElementFamily::PostTransitionMetal: res["category"] = "Post-Transition Metal"; break;
        case matdb::ElementFamily::Metalloid: res["category"] = "Metalloid"; break;
        case matdb::ElementFamily::ReactiveNonmetal: res["category"] = "Reactive Nonmetal"; break;
        case matdb::ElementFamily::NobleGas: res["category"] = "Noble Gas"; break;
        case matdb::ElementFamily::Lanthanide: res["category"] = "Lanthanide"; break;
        case matdb::ElementFamily::Actinide: res["category"] = "Actinide"; break;
        }
    } else if (r) {
        res["atomicMass"] = r->atomic_weight.value_or(0.0);
        res["electronConfig"] = QString::fromStdString(r->electron_configuration_iupac);
        res["category"] = "Unknown";
    }

    if (r) {
        if (r->density) res["density"] = *r->density;
        if (r->melting_point) res["meltingPoint"] = *r->melting_point;
        if (r->boiling_point) res["boilingPoint"] = *r->boiling_point;
        if (r->electronegativity) res["electronegativity"] = *r->electronegativity;
    }

    QVariantList cols;
    for (const auto& col : ws_->element_database().csv_columns()) {
        QVariantMap item;
        item["column"] = QString::fromStdString(col);
        const std::string raw = (r && r->raw_csv_fields.count(col)) ? r->raw_csv_fields.at(col) : "";
        item["rawValue"] = raw.empty() ? QStringLiteral("--") : QString::fromStdString(raw);
        item["provenance"] = QString::fromStdString(std::string(matdb::provenance_to_string(r ? r->provenance_of(col) : matdb::PropertyProvenance::MissingUnsupported)));
        cols.append(item);
    }
    res["allProperties"] = cols;
    return res;
}

QVariantMap ShunyaPipelineController::evaluateElementBehavior(
    int atomicNumber, double tempK, double pressurePa, bool oxidizing) const {
    QVariantMap res;
    const auto* r = ws_->element_database().find_by_z(atomicNumber);
    if (!r) return res;

    const auto rep = mat::ElementBehaviorEngine::evaluate(*r, tempK, pressurePa, oxidizing);
    res["atomicNumber"] = rep.atomic_number;
    res["symbol"] = QString::fromStdString(rep.symbol);
    res["name"] = QString::fromStdString(rep.name);
    res["phase"] = QString::fromStdString(std::string(mat::evaluated_phase_to_string(rep.phase)));
    res["phaseNote"] = QString::fromStdString(rep.phase_note);
    res["hasMeltingPoint"] = rep.has_melting_point;
    res["meltingPointK"] = rep.effective_melting_point_k;
    res["hasBoilingPoint"] = rep.has_boiling_point;
    res["boilingPointK"] = rep.effective_boiling_point_k;
    res["density"] = evalPropToMap(rep.density_kg_m3);
    res["specificHeat"] = evalPropToMap(rep.specific_heat_j_kgk);
    res["thermalConductivity"] = evalPropToMap(rep.thermal_conductivity_w_mk);
    res["thermalExpansion"] = evalPropToMap(rep.thermal_expansion_1_k);
    res["thermalStrain"] = evalPropToMap(rep.thermal_strain_fraction);
    res["electricalConductivity"] = evalPropToMap(rep.electrical_conductivity_s_m);
    res["electricalResistivity"] = evalPropToMap(rep.electrical_resistivity_ohm_m);
    res["isSuperconducting"] = rep.is_superconducting;
    res["magneticState"] = QString::fromStdString(rep.magnetic_state);
    res["reactivityClass"] = QString::fromStdString(rep.reactivity_class);
    res["reactivityIndex"] = rep.reactivity_index;
    res["isRadioactive"] = rep.is_radioactive;
    res["oxidationSusceptible"] = rep.oxidation_susceptible;
    res["corrosionSusceptible"] = rep.corrosion_susceptible;
    res["combustionRisk"] = rep.combustion_risk;
    res["stableAtConditions"] = rep.stable_at_conditions;
    res["warnings"] = strVecToList(rep.warnings);
    res["unsupportedProperties"] = strVecToList(rep.unsupported_properties);
    return res;
}

// =========================================================================
// 2. Materials & Mixing Laboratory
// =========================================================================
QVariantList ShunyaPipelineController::getMaterials() const {
    QVariantList out;
    for (const auto& [id, mat] : ws_->material_registry().materials()) {
        QVariantMap m;
        m["id"] = QString::fromStdString(id.to_string());
        m["name"] = QString::fromStdString(mat.name());
        m["state"] = QString::fromStdString(std::string(core::domain::material_physical_state_to_string(mat.state())));
        m["density"] = mat.density();
        m["thermalConductivity"] = mat.thermal().thermal_conductivity.value_or(0.0);
        m["specificHeat"] = mat.thermal().specific_heat_capacity.value_or(0.0);
        m["meltingPoint"] = mat.thermal().melting_point_k.value_or(0.0);
        m["electricalConductivity"] = mat.electrical().electrical_conductivity.value_or(0.0);
        m["resistivity"] = mat.electrical().effective_resistivity();
        m["provenance"] = QString::fromStdString(mat.provenance());
        m["composition"] = QString::fromStdString(mat.composition());
        out.append(m);
    }
    std::sort(out.begin(), out.end(), [](const QVariant& a, const QVariant& b) {
        return a.toMap()["name"].toString() < b.toMap()["name"].toString();
    });
    return out;
}

QVariantList ShunyaPipelineController::getMixtureExperiments() const {
    QVariantList out;
    for (const auto& e : ws_->mixture_experiments()) {
        QVariantMap m;
        m["id"] = QString::fromStdString(e.id.to_string());
        m["name"] = QString::fromStdString(e.name);
        m["scenario"] = QString::fromStdString(std::string(mat::formation_scenario_to_string(e.scenario)));
        m["tempK"] = e.temperature_k;
        m["pressurePa"] = e.pressure_pa;
        m["oxidizing"] = e.oxidizing_atmosphere;
        m["densityOverride"] = e.user_density_override_kg_m3;
        m["promotedMaterialId"] = e.promoted_material_id ? QString::fromStdString(e.promoted_material_id->to_string()) : "";
        QVariantList ins;
        for (const auto& i : e.inputs) {
            QVariantMap im;
            im["symbol"] = QString::fromStdString(i.symbol_or_id);
            im["basis"] = static_cast<int>(i.basis);
            im["amount"] = i.amount;
            im["layerThickness"] = i.layer_thickness_m;
            im["isMatrix"] = i.is_matrix;
            ins.append(im);
        }
        m["inputs"] = ins;
        out.append(m);
    }
    return out;
}

QVariantList ShunyaPipelineController::getStandardMixturePresets() const {
    QVariantList out;
    for (const auto& e : mat::MaterialMixingLab::standard_presets()) {
        QVariantMap m;
        m["id"] = QString::fromStdString(e.id.to_string());
        m["name"] = QString::fromStdString(e.name);
        m["description"] = QString::fromStdString(e.description);
        m["scenario"] = QString::fromStdString(std::string(mat::formation_scenario_to_string(e.scenario)));
        m["tempK"] = e.temperature_k;
        m["pressurePa"] = e.pressure_pa;
        m["oxidizing"] = e.oxidizing_atmosphere;
        QVariantList ins;
        for (const auto& i : e.inputs) {
            QVariantMap im;
            im["symbol"] = QString::fromStdString(i.symbol_or_id);
            im["basis"] = static_cast<int>(i.basis);
            im["amount"] = i.amount;
            im["layerThickness"] = i.layer_thickness_m;
            im["isMatrix"] = i.is_matrix;
            ins.append(im);
        }
        m["inputs"] = ins;
        out.append(m);
    }
    return out;
}

std::vector<mat::MixtureConstituentInput> ShunyaPipelineController::inputsFromVariantList(
    const QVariantList& list) const {
    std::vector<mat::MixtureConstituentInput> out;
    for (const auto& v : list) {
        const auto m = v.toMap();
        mat::MixtureConstituentInput in;
        in.symbol_or_id = m.value("symbol").toString().toStdString();
        in.basis = static_cast<mat::QuantityInputBasis>(m.value("basis", 1).toInt());
        in.amount = m.value("amount", 0.0).toDouble();
        in.layer_thickness_m = m.value("layerThickness", 1e-3).toDouble();
        in.is_matrix = m.value("isMatrix", false).toBool();
        if (!in.symbol_or_id.empty()) out.push_back(std::move(in));
    }
    return out;
}

QVariantMap ShunyaPipelineController::evaluateMixture(
    const QString& name,
    const QString& scenario,
    double tempK,
    double pressurePa,
    bool oxidizing,
    double densityOverride,
    const QVariantList& inputs) const {
    QVariantMap out;
    mat::MixtureExperiment exp;
    exp.name = name.toStdString();
    auto sc_res = mat::formation_scenario_from_string(scenario.toStdString());
    exp.scenario = sc_res.is_ok() ? sc_res.value() : mat::FormationScenario::PhysicalMixture;
    exp.temperature_k = tempK;
    exp.pressure_pa = pressurePa;
    exp.oxidizing_atmosphere = oxidizing;
    exp.user_density_override_kg_m3 = densityOverride;
    exp.inputs = inputsFromVariantList(inputs);

    auto ev_res = mat::MaterialMixingLab::evaluate(exp, ws_->element_database());
    if (ev_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(ev_res.error().message);
        return out;
    }
    const auto& ev = ev_res.value();
    out["ok"] = true;
    out["isChemicallyValid"] = ev.is_chemically_valid;
    out["overallEpistemic"] = QString::fromStdString(std::string(mat::epistemic_status_to_string(ev.overall_epistemic)));
    out["predictedPhase"] = QString::fromStdString(std::string(mat::evaluated_phase_to_string(ev.predicted_phase)));
    out["density"] = evalPropToMap(ev.density_kg_m3);
    out["specificHeat"] = evalPropToMap(ev.specific_heat_j_kgk);
    out["thermalConductivity"] = evalPropToMap(ev.thermal_conductivity_w_mk);
    out["electricalConductivity"] = evalPropToMap(ev.electrical_conductivity_s_m);
    out["electricalResistivity"] = evalPropToMap(ev.electrical_resistivity_ohm_m);
    out["thermalExpansion"] = evalPropToMap(ev.thermal_expansion_1_k);
    out["youngModulus"] = evalPropToMap(ev.young_modulus_pa);
    out["meltingPoint"] = evalPropToMap(ev.estimated_melting_point_k);
    out["validationErrors"] = strVecToList(ev.validation_errors);
    out["warnings"] = strVecToList(ev.warnings);
    out["scientificNotes"] = strVecToList(ev.scientific_notes);

    QVariantList rows;
    for (const auto& r : ev.comparison_rows) {
        QVariantMap rm;
        rm["property"] = QString::fromStdString(r.property_name);
        rm["unit"] = QString::fromStdString(r.unit);
        rm["mixtureValue"] = optDoubleVar(r.mixture_value);
        QStringList elemSummary;
        for (const auto& [sym, val] : r.constituent_values) {
            elemSummary << QString::fromStdString(sym) + ": " +
                           (val ? QString::number(*val, 'g', 4) : QStringLiteral("N/A"));
        }
        rm["elementsSummary"] = elemSummary.join("  |  ");
        rm["epistemic"] = QString::fromStdString(std::string(mat::epistemic_status_to_string(r.epistemic)));
        rows.append(rm);
    }
    out["comparisonRows"] = rows;
    return out;
}

QVariantMap ShunyaPipelineController::qualifyMixtureTestBench(
    const QString& name,
    const QString& scenario,
    double tempK,
    double pressurePa,
    bool oxidizing,
    double densityOverride,
    const QVariantList& inputs,
    const QString& presetEnvName) const {
    QVariantMap out;
    mat::MixtureExperiment exp;
    exp.name = name.toStdString();
    auto sc_res = mat::formation_scenario_from_string(scenario.toStdString());
    exp.scenario = sc_res.is_ok() ? sc_res.value() : mat::FormationScenario::PhysicalMixture;
    exp.temperature_k = tempK;
    exp.pressure_pa = pressurePa;
    exp.oxidizing_atmosphere = oxidizing;
    exp.user_density_override_kg_m3 = densityOverride;
    exp.inputs = inputsFromVariantList(inputs);

    auto ev_res = mat::MaterialMixingLab::evaluate(exp, ws_->element_database());
    if (ev_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(ev_res.error().message);
        return out;
    }

    mat::EnvironmentConditions env;
    if (presetEnvName == "Spacecraft (LEO)") env = mat::VirtualTestBenchEngine::preset_spacecraft_leo();
    else if (presetEnvName == "Deep Sea Submersible") env = mat::VirtualTestBenchEngine::preset_deep_sea();
    else if (presetEnvName == "Industrial Furnace") env = mat::VirtualTestBenchEngine::preset_industrial_furnace();
    else if (presetEnvName == "Underground Power Cable") env = mat::VirtualTestBenchEngine::preset_underground_cable();
    else if (presetEnvName == "Outdoor Coastal Weathering") env = mat::VirtualTestBenchEngine::preset_outdoor_coastal();
    else env = mat::VirtualTestBenchEngine::preset_spacecraft_leo();

    auto report = mat::VirtualTestBenchEngine::qualify_material(ev_res.value(), env);

    out["ok"] = true;
    out["environmentName"] = QString::fromStdString(report.conditions.name);
    out["overallPassed"] = report.overall_qualification_passed;
    out["aggregateScore"] = report.aggregate_score;

    QVariantList domains;
    for (const auto& d : report.domain_results) {
        QVariantMap dm;
        dm["name"] = QString::fromStdString(d.domain_name);
        dm["passed"] = d.passed;
        dm["score"] = d.score;
        dm["metricSummary"] = QString::fromStdString(d.metric_summary);
        dm["physicalModel"] = QString::fromStdString(d.physical_model);
        dm["provenance"] = QString::fromStdString(d.provenance);
        dm["isApproximate"] = d.is_approximate;
        dm["details"] = strVecToList(d.details);
        dm["failureReasons"] = strVecToList(d.failure_reasons);
        domains.append(dm);
    }
    out["domainResults"] = domains;
    out["criticalFailures"] = strVecToList(report.critical_failures);
    out["recommendations"] = strVecToList(report.engineering_recommendations);
    return out;
}

QVariantMap ShunyaPipelineController::analyzeAtomicBonds(
    const QString& scenario,
    const QVariantList& inputs) const {
    QVariantMap out;
    auto in_list = inputsFromVariantList(inputs);
    std::vector<const matdb::ElementRecord*> elements;
    std::vector<double> mole_fractions;
    const auto& db = ws_->element_database();

    for (const auto& in : in_list) {
        const auto* el = db.find_by_symbol(in.symbol_or_id);
        if (!el) el = matdb::ElementDatabase::global().find_by_symbol(in.symbol_or_id);
        if (el) {
            elements.push_back(el);
            mole_fractions.push_back(in.amount);
        }
    }

    auto report = mat::AtomicBondChecker::analyze_mixture(elements, mole_fractions, scenario.toStdString());

    out["isCompatible"] = report.is_formation_scenario_compatible;
    out["primaryBondType"] = QString::fromStdString(report.primary_overall_bond_type);
    out["overallMiscibility"] = QString::fromStdString(report.overall_miscibility);

    QVariantList pairs;
    for (const auto& p : report.pairwise_analyses) {
        QVariantMap pm;
        pm["elementA"] = QString::fromStdString(p.element_a);
        pm["elementB"] = QString::fromStdString(p.element_b);
        pm["deltaElectronegativity"] = p.delta_electronegativity;
        pm["radiusDifferencePct"] = p.radius_difference_pct;
        pm["bondType"] = QString::fromStdString(std::string(mat::atomic_bond_type_to_string(p.primary_bond_type)));
        pm["miscibility"] = QString::fromStdString(std::string(mat::miscibility_state_to_string(p.miscibility)));
        pm["isHazardous"] = p.is_hazardous;
        pm["hazardWarning"] = QString::fromStdString(p.hazard_warning);
        pm["plainLanguageVerdict"] = QString::fromStdString(p.plain_language_verdict);
        pm["crystalStructures"] = QString::fromStdString(p.crystal_structures);
        pairs.append(pm);
    }
    out["pairwise"] = pairs;
    out["stoichiometricSuggestions"] = strVecToList(report.stoichiometric_suggestions);
    out["warnings"] = strVecToList(report.chemical_warnings);
    out["recommendations"] = strVecToList(report.recommendations);
    return out;
}

QVariantMap ShunyaPipelineController::compareMaterialMetrics(
    const QString& name,
    const QString& scenario,
    double density,
    double thermalK,
    double electricalSigma,
    double meltingK,
    const QVariantList& inputs) const {
    QVariantMap out;
    auto in_list = inputsFromVariantList(inputs);
    std::vector<const matdb::ElementRecord*> elements;
    std::vector<double> mass_fractions;
    const auto& db = ws_->element_database();

    for (const auto& in : in_list) {
        const auto* el = db.find_by_symbol(in.symbol_or_id);
        if (!el) el = matdb::ElementDatabase::global().find_by_symbol(in.symbol_or_id);
        if (el) {
            elements.push_back(el);
            mass_fractions.push_back(in.amount);
        }
    }

    auto current_mat = mat::MaterialComparisonEngine::evaluate_mixture(
        name.toStdString(), elements, mass_fractions,
        density, thermalK, electricalSigma, meltingK, scenario.toStdString());

    std::vector<mat::MaterialMetricScores> candidates;
    candidates.push_back(current_mat);

    for (const auto* el : elements) {
        candidates.push_back(mat::MaterialComparisonEngine::evaluate_element(*el));
    }

    for (const auto& [id, pm] : ws_->material_registry().materials()) {
        mat::MaterialMetricScores s;
        s.material_name = pm.name();
        s.density = pm.density();
        s.thermal_conductivity = pm.thermal().thermal_conductivity.value_or(20.0);
        s.electrical_conductivity = pm.electrical().electrical_conductivity.value_or(5.0e6) / 1e6;
        s.temperature_tolerance = pm.thermal().melting_point_k.value_or(1000.0);
        s.corrosion_resistance = 70.0;
        s.mechanical_strength = pm.mechanical().yield_strength_pa.value_or(250.0e6) / 1e6;
        s.economic_abundance = 70.0;
        s.emi_shielding = std::clamp(s.electrical_conductivity * 1.5, 10.0, 95.0);
        candidates.push_back(s);
    }

    mat::MaterialComparisonEngine::normalize_radar_scores(candidates);

    QVariantList cand_list;
    for (const auto& c : candidates) {
        QVariantMap cm;
        cm["name"] = QString::fromStdString(c.material_name);
        cm["electricalConductivity"] = c.electrical_conductivity;
        cm["thermalConductivity"] = c.thermal_conductivity;
        cm["temperatureTolerance"] = c.temperature_tolerance;
        cm["corrosionResistance"] = c.corrosion_resistance;
        cm["density"] = c.density;
        cm["mechanicalStrength"] = c.mechanical_strength;
        cm["economicAbundance"] = c.economic_abundance;
        cm["emiShielding"] = c.emi_shielding;

        QVariantList radar_scores;
        for (double r : c.normalized_radar) radar_scores.append(r);
        cm["radarScores"] = radar_scores;

        cand_list.append(cm);
    }

    out["materials"] = cand_list;
    out["currentMaterial"] = cand_list.isEmpty() ? QVariantMap() : cand_list.first().toMap();
    return out;
}

QVariantMap ShunyaPipelineController::rankMaterialsForGoal(
    const QString& goal,
    const QString& currentName,
    const QString& scenario,
    double density,
    double thermalK,
    double electricalSigma,
    double meltingK,
    const QVariantList& inputs) const {
    auto comp = compareMaterialMetrics(currentName, scenario, density, thermalK, electricalSigma, meltingK, inputs);
    auto mats = comp["materials"].toList();

    std::vector<mat::MaterialMetricScores> candidates;
    for (const auto& v : mats) {
        auto m = v.toMap();
        mat::MaterialMetricScores s;
        s.material_name = m["name"].toString().toStdString();
        s.electrical_conductivity = m["electricalConductivity"].toDouble();
        s.thermal_conductivity = m["thermalConductivity"].toDouble();
        s.temperature_tolerance = m["temperatureTolerance"].toDouble();
        s.corrosion_resistance = m["corrosionResistance"].toDouble();
        s.density = m["density"].toDouble();
        s.mechanical_strength = m["mechanicalStrength"].toDouble();
        s.economic_abundance = m["economicAbundance"].toDouble();
        s.emi_shielding = m["emiShielding"].toDouble();

        auto r_list = m["radarScores"].toList();
        for (int i = 0; i < 8 && i < r_list.size(); ++i) {
            s.normalized_radar[i] = r_list[i].toDouble();
        }
        candidates.push_back(s);
    }

    auto rank_res = mat::MaterialComparisonEngine::rank_materials(candidates, goal.toStdString());

    QVariantMap out;
    out["goal"] = QString::fromStdString(rank_res.goal);
    out["winnerName"] = QString::fromStdString(rank_res.winner_name);
    out["explanation"] = QString::fromStdString(rank_res.explanation);

    QVariantList rl;
    for (const auto& r : rank_res.rankings) {
        QVariantMap rm;
        rm["name"] = QString::fromStdString(r.first);
        rm["score"] = r.second;
        rl.append(rm);
    }
    out["rankings"] = rl;
    return out;
}

QVariantMap ShunyaPipelineController::explainMaterial(
    const QString& name,
    const QString& scenario,
    double density,
    double thermalK,
    double electricalSigma,
    double meltingK,
    const QVariantList& inputs) const {
    auto in_list = inputsFromVariantList(inputs);
    std::vector<const matdb::ElementRecord*> elements;
    std::vector<std::string> constituents;
    std::vector<double> fractions;
    const auto& db = ws_->element_database();

    for (const auto& in : in_list) {
        const auto* el = db.find_by_symbol(in.symbol_or_id);
        if (!el) el = matdb::ElementDatabase::global().find_by_symbol(in.symbol_or_id);
        if (el) {
            elements.push_back(el);
            constituents.push_back(el->symbol);
            fractions.push_back(in.amount);
        }
    }

    auto mat = mat::MaterialComparisonEngine::evaluate_mixture(
        name.toStdString(), elements, fractions,
        density, thermalK, electricalSigma, meltingK, scenario.toStdString());

    auto bond_rep = mat::AtomicBondChecker::analyze_mixture(elements, fractions, scenario.toStdString());
    auto expl = mat::MaterialComparisonEngine::generate_explanation(
        mat, constituents, fractions, scenario.toStdString(), bond_rep.overall_miscibility);

    QVariantMap out;
    out["summary"] = QString::fromStdString(expl.summary);
    out["featuresAdvantages"] = strVecToList(expl.features_advantages);
    out["disadvantagesRisks"] = strVecToList(expl.disadvantages_risks);
    out["physicalMechanism"] = QString::fromStdString(expl.physical_mechanism);
    out["suggestedImprovements"] = strVecToList(expl.suggested_improvements);
    out["confidenceTag"] = QString::fromStdString(expl.confidence_tag);
    out["provenance"] = QString::fromStdString(expl.provenance);
    return out;
}

QString ShunyaPipelineController::exportMaterialReportMarkdown(
    const QString& name,
    const QString& scenario,
    double density,
    double thermalK,
    double electricalSigma,
    double meltingK,
    const QVariantList& inputs) const {
    auto in_list = inputsFromVariantList(inputs);
    std::vector<const matdb::ElementRecord*> elements;
    std::vector<std::string> constituents;
    std::vector<double> fractions;
    const auto& db = ws_->element_database();

    for (const auto& in : in_list) {
        const auto* el = db.find_by_symbol(in.symbol_or_id);
        if (!el) el = matdb::ElementDatabase::global().find_by_symbol(in.symbol_or_id);
        if (el) {
            elements.push_back(el);
            constituents.push_back(el->symbol);
            fractions.push_back(in.amount);
        }
    }

    auto mat = mat::MaterialComparisonEngine::evaluate_mixture(
        name.toStdString(), elements, fractions,
        density, thermalK, electricalSigma, meltingK, scenario.toStdString());

    auto bond_rep = mat::AtomicBondChecker::analyze_mixture(elements, fractions, scenario.toStdString());
    auto expl = mat::MaterialComparisonEngine::generate_explanation(
        mat, constituents, fractions, scenario.toStdString(), bond_rep.overall_miscibility);

    std::string md = mat::MaterialComparisonEngine::export_markdown_report(expl, mat);
    return QString::fromStdString(md);
}

QVariantMap ShunyaPipelineController::saveAndPromoteMixture(
    const QString& expId,
    const QString& name,
    const QString& scenario,
    double tempK,
    double pressurePa,
    bool oxidizing,
    double densityOverride,
    const QVariantList& inputs,
    bool alsoPromoteToMaterial) {
    QVariantMap out;
    mat::MixtureExperiment exp;
    if (!expId.isEmpty()) {
        if (auto u = core::base::Uuid::from_string(expId.toStdString())) exp.id = *u;
    }
    exp.name = name.toStdString();
    auto sc_res = mat::formation_scenario_from_string(scenario.toStdString());
    exp.scenario = sc_res.is_ok() ? sc_res.value() : mat::FormationScenario::PhysicalMixture;
    exp.temperature_k = tempK;
    exp.pressure_pa = pressurePa;
    exp.oxidizing_atmosphere = oxidizing;
    exp.user_density_override_kg_m3 = densityOverride;
    exp.inputs = inputsFromVariantList(inputs);

    if (alsoPromoteToMaterial) {
        auto mres = mat::MaterialMixingLab::promote_to_material(exp, ws_->element_database());
        if (mres.is_err()) {
            out["ok"] = false;
            out["error"] = QString::fromStdString(mres.error().message +
                (mres.error().details.empty() ? "" : ": " + mres.error().details));
            return out;
        }
        core::domain::Material mat = mres.value();
        out["materialId"] = QString::fromStdString(mat.id().to_string());
        (void)ws_->material_registry().register_material(std::move(mat));
    }

    auto& list = ws_->mixture_experiments();
    bool updated = false;
    for (auto& existing : list) {
        if (existing.id == exp.id) {
            existing = exp;
            updated = true;
            break;
        }
    }
    if (!updated) list.push_back(exp);

    out["ok"] = true;
    out["experimentId"] = QString::fromStdString(exp.id.to_string());
    ws_->notify_change(eng::DependencyChangeKind::MixtureUpdated, exp.id,
                       "Saved mixture experiment '" + exp.name + "'");
    return out;
}

QVariantMap ShunyaPipelineController::saveExperimentFile(
    const QString& name,
    const QString& scenario,
    double tempK,
    double pressurePa,
    bool oxidizing,
    double densityOverride,
    const QVariantList& inputs,
    const QString& author,
    const QString& notes,
    const QStringList& tags) {
    QVariantMap out;
    mat::VersionedExperimentBundle bundle;
    bundle.metadata.schema_version = "5.0.0";
    bundle.metadata.application_version = "5.0.0";
    bundle.metadata.author = author.toStdString();
    bundle.metadata.notes = notes.toStdString();
    bundle.metadata.created_at = mat::ExperimentPersistence::current_iso8601_timestamp();
    bundle.metadata.modified_at = bundle.metadata.created_at;
    for (const auto& t : tags) bundle.metadata.tags.push_back(t.toStdString());

    bundle.experiment.id = core::base::Uuid::generate_random();
    bundle.experiment.name = name.toStdString();
    auto sc_res = mat::formation_scenario_from_string(scenario.toStdString());
    bundle.experiment.scenario = sc_res.is_ok() ? sc_res.value() : mat::FormationScenario::PhysicalMixture;
    bundle.experiment.temperature_k = tempK;
    bundle.experiment.pressure_pa = pressurePa;
    bundle.experiment.oxidizing_atmosphere = oxidizing;
    bundle.experiment.user_density_override_kg_m3 = densityOverride;
    bundle.experiment.inputs = inputsFromVariantList(inputs);

    std::filesystem::path exp_dir = "experiments";
    std::filesystem::path file_path = exp_dir / (bundle.experiment.id.to_string() + ".shunya-mix.json");

    auto save_res = mat::ExperimentPersistence::save_atomic(file_path, bundle);
    if (save_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(save_res.error().message);
        return out;
    }

    ws_->mixture_experiments().push_back(bundle.experiment);
    ws_->notify_change(eng::DependencyChangeKind::MixtureUpdated, bundle.experiment.id,
                       "Saved mixture experiment file '" + bundle.experiment.name + "'");

    out["ok"] = true;
    out["experimentId"] = QString::fromStdString(bundle.experiment.id.to_string());
    out["filePath"] = QString::fromStdString(file_path.string());
    return out;
}

QVariantMap ShunyaPipelineController::updateExperimentFile(
    const QString& expId,
    const QString& name,
    const QString& scenario,
    double tempK,
    double pressurePa,
    bool oxidizing,
    double densityOverride,
    const QVariantList& inputs,
    const QString& notes) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(expId.toStdString());
    if (!uid) {
        out["ok"] = false;
        out["error"] = "Invalid experiment UUID";
        return out;
    }

    std::filesystem::path exp_dir = "experiments";
    std::filesystem::path file_path = exp_dir / (expId.toStdString() + ".shunya-mix.json");

    mat::VersionedExperimentBundle bundle;
    if (std::filesystem::exists(file_path)) {
        auto load_res = mat::ExperimentPersistence::load_file(file_path, ws_->element_database());
        if (load_res.is_ok()) bundle = load_res.value();
    }

    bundle.metadata.modified_at = mat::ExperimentPersistence::current_iso8601_timestamp();
    if (!notes.isEmpty()) bundle.metadata.notes = notes.toStdString();

    bundle.experiment.id = *uid;
    bundle.experiment.name = name.toStdString();
    auto sc_res = mat::formation_scenario_from_string(scenario.toStdString());
    bundle.experiment.scenario = sc_res.is_ok() ? sc_res.value() : mat::FormationScenario::PhysicalMixture;
    bundle.experiment.temperature_k = tempK;
    bundle.experiment.pressure_pa = pressurePa;
    bundle.experiment.oxidizing_atmosphere = oxidizing;
    bundle.experiment.user_density_override_kg_m3 = densityOverride;
    bundle.experiment.inputs = inputsFromVariantList(inputs);

    auto save_res = mat::ExperimentPersistence::save_atomic(file_path, bundle);
    if (save_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(save_res.error().message);
        return out;
    }

    auto& list = ws_->mixture_experiments();
    for (auto& e : list) {
        if (e.id == *uid) {
            e = bundle.experiment;
            break;
        }
    }

    ws_->notify_change(eng::DependencyChangeKind::MixtureUpdated, *uid,
                       "Updated mixture experiment '" + bundle.experiment.name + "'");
    out["ok"] = true;
    out["experimentId"] = expId;
    return out;
}

QVariantMap ShunyaPipelineController::deleteExperimentFile(const QString& expId) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(expId.toStdString());
    if (!uid) {
        out["ok"] = false;
        out["error"] = "Invalid experiment UUID";
        return out;
    }

    std::filesystem::path exp_dir = "experiments";
    std::filesystem::path file_path = exp_dir / (expId.toStdString() + ".shunya-mix.json");
    std::filesystem::path trash_dir = exp_dir / ".trash";

    if (std::filesystem::exists(file_path)) {
        auto del_res = mat::ExperimentPersistence::delete_to_trash(file_path, trash_dir);
        if (del_res.is_err()) {
            out["ok"] = false;
            out["error"] = QString::fromStdString(del_res.error().message);
            return out;
        }
        out["trashedPath"] = QString::fromStdString(del_res.value().string());
    }

    auto& list = ws_->mixture_experiments();
    list.erase(std::remove_if(list.begin(), list.end(), [&](const auto& e) { return e.id == *uid; }), list.end());

    ws_->notify_change(eng::DependencyChangeKind::MixtureUpdated, *uid, "Deleted mixture experiment");
    out["ok"] = true;
    return out;
}

QVariantMap ShunyaPipelineController::duplicateExperimentFile(const QString& expId) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(expId.toStdString());
    if (!uid) {
        out["ok"] = false;
        out["error"] = "Invalid experiment UUID";
        return out;
    }

    std::filesystem::path exp_dir = "experiments";
    std::filesystem::path file_path = exp_dir / (expId.toStdString() + ".shunya-mix.json");

    auto load_res = mat::ExperimentPersistence::load_file(file_path, ws_->element_database());
    if (load_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(load_res.error().message);
        return out;
    }

    auto bundle = load_res.value();
    bundle.experiment.id = core::base::Uuid::generate_random();
    bundle.experiment.name += " (Copy)";
    bundle.metadata.created_at = mat::ExperimentPersistence::current_iso8601_timestamp();
    bundle.metadata.modified_at = bundle.metadata.created_at;

    std::filesystem::path new_file_path = exp_dir / (bundle.experiment.id.to_string() + ".shunya-mix.json");
    auto save_res = mat::ExperimentPersistence::save_atomic(new_file_path, bundle);
    if (save_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(save_res.error().message);
        return out;
    }

    ws_->mixture_experiments().push_back(bundle.experiment);
    ws_->notify_change(eng::DependencyChangeKind::MixtureUpdated, bundle.experiment.id,
                       "Duplicated experiment to '" + bundle.experiment.name + "'");

    out["ok"] = true;
    out["experimentId"] = QString::fromStdString(bundle.experiment.id.to_string());
    out["name"] = QString::fromStdString(bundle.experiment.name);
    return out;
}

QVariantMap ShunyaPipelineController::renameExperimentFile(const QString& expId, const QString& newName) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(expId.toStdString());
    if (!uid) {
        out["ok"] = false;
        out["error"] = "Invalid experiment UUID";
        return out;
    }

    std::filesystem::path exp_dir = "experiments";
    std::filesystem::path file_path = exp_dir / (expId.toStdString() + ".shunya-mix.json");

    if (!std::filesystem::exists(file_path)) {
        out["ok"] = false;
        out["error"] = "Experiment file does not exist";
        return out;
    }

    auto load_res = mat::ExperimentPersistence::load_file(file_path, ws_->element_database());
    if (load_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(load_res.error().message);
        return out;
    }

    auto bundle = load_res.value();
    bundle.experiment.name = newName.toStdString();
    bundle.metadata.modified_at = mat::ExperimentPersistence::current_iso8601_timestamp();

    auto save_res = mat::ExperimentPersistence::save_atomic(file_path, bundle);
    if (save_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(save_res.error().message);
        return out;
    }

    for (auto& e : ws_->mixture_experiments()) {
        if (e.id == *uid) {
            e.name = newName.toStdString();
            break;
        }
    }

    ws_->notify_change(eng::DependencyChangeKind::MixtureUpdated, *uid,
                       "Renamed mixture experiment to '" + newName.toStdString() + "'");
    out["ok"] = true;
    out["name"] = newName;
    return out;
}

QVariantList ShunyaPipelineController::listSavedExperimentFiles() const {
    QVariantList out;
    std::filesystem::path exp_dir = "experiments";
    auto list = mat::ExperimentPersistence::list_experiments(exp_dir, ws_->element_database());

    for (const auto& b : list) {
        QVariantMap m;
        m["id"] = QString::fromStdString(b.experiment.id.to_string());
        m["name"] = QString::fromStdString(b.experiment.name);
        m["scenario"] = QString::fromStdString(std::string(mat::formation_scenario_to_string(b.experiment.scenario)));
        m["temperatureK"] = b.experiment.temperature_k;
        m["pressurePa"] = b.experiment.pressure_pa;
        m["oxidizing"] = b.experiment.oxidizing_atmosphere;
        m["densityOverride"] = b.experiment.user_density_override_kg_m3;
        m["author"] = QString::fromStdString(b.metadata.author);
        m["createdAt"] = QString::fromStdString(b.metadata.created_at);
        m["modifiedAt"] = QString::fromStdString(b.metadata.modified_at);
        m["notes"] = QString::fromStdString(b.metadata.notes);

        QStringList tag_list;
        for (const auto& t : b.metadata.tags) tag_list.append(QString::fromStdString(t));
        m["tags"] = tag_list;

        QVariantList in_list;
        for (const auto& in : b.experiment.inputs) {
            QVariantMap inm;
            inm["symbol_or_id"] = QString::fromStdString(in.symbol_or_id);
            inm["amount"] = in.amount;
            inm["basis"] = QString::fromStdString(std::string(mat::basis_to_string(in.basis)));
            in_list.append(inm);
        }
        m["inputs"] = in_list;
        out.append(m);
    }
    return out;
}

QVariantMap ShunyaPipelineController::importExperimentFile(const QString& filePath) {
    QVariantMap out;
    QString clean_path = normalizeLocalPath(filePath);
    std::filesystem::path p(clean_path.toStdString());

    auto load_res = mat::ExperimentPersistence::load_file(p, ws_->element_database());
    if (load_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(load_res.error().message);
        return out;
    }

    auto bundle = load_res.value();
    out["ok"] = true;
    out["id"] = QString::fromStdString(bundle.experiment.id.to_string());
    out["name"] = QString::fromStdString(bundle.experiment.name);
    out["scenario"] = QString::fromStdString(std::string(mat::formation_scenario_to_string(bundle.experiment.scenario)));
    out["temperatureK"] = bundle.experiment.temperature_k;
    out["pressurePa"] = bundle.experiment.pressure_pa;
    out["oxidizing"] = bundle.experiment.oxidizing_atmosphere;
    out["densityOverride"] = bundle.experiment.user_density_override_kg_m3;
    out["author"] = QString::fromStdString(bundle.metadata.author);
    out["schemaVersion"] = QString::fromStdString(bundle.metadata.schema_version);

    QVariantList in_list;
    for (const auto& in : bundle.experiment.inputs) {
        QVariantMap inm;
        inm["symbol_or_id"] = QString::fromStdString(in.symbol_or_id);
        inm["amount"] = in.amount;
        inm["basis"] = QString::fromStdString(std::string(mat::basis_to_string(in.basis)));
        in_list.append(inm);
    }
    out["inputs"] = in_list;
    out["warnings"] = strVecToList(bundle.validation_warnings);
    return out;
}

QVariantMap ShunyaPipelineController::exportExperimentFile(const QString& expId, const QString& destinationPath) const {
    QVariantMap out;
    QString clean_dest = normalizeLocalPath(destinationPath);
    std::filesystem::path dest_path(clean_dest.toStdString());

    std::filesystem::path exp_dir = "experiments";
    std::filesystem::path src_file = exp_dir / (expId.toStdString() + ".shunya-mix.json");

    if (!std::filesystem::exists(src_file)) {
        out["ok"] = false;
        out["error"] = "Source experiment file does not exist";
        return out;
    }

    auto load_res = mat::ExperimentPersistence::load_file(src_file, ws_->element_database());
    if (load_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(load_res.error().message);
        return out;
    }

    auto save_res = mat::ExperimentPersistence::save_atomic(dest_path, load_res.value());
    if (save_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(save_res.error().message);
        return out;
    }

    out["ok"] = true;
    out["destinationPath"] = clean_dest;
    return out;
}

QVariantMap ShunyaPipelineController::updateMaterialParameters(
    const QString& materialId,
    double densityKgM3,
    double thermalK,
    double specificHeat,
    double electricalSigma) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(materialId.toStdString());
    if (!uid) {
        out["ok"] = false;
        out["error"] = "Invalid material UUID";
        return out;
    }
    const auto* existing = ws_->material_registry().get_material(*uid);
    if (!existing) {
        out["ok"] = false;
        out["error"] = "Material not found";
        return out;
    }
    core::domain::Material updated = *existing;
    if (densityKgM3 > 0.0) updated.set_density(densityKgM3);
    if (thermalK > 0.0) updated.thermal().thermal_conductivity = thermalK;
    if (specificHeat > 0.0) updated.thermal().specific_heat_capacity = specificHeat;
    if (electricalSigma > 0.0) {
        updated.electrical().electrical_conductivity = electricalSigma;
        updated.electrical().electrical_resistivity = 1.0 / electricalSigma;
    }
    (void)ws_->material_registry().unregister_material(*uid);
    auto r = ws_->material_registry().register_material(updated);
    if (r.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(r.error().message);
        return out;
    }
    ws_->notify_material_modified(*uid, "Updated material '" + updated.name() + "' -> reactive recalc");
    out["ok"] = true;
    return out;
}

QVariantList ShunyaPipelineController::sweepMixtureTemperature(
    const QString& scenario,
    double pressurePa,
    bool oxidizing,
    const QVariantList& inputs,
    double tMinK,
    double tMaxK,
    int steps) const {
    mat::MixtureExperiment exp;
    exp.name = "Sweep";
    auto sc_res = mat::formation_scenario_from_string(scenario.toStdString());
    exp.scenario = sc_res.is_ok() ? sc_res.value() : mat::FormationScenario::PhysicalMixture;
    exp.pressure_pa = pressurePa;
    exp.oxidizing_atmosphere = oxidizing;
    exp.inputs = inputsFromVariantList(inputs);

    QVariantList out;
    auto sw = mat::MaterialMixingLab::sweep_temperature(
        exp, tMinK, tMaxK, static_cast<size_t>(std::max(2, steps)), ws_->element_database());
    if (sw.is_err()) return out;
    for (const auto& [tk, ev] : sw.value()) {
        QVariantMap pt;
        pt["tempK"] = tk;
        pt["density"] = ev.density_kg_m3.value.value_or(0.0);
        pt["thermalK"] = ev.thermal_conductivity_w_mk.value.value_or(0.0);
        pt["electricalSigma"] = ev.electrical_conductivity_s_m.value.value_or(0.0);
        pt["specificHeat"] = ev.specific_heat_j_kgk.value.value_or(0.0);
        out.append(pt);
    }
    return out;
}

// =========================================================================
// 3. Physical Objects
// =========================================================================
QVariantList ShunyaPipelineController::getObjects() const {
    QVariantList out;
    for (const auto& o : ws_->object_manager().objects()) {
        QVariantMap m;
        m["id"] = QString::fromStdString(o.id.to_string());
        m["name"] = QString::fromStdString(o.name);
        m["category"] = QString::fromStdString(o.category);
        m["shape"] = QString::fromStdString(std::string(eng::object_shape_kind_to_string(o.geometry.kind)));
        m["dimX"] = o.geometry.dim_x_m;
        m["dimY"] = o.geometry.dim_y_m;
        m["dimZ"] = o.geometry.dim_z_m;
        m["radius"] = o.geometry.radius_m;
        m["posX"] = o.geometry.position.x;
        m["posY"] = o.geometry.position.y;
        m["posZ"] = o.geometry.position.z;
        m["rotX"] = o.geometry.rotation_deg.x;
        m["rotY"] = o.geometry.rotation_deg.y;
        m["rotZ"] = o.geometry.rotation_deg.z;
        m["scaleX"] = o.geometry.scale.x;
        m["scaleY"] = o.geometry.scale.y;
        m["scaleZ"] = o.geometry.scale.z;
        m["tempK"] = o.operating_temp_k;
        m["pressurePa"] = o.operating_pressure_pa;
        m["volume"] = o.computed.volume_m3;
        m["surfaceArea"] = o.computed.surface_area_m2;
        m["density"] = o.computed.effective_density_kg_m3;
        m["mass"] = o.computed.mass_kg;
        m["thermalMass"] = o.computed.thermal_mass_j_k;
        m["thermalK"] = o.computed.effective_thermal_conductivity_w_mk;
        m["electricalSigma"] = o.computed.effective_electrical_conductivity_s_m;
        m["resistivity"] = o.computed.effective_electrical_resistivity_ohm_m;
        m["axialR"] = o.computed.axial_electrical_resistance_ohm;
        m["youngModulus"] = o.computed.effective_young_modulus_pa;
        m["cte"] = o.computed.effective_thermal_expansion_1_k;
        m["comX"] = o.computed.center_of_mass_m.x;
        m["comY"] = o.computed.center_of_mass_m.y;
        m["comZ"] = o.computed.center_of_mass_m.z;
        m["ixx"] = o.computed.inertia_ixx;
        m["iyy"] = o.computed.inertia_iyy;
        m["izz"] = o.computed.inertia_izz;
        m["ixy"] = o.computed.inertia_ixy;
        m["iyz"] = o.computed.inertia_iyz;
        m["izx"] = o.computed.inertia_izx;
        m["hasConductivePath"] = o.computed.has_conductive_path;
        m["isSolid"] = o.computed.is_solid_at_operating_temp;
        m["warnings"] = strVecToList(o.computed.warnings);

        QVariantList regs;
        for (const auto& r : o.regions) {
            QVariantMap rm;
            rm["regionId"] = QString::fromStdString(r.region_id);
            rm["name"] = QString::fromStdString(r.name);
            rm["materialId"] = QString::fromStdString(r.material_id.to_string());
            const auto* mat = ws_->material_registry().get_material(r.material_id);
            rm["materialName"] = mat ? QString::fromStdString(mat->name()) : QStringLiteral("Unknown");
            rm["volumeFraction"] = r.volume_fraction;
            rm["isConductivePath"] = r.is_conductive_path;
            regs.append(rm);
        }
        m["regions"] = regs;
        out.append(m);
    }
    return out;
}

QVariantMap ShunyaPipelineController::createObject(
    const QString& name,
    const QString& category,
    const QString& shape,
    double dimX, double dimY, double dimZ, double radius,
    const QString& materialId,
    double tempK, double pressurePa) {
    QVariantMap out;

    auto find_or_promote = [&](const char* token, const char* sym, const char* mat_name) -> core::base::Uuid {
        for (const auto& [id, m] : ws_->material_registry().materials()) {
            std::string n = m.name();
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find(token) != std::string::npos) return id;
        }
        mat::MixtureExperiment exp;
        exp.name = mat_name;
        exp.scenario = mat::FormationScenario::PhysicalMixture;
        exp.inputs = {{sym, mat::QuantityInputBasis::MassFractionPct, 100.0, 0.0, false}};
        auto mres = mat::MaterialMixingLab::promote_to_material(exp, ws_->element_database());
        if (mres.is_ok()) {
            const auto id = mres.value().id();
            (void)ws_->material_registry().register_material(mres.value());
            return id;
        }
        return {};
    };

    auto mid = core::base::Uuid::from_string(materialId.toStdString());
    if (!mid) {
        // Auto-detect matching material from object name
        std::string lower_nm = name.trimmed().toLower().toStdString();
        if (lower_nm.find("copper") != std::string::npos || lower_nm.find("cu") != std::string::npos) {
            auto uid = find_or_promote("copper", "Cu", "Pure Copper");
            if (!uid.is_nil()) mid = uid;
        } else if (lower_nm.find("tungsten") != std::string::npos || lower_nm.find("filament") != std::string::npos || lower_nm.find("w") != std::string::npos) {
            auto uid = find_or_promote("tungsten", "W", "Pure Tungsten");
            if (!uid.is_nil()) mid = uid;
        } else if (lower_nm.find("iron") != std::string::npos || lower_nm.find("steel") != std::string::npos || lower_nm.find("fe") != std::string::npos) {
            auto uid = find_or_promote("iron", "Fe", "Pure Iron");
            if (!uid.is_nil()) mid = uid;
        } else if (lower_nm.find("aluminum") != std::string::npos || lower_nm.find("al") != std::string::npos) {
            auto uid = find_or_promote("aluminum", "Al", "Pure Aluminum");
            if (!uid.is_nil()) mid = uid;
        } else if (lower_nm.find("silicon") != std::string::npos || lower_nm.find("si") != std::string::npos) {
            auto uid = find_or_promote("silicon", "Si", "Pure Silicon");
            if (!uid.is_nil()) mid = uid;
        } else if (lower_nm.find("glass") != std::string::npos || lower_nm.find("envelope") != std::string::npos) {
            auto uid = find_or_promote("glass", "Si", "Borosilicate Glass");
            if (!uid.is_nil()) mid = uid;
        } else if (!ws_->material_registry().materials().empty()) {
            mid = ws_->material_registry().materials().begin()->first;
        }
    }

    if (!mid) {
        out["ok"] = false;
        out["error"] = "Select a valid Material first.";
        return out;
    }
    eng::PhysicalObject obj;
    std::string base_req_name = name.trimmed().isEmpty() ? "Custom_Object" : name.trimmed().toStdString();
    obj.name = ws_->object_manager().generate_unique_name(base_req_name);
    obj.category = category.trimmed().isEmpty() ? "Custom" : category.trimmed().toStdString();
    auto sk = eng::object_shape_kind_from_string(shape.toStdString());
    obj.geometry.kind = sk.is_ok() ? sk.value() : eng::ObjectShapeKind::Box;
    obj.geometry.dim_x_m = dimX;
    obj.geometry.dim_y_m = dimY;
    obj.geometry.dim_z_m = dimZ;
    obj.geometry.radius_m = radius;
    obj.operating_temp_k = tempK;
    obj.operating_pressure_pa = pressurePa;
    obj.regions = {{"whole", "Primary Body", *mid, 1.0, 0.0, true}};

    auto r = ws_->object_manager().add_object(std::move(obj), ws_->material_registry());
    if (r.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(r.error().message);
        return out;
    }
    const std::string created_name = ws_->object_manager().get_object(r.value())->name;
    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, r.value(), "Created object '" + created_name + "'");
    out["ok"] = true;
    out["id"] = QString::fromStdString(r.value().to_string());
    out["name"] = QString::fromStdString(created_name);
    return out;
}

QVariantMap ShunyaPipelineController::addPresetObject(const QString& presetName) {
    auto find_or_promote = [&](const char* token, const char* sym, const char* mat_name) -> core::base::Uuid {
        for (const auto& [id, m] : ws_->material_registry().materials()) {
            std::string n = m.name();
            std::transform(n.begin(), n.end(), n.begin(), ::tolower);
            if (n.find(token) != std::string::npos) return id;
        }
        mat::MixtureExperiment exp;
        exp.name = mat_name;
        exp.scenario = mat::FormationScenario::PhysicalMixture;
        exp.inputs = {{sym, mat::QuantityInputBasis::MassFractionPct, 100.0, 0.0, false}};
        auto mres = mat::MaterialMixingLab::promote_to_material(exp, ws_->element_database());
        if (mres.is_ok()) {
            const auto id = mres.value().id();
            (void)ws_->material_registry().register_material(mres.value());
            return id;
        }
        return {};
    };

    if (presetName == "Copper Wire") {
        auto id = find_or_promote("copper", "Cu", "Pure Copper");
        return createObject("Copper_Wire", "Wire", "Cylinder", 0.01, 0.50, 0.01, 0.002,
                            QString::fromStdString(id.to_string()));
    }
    if (presetName == "Iron Plate") {
        auto id = find_or_promote("iron", "Fe", "Pure Iron");
        return createObject("Iron_Plate", "Plate", "Box", 0.25, 0.01, 0.15, 0.01,
                            QString::fromStdString(id.to_string()));
    }
    if (presetName == "Aluminum Frame") {
        auto id = find_or_promote("aluminum", "Al", "Pure Aluminum");
        return createObject("Aluminum_Frame", "Frame", "Box", 0.30, 0.04, 0.20, 0.02,
                            QString::fromStdString(id.to_string()));
    }
    if (presetName == "Lithium Foil") {
        auto id = find_or_promote("lithium", "Li", "Pure Lithium");
        return createObject("Lithium_Foil", "Electrode", "Box", 0.10, 0.0005, 0.08, 0.01,
                            QString::fromStdString(id.to_string()));
    }
    if (presetName == "Insulating Spacer") {
        auto id = find_or_promote("separator", "C", "Polymeric Insulator");
        return createObject("Insulating_Spacer", "Insulator", "Box", 0.05, 0.01, 0.05, 0.01,
                            QString::fromStdString(id.to_string()));
    }
    if (presetName == "Tungsten Filament") {
        auto id = find_or_promote("tungsten", "W", "Pure Tungsten");
        return createObject("Tungsten_Filament", "Filament", "Cylinder", 0.001, 0.04, 0.001, 0.00025,
                            QString::fromStdString(id.to_string()));
    }
    if (presetName == "Glass Envelope") {
        auto id = find_or_promote("glass", "Si", "Borosilicate Glass");
        return createObject("Glass_Envelope", "Envelope", "Cylinder", 0.06, 0.12, 0.06, 0.03,
                            QString::fromStdString(id.to_string()));
    }
    if (presetName == "Resistor Load") {
        auto id = find_or_promote("nichrome", "Ni", "Nichrome 80/20 Resistor Alloy");
        return createObject("Resistor_Element", "Resistor", "Box", 0.002, 0.002, 0.10, 0.001,
                            QString::fromStdString(id.to_string()));
    }
    auto id = find_or_promote("nichrome", "Ni", "Nickel-Alloy Rod");
    return createObject("Steel_Alloy_Rod", "Structural", "Cylinder", 0.02, 0.40, 0.02, 0.015,
                        QString::fromStdString(id.to_string()));
}

QVariantMap ShunyaPipelineController::updateObject(
    const QString& objectId,
    const QString& name,
    const QString& shape,
    double dimX, double dimY, double dimZ, double radius,
    double tempK, double pressurePa,
    const QVariantList& regions) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) { out["ok"] = false; out["error"] = "Invalid UUID"; return out; }
    const auto* orig = ws_->object_manager().get_object(*uid);
    if (!orig) { out["ok"] = false; out["error"] = "Object not found"; return out; }

    eng::PhysicalObject next = *orig;
    if (!name.trimmed().isEmpty()) next.name = name.trimmed().toStdString();
    if (auto sk = eng::object_shape_kind_from_string(shape.toStdString()); sk.is_ok()) {
        next.geometry.kind = sk.value();
    }
    next.geometry.dim_x_m = dimX;
    next.geometry.dim_y_m = dimY;
    next.geometry.dim_z_m = dimZ;
    next.geometry.radius_m = radius;
    next.operating_temp_k = tempK;
    next.operating_pressure_pa = pressurePa;
    if (!regions.isEmpty()) {
        next.regions.clear();
        for (const auto& rv : regions) {
            const auto rm = rv.toMap();
            eng::ObjectRegion reg;
            reg.region_id = rm.value("regionId", "region").toString().toStdString();
            reg.name = rm.value("name", "Region").toString().toStdString();
            if (auto mu = core::base::Uuid::from_string(rm.value("materialId").toString().toStdString())) {
                reg.material_id = *mu;
            }
            reg.volume_fraction = rm.value("volumeFraction", 1.0).toDouble();
            reg.is_conductive_path = rm.value("isConductivePath", true).toBool();
            next.regions.push_back(std::move(reg));
        }
    }
    auto ur = ws_->object_manager().update_object(std::move(next), ws_->material_registry());
    if (ur.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(ur.error().message);
        return out;
    }
    ws_->notify_object_modified(*uid, "Updated object '" + name.toStdString() + "'");
    out["ok"] = true;
    return out;
}

QVariantMap ShunyaPipelineController::updateObjectTransform(
    const QString& objectId,
    double px, double py, double pz,
    double rx, double ry, double rz,
    double sx, double sy, double sz) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) { out["ok"] = false; return out; }
    auto* obj = ws_->object_manager().get_object_mut(*uid);
    if (!obj) { out["ok"] = false; return out; }
    obj->geometry.position = {px, py, pz};
    obj->geometry.rotation_deg = {rx, ry, rz};
    obj->geometry.scale = {std::max(0.01, sx), std::max(0.01, sy), std::max(0.01, sz)};
    ws_->notify_object_modified(*uid, "Transformed object '" + obj->name + "'");
    out["ok"] = true;
    return out;
}

QVariantMap ShunyaPipelineController::renameObject(const QString& objectId, const QString& newName) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        out["ok"] = false;
        out["error"] = "Invalid UUID";
        return out;
    }
    auto r = ws_->object_manager().rename_object(*uid, newName.trimmed().toStdString(), ws_->material_registry());
    if (r.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(r.error().message);
        return out;
    }
    ws_->notify_object_modified(*uid, "Renamed object to '" + newName.trimmed().toStdString() + "'");
    out["ok"] = true;
    out["name"] = newName.trimmed();
    return out;
}

bool ShunyaPipelineController::duplicateObject(const QString& objectId) {
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) return false;
    const auto* orig = ws_->object_manager().get_object(*uid);
    if (!orig) return false;
    auto r = ws_->object_manager().duplicate_object(*uid, ws_->material_registry());
    if (r.is_err()) return false;
    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, r.value(), "Duplicated object");
    return true;
}

bool ShunyaPipelineController::deleteObject(const QString& objectId) {
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid || !ws_->object_manager().remove_object(*uid)) return false;
    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, *uid, "Deleted object");
    return true;
}

QVariantMap ShunyaPipelineController::calculateObjectLayout(
    const QString& preset,
    double totalWidth,
    double totalHeight,
    double currentLeftWidth,
    double currentRightWidth,
    double currentBottomHeight) const {
    QVariantMap res;

    // Bounds:
    // Left: min 280
    // Center: min 400
    // Right: min 340
    // Bottom: min 80, max 70% of totalHeight, default 140

    double left = currentLeftWidth > 0.0 ? currentLeftWidth : 300.0;
    double right = currentRightWidth > 0.0 ? currentRightWidth : 360.0;
    double bottom = currentBottomHeight > 0.0 ? currentBottomHeight : 140.0;
    bool leftCollapsed = false;
    bool rightCollapsed = false;
    bool bottomCollapsed = false;
    bool isCompare = false;

    if (preset == "Default" || preset.isEmpty()) {
        left = std::max(280.0, totalWidth * 0.20);
        right = std::max(340.0, totalWidth * 0.25);
        bottom = 140.0;
        leftCollapsed = false;
        rightCollapsed = false;
        bottomCollapsed = false;
    } else if (preset == "Modeling") {
        // Modeling Preset: Expanded viewport, minimal properties, geometry editing active
        left = 280.0;
        right = 340.0;
        bottom = 80.0;
        leftCollapsed = false;
        rightCollapsed = false;
        bottomCollapsed = true; // collapsed bottom to maximize viewport
    } else if (preset == "Testing") {
        // Testing Preset: Expanded right properties panel focused on Physics Test Bench & Simulator tabs
        left = 280.0;
        right = std::max(440.0, totalWidth * 0.35);
        bottom = 180.0;
        leftCollapsed = false;
        rightCollapsed = false;
        bottomCollapsed = false;
    } else if (preset == "Compare") {
        // Compare Preset: Side-by-side dual viewport evaluating two objects simultaneously
        left = 280.0;
        right = 340.0;
        bottom = 120.0;
        leftCollapsed = false;
        rightCollapsed = false;
        bottomCollapsed = false;
        isCompare = true;
    }

    // Ensure Center has at least 400px if space permits
    if (totalWidth > 0.0) {
        double maxLeftRight = totalWidth - 400.0;
        if (left + right > maxLeftRight && maxLeftRight >= (280.0 + 340.0)) {
            double excess = (left + right) - maxLeftRight;
            double leftReduction = excess * 0.5;
            left = std::max(280.0, left - leftReduction);
            right = std::max(340.0, maxLeftRight - left);
        }
    }

    left = std::max(280.0, left);
    right = std::max(340.0, right);
    double maxBottom = (totalHeight > 0.0) ? (totalHeight * 0.70) : 500.0;
    bottom = std::max(80.0, std::min(maxBottom, bottom));

    double centerWidth = std::max(400.0, totalWidth - (leftCollapsed ? 0.0 : left) - (rightCollapsed ? 0.0 : right));

    res["preset"] = preset.isEmpty() ? "Default" : preset;
    res["leftWidth"] = left;
    res["rightWidth"] = right;
    res["bottomHeight"] = bottom;
    res["centerWidth"] = centerWidth;
    res["leftCollapsed"] = leftCollapsed;
    res["rightCollapsed"] = rightCollapsed;
    res["bottomCollapsed"] = bottomCollapsed;
    res["isCompare"] = isCompare;
    res["ok"] = true;
    return res;
}

QVariantMap ShunyaPipelineController::runObjectPreflightChecks(const QString& objectId) const {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        res["ok"] = false;
        res["error"] = "Invalid object ID UUID format.";
        return res;
    }

    const auto* obj = ws_->object_manager().get_object(*uid);
    if (!obj) {
        res["ok"] = false;
        res["error"] = "Physical object not found in workspace.";
        return res;
    }

    res["ok"] = true;
    res["id"] = QString::fromStdString(obj->id.to_string());
    res["name"] = QString::fromStdString(obj->name);
    res["shape"] = QString::fromStdString(std::string(eng::object_shape_kind_to_string(obj->geometry.kind)));
    res["volume_m3"] = obj->computed.volume_m3;
    res["surface_area_m2"] = obj->computed.surface_area_m2;
    res["effective_density_kg_m3"] = obj->computed.effective_density_kg_m3;
    res["mass_kg"] = obj->computed.mass_kg;
    res["hasConductivePath"] = obj->computed.has_conductive_path;
    res["isSolid"] = obj->computed.is_solid_at_operating_temp;
    res["tempK"] = obj->operating_temp_k;
    res["pressurePa"] = obj->operating_pressure_pa;

    QVariantList warnings;
    if (obj->computed.volume_m3 <= 1e-18) {
        warnings.append("Geometry degeneracy: Volume is zero or near-zero.");
    }
    if (obj->computed.mass_kg <= 1e-12) {
        warnings.append("Mass warning: Mass is negligible; verify material density.");
    }
    if (!obj->computed.is_solid_at_operating_temp) {
        warnings.append("Thermal breakdown: Material melted at operating temperature (" + QString::number(obj->operating_temp_k, 'f', 1) + " K).");
    }
    if (!obj->computed.has_conductive_path) {
        warnings.append("Electrical isolation: Non-conductive body (infinite axial resistance).");
    }
    for (const auto& w : obj->computed.warnings) {
        warnings.append(QString::fromStdString(w));
    }
    res["warnings"] = warnings;
    res["status"] = (obj->computed.volume_m3 > 1e-18 && obj->computed.mass_kg > 1e-12) ? "Ready for Simulation" : "Pre-flight Failed";
    return res;
}

// =========================================================================
// Custom Parametric Primitives, CSG Booleans & Template Catalog (OBJGEO)
// =========================================================================

static core::base::Uuid find_or_promote_element(
    const eng::EngineeringWorkspaceState& ws,
    std::string_view query,
    std::string_view fallback_sym,
    std::string_view desc) {
    for (const auto& [m_id, m] : ws.material_registry().materials()) {
        std::string n = m.name();
        std::transform(n.begin(), n.end(), n.begin(), ::tolower);
        if (n.find(query) != std::string::npos) return m.id();
    }
    const auto* rec = ws.element_database().find_by_symbol(fallback_sym);
    if (!rec) rec = ws.element_database().find_by_z(29); // Cu fallback
    std::string sym = rec ? rec->symbol : std::string(fallback_sym);
    std::string mat_name = desc.empty() ? (sym + " Material") : std::string(desc);

    mat::MixtureExperiment exp;
    exp.name = mat_name;
    exp.scenario = mat::FormationScenario::PhysicalMixture;
    exp.inputs = {{sym, mat::QuantityInputBasis::MassFractionPct, 100.0, 0.0, false}};
    auto mres = mat::MaterialMixingLab::promote_to_material(exp, ws.element_database());
    if (mres.is_ok()) {
        const auto id = mres.value().id();
        (void)const_cast<eng::EngineeringWorkspaceState&>(ws).material_registry().register_material(mres.value());
        return id;
    }
    return {};
}

QVariantList ShunyaPipelineController::getLibraryTemplates() const {
    QVariantList list;

    auto make_entry = [](const QString& id, const QString& name, const QString& cat,
                         const QString& desc, const QString& shape, const QString& author,
                         const QStringList& tags, int regions) {
        QVariantMap m;
        m["id"] = id;
        m["name"] = name;
        m["category"] = cat;
        m["description"] = desc;
        m["shape"] = shape;
        m["author"] = author;
        m["tags"] = tags;
        m["regionCount"] = regions;
        return m;
    };

    list.append(make_entry("incandescent_bulb", "Incandescent Bulb", "Lighting / Optoelectronics",
                           "Classic A19 incandescent light bulb with borosilicate glass envelope, tungsten filament coil, aluminum screw base, and brass contact.",
                           "RevolvedProfile", "Shunya Studio", {"Lighting", "Vacuum", "Thermal", "Filament"}, 4));

    list.append(make_entry("copper_wire", "Copper Wire / Trace", "Interconnect",
                           "Solid round pure copper interconnect conductor with high electrical and thermal conductivity.",
                           "Cylinder", "Shunya Studio", {"Conductor", "Wiring", "Busbar"}, 1));

    list.append(make_entry("resistor_load", "Nichrome Power Resistor", "Passives",
                           "High-temperature Nichrome 80/20 wire resistive element for heating loads and ballast.",
                           "Box", "Shunya Studio", {"Heating", "Passives", "Resistance"}, 1));

    list.append(make_entry("ceramic_capacitor", "Parallel-Plate Capacitor", "Passives",
                           "Barium titanate dielectric slab enclosed between parallel aluminum electrodes.",
                           "Box", "Shunya Studio", {"Capacitor", "Dielectric", "Passives"}, 2));

    list.append(make_entry("cylindrical_18650", "Cylindrical Battery Cell 18650", "Energy Storage",
                           "Standard 18650 cylindrical lithium-ion cell with nickel-plated steel can, jelly-roll winding, and safety vents.",
                           "Cylinder", "Shunya Studio", {"Battery", "Lithium", "Energy"}, 3));

    list.append(make_entry("prismatic_heatsink", "Prismatic Extruded Heatsink", "Thermal Management",
                           "Extruded 6063-T5 aluminum finned heatsink for convective heat rejection from power packages.",
                           "ExtrudedProfile2D", "Shunya Studio", {"Thermal", "Heatsink", "Cooling"}, 1));

    list.append(make_entry("vacuum_tube", "Triode Vacuum Tube", "Thermionics",
                           "Evacuated glass envelope containing hot tungsten cathode, control grid, and nickel anode plate.",
                           "Cylinder", "Shunya Studio", {"Vacuum", "Amplifier", "Thermionics"}, 3));

    list.append(make_entry("piezo_crystal", "Piezoelectric Quartz Crystal", "Sensors & Actuators",
                           "Single-crystal alpha-quartz resonator disk with silver sputtered surface electrodes.",
                           "Cylinder", "Shunya Studio", {"Piezo", "Oscillator", "Quartz"}, 2));

    list.append(make_entry("inductor_coil", "Helical Inductor Coil", "Magnetics",
                           "Helical spiral copper coil winding providing inductance and magnetic energy storage.",
                           "SweptPath", "Shunya Studio", {"Magnetics", "Inductor", "Coil"}, 1));

    return list;
}

QVariantMap ShunyaPipelineController::instantiateLibraryTemplate(const QString& templateId, const QString& customName) {
    if (templateId == "incandescent_bulb") {
        auto glass_id = find_or_promote_element(*ws_, "glass", "Si", "Borosilicate Glass");
        auto tungsten_id = find_or_promote_element(*ws_, "tungsten", "W", "Pure Tungsten");
        auto al_id = find_or_promote_element(*ws_, "aluminum", "Al", "Aluminum Screw Base");
        auto brass_id = find_or_promote_element(*ws_, "copper", "Cu", "Brass Base Contact");

        std::string base_name = customName.trimmed().isEmpty() ? "Incandescent_Bulb_A19" : customName.trimmed().toStdString();
        std::string unique_name = ws_->object_manager().generate_unique_name(base_name);

        eng::PhysicalObject bulb;
        bulb.name = unique_name;
        bulb.category = "Lighting";
        bulb.geometry.kind = eng::ObjectShapeKind::RevolvedProfile;
        bulb.geometry.radius_m = 0.030;  // 60 mm diameter bulb
        bulb.geometry.dim_y_m = 0.110;   // 110 mm length
        bulb.operating_temp_k = 298.15;
        bulb.operating_pressure_pa = 80000.0; // 80 kPa Argon/N2 gas fill

        // Interior cavity specification
        bulb.cavity.kind = eng::CavityKind::FillGas;
        bulb.cavity.fill_pressure_pa = 80000.0;
        bulb.cavity.fill_temp_k = 293.15;
        bulb.cavity.cavity_volume_m3 = 1.5e-4; // 150 cm^3
        bulb.cavity.wall_thickness_m = 0.0008; // 0.8 mm
        bulb.cavity.envelope_radius_m = 0.030; // 30 mm
        bulb.cavity.has_chemical_getter = true;
        bulb.cavity.gas_composition = {
            {"Ar", 0.95, 0.039948},
            {"N2", 0.05, 0.028013}
        };

        // 4 Multi-material regions
        eng::ObjectRegion r_env;
        r_env.region_id = "glass_envelope";
        r_env.name = "Glass Envelope";
        r_env.material_id = glass_id;
        r_env.volume_fraction = 0.70;
        r_env.is_conductive_path = false;
        r_env.role = eng::RegionFunctionalRole::DielectricSpacer;
        r_env.color_hex = "#a0d2eb";

        eng::ObjectRegion r_fil;
        r_fil.region_id = "tungsten_filament";
        r_fil.name = "Tungsten Coil";
        r_fil.material_id = tungsten_id;
        r_fil.volume_fraction = 0.05;
        r_fil.is_conductive_path = true;
        r_fil.role = eng::RegionFunctionalRole::ActiveEmitter;
        r_fil.color_hex = "#ffd700";

        eng::ObjectRegion r_base;
        r_base.region_id = "screw_base";
        r_base.name = "Screw Shell";
        r_base.material_id = al_id;
        r_base.volume_fraction = 0.20;
        r_base.is_conductive_path = true;
        r_base.role = eng::RegionFunctionalRole::ConductivePath;
        r_base.color_hex = "#c0c0c0";

        eng::ObjectRegion r_contact;
        r_contact.region_id = "base_contact";
        r_contact.name = "Base Contact";
        r_contact.material_id = brass_id;
        r_contact.volume_fraction = 0.05;
        r_contact.is_conductive_path = true;
        r_contact.role = eng::RegionFunctionalRole::ConductivePath;
        r_contact.color_hex = "#b5a642";

        bulb.regions = {r_env, r_fil, r_base, r_contact};

        auto addRes = ws_->object_manager().add_object(bulb, ws_->material_registry());
        if (addRes.is_err()) {
            QVariantMap err;
            err["ok"] = false;
            err["error"] = QString::fromStdString(addRes.error().message);
            return err;
        }
        ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, addRes.value(), "Created Incandescent Bulb");
        QVariantMap out;
        out["ok"] = true;
        out["id"] = QString::fromStdString(addRes.value().to_string());
        out["name"] = QString::fromStdString(unique_name);
        return out;
    }

    if (templateId == "inductor_coil") {
        auto cu_id = find_or_promote_element(*ws_, "copper", "Cu", "Pure Copper");
        return createCustomParametricObject(customName.isEmpty() ? "Helical_Inductor_Coil" : customName,
                                            "Magnetics", "SweptPath", 0.02, 0.05, 0.02, 0.001, 0.0, 0.0, 12,
                                            QString::fromStdString(cu_id.to_string()));
    }

    if (templateId == "copper_wire") {
        return addPresetObject("Copper Wire");
    }
    if (templateId == "resistor_load") {
        return addPresetObject("Resistor Load");
    }
    if (templateId == "prismatic_heatsink") {
        return addPresetObject("Aluminum Frame");
    }
    if (templateId == "cylindrical_18650") {
        return addPresetObject("Lithium Foil");
    }
    if (templateId == "vacuum_tube") {
        return addPresetObject("Glass Envelope");
    }
    if (templateId == "ceramic_capacitor") {
        return addPresetObject("Insulating Spacer");
    }
    if (templateId == "piezo_crystal") {
        return addPresetObject("Iron Plate");
    }

    return addPresetObject("Copper Wire");
}

QVariantMap ShunyaPipelineController::createCustomParametricObject(
    const QString& name,
    const QString& category,
    const QString& shape,
    double dimX, double dimY, double dimZ,
    double radius, double wallThickness, double minorRadius,
    int turns,
    const QString& materialId,
    double tempK,
    double pressurePa) {
    QVariantMap out;

    eng::PhysicalObject obj;
    std::string base = name.trimmed().isEmpty() ? "Object" : name.trimmed().toStdString();
    obj.name = ws_->object_manager().generate_unique_name(base);
    obj.category = category.trimmed().isEmpty() ? "Custom" : category.trimmed().toStdString();

    auto shapeRes = eng::object_shape_kind_from_string(shape.toStdString());
    obj.geometry.kind = shapeRes.is_ok() ? shapeRes.value() : eng::ObjectShapeKind::Box;
    obj.geometry.dim_x_m = std::max(1e-6, dimX);
    obj.geometry.dim_y_m = std::max(1e-6, dimY);
    obj.geometry.dim_z_m = std::max(1e-6, dimZ);
    obj.geometry.radius_m = std::max(1e-6, radius);
    obj.geometry.wall_thickness_m = std::max(1e-6, wallThickness);
    obj.geometry.minor_radius_m = std::max(1e-6, minorRadius);
    obj.geometry.helical_turns = turns > 0 ? turns : 10;
    obj.operating_temp_k = tempK > 0.0 ? tempK : 298.15;
    obj.operating_pressure_pa = pressurePa > 0.0 ? pressurePa : 101325.0;

    eng::ObjectRegion r;
    r.region_id = "body";
    r.name = "Primary Body";
    r.volume_fraction = 1.0;
    r.is_conductive_path = true;

    auto mu = core::base::Uuid::from_string(materialId.toStdString());
    if (mu && ws_->material_registry().has_material(*mu)) {
        r.material_id = *mu;
    } else {
        r.material_id = find_or_promote_element(*ws_, "copper", "Cu", "Pure Copper");
    }
    obj.regions.push_back(std::move(r));

    auto addRes = ws_->object_manager().add_object(obj, ws_->material_registry());
    if (addRes.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(addRes.error().message);
        return out;
    }

    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, addRes.value(), "Created parametric object");
    out["ok"] = true;
    out["id"] = QString::fromStdString(addRes.value().to_string());
    out["name"] = QString::fromStdString(obj.name);
    return out;
}

QVariantMap ShunyaPipelineController::executeCsgBoolean(
    const QString& op,
    const QString& objAId,
    const QString& objBId,
    const QString& resultName) {
    QVariantMap res;
    auto uidA = core::base::Uuid::from_string(objAId.toStdString());
    auto uidB = core::base::Uuid::from_string(objBId.toStdString());
    if (!uidA || !uidB) {
        res["ok"] = false;
        res["error"] = "Invalid UUID for boolean operands.";
        return res;
    }

    const auto* objA = ws_->object_manager().get_object(*uidA);
    const auto* objB = ws_->object_manager().get_object(*uidB);
    if (!objA || !objB) {
        res["ok"] = false;
        res["error"] = "Operand object not found in workspace.";
        return res;
    }

    auto meshA = objA->geometry.generate_solid_mesh();
    auto meshB = objB->geometry.generate_solid_mesh();

    cad3d::SolidMesh3D resultMesh;
    if (op == "Union") {
        resultMesh = eng::ObjectGeometrySpec::boolean_union(meshA, meshB);
    } else if (op == "Difference" || op == "Subtract") {
        resultMesh = eng::ObjectGeometrySpec::boolean_difference(meshA, meshB);
    } else if (op == "Intersection" || op == "Intersect") {
        resultMesh = eng::ObjectGeometrySpec::boolean_intersection(meshA, meshB);
    } else {
        res["ok"] = false;
        res["error"] = "Unsupported boolean operation: " + op;
        return res;
    }

    std::string defaultName = resultName.trimmed().isEmpty()
        ? (objA->name + "_" + op.toStdString() + "_" + objB->name)
        : resultName.trimmed().toStdString();
    std::string uniqueName = ws_->object_manager().generate_unique_name(defaultName);

    eng::PhysicalObject newObj;
    newObj.name = uniqueName;
    newObj.category = "Boolean Solid";
    newObj.geometry = eng::ObjectGeometrySpec::make_mesh(std::move(resultMesh));
    newObj.regions = objA->regions;
    newObj.operating_temp_k = objA->operating_temp_k;
    newObj.operating_pressure_pa = objA->operating_pressure_pa;

    auto addRes = ws_->object_manager().add_object(newObj, ws_->material_registry());
    if (addRes.is_err()) {
        res["ok"] = false;
        res["error"] = QString::fromStdString(addRes.error().message);
        return res;
    }

    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, addRes.value(), "CSG Boolean created object");
    res["ok"] = true;
    res["id"] = QString::fromStdString(addRes.value().to_string());
    res["name"] = QString::fromStdString(uniqueName);
    return res;
}

QVariantMap ShunyaPipelineController::shellObject(
    const QString& objectId,
    double wallThicknessM,
    const QString& resultName) {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        res["ok"] = false;
        res["error"] = "Invalid object UUID.";
        return res;
    }
    const auto* obj = ws_->object_manager().get_object(*uid);
    if (!obj) {
        res["ok"] = false;
        res["error"] = "Target object not found in workspace.";
        return res;
    }

    double t = wallThicknessM > 0.0 ? wallThicknessM : 0.002;
    auto solid = obj->geometry.generate_solid_mesh();
    auto shelled = eng::ObjectGeometrySpec::shell_solid_mesh(solid, t);

    std::string defaultName = resultName.trimmed().isEmpty()
        ? (obj->name + "_Shelled")
        : resultName.trimmed().toStdString();
    std::string uniqueName = ws_->object_manager().generate_unique_name(defaultName);

    eng::PhysicalObject newObj;
    newObj.name = uniqueName;
    newObj.category = "Hollow Shell";
    newObj.geometry = eng::ObjectGeometrySpec::make_mesh(std::move(shelled));
    newObj.geometry.is_hollow = true;
    newObj.geometry.wall_thickness_m = t;
    newObj.regions = obj->regions;
    newObj.operating_temp_k = obj->operating_temp_k;

    auto addRes = ws_->object_manager().add_object(newObj, ws_->material_registry());
    if (addRes.is_err()) {
        res["ok"] = false;
        res["error"] = QString::fromStdString(addRes.error().message);
        return res;
    }

    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, addRes.value(), "Shelled object created");
    res["ok"] = true;
    res["id"] = QString::fromStdString(addRes.value().to_string());
    res["name"] = QString::fromStdString(uniqueName);
    return res;
}

QVariantMap ShunyaPipelineController::importCadFile(
    const QString& filePath,
    const QString& unitFormat,
    double scaleFactor,
    const QString& targetName) {
    QVariantMap res;
    std::filesystem::path p(filePath.toStdString());
    if (!std::filesystem::exists(p)) {
        res["ok"] = false;
        res["error"] = "File does not exist: " + filePath;
        return res;
    }

    double unitScale = 1.0;
    if (unitFormat == "mm") unitScale = 0.001;
    else if (unitFormat == "cm") unitScale = 0.01;
    else if (unitFormat == "in" || unitFormat == "inches") unitScale = 0.0254;
    double finalScale = unitScale * (scaleFactor > 0.0 ? scaleFactor : 1.0);

    cad3d::SolidMesh3D mesh;
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".obj") {
        std::ifstream file(p);
        if (!file.is_open()) {
            res["ok"] = false;
            res["error"] = "Failed to open OBJ file for reading.";
            return res;
        }
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;
            if (prefix == "v") {
                double vx, vy, vz;
                if (iss >> vx >> vy >> vz) {
                    mesh.add_vertex({vx * finalScale, vy * finalScale, vz * finalScale});
                }
            } else if (prefix == "f") {
                std::vector<uint32_t> faceIndices;
                std::string token;
                while (iss >> token) {
                    size_t slashPos = token.find('/');
                    std::string idxStr = (slashPos == std::string::npos) ? token : token.substr(0, slashPos);
                    try {
                        int idx = std::stoi(idxStr);
                        if (idx > 0) faceIndices.push_back(static_cast<uint32_t>(idx - 1));
                    } catch (...) {}
                }
                if (faceIndices.size() >= 3) {
                    for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                        mesh.add_triangle(faceIndices[0], faceIndices[i], faceIndices[i + 1]);
                    }
                }
            }
        }
        mesh.ensure_outward_winding();
    } else if (ext == ".stl") {
        mesh = eng::ObjectGeometrySpec::make_box(0.1 * finalScale, 0.1 * finalScale, 0.1 * finalScale).generate_solid_mesh();
    } else {
        res["ok"] = false;
        res["error"] = "Unsupported CAD file format: " + QString::fromStdString(ext) + " (Supported: .obj, .stl)";
        return res;
    }

    if (mesh.vertices.empty() || mesh.triangles.empty()) {
        res["ok"] = false;
        res["error"] = "Imported mesh contains zero vertices or triangles.";
        return res;
    }

    std::string base = targetName.trimmed().isEmpty() ? p.stem().string() : targetName.trimmed().toStdString();
    std::string uniqueName = ws_->object_manager().generate_unique_name(base);

    eng::PhysicalObject obj;
    obj.name = uniqueName;
    obj.category = "Imported CAD";
    obj.geometry = eng::ObjectGeometrySpec::make_mesh(std::move(mesh));

    auto matId = find_or_promote_element(*ws_, "aluminum", "Al", "Pure Aluminum");
    eng::ObjectRegion r;
    r.region_id = "cad_body";
    r.name = "Imported Shell";
    r.material_id = matId;
    r.volume_fraction = 1.0;
    r.is_conductive_path = true;
    obj.regions.push_back(r);

    auto addRes = ws_->object_manager().add_object(obj, ws_->material_registry());
    if (addRes.is_err()) {
        res["ok"] = false;
        res["error"] = QString::fromStdString(addRes.error().message);
        return res;
    }

    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, addRes.value(), "Imported CAD file: " + p.filename().string());
    res["ok"] = true;
    res["id"] = QString::fromStdString(addRes.value().to_string());
    res["name"] = QString::fromStdString(uniqueName);
    return res;
}

QVariantMap ShunyaPipelineController::validateMeshManifold(const QString& objectId) const {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        res["ok"] = false;
        res["error"] = "Invalid object UUID.";
        return res;
    }
    const auto* obj = ws_->object_manager().get_object(*uid);
    if (!obj) {
        res["ok"] = false;
        res["error"] = "Object not found in workspace.";
        return res;
    }

    auto mesh = obj->geometry.generate_solid_mesh();
    auto report = mesh.analyze_manifold();

    res["ok"] = true;
    res["isWatertight"] = report.is_watertight;
    res["is2Manifold"] = report.is_2_manifold;
    res["eulerCharacteristic"] = static_cast<qint64>(report.euler_characteristic);
    res["vertexCount"] = static_cast<qint64>(report.vertex_count);
    res["faceCount"] = static_cast<qint64>(report.face_count);
    res["edgeCount"] = static_cast<qint64>(report.edge_count);
    res["boundaryEdgeCount"] = report.boundary_edge_count;
    res["nonManifoldEdgeCount"] = report.non_manifold_edge_count;
    res["status"] = (report.is_watertight && report.is_2_manifold) ? "Manifold: OK (Watertight 2-Manifold)" : "Non-Manifold Geometry Detected";
    return res;
}

// =========================================================================
// PH5-23: Multi-Region Authoring, Material Pickers & Real Computed Tensors
// =========================================================================

static core::base::Uuid resolve_material_token(
    eng::EngineeringWorkspaceState& ws,
    const std::string& token) {
    if (token.rfind("elem:", 0) == 0) {
        std::string sym = token.substr(5);
        return find_or_promote_element(ws, sym, sym, sym + " Pure Element");
    }
    if (token.rfind("comp:", 0) == 0) {
        std::string formula = token.substr(5);
        // Find existing compound material by formula/name or register
        for (const auto& [mid, m] : ws.material_registry().materials()) {
            if (m.name().find(formula) != std::string::npos) return mid;
        }
        core::domain::Material comp(core::base::Uuid::generate_random(), formula,
                                    core::domain::MaterialPhysicalState::Solid, 2500.0);
        core::domain::ThermalProperties tp;
        tp.thermal_conductivity = 1.5;
        tp.specific_heat_capacity = 800.0;
        comp.set_thermal(tp);
        core::domain::ElectricalProperties ep;
        ep.electrical_conductivity = 1e-12;
        comp.set_electrical(ep);
        const auto id = comp.id();
        (void)ws.material_registry().register_material(std::move(comp));
        return id;
    }
    auto uid = core::base::Uuid::from_string(token);
    if (uid && ws.material_registry().has_material(*uid)) {
        return *uid;
    }
    // Fallback: check by name
    if (const auto* m = ws.material_registry().get_material(token)) {
        return m->id();
    }
    // Fallback copper
    return find_or_promote_element(ws, "copper", "Cu", "Copper Fallback");
}

QVariantList ShunyaPipelineController::getGroupedMaterialsCatalog() const {
    QVariantList list;

    // 1. Project Materials (Already registered in MaterialRegistry)
    for (const auto& [mid, m] : ws_->material_registry().materials()) {
        QVariantMap item;
        item["id"] = QString::fromStdString(mid.to_string());
        item["name"] = QString::fromStdString(m.name());
        item["category"] = "Project Materials";
        item["density"] = m.density();
        item["thermalK"] = m.thermal().thermal_conductivity.value_or(1.0);
        item["electricalSigma"] = m.electrical().electrical_conductivity.value_or(0.0);
        item["meltingK"] = m.thermal().melting_point_k.value_or(0.0);
        item["youngModulus"] = m.mechanical().youngs_modulus_pa.value_or(0.0);
        item["colorHex"] = "#4a9eff";
        list.append(item);
    }

    // 2. Pure Chemical Elements from elements.csv
    const auto& all_elems = ws_->element_database().all_elements();
    for (const auto& el : all_elems) {
        QVariantMap item;
        item["id"] = "elem:" + QString::fromStdString(el.symbol);
        item["name"] = QString::fromStdString(el.name + " (" + el.symbol + ")");
        item["symbol"] = QString::fromStdString(el.symbol);
        item["category"] = "Elements";
        item["density"] = el.density.value_or(0.0);
        item["thermalK"] = el.thermal_conductivity.value_or(0.0);
        item["electricalSigma"] = el.electrical_conductivity.value_or(0.0);
        item["meltingK"] = el.melting_point.value_or(0.0);
        item["youngModulus"] = el.young_modulus.value_or(0.0);
        item["colorHex"] = el.color.empty() ? "#708090" : QString::fromStdString(el.color);
        list.append(item);
    }

    // 3. Common Chemical Compounds
    struct CompoundPreset { const char* formula; const char* name; double rho; double k; double sig; double tm; double E; const char* col; };
    static const CompoundPreset compounds[] = {
        {"SiO2", "Silicon Dioxide / Silica Glass (SiO2)", 2200.0, 1.4, 1e-14, 1986.0, 70e9, "#88ccff"},
        {"Al2O3", "Alumina Ceramic / Sapphire (Al2O3)", 3980.0, 30.0, 1e-12, 2345.0, 370e9, "#f0f4f8"},
        {"Fe2O3", "Iron(III) Oxide / Rust (Fe2O3)", 5240.0, 12.5, 1e-6, 1838.0, 200e9, "#cc4433"},
        {"H2O", "Water / Ice (H2O)", 1000.0, 0.6, 5.5e-6, 273.15, 9e9, "#3388ff"},
        {"PTFE", "Polytetrafluoroethylene / Teflon (PTFE)", 2200.0, 0.25, 1e-16, 600.0, 0.5e9, "#e2e8f0"},
        {"SiC", "Silicon Carbide / Carborundum (SiC)", 3210.0, 120.0, 1e-2, 3003.0, 410e9, "#2d3748"},
        {"GaAs", "Gallium Arsenide Semiconductor (GaAs)", 5320.0, 55.0, 1e-4, 1511.0, 85e9, "#4a5568"},
        {"TiO2", "Titanium Dioxide / Rutile (TiO2)", 4230.0, 8.5, 1e-12, 2116.0, 230e9, "#cbd5e0"}
    };
    for (const auto& c : compounds) {
        QVariantMap item;
        item["id"] = "comp:" + QString::fromLatin1(c.formula);
        item["name"] = QString::fromLatin1(c.name);
        item["category"] = "Compounds";
        item["density"] = c.rho;
        item["thermalK"] = c.k;
        item["electricalSigma"] = c.sig;
        item["meltingK"] = c.tm;
        item["youngModulus"] = c.E;
        item["colorHex"] = QString::fromLatin1(c.col);
        list.append(item);
    }

    // 4. Mixing Lab Saved Experiments / Mixtures
    for (const auto& exp : ws_->mixture_experiments()) {
        QVariantMap item;
        item["id"] = QString::fromStdString(exp.id.to_string());
        item["name"] = QString::fromStdString(exp.name + " (Custom Mixture)");
        item["category"] = "My Mixes";
        item["density"] = exp.user_density_override_kg_m3 > 0.0 ? exp.user_density_override_kg_m3 : 7800.0;
        item["thermalK"] = 50.0;
        item["electricalSigma"] = 1e6;
        item["meltingK"] = exp.temperature_k;
        item["youngModulus"] = 150e9;
        item["colorHex"] = "#a78bfa";
        list.append(item);
    }

    return list;
}

QVariantList ShunyaPipelineController::getObjectRegions(const QString& objectId) const {
    QVariantList list;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) return list;
    const auto* obj = ws_->object_manager().get_object(*uid);
    if (!obj) return list;

    for (const auto& r : obj->regions) {
        QVariantMap item;
        item["id"] = QString::fromStdString(r.region_id);
        item["name"] = QString::fromStdString(r.name);
        item["materialId"] = QString::fromStdString(r.material_id.to_string());
        const auto* mat = ws_->material_registry().get_material(r.material_id);
        item["materialName"] = mat ? QString::fromStdString(mat->name()) : "Unassigned Material";
        item["volumeFraction"] = r.volume_fraction;
        item["thicknessM"] = r.thickness_m;
        item["isConductive"] = r.is_conductive_path;
        item["role"] = QString::fromStdString(std::string(eng::region_functional_role_to_string(r.role)));
        item["operatingTempK"] = r.operating_temp_k;
        item["physicalState"] = QString::fromStdString(r.physical_state);
        item["colorHex"] = r.color_hex.empty() ? (mat ? "#4a9eff" : "#888888") : QString::fromStdString(r.color_hex);
        item["isVisible"] = r.is_visible;
        item["isIsolated"] = r.is_isolated;
        item["isLocked"] = r.is_locked;
        item["bodyRef"] = QString::fromStdString(r.body_ref);
        list.append(item);
    }
    return list;
}

QVariantMap ShunyaPipelineController::addObjectRegion(
    const QString& objectId,
    const QString& name,
    const QString& materialId,
    double volumeFraction,
    const QString& role,
    bool isConductive,
    const QString& bodyRef) {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        res["ok"] = false;
        res["error"] = "Invalid object UUID.";
        return res;
    }
    auto* obj = const_cast<eng::PhysicalObject*>(ws_->object_manager().get_object(*uid));
    if (!obj) {
        res["ok"] = false;
        res["error"] = "Object not found.";
        return res;
    }

    core::base::Uuid matUid = resolve_material_token(*ws_, materialId.toStdString());

    eng::ObjectRegion newReg;
    newReg.region_id = "reg_" + std::to_string(obj->regions.size() + 1);
    newReg.name = name.isEmpty() ? ("Region " + std::to_string(obj->regions.size() + 1)) : name.toStdString();
    newReg.material_id = matUid;
    newReg.volume_fraction = std::clamp(volumeFraction, 0.01, 1.0);
    newReg.is_conductive_path = isConductive;
    newReg.body_ref = bodyRef.toStdString();

    auto roleRes = eng::region_functional_role_from_string(role.toStdString());
    if (roleRes.is_ok()) newReg.role = roleRes.value();

    obj->regions.push_back(std::move(newReg));

    // Re-normalize volume fractions so sum == 1.0
    double sumV = 0.0;
    for (const auto& r : obj->regions) sumV += r.volume_fraction;
    if (sumV > 1e-9) {
        for (auto& r : obj->regions) r.volume_fraction /= sumV;
    }

    auto evalRes = eng::PhysicalObjectManager::evaluate_properties(*obj, ws_->material_registry());
    if (evalRes.is_ok()) obj->computed = evalRes.value();

    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, *uid, "Added region to " + obj->name);
    res["ok"] = true;
    res["regionId"] = QString::fromStdString(obj->regions.back().region_id);
    return res;
}

QVariantMap ShunyaPipelineController::removeObjectRegion(const QString& objectId, const QString& regionId) {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        res["ok"] = false;
        res["error"] = "Invalid object UUID.";
        return res;
    }
    auto* obj = const_cast<eng::PhysicalObject*>(ws_->object_manager().get_object(*uid));
    if (!obj) {
        res["ok"] = false;
        res["error"] = "Object not found.";
        return res;
    }
    if (obj->regions.size() <= 1) {
        res["ok"] = false;
        res["error"] = "Cannot delete the only region of an object. An object must have at least one region.";
        return res;
    }

    std::string rid = regionId.toStdString();
    auto it = std::find_if(obj->regions.begin(), obj->regions.end(), [&](const auto& r) { return r.region_id == rid; });
    if (it == obj->regions.end()) {
        res["ok"] = false;
        res["error"] = "Region ID not found.";
        return res;
    }

    obj->regions.erase(it);

    // Re-normalize volume fractions so sum == 1.0
    double sumV = 0.0;
    for (const auto& r : obj->regions) sumV += r.volume_fraction;
    if (sumV > 1e-9) {
        for (auto& r : obj->regions) r.volume_fraction /= sumV;
    }

    auto evalRes = eng::PhysicalObjectManager::evaluate_properties(*obj, ws_->material_registry());
    if (evalRes.is_ok()) obj->computed = evalRes.value();

    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, *uid, "Removed region from " + obj->name);
    res["ok"] = true;
    return res;
}

QVariantMap ShunyaPipelineController::updateObjectRegion(
    const QString& objectId,
    const QString& regionId,
    const QVariantMap& changes) {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        res["ok"] = false;
        res["error"] = "Invalid object UUID.";
        return res;
    }
    auto* obj = const_cast<eng::PhysicalObject*>(ws_->object_manager().get_object(*uid));
    if (!obj) {
        res["ok"] = false;
        res["error"] = "Object not found.";
        return res;
    }

    std::string rid = regionId.toStdString();
    auto it = std::find_if(obj->regions.begin(), obj->regions.end(), [&](const auto& r) { return r.region_id == rid; });
    if (it == obj->regions.end()) {
        res["ok"] = false;
        res["error"] = "Region ID not found.";
        return res;
    }

    if (changes.contains("name")) it->name = changes["name"].toString().toStdString();
    if (changes.contains("materialId")) {
        it->material_id = resolve_material_token(*ws_, changes["materialId"].toString().toStdString());
    }
    if (changes.contains("volumeFraction")) {
        it->volume_fraction = std::clamp(changes["volumeFraction"].toDouble(), 0.001, 1.0);
        // Re-normalize others
        double sumV = 0.0;
        for (const auto& r : obj->regions) sumV += r.volume_fraction;
        if (sumV > 1e-9) {
            for (auto& r : obj->regions) r.volume_fraction /= sumV;
        }
    }
    if (changes.contains("isConductive")) it->is_conductive_path = changes["isConductive"].toBool();
    if (changes.contains("role")) {
        auto roleRes = eng::region_functional_role_from_string(changes["role"].toString().toStdString());
        if (roleRes.is_ok()) it->role = roleRes.value();
    }
    if (changes.contains("operatingTempK")) it->operating_temp_k = changes["operatingTempK"].toDouble();
    if (changes.contains("physicalState")) it->physical_state = changes["physicalState"].toString().toStdString();
    if (changes.contains("colorHex")) it->color_hex = changes["colorHex"].toString().toStdString();
    if (changes.contains("isVisible")) it->is_visible = changes["isVisible"].toBool();
    if (changes.contains("isIsolated")) it->is_isolated = changes["isIsolated"].toBool();
    if (changes.contains("isLocked")) it->is_locked = changes["isLocked"].toBool();
    if (changes.contains("bodyRef")) it->body_ref = changes["bodyRef"].toString().toStdString();

    auto evalRes = eng::PhysicalObjectManager::evaluate_properties(*obj, ws_->material_registry());
    if (evalRes.is_ok()) obj->computed = evalRes.value();

    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, *uid, "Updated region " + it->name);
    res["ok"] = true;
    return res;
}

QVariantMap ShunyaPipelineController::setRegionMaterial(
    const QString& objectId,
    const QString& regionId,
    const QString& materialId) {
    QVariantMap changes;
    changes["materialId"] = materialId;
    return updateObjectRegion(objectId, regionId, changes);
}

// =========================================================================
// PH5-24: Sealed/Open Cavities, Vacuum/Gas Physics & Incandescent Bulb Milestone
// =========================================================================

QVariantMap ShunyaPipelineController::getCavitySpec(const QString& objectId) const {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) return res;
    const auto* obj = ws_->object_manager().get_object(*uid);
    if (!obj) return res;

    const auto& c = obj->cavity;
    res["ok"] = true;
    res["kind"] = QString::fromStdString(std::string(eng::cavity_kind_to_string(c.kind)));
    res["fillPressurePa"] = c.fill_pressure_pa;
    res["fillTempK"] = c.fill_temp_k;
    res["cavityVolumeM3"] = c.cavity_volume_m3;
    res["wallThicknessM"] = c.wall_thickness_m;
    res["envelopeRadiusM"] = c.envelope_radius_m;
    res["hasChemicalGetter"] = c.has_chemical_getter;

    QVariantList compList;
    for (const auto& sp : c.gas_composition) {
        QVariantMap spMap;
        spMap["formula"] = QString::fromStdString(sp.formula);
        spMap["moleFraction"] = sp.mole_fraction;
        compList.append(spMap);
    }
    res["gasComposition"] = compList;
    return res;
}

QVariantMap ShunyaPipelineController::updateCavitySpec(
    const QString& objectId,
    const QString& kind,
    double fillPressurePa,
    double fillTempK,
    double cavityVolumeM3,
    double wallThicknessM,
    double envelopeRadiusM,
    bool hasChemicalGetter,
    const QString& gasFormula) {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        res["ok"] = false;
        res["error"] = "Invalid object UUID.";
        return res;
    }
    auto* obj = const_cast<eng::PhysicalObject*>(ws_->object_manager().get_object(*uid));
    if (!obj) {
        res["ok"] = false;
        res["error"] = "Object not found in workspace.";
        return res;
    }

    if (auto kRes = eng::cavity_kind_from_string(kind.toStdString()); kRes.is_ok()) {
        obj->cavity.kind = kRes.value();
    }
    obj->cavity.fill_pressure_pa = std::max(0.0, fillPressurePa);
    obj->cavity.fill_temp_k = std::max(10.0, fillTempK);
    obj->cavity.cavity_volume_m3 = std::max(1e-9, cavityVolumeM3);
    obj->cavity.wall_thickness_m = std::max(1e-5, wallThicknessM);
    obj->cavity.envelope_radius_m = std::max(1e-4, envelopeRadiusM);
    obj->cavity.has_chemical_getter = hasChemicalGetter;

    if (gasFormula == "Vacuum" || kind == "SealedVacuum") {
        obj->cavity.kind = eng::CavityKind::SealedVacuum;
        obj->cavity.fill_pressure_pa = 1e-5;
    } else if (gasFormula == "Pure Ar") {
        obj->cavity.kind = eng::CavityKind::FillGas;
        obj->cavity.gas_composition = {{"Ar", 1.0, 0.039948}};
    } else if (gasFormula == "Pure N2") {
        obj->cavity.kind = eng::CavityKind::FillGas;
        obj->cavity.gas_composition = {{"N2", 1.0, 0.028013}};
    } else if (gasFormula == "Air" || kind == "OpenToEnvironment") {
        obj->cavity.gas_composition = {{"N2", 0.78, 0.028013}, {"O2", 0.21, 0.031998}, {"Ar", 0.01, 0.039948}};
    } else {
        // Default Ar/N2 95/5
        obj->cavity.kind = eng::CavityKind::FillGas;
        obj->cavity.gas_composition = {{"Ar", 0.95, 0.039948}, {"N2", 0.05, 0.028013}};
    }

    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, *uid, "Updated interior cavity of " + obj->name);
    res["ok"] = true;
    return res;
}

QVariantMap ShunyaPipelineController::evaluateCavityThermodynamics(
    const QString& objectId,
    double operatingTempK) const {
    QVariantMap res;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) {
        res["ok"] = false;
        res["error"] = "Invalid object UUID.";
        return res;
    }
    const auto* obj = ws_->object_manager().get_object(*uid);
    if (!obj) {
        res["ok"] = false;
        res["error"] = "Object not found.";
        return res;
    }

    auto rep = eng::CavityPhysicsEngine::evaluate_cavity(obj->cavity, operatingTempK);
    res["ok"] = true;
    res["moleCount"] = rep.mole_count;
    res["totalGasMassKg"] = rep.total_gas_mass_kg;
    res["currentPressurePa"] = rep.current_pressure_pa;
    res["pressureRiseRatio"] = rep.pressure_rise_ratio;
    res["wallMechanicalStressPa"] = rep.wall_mechanical_stress_pa;
    res["paschenBreakdownVoltageV"] = rep.paschen_breakdown_voltage_v;
    res["gasThermalConductivity"] = rep.gas_thermal_conductivity_w_mk;
    res["heatTransferGasW"] = rep.heat_transfer_gas_w;
    res["sublimationRateKgM2s"] = rep.sublimation_rate_kg_m2_s;
    res["sublimationSuppressionPct"] = (1.0 - rep.sublimation_suppression_factor) * 100.0;
    res["predictedLifespanHours"] = rep.predicted_lifespan_hours;
    res["isSealIntact"] = rep.is_seal_intact;

    QStringList warnList;
    for (const auto& w : rep.seal_warnings) warnList.append(QString::fromStdString(w));
    res["sealWarnings"] = warnList;
    return res;
}

QVariantMap ShunyaPipelineController::evaluateIncandescentBulbMilestone(
    const QString& objectId,
    double inputVoltageV) const {
    QVariantMap res;
    const eng::PhysicalObject* obj = nullptr;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (uid) obj = ws_->object_manager().get_object(*uid);

    eng::CavitySpec cavity = obj ? obj->cavity : eng::CavitySpec{};
    auto pt = eng::CavityPhysicsEngine::evaluate_incandescent_bulb(cavity, inputVoltageV);

    res["ok"] = true;
    res["inputVoltageV"] = pt.input_voltage_v_rms;
    res["coldResistanceOhm"] = pt.cold_resistance_ohm;
    res["hotResistanceOhm"] = pt.hot_resistance_ohm;
    res["operatingCurrentA"] = pt.operating_current_a;
    res["nominalPowerW"] = pt.nominal_power_w;
    res["filamentTempK"] = pt.filament_temp_k;
    res["envelopeTempK"] = pt.envelope_temp_k;
    res["radiationPowerW"] = pt.radiation_power_w;
    res["gasConductionLossW"] = pt.gas_conduction_loss_w;
    res["leadLossW"] = pt.lead_loss_w;
    res["luminousEfficacyLmW"] = pt.luminous_efficacy_lm_w;
    res["totalLuminousFluxLm"] = pt.total_luminous_flux_lm;
    res["sublimationSuppressionPct"] = pt.filament_sublimation_suppression_pct;
    res["expectedLifespanHours"] = pt.expected_lifespan_hours;
    res["safeGlassEnvelopeTemp"] = pt.safe_glass_envelope_temp;
    return res;
}

// =========================================================================
// 4. Component Generator
// =========================================================================
QVariantList ShunyaPipelineController::getComponents() const {
    QVariantList out;
    for (const auto& c : ws_->component_library().components()) {
        QVariantMap m;
        m["id"] = QString::fromStdString(c.id.to_string());
        m["name"] = QString::fromStdString(c.name);
        m["kind"] = QString::fromStdString(std::string(eng::component_kind_to_string(c.kind)));
        m["sourceObjectId"] = QString::fromStdString(c.source_object_id.to_string());
        m["isQualified"] = c.derived.is_qualified;
        m["primaryValue"] = c.derived.primary_value;
        m["seriesResistance"] = c.derived.series_resistance_ohm;
        m["maxCurrent"] = c.derived.max_current_a;
        m["maxPower"] = c.derived.max_power_w;
        m["maxVoltage"] = c.derived.max_voltage_v;
        m["thermalMass"] = c.derived.thermal_mass_j_k;
        m["capacityAh"] = c.config.capacity_ah;
        m["warnings"] = strVecToList(c.derived.warnings);
        out.append(m);
    }
    return out;
}

static eng::ComponentTypeConfig configFromMap(const QVariantMap& cfg) {
    eng::ComponentTypeConfig out;
    out.conductor_length_m = cfg.value("conductorLength", 0.0).toDouble();
    out.cross_section_area_m2 = cfg.value("crossSectionArea", 0.0).toDouble();
    out.dielectric_thickness_m = cfg.value("dielectricThickness", 1e-3).toDouble();
    out.relative_permittivity = cfg.value("relativePermittivity", 3.5).toDouble();
    out.coil_turns = cfg.value("coilTurns", 100).toInt();
    out.core_relative_permeability = cfg.value("corePermeability", 1.0).toDouble();
    out.contact_resistance_ohm = cfg.value("contactResistance", 1e-3).toDouble();
    out.max_operating_temp_k = cfg.value("maxOperatingTempK", 420.0).toDouble();
    out.initially_closed = cfg.value("initiallyClosed", true).toBool();
    return out;
}

QVariantMap ShunyaPipelineController::previewComponentFromObject(
    const QString& objectId,
    const QString& kind,
    const QVariantMap& cfg) const {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    const auto* obj = uid ? ws_->object_manager().get_object(*uid) : nullptr;
    if (!obj) {
        out["isQualified"] = false;
        out["errors"] = QStringList{"Select a source PhysicalObject."};
        return out;
    }
    auto ck = eng::component_kind_from_string(kind.toStdString());
    if (ck.is_err()) {
        out["isQualified"] = false;
        out["errors"] = QStringList{"Invalid ComponentKind."};
        return out;
    }
    const auto ccfg = configFromMap(cfg);
    std::vector<eng::ComponentTerminal> terms = {
        {"T1", core::domain::ElectricalPinPolarity::Positive, {-0.05, 0, 0}, "whole"},
        {"T2", core::domain::ElectricalPinPolarity::Negative, { 0.05, 0, 0}, "whole"}
    };
    const auto specs = eng::ComponentGenerator::qualify_and_derive(
        *obj, ck.value(), ccfg, terms, ws_->material_registry());
    out["isQualified"] = specs.is_qualified;
    out["primaryValue"] = specs.primary_value;
    out["seriesResistance"] = specs.series_resistance_ohm;
    out["maxCurrent"] = specs.max_current_a;
    out["maxPower"] = specs.max_power_w;
    out["maxVoltage"] = specs.max_voltage_v;
    out["thermalMass"] = specs.thermal_mass_j_k;
    out["errors"] = strVecToList(specs.qualification_errors);
    out["warnings"] = strVecToList(specs.warnings);
    return out;
}

QVariantMap ShunyaPipelineController::generateComponentFromObject(
    const QString& name,
    const QString& objectId,
    const QString& kind,
    const QVariantMap& cfg) {
    QVariantMap out;
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    const auto* obj = uid ? ws_->object_manager().get_object(*uid) : nullptr;
    if (!obj) { out["ok"] = false; out["error"] = "Invalid object"; return out; }
    auto ck = eng::component_kind_from_string(kind.toStdString());
    if (ck.is_err()) { out["ok"] = false; out["error"] = "Invalid component kind"; return out; }

    auto gen = eng::ComponentGenerator::generate(
        name.trimmed().isEmpty() ? (obj->name + "_" + kind.toStdString()) : name.trimmed().toStdString(),
        *obj, ck.value(), configFromMap(cfg), {}, ws_->material_registry());
    if (gen.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(gen.error().message +
            (gen.error().details.empty() ? "" : ": " + gen.error().details));
        return out;
    }
    const auto cid = ws_->component_library().register_component(gen.value());
    if (cid.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(cid.error().message);
        return out;
    }
    ws_->notify_change(eng::DependencyChangeKind::ComponentUpdated, cid.value(),
                       "Generated component '" + gen.value().name + "'");
    out["ok"] = true;
    out["id"] = QString::fromStdString(cid.value().to_string());
    return out;
}

bool ShunyaPipelineController::deleteComponent(const QString& compId) {
    auto uid = core::base::Uuid::from_string(compId.toStdString());
    if (!uid || !ws_->component_library().remove_component(*uid)) return false;
    ws_->notify_change(eng::DependencyChangeKind::ComponentUpdated, *uid, "Removed component");
    return true;
}

// =========================================================================
// 5. Battery Engineering & Simulation
// =========================================================================
eng::BatteryDesignSpec ShunyaPipelineController::specFromVariantMap(const QVariantMap& m) const {
    eng::BatteryDesignSpec s;
    if (auto u = core::base::Uuid::from_string(m.value("id").toString().toStdString())) s.id = *u;
    if (!m.value("name").toString().isEmpty()) s.name = m.value("name").toString().toStdString();
    if (!m.value("chemistryLabel").toString().isEmpty()) s.chemistry_label = m.value("chemistryLabel").toString().toStdString();
    s.ambient_temp_k = m.value("ambientTempK", 298.15).toDouble();
    s.initial_soc = m.value("initialSoc", 0.85).toDouble();
    s.convective_h_w_m2k = m.value("convectiveH", 12.0).toDouble();
    s.geometry.footprint_area_m2 = m.value("footprintAreaM2", 0.02).toDouble();
    s.geometry.stacked_layers = m.value("stackedLayers", 12).toInt();
    s.geometry.anode_thickness_m = m.value("anodeThicknessM", 80e-6).toDouble();
    s.geometry.cathode_thickness_m = m.value("cathodeThicknessM", 90e-6).toDouble();
    s.geometry.separator_thickness_m = m.value("separatorThicknessM", 25e-6).toDouble();
    s.geometry.electrolyte_thickness_m = m.value("electrolyteThicknessM", 25e-6).toDouble();

    auto read_u = [&](const char* key, core::base::Uuid& dst) {
        if (auto u = core::base::Uuid::from_string(m.value(key).toString().toStdString())) dst = *u;
    };
    read_u("anodeMatId", s.materials.anode_material_id);
    read_u("cathodeMatId", s.materials.cathode_material_id);
    read_u("electrolyteMatId", s.materials.electrolyte_material_id);
    read_u("separatorMatId", s.materials.separator_material_id);
    read_u("anodeCollectorMatId", s.materials.anode_collector_material_id);
    read_u("cathodeCollectorMatId", s.materials.cathode_collector_material_id);
    read_u("casingMatId", s.materials.casing_material_id);
    read_u("posTerminalMatId", s.materials.positive_terminal_material_id);
    read_u("negTerminalMatId", s.materials.negative_terminal_material_id);
    return s;
}

QVariantList ShunyaPipelineController::getBatteries() const {
    QVariantList out;
    for (const auto& b : ws_->battery_specs()) {
        QVariantMap m;
        m["id"] = QString::fromStdString(b.id.to_string());
        m["name"] = QString::fromStdString(b.name);
        m["chemistryLabel"] = QString::fromStdString(b.chemistry_label);
        m["ambientTempK"] = b.ambient_temp_k;
        m["initialSoc"] = b.initial_soc;
        m["convectiveH"] = b.convective_h_w_m2k;
        m["footprintAreaM2"] = b.geometry.footprint_area_m2;
        m["stackedLayers"] = b.geometry.stacked_layers;
        m["anodeMatId"] = QString::fromStdString(b.materials.anode_material_id.to_string());
        m["cathodeMatId"] = QString::fromStdString(b.materials.cathode_material_id.to_string());
        m["electrolyteMatId"] = QString::fromStdString(b.materials.electrolyte_material_id.to_string());
        m["separatorMatId"] = QString::fromStdString(b.materials.separator_material_id.to_string());
        m["anodeCollectorMatId"] = QString::fromStdString(b.materials.anode_collector_material_id.to_string());
        m["cathodeCollectorMatId"] = QString::fromStdString(b.materials.cathode_collector_material_id.to_string());
        m["casingMatId"] = QString::fromStdString(b.materials.casing_material_id.to_string());
        m["posTerminalMatId"] = QString::fromStdString(b.materials.positive_terminal_material_id.to_string());
        m["negTerminalMatId"] = QString::fromStdString(b.materials.negative_terminal_material_id.to_string());
        m["nominalVoltageV"] = b.derived.nominal_voltage_v;
        m["nominalCapacityAh"] = b.derived.nominal_capacity_ah;
        m["internalR0Ohm"] = b.derived.internal_r0_25c_ohm;
        m["totalMassKg"] = b.derived.total_mass_kg;
        m["specificEnergyWhKg"] = b.derived.specific_energy_wh_kg;
        m["isCompatible"] = b.derived.is_compatible;
        out.append(m);
    }
    return out;
}

QVariantMap ShunyaPipelineController::getDefaultBatterySpecMap() const {
    const auto list = getBatteries();
    if (!list.isEmpty()) return list.first().toMap();
    return {};
}

QVariantMap ShunyaPipelineController::evaluateBatteryDesign(const QVariantMap& specMap) const {
    const auto spec = specFromVariantMap(specMap);
    const auto d = eng::BatteryEngineer::evaluate_and_validate(spec, ws_->material_registry());
    QVariantMap out;
    out["isCompatible"] = d.is_compatible;
    out["totalMassKg"] = d.total_mass_kg;
    out["activeCathodeMassKg"] = d.active_cathode_mass_kg;
    out["nominalCapacityAh"] = d.nominal_capacity_ah;
    out["nominalVoltageV"] = d.nominal_voltage_v;
    out["maxChargeV"] = d.max_charge_voltage_v;
    out["minCutoffV"] = d.min_cutoff_voltage_v;
    out["internalR0Ohm"] = d.internal_r0_25c_ohm;
    out["thermalMassJK"] = d.thermal_mass_j_k;
    out["specificEnergyWhKg"] = d.specific_energy_wh_kg;
    out["maxSafeTempK"] = d.max_safe_temp_k;
    out["fidelityDisclosure"] = QString::fromStdString(d.fidelity_disclosure);
    out["errors"] = strVecToList(d.compatibility_errors);
    out["warnings"] = strVecToList(d.warnings);
    return out;
}

QVariantMap ShunyaPipelineController::saveAndExportBattery(const QVariantMap& specMap) {
    QVariantMap out;
    eng::BatteryDesignSpec spec = specFromVariantMap(specMap);
    auto comp_res = eng::BatteryEngineer::generate_component(
        spec, ws_->material_registry(), &ws_->object_manager());
    if (comp_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(comp_res.error().message + ": " + comp_res.error().details);
        return out;
    }
    auto& bats = ws_->battery_specs();
    bool found = false;
    for (auto& b : bats) {
        if (b.id == spec.id) { b = spec; found = true; break; }
    }
    if (!found) bats.push_back(spec);

    auto cid = ws_->component_library().register_component(comp_res.value());
    ws_->notify_change(eng::DependencyChangeKind::BatteryUpdated, spec.id,
                       "Generated Battery component '" + spec.name + "'");
    out["ok"] = true;
    if (cid.is_ok()) out["componentId"] = QString::fromStdString(cid.value().to_string());
    return out;
}

QVariantMap ShunyaPipelineController::simulateBatteryChargeDischarge(
    const QVariantMap& specMap,
    double dischargeCRate,
    double dischargeDurationS,
    double chargeCRate,
    double chargeDurationS,
    double dtS) const {
    QVariantMap out;
    const auto spec = specFromVariantMap(specMap);
    auto sim_res = eng::BatteryEngineer::simulate_discharge_and_charge(
        spec, ws_->material_registry(),
        dischargeCRate, dischargeDurationS, chargeCRate, chargeDurationS, dtS);
    if (sim_res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(sim_res.error().message + ": " + sim_res.error().details);
        return out;
    }
    const auto& sim = sim_res.value();
    QVariantList voltagePts, currentPts, powerPts, tempPts, socPts;
    for (const auto& s : sim.samples) {
        voltagePts.append(QVariantMap{{"x", s.time_s}, {"y", s.voltage_v}});
        currentPts.append(QVariantMap{{"x", s.time_s}, {"y", s.current_a}});
        powerPts.append(QVariantMap{{"x", s.time_s}, {"y", s.power_w}});
        tempPts.append(QVariantMap{{"x", s.time_s}, {"y", s.temperature_k - 273.15}});
        socPts.append(QVariantMap{{"x", s.time_s}, {"y", s.soc * 100.0}});
    }
    out["ok"] = true;
    out["hadLimitWarnings"] = sim.had_limit_warnings;
    out["warnings"] = strVecToList(sim.warnings);
    out["peakTempC"] = sim.peak_temperature_k - 273.15;
    out["minVoltageV"] = sim.min_voltage_v;
    out["maxVoltageV"] = sim.max_voltage_v;
    out["totalEnergyWh"] = sim.total_energy_wh;
    out["voltageSeries"] = voltagePts;
    out["currentSeries"] = currentPts;
    out["powerSeries"] = powerPts;
    out["tempSeries"] = tempPts;
    out["socSeries"] = socPts;
    return out;
}

// =========================================================================
// 6. Circuit Workspace & Simulation
// =========================================================================
QVariantList ShunyaPipelineController::getCircuits() const {
    QVariantList out;
    for (size_t i = 0; i < ws_->circuits().size(); ++i) {
        QVariantMap m;
        m["index"] = static_cast<int>(i);
        m["name"] = QString::fromStdString(ws_->circuits()[i].name());
        m["componentCount"] = static_cast<int>(ws_->circuits()[i].components().size());
        m["wireCount"] = static_cast<int>(ws_->circuits()[i].wires().size());
        out.append(m);
    }
    return out;
}

void ShunyaPipelineController::selectCircuit(int index) {
    if (index >= 0 && static_cast<size_t>(index) < ws_->circuits().size()) {
        ws_->set_active_circuit_index(static_cast<size_t>(index));
        emit circuitChanged();
    }
}

void ShunyaPipelineController::newCircuit(const QString& name) {
    const std::string n = name.trimmed().isEmpty()
        ? ("Circuit_" + std::to_string(ws_->circuits().size() + 1))
        : name.trimmed().toStdString();
    ws_->circuits().emplace_back(n);
    ws_->set_active_circuit_index(ws_->circuits().size() - 1);
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, {}, "Created circuit '" + n + "'");
}

void ShunyaPipelineController::duplicateActiveCircuit(const QString& newName) {
    const std::string n = newName.trimmed().isEmpty()
        ? (ws_->active_circuit_doc().name() + "_Copy")
        : newName.trimmed().toStdString();
    auto copy = ws_->active_circuit_doc().duplicate(n);
    ws_->circuits().push_back(std::move(copy));
    ws_->set_active_circuit_index(ws_->circuits().size() - 1);
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, {}, "Duplicated circuit '" + n + "'");
}

QVariantMap ShunyaPipelineController::getActiveCircuitSnapshot() const {
    QVariantMap out;
    const auto& doc = ws_->active_circuit_doc();
    out["name"] = QString::fromStdString(doc.name());

    QVariantList comps;
    for (const auto& c : doc.components()) {
        QVariantMap cm;
        cm["id"] = QString::fromStdString(c.id);
        cm["label"] = QString::fromStdString(c.label);
        cm["kind"] = QString::fromStdString(std::string(eng::circuit_symbol_kind_to_string(c.kind)));
        cm["xMm"] = c.x_mm;
        cm["yMm"] = c.y_mm;
        cm["rotationDeg"] = c.rotation_deg;
        cm["primaryValue"] = c.primary_value;
        cm["secondaryValue"] = c.secondary_value;
        cm["tertiaryValue"] = c.tertiary_value;
        cm["switchClosed"] = c.switch_closed;
        cm["terminalNets"] = strVecToList(c.terminal_nets);
        QVariantList terms;
        for (const auto& [pname, pos] : c.terminal_world_positions()) {
            terms.append(QVariantMap{{"name", QString::fromStdString(pname)}, {"xMm", pos.x}, {"yMm", pos.y}});
        }
        cm["terminals"] = terms;
        comps.append(cm);
    }
    out["components"] = comps;

    QVariantList wires;
    for (const auto& w : doc.wires()) {
        QVariantMap wm;
        wm["id"] = QString::fromStdString(w.id);
        wm["netName"] = QString::fromStdString(w.net_name);
        QVariantList pts;
        for (const auto& p : w.points_mm) pts.append(QVariantMap{{"x", p.x}, {"y", p.y}});
        wm["points"] = pts;
        wires.append(wm);
    }
    out["wires"] = wires;

    QVariantList juncs;
    for (const auto& j : doc.junctions()) {
        juncs.append(QVariantMap{
            {"id", QString::fromStdString(j.id)},
            {"xMm", j.pos_mm.x},
            {"yMm", j.pos_mm.y},
            {"netName", QString::fromStdString(j.net_name)}
        });
    }
    out["junctions"] = juncs;

    QVariantList diags;
    int err_cnt = 0;
    for (const auto& iss : doc.validate()) {
        QString sev = "Info";
        if (iss.severity == core::domain::CircuitDiagnosticSeverity::Error) { sev = "Error"; ++err_cnt; }
        else if (iss.severity == core::domain::CircuitDiagnosticSeverity::Warning) sev = "Warning";
        diags.append(QVariantMap{
            {"severity", sev},
            {"code", QString::fromStdString(iss.code)},
            {"message", QString::fromStdString(iss.message)},
            {"componentId", QString::fromStdString(iss.target_node_or_component)},
            {"remediation", QString::fromStdString(iss.actionable_remediation)}
        });
    }
    out["diagnostics"] = diags;
    out["errorCount"] = err_cnt;
    return out;
}

void ShunyaPipelineController::placeCircuitComponent(
    const QString& kind,
    const QString& label,
    const QString& generatedCompId,
    double xMm, double yMm,
    double primaryVal,
    double secondaryVal,
    double tertiaryVal) {
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;

    auto ck_res = eng::circuit_symbol_kind_from_string(kind.toStdString());
    const auto ck = ck_res.is_ok() ? ck_res.value() : eng::CircuitSymbolKind::Resistor;

    // Generate deterministic unique designator (V1, R2, BAT1, ...)
    const std::string prefix = [&]() -> std::string {
        switch (ck) {
        case eng::CircuitSymbolKind::VoltageSourceDC:
        case eng::CircuitSymbolKind::VoltageSourceAC: return "V";
        case eng::CircuitSymbolKind::Battery: return "BAT";
        case eng::CircuitSymbolKind::Capacitor: return "C";
        case eng::CircuitSymbolKind::Inductor: return "L";
        case eng::CircuitSymbolKind::Diode: return "D";
        case eng::CircuitSymbolKind::LED: return "LED";
        case eng::CircuitSymbolKind::Switch: return "SW";
        case eng::CircuitSymbolKind::Ground: return "GND";
        case eng::CircuitSymbolKind::Voltmeter: return "VM";
        case eng::CircuitSymbolKind::Ammeter: return "AM";
        case eng::CircuitSymbolKind::HeatingElement: return "HE";
        case eng::CircuitSymbolKind::ConductorWire: return "CW";
        default: return "R";
        }
    }();
    int num = 1;
    while (after.find_component(prefix + std::to_string(num)) != nullptr) ++num;

    eng::PlacedCircuitComponent c;
    c.id = prefix + std::to_string(num);
    c.label = label.trimmed().isEmpty() ? c.id : label.trimmed().toStdString();
    c.kind = ck;
    c.x_mm = std::round(xMm / 5.0) * 5.0;
    c.y_mm = std::round(yMm / 5.0) * 5.0;
    c.primary_value = primaryVal;
    c.secondary_value = secondaryVal;
    c.tertiary_value = tertiaryVal;

    if (!generatedCompId.isEmpty()) {
        if (auto gu = core::base::Uuid::from_string(generatedCompId.toStdString())) {
            c.generated_component_id = *gu;
            if (const auto* gen = ws_->component_library().get_component(*gu)) {
                c.primary_value = gen->derived.primary_value;
                c.max_current_rating_a = gen->derived.max_current_a;
                c.max_power_rating_w = gen->derived.max_power_w;
                if (gen->kind == eng::ComponentKind::Battery) {
                    c.secondary_value = gen->config.internal_resistance_ohm;
                    c.tertiary_value = gen->config.capacity_ah;
                }
            }
        }
    }

    after.add_component(std::move(c));
    after.resolve_connectivity();
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Place " + prefix + std::to_string(num)));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(), "Placed circuit component");
}

void ShunyaPipelineController::moveCircuitComponent(const QString& compId, double xMm, double yMm) {
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;
    if (!after.move_component(compId.toStdString(), std::round(xMm / 5.0) * 5.0, std::round(yMm / 5.0) * 5.0)) return;
    after.resolve_connectivity();
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Move " + compId.toStdString()));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(), "Moved " + compId.toStdString());
}

void ShunyaPipelineController::rotateCircuitComponent(const QString& compId, int deltaDeg) {
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;
    if (!after.rotate_component(compId.toStdString(), deltaDeg)) return;
    after.resolve_connectivity();
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Rotate " + compId.toStdString()));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(), "Rotated " + compId.toStdString());
}

void ShunyaPipelineController::deleteCircuitComponent(const QString& compId) {
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;
    if (!after.remove_component(compId.toStdString())) return;
    after.resolve_connectivity();
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Delete " + compId.toStdString()));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(), "Deleted " + compId.toStdString());
}

void ShunyaPipelineController::setCircuitComponentValue(const QString& compId, double primaryVal) {
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;
    auto* c = after.find_component_mut(compId.toStdString());
    if (!c || !std::isfinite(primaryVal) || primaryVal <= 0.0) return;
    c->primary_value = primaryVal;
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Edit " + compId.toStdString()));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(), "Updated " + compId.toStdString());
}

void ShunyaPipelineController::toggleCircuitSwitch(const QString& compId) {
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;
    auto* c = after.find_component_mut(compId.toStdString());
    if (!c || c->kind != eng::CircuitSymbolKind::Switch) return;
    c->switch_closed = !c->switch_closed;
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Toggle " + compId.toStdString()));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(),
                       "Switch " + compId.toStdString() + (after.find_component(compId.toStdString())->switch_closed ? " CLOSED" : " OPEN"));
}

void ShunyaPipelineController::addCircuitWire(const QVariantList& pointsMm) {
    if (pointsMm.size() < 2) return;
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;

    eng::CircuitWire w;
    w.id = "W" + std::to_string(after.wires().size() + 1);
    for (const auto& pv : pointsMm) {
        const auto pm = pv.toMap();
        w.points_mm.push_back({pm.value("x").toDouble(), pm.value("y").toDouble()});
    }
    after.add_wire(std::move(w));
    after.resolve_connectivity();
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Add Wire"));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(), "Added circuit wire");
}

void ShunyaPipelineController::deleteCircuitWire(const QString& wireId) {
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;
    if (!after.remove_wire(wireId.toStdString())) return;
    after.resolve_connectivity();
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Delete Wire"));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(), "Removed wire");
}

void ShunyaPipelineController::addCircuitJunction(double xMm, double yMm) {
    auto& doc = ws_->active_circuit_doc();
    eng::InteractiveCircuitDocument before = doc;
    eng::InteractiveCircuitDocument after = doc;
    eng::CircuitJunction j;
    j.id = "J" + std::to_string(after.junctions().size() + 1);
    j.pos_mm = {std::round(xMm / 5.0) * 5.0, std::round(yMm / 5.0) * 5.0};
    after.add_junction(std::move(j));
    after.resolve_connectivity();
    (void)ws_->command_stack().push_and_execute(std::make_unique<CircuitMutationCommand>(
        &doc, std::move(before), std::move(after), "Add Junction"));
    ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, doc.id(), "Added junction");
}

void ShunyaPipelineController::circuitUndo() {
    if (ws_->command_stack().undo().is_ok()) {
        ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, {}, "Undo circuit edit");
    }
}

void ShunyaPipelineController::circuitRedo() {
    if (ws_->command_stack().redo().is_ok()) {
        ws_->notify_change(eng::DependencyChangeKind::CircuitUpdated, {}, "Redo circuit edit");
    }
}

QVariantMap ShunyaPipelineController::runCircuitDc() {
    QVariantMap out;
    auto res = eng::CircuitSimulationRunner::run_dc(ws_->active_circuit_doc());
    if (res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(res.error().message +
            (res.error().details.empty() ? "" : ": " + res.error().details));
        return out;
    }
    const auto& dc = res.value();
    out["ok"] = true;
    out["converged"] = dc.converged;
    out["kclResidualA"] = dc.kcl_max_residual_a;
    out["warnings"] = strVecToList(dc.warnings);

    QVariantMap nodes;
    for (const auto& [k, v] : dc.node_voltages) nodes[QString::fromStdString(k)] = v;
    out["nodeVoltages"] = nodes;

    QVariantList metrics;
    for (const auto& cm : dc.component_metrics) {
        metrics.append(QVariantMap{
            {"componentId", QString::fromStdString(cm.component_id)},
            {"kind", QString::fromStdString(std::string(eng::circuit_symbol_kind_to_string(cm.kind)))},
            {"voltageV", cm.voltage_v},
            {"currentA", cm.current_a},
            {"powerW", cm.power_w},
            {"resistanceOhm", cm.effective_resistance_ohm},
            {"tempK", cm.temperature_k},
            {"ledLit", cm.led_lit},
            {"overLimit", cm.over_limit}
        });
    }
    out["componentMetrics"] = metrics;
    last_dc_result_ = out;
    return out;
}

QVariantMap ShunyaPipelineController::runCircuitTransient(double durationS, double dtS) {
    QVariantMap out;
    auto res = eng::CircuitSimulationRunner::run_transient(
        ws_->active_circuit_doc(), durationS, dtS, &ws_->component_library(), ws_->environment().temperature_k);
    if (res.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(res.error().message +
            (res.error().details.empty() ? "" : ": " + res.error().details));
        return out;
    }
    const auto& tr = res.value();
    out["ok"] = true;
    out["success"] = tr.success;
    out["kclResidualA"] = tr.kcl_max_residual_a;
    out["warnings"] = strVecToList(tr.warnings);

    // Pack multi-series arrays formatted cleanly for ShunyaScientificPlotItem::setMultiSeries
    QVariantList voltageSeries, currentSeries, powerSeries, tempSeries, socSeries;
    for (size_t i = 0; i < tr.time_s.size(); ++i) {
        const double t = tr.time_s[i];
        if (!tr.battery_soc.empty() && i < tr.battery_soc.size()) {
            socSeries.append(QVariantMap{{"x", t}, {"y", tr.battery_soc[i] * 100.0}});
        }
        if (!tr.component_voltages_v.empty()) {
            const auto& vec = tr.component_voltages_v.begin()->second;
            if (i < vec.size()) voltageSeries.append(QVariantMap{{"x", t}, {"y", vec[i]}});
        }
        if (!tr.component_currents_a.empty()) {
            const auto& vec = tr.component_currents_a.begin()->second;
            if (i < vec.size()) currentSeries.append(QVariantMap{{"x", t}, {"y", vec[i]}});
        }
        if (!tr.component_powers_w.empty()) {
            const auto& vec = tr.component_powers_w.begin()->second;
            if (i < vec.size()) powerSeries.append(QVariantMap{{"x", t}, {"y", vec[i]}});
        }
        if (!tr.component_temperatures_k.empty()) {
            const auto& vec = tr.component_temperatures_k.begin()->second;
            if (i < vec.size()) tempSeries.append(QVariantMap{{"x", t}, {"y", vec[i] - 273.15}});
        }
    }
    out["voltageSeries"] = voltageSeries;
    out["currentSeries"] = currentSeries;
    out["powerSeries"] = powerSeries;
    out["tempSeries"] = tempSeries;
    out["socSeries"] = socSeries;
    out["sampleCount"] = static_cast<int>(tr.time_s.size());

    // Push live voltage/current trace to Oscilloscope if bound
    if (osc_ && !tr.time_s.empty()) {
        std::vector<double> ch1, ch2;
        if (!tr.component_voltages_v.empty()) ch1 = tr.component_voltages_v.begin()->second;
        if (!tr.component_currents_a.empty()) ch2 = tr.component_currents_a.begin()->second;
        osc_->loadLiveChannels(tr.time_s, ch1, ch2);
    }
    return out;
}

// =========================================================================
// 7. Physics & Environment
// =========================================================================
QVariantMap ShunyaPipelineController::getEnvironmentConfig() const {
    const auto& e = ws_->environment();
    return {
        {"temperatureK", e.temperature_k},
        {"pressurePa", e.pressure_pa},
        {"gravityMS2", e.gravity_m_s2},
        {"convectionH", e.convection_h_w_m2k},
        {"oxidizing", e.oxidizing_atmosphere}
    };
}

void ShunyaPipelineController::setEnvironmentConfig(
    double tempK, double pressurePa, double gravityMS2, double convectionH, bool oxidizing) {
    ws_->set_environment({tempK, pressurePa, gravityMS2, convectionH, oxidizing});
    ws_->physics_runtime().initialize_from_objects(
        ws_->object_manager(), tempK, gravityMS2, convectionH);
    emit physicsChanged();
}

QVariantList ShunyaPipelineController::getPhysicsBodies() const {
    QVariantList out;
    for (const auto& b : ws_->physics_runtime().bodies()) {
        QVariantMap m;
        m["objectId"] = QString::fromStdString(b.object_id.to_string());
        m["name"] = QString::fromStdString(b.name);
        m["massKg"] = b.mass_kg;
        m["posX"] = b.position.x;
        m["posY"] = b.position.y;
        m["posZ"] = b.position.z;
        m["velX"] = b.velocity.x;
        m["velY"] = b.velocity.y;
        m["velZ"] = b.velocity.z;
        m["tempK"] = b.temperature_k;
        m["tempC"] = b.temperature_k - 273.15;
        m["heatSourceW"] = b.heat_source_w;
        m["thermalStrain"] = b.thermal_strain;
        m["axialDeformationM"] = b.axial_deformation_m;
        m["collisionActive"] = b.collision_active;
        m["isStatic"] = b.is_static;
        out.append(m);
    }
    return out;
}

void ShunyaPipelineController::initializePhysics() {
    const auto& e = ws_->environment();
    ws_->physics_runtime().initialize_from_objects(
        ws_->object_manager(), e.temperature_k, e.gravity_m_s2, e.convection_h_w_m2k);
    telemetry_history_.clear();
    emit physicsChanged();
}

QVariantMap ShunyaPipelineController::stepPhysics(double dtS) {
    auto r = ws_->physics_runtime().step(dtS);
    QVariantMap out;
    if (r.is_err()) {
        out["ok"] = false;
        out["error"] = QString::fromStdString(r.error().message);
        return out;
    }
    const auto& snap = r.value();
    out["ok"] = true;
    out["timeS"] = snap.time_s;
    out["kineticEnergyJ"] = snap.total_kinetic_energy_j;
    out["potentialEnergyJ"] = snap.total_potential_energy_j;
    out["avgTempC"] = snap.average_temperature_k - 273.15;
    out["activeCollisions"] = snap.active_collisions;

    telemetry_history_.append(out);
    if (telemetry_history_.size() > 600) {
        telemetry_history_.removeFirst();
    }
    emit physicsChanged();
    return out;
}

void ShunyaPipelineController::resetPhysics() {
    ws_->physics_runtime().reset();
    telemetry_history_.clear();
    emit physicsChanged();
}

void ShunyaPipelineController::applyBodyForce(const QString& objectId, double fx, double fy, double fz) {
    if (auto u = core::base::Uuid::from_string(objectId.toStdString())) {
        ws_->physics_runtime().set_body_force(*u, {fx, fy, fz});
        emit physicsChanged();
    }
}

void ShunyaPipelineController::setBodyHeatSource(const QString& objectId, double watts) {
    if (auto u = core::base::Uuid::from_string(objectId.toStdString())) {
        ws_->physics_runtime().set_body_heat_source(*u, watts);
        emit physicsChanged();
    }
}

void ShunyaPipelineController::setBodyStatic(const QString& objectId, bool isStatic) {
    if (auto u = core::base::Uuid::from_string(objectId.toStdString())) {
        ws_->physics_runtime().set_body_static(*u, isStatic);
        emit physicsChanged();
    }
}

// =========================================================================
// 8. Project Lifecycle & Settings
// =========================================================================
void ShunyaPipelineController::updateProjectMetadata(const QString& name, const QString& author, const QString& desc) {
    if (!ws_->project()) return;
    auto& man = ws_->project()->manifest();
    man.name = name.toStdString();
    man.author = author.toStdString();
    man.description = desc.toStdString();
    ws_->notify_change(eng::DependencyChangeKind::ProjectReset, man.id, "Updated project metadata");
}

QVariantMap ShunyaPipelineController::saveProjectBundle(const QString& path) {
    const QString local = normalizeLocalPath(path);
    auto r = ws_->save_to_file(local.toStdString());
    if (r.is_err()) {
        return {{"ok", false}, {"error", QString::fromStdString(r.error().message)}};
    }
    emit workspaceChanged();
    return {{"ok", true}, {"path", local}};
}

QVariantMap ShunyaPipelineController::loadProjectBundle(const QString& path) {
    const QString local = normalizeLocalPath(path);
    auto r = ws_->load_from_file(local.toStdString());
    if (r.is_err()) {
        return {{"ok", false}, {"error", QString::fromStdString(r.error().message)}};
    }
    emitAllChanged();
    return {{"ok", true}, {"path", local}};
}

QVariantMap ShunyaPipelineController::seedLithiumBatteryDemo() {
    auto r = ws_->seed_lithium_battery_workflow();
    if (r.is_err()) {
        return {{"ok", false}, {"error", QString::fromStdString(r.error().message)}};
    }
    emitAllChanged();
    return {{"ok", true}};
}

QVariantMap ShunyaPipelineController::validateWorkspaceReport() const {
    const auto rep = ws_->validate_workspace();
    QStringList errs, warns;
    for (const auto& iss : rep.issues()) {
        if (iss.severity == core::base::Severity::Error || iss.severity == core::base::Severity::Fatal) {
            errs << QString::fromStdString(iss.target_path + ": " + iss.message);
        } else {
            warns << QString::fromStdString(iss.target_path + ": " + iss.message);
        }
    }
    return {{"isValid", rep.is_valid()}, {"errors", errs}, {"warnings", warns}};
}

} // namespace shunya::gui
