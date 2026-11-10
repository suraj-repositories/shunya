#include "layer_panel_controller.hpp"

#include "shunya/core/base/math.hpp"

namespace shunya::gui {

namespace {
cad2d::LinePattern parse_pattern(const QString& pat) {
    if (pat == "Dashed") return cad2d::LinePattern::Dashed;
    if (pat == "Dotted") return cad2d::LinePattern::Dotted;
    if (pat == "DashDot") return cad2d::LinePattern::DashDot;
    if (pat == "Center") return cad2d::LinePattern::Center;
    return cad2d::LinePattern::Solid;
}
} // namespace

ShunyaLayerListModel::ShunyaLayerListModel(const cad2d::LayerManager* mgr, QObject* parent)
    : QAbstractListModel(parent), mgr_(mgr) {
    refresh();
}

int ShunyaLayerListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(snapshot_.size());
}

QVariant ShunyaLayerListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= snapshot_.size()) {
        return {};
    }
    const auto& layer = snapshot_[static_cast<size_t>(index.row())];
    switch (role) {
        case NameRole:
            return QString::fromStdString(layer.name);
        case ParentLayerRole:
            return QString::fromStdString(layer.parent_layer);
        case VisibleRole:
            return layer.visible;
        case EffectiveVisibleRole:
            return mgr_ ? mgr_->is_effectively_visible(layer.name) : layer.visible;
        case LockedRole:
            return layer.locked;
        case EffectiveLockedRole:
            return mgr_ ? mgr_->is_effectively_locked(layer.name) : layer.locked;
        case ColorHexRole:
            return QString::fromStdString(layer.style.color_hex);
        case LineWeightRole:
            return layer.style.line_weight_mm;
        case PatternNameRole:
            return QString::fromUtf8(
                cad2d::line_pattern_to_string(layer.style.pattern).data(),
                static_cast<qsizetype>(cad2d::line_pattern_to_string(layer.style.pattern).size())
            );
        default:
            return {};
    }
}

QHash<int, QByteArray> ShunyaLayerListModel::roleNames() const {
    return {
        {NameRole, "layerName"},
        {ParentLayerRole, "parentLayer"},
        {VisibleRole, "layerVisible"},
        {EffectiveVisibleRole, "effectiveVisible"},
        {LockedRole, "layerLocked"},
        {EffectiveLockedRole, "effectiveLocked"},
        {ColorHexRole, "colorHex"},
        {LineWeightRole, "lineWeight"},
        {PatternNameRole, "patternName"}
    };
}

void ShunyaLayerListModel::refresh() {
    beginResetModel();
    snapshot_.clear();
    if (mgr_) {
        for (const auto* ptr : mgr_->ordered_layers()) {
            if (ptr) snapshot_.push_back(*ptr);
        }
    }
    endResetModel();
}

ShunyaBlockListModel::ShunyaBlockListModel(const cad2d::BlockManager* mgr, QObject* parent)
    : QAbstractListModel(parent), mgr_(mgr) {
    refresh();
}

int ShunyaBlockListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(rows_.size());
}

QVariant ShunyaBlockListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || static_cast<size_t>(index.row()) >= rows_.size()) {
        return {};
    }
    const auto& row = rows_[static_cast<size_t>(index.row())];
    switch (role) {
        case NameRole:          return row.name;
        case ItemCountRole:     return row.item_count;
        case NestedCountRole:   return row.nested_count;
        case InstanceCountRole: return row.instance_count;
        default:                return {};
    }
}

QHash<int, QByteArray> ShunyaBlockListModel::roleNames() const {
    return {
        {NameRole, "blockName"},
        {ItemCountRole, "itemCount"},
        {NestedCountRole, "nestedCount"},
        {InstanceCountRole, "instanceCount"}
    };
}

void ShunyaBlockListModel::refresh() {
    beginResetModel();
    rows_.clear();
    if (mgr_) {
        for (const auto* def : mgr_->blocks()) {
            if (!def) continue;
            int inst_count = 0;
            for (const auto& inst : mgr_->instances()) {
                if (inst.block_name == def->name) ++inst_count;
            }
            rows_.push_back({
                QString::fromStdString(def->name),
                static_cast<int>(def->items.size()),
                static_cast<int>(def->nested_refs.size()),
                inst_count
            });
        }
    }
    endResetModel();
}

