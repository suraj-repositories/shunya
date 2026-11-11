#include "periodic_table_controller.hpp"

#include "shunya/matdb/element_database.hpp"
#include "shunya/matdb/elements_118_catalog.hpp"
#include "shunya/matdb/ionization_repository.hpp"
#include "shunya/matdb/periodic_table_extended.hpp"

namespace shunya::gui {

namespace {

matdb::HeatmapProperty parse_property_name(const QString& s) {
    if (s == "BlockCategory")         return matdb::HeatmapProperty::BlockCategory;
    if (s == "AtomicMass")            return matdb::HeatmapProperty::AtomicMass;
    if (s == "Electronegativity")     return matdb::HeatmapProperty::Electronegativity;
    if (s == "FirstIonizationEnergy") return matdb::HeatmapProperty::FirstIonizationEnergy;
    if (s == "CovalentRadius")        return matdb::HeatmapProperty::CovalentRadius;
    if (s == "MeltingPointK")         return matdb::HeatmapProperty::MeltingPointK;
    if (s == "DensityGcm3")           return matdb::HeatmapProperty::DensityGcm3;
    if (s == "ThermalConductivity")   return matdb::HeatmapProperty::ThermalConductivityWmK;
    return matdb::HeatmapProperty::FamilyCategory;
}

} // namespace

ShunyaPeriodicTableController::ShunyaPeriodicTableController(QObject* parent)
    : QObject(parent) {
    if (!matdb::ElementDatabase::global().is_loaded()) {
        (void)matdb::ElementDatabase::global().load_default();
    }
    rebuild_grid();
}

void ShunyaPeriodicTableController::setActiveProperty(const QString& propName) {
    if (active_property_ == propName) return;
    active_property_ = propName;
    rebuild_grid();
    emit gridChanged();
}

void ShunyaPeriodicTableController::setBlockFilter(const QString& block) {
    if (block_filter_ == block) return;
    block_filter_ = block;
    rebuild_grid();
    emit gridChanged();
}

void ShunyaPeriodicTableController::setSearchText(const QString& text) {
    if (search_text_ == text) return;
    search_text_ = text;
    rebuild_grid();
    emit gridChanged();
}

void ShunyaPeriodicTableController::setPhaseFilter(const QString& phase) {
    if (phase_filter_ == phase) return;
    phase_filter_ = phase;
    rebuild_grid();
    emit gridChanged();
}

void ShunyaPeriodicTableController::setHideFBlock(bool hide) {
    if (hide_f_block_ == hide) return;
    hide_f_block_ = hide;
    rebuild_grid();
    emit gridChanged();
}

void ShunyaPeriodicTableController::setSelectedAtomicNumber(int z) {
    if (z < 1 || z > 118 || selected_z_ == z) return;
    selected_z_ = z;
    emit selectionChanged();
}

void ShunyaPeriodicTableController::rebuild_grid() {
    matdb::HeatmapFilterOptions opts;
    opts.property = parse_property_name(active_property_);
    opts.colormap = core::domain::ColormapType::CoolWarm;
    opts.hide_f_block = hide_f_block_;
    if (block_filter_ == "s" || block_filter_ == "p" || block_filter_ == "d" || block_filter_ == "f") {
        opts.filter_block = block_filter_.at(0).toLatin1();
    }

    range_stats_ = matdb::PeriodicHeatmapFilter::compute_property_range(opts.property);
    auto grid = matdb::PeriodicHeatmapFilter::build_grid(opts);

    QVariantList list;
    list.reserve(static_cast<qsizetype>(grid.size()));
    const QString needle = search_text_.trimmed().toLower();
    const QString phase_need = phase_filter_.trimmed().toLower();
    for (const auto& c : grid) {
        const auto* rec = matdb::ElementDatabase::global().find_by_z(c.atomic_number);
        bool vis = c.visible;
        if (vis && !needle.isEmpty()) {
            const QString sym = QString::fromStdString(c.symbol).toLower();
            const QString nm = QString::fromStdString(c.name).toLower();
            const QString iupac = rec ? QString::fromStdString(rec->iupac_symbol).toLower() : "";
            const QString zstr = QString::number(c.atomic_number);
            vis = sym.contains(needle) || nm.contains(needle) || iupac.contains(needle) || zstr == needle;
        }
        if (vis && !phase_need.isEmpty() && rec) {
            vis = QString::fromStdString(rec->phase).toLower().contains(phase_need);
        }
        QVariantMap m;
        m["z"] = c.atomic_number;
        m["symbol"] = QString::fromStdString(rec && !rec->iupac_symbol.empty() ? rec->iupac_symbol : c.symbol);
        m["csvSymbol"] = QString::fromStdString(c.symbol);
        m["name"] = QString::fromStdString(c.name);
        m["row"] = c.grid_row;
        m["col"] = c.grid_col;
        m["block"] = QString(QChar::fromLatin1(c.block));
        m["phase"] = rec ? QString::fromStdString(rec->phase) : QStringLiteral("Solid");
        m["mass"] = rec && rec->atomic_weight ? *rec->atomic_weight : c.atomic_weight_amu;
        m["heat"] = c.normalized_heat;
        m["colorHex"] = QString::fromStdString(c.color_hex);
        m["valueLabel"] = QString::fromStdString(c.formatted_value);
        m["visible"] = vis;
        m["highlighted"] = c.highlighted && vis;
        list.append(m);
    }
    cells_cache_ = std::move(list);
}

QVariantMap ShunyaPeriodicTableController::selectedElementDetails() const {
    QVariantMap m;
    const auto* rec = matdb::ElementDatabase::global().find_by_z(selected_z_);
    const auto* e = matdb::Elements118Catalog::find_by_z(selected_z_);
    if (!rec && !e) return m;
    m["z"] = selected_z_;
    m["symbol"] = QString::fromStdString(rec ? (rec->iupac_symbol.empty() ? rec->symbol : rec->iupac_symbol) : e->symbol);
    m["csvSymbol"] = QString::fromStdString(rec ? rec->symbol : e->symbol);
    m["name"] = QString::fromStdString(rec ? rec->name : e->name);
    m["mass"] = rec && rec->atomic_weight ? *rec->atomic_weight : (e ? e->atomic_weight_amu : 0.0);
    m["electronConfig"] = QString::fromStdString(
        rec && !rec->electron_configuration_iupac.empty() ? rec->electron_configuration_iupac
        : (e ? e->electron_configuration : ""));
    m["block"] = rec ? QString::fromStdString(rec->block) : QString(QChar::fromLatin1(e->block));
    if (rec) {
        m["phase"] = QString::fromStdString(rec->phase);
        if (rec->density) m["density"] = *rec->density;
        if (rec->melting_point) m["meltingPoint"] = *rec->melting_point;
        if (rec->boiling_point) m["boilingPoint"] = *rec->boiling_point;
        if (rec->thermal_conductivity) m["thermalConductivity"] = *rec->thermal_conductivity;
        if (auto c = rec->effective_conductivity()) m["electricalConductivity"] = *c;
        if (rec->specific_heat) m["specificHeat"] = *rec->specific_heat;
        if (rec->electronegativity) m["pauling"] = *rec->electronegativity;
    }
    if (!m.contains("pauling")) {
        if (const auto* ext = matdb::PeriodicTableExtended::find_by_z(selected_z_)) {
            if (ext->electronegativity.pauling) m["pauling"] = *ext->electronegativity.pauling;
        }
    }
    if (auto ie1 = matdb::IonizationEnergyRepository::first_ionization_ev(selected_z_)) {
        m["ie1Ev"] = *ie1;
    }
    return m;
}

} // namespace shunya::gui
