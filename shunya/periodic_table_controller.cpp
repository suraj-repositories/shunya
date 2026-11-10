#include "periodic_table_controller.hpp"

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
    for (const auto& c : grid) {
        QVariantMap m;
        m["z"] = c.atomic_number;
        m["symbol"] = QString::fromStdString(c.symbol);
        m["name"] = QString::fromStdString(c.name);
        m["row"] = c.grid_row;
        m["col"] = c.grid_col;
        m["block"] = QString(QChar::fromLatin1(c.block));
        m["mass"] = c.atomic_weight_amu;
        m["heat"] = c.normalized_heat;
        m["colorHex"] = QString::fromStdString(c.color_hex);
        m["valueLabel"] = QString::fromStdString(c.formatted_value);
        m["visible"] = c.visible;
        m["highlighted"] = c.highlighted;
        list.append(m);
    }
    cells_cache_ = std::move(list);
}

QVariantMap ShunyaPeriodicTableController::selectedElementDetails() const {
    QVariantMap m;
    const auto* e = matdb::Elements118Catalog::find_by_z(selected_z_);
    if (!e) return m;
    m["z"] = e->atomic_number;
    m["symbol"] = QString::fromStdString(e->symbol);
    m["name"] = QString::fromStdString(e->name);
    m["mass"] = e->atomic_weight_amu;
    m["electronConfig"] = QString::fromStdString(e->electron_configuration);
    m["block"] = QString(QChar::fromLatin1(e->block));
    if (const auto* ext = matdb::PeriodicTableExtended::find_by_z(selected_z_)) {
        if (ext->electronegativity.pauling) {
            m["pauling"] = *ext->electronegativity.pauling;
        }
    }
    if (auto ie1 = matdb::IonizationEnergyRepository::first_ionization_ev(selected_z_)) {
        m["ie1Ev"] = *ie1;
    }
    return m;
}

} // namespace shunya::gui