ShunyaLayerPanelController::ShunyaLayerPanelController(QObject* parent)
    : QObject(parent) {
    layer_mgr_.add_layer("Construction", "0", {"#e5c07b", 0.18, cad2d::LinePattern::Dashed});
    layer_mgr_.add_layer("Dimensions", "0", {"#98c379", 0.18, cad2d::LinePattern::Solid});

    cad2d::BlockDefinition plate;
    plate.name = "MountPlate";
    plate.items.push_back({
        std::make_shared<cad2d::Polyline2D>(
            std::vector<cad2d::Point2D>{{-20, -10}, {20, -10}, {20, 10}, {-20, 10}}, true),
        "0",
        false
    });
    plate.items.push_back({std::make_shared<cad2d::Circle2D>(cad2d::Point2D{0.0, 0.0}, 4.0), "0", false});
    block_mgr_.create_block(std::move(plate));

    layer_model_ = new ShunyaLayerListModel(&layer_mgr_, this);
    block_model_ = new ShunyaBlockListModel(&block_mgr_, this);
}

void ShunyaLayerPanelController::setActiveLayer(const QString& layerName) {
    const std::string s = layerName.toStdString();
    if (layer_mgr_.find_layer(s) == nullptr || active_layer_ == layerName) return;
    active_layer_ = layerName;
    emit activeLayerChanged();
}

bool ShunyaLayerPanelController::addLayer(
    const QString& name,
    const QString& parentLayer,
    const QString& colorHex,
    double weightMm,
    const QString& pattern
) {
    const cad2d::LayerStyle style{colorHex.toStdString(), weightMm, parse_pattern(pattern)};
    if (!layer_mgr_.add_layer(name.toStdString(), parentLayer.toStdString(), style)) {
        return false;
    }
    layer_model_->refresh();
    emit layersChanged();
    return true;
}

bool ShunyaLayerPanelController::toggleLayerVisible(const QString& name) {
    const auto* layer = layer_mgr_.find_layer(name.toStdString());
    if (!layer) return false;
    if (!layer_mgr_.set_visible(name.toStdString(), !layer->visible)) return false;
    layer_model_->refresh();
    emit layersChanged();
    return true;
}

bool ShunyaLayerPanelController::toggleLayerLocked(const QString& name) {
    const auto* layer = layer_mgr_.find_layer(name.toStdString());
    if (!layer) return false;
    if (!layer_mgr_.set_locked(name.toStdString(), !layer->locked)) return false;
    layer_model_->refresh();
    emit layersChanged();
    return true;
}

bool ShunyaLayerPanelController::createRectBlock(const QString& name, double widthMm, double heightMm) {
    if (name.isEmpty() || widthMm <= 0.0 || heightMm <= 0.0) return false;
    cad2d::BlockDefinition def;
    def.name = name.toStdString();
    const double hw = widthMm * 0.5;
    const double hh = heightMm * 0.5;
    def.items.push_back({
        std::make_shared<cad2d::Polyline2D>(
            std::vector<cad2d::Point2D>{{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}}, true),
        active_layer_.toStdString(),
        false
    });
    if (!block_mgr_.create_block(std::move(def))) return false;
    block_model_->refresh();
    emit blocksChanged();
    return true;
}

QString ShunyaLayerPanelController::placeBlockInstance(
    const QString& blockName,
    double x,
    double y,
    double rotDeg,
    double scale
) {
    const double rot_rad = rotDeg * core::base::math::PI / 180.0;
    const std::string id = block_mgr_.place_instance(
        blockName.toStdString(),
        {x, y},
        rot_rad,
        scale,
        scale,
        active_layer_.toStdString()
    );
    if (id.empty()) return {};
    block_model_->refresh();
    emit blocksChanged();
    return QString::fromStdString(id);
}

void ShunyaLayerPanelController::syncModels() {
    if (layer_model_) layer_model_->refresh();
    if (block_model_) block_model_->refresh();
    emit layersChanged();
    emit blocksChanged();
}

} // namespace shunya::gui
