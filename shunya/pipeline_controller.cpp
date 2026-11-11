#include "pipeline_controller.hpp"
#include "oscilloscope_controller.hpp"
#include "viewport_3d_item.hpp"

#include <QFileInfo>
#include <QUrl>
#include <algorithm>
#include <cmath>
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
    if (!r) return res;

    res["atomicNumber"] = r->atomic_number;
    res["symbol"] = QString::fromStdString(r->symbol);
    res["iupacSymbol"] = QString::fromStdString(r->iupac_symbol);
    res["name"] = QString::fromStdString(r->name);
    res["phase"] = QString::fromStdString(r->phase);
    res["block"] = QString::fromStdString(r->block);
    res["notes"] = strVecToList(r->normalization_notes);

    QVariantList cols;
    for (const auto& col : ws_->element_database().csv_columns()) {
        QVariantMap item;
        item["column"] = QString::fromStdString(col);
        auto it = r->raw_csv_fields.find(col);
        const std::string raw = (it != r->raw_csv_fields.end()) ? it->second : "";
        item["rawValue"] = raw.empty() ? QStringLiteral("--") : QString::fromStdString(raw);
        item["provenance"] = QString::fromStdString(std::string(matdb::provenance_to_string(r->provenance_of(col))));
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
    auto mid = core::base::Uuid::from_string(materialId.toStdString());
    if (!mid) {
        out["ok"] = false;
        out["error"] = "Select a valid Material first.";
        return out;
    }
    eng::PhysicalObject obj;
    obj.name = name.trimmed().isEmpty() ? "Custom_Object" : name.trimmed().toStdString();
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
    ws_->notify_change(eng::DependencyChangeKind::ObjectUpdated, r.value(), "Created object '" + name.toStdString() + "'");
    out["ok"] = true;
    out["id"] = QString::fromStdString(r.value().to_string());
    return out;
}

QVariantMap ShunyaPipelineController::addPresetObject(const QString& presetName) {
    // Find best matching material in registry or promote elemental material on the fly
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

bool ShunyaPipelineController::duplicateObject(const QString& objectId) {
    auto uid = core::base::Uuid::from_string(objectId.toStdString());
    if (!uid) return false;
    const auto* orig = ws_->object_manager().get_object(*uid);
    if (!orig) return false;
    eng::PhysicalObject copy = *orig;
    copy.id = core::base::Uuid::generate_random();
    copy.name += "_Copy";
    copy.geometry.position.x += 0.15;
    auto r = ws_->object_manager().add_object(std::move(copy), ws_->material_registry());
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
