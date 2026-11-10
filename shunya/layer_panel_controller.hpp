#pragma once

#include <QAbstractListModel>
#include <QObject>
#include <QString>
#include <vector>

#include "shunya/cad2d/layer_manager.hpp"
#include "shunya/cad2d/block_manager.hpp"

namespace shunya::gui {

class ShunyaLayerListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        ParentLayerRole,
        VisibleRole,
        EffectiveVisibleRole,
        LockedRole,
        EffectiveLockedRole,
        ColorHexRole,
        LineWeightRole,
        PatternNameRole
    };

    explicit ShunyaLayerListModel(const cad2d::LayerManager* mgr, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void refresh();

private:
    const cad2d::LayerManager* mgr_;
    std::vector<cad2d::Layer2D> snapshot_;
};

class ShunyaBlockListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        ItemCountRole,
        NestedCountRole,
        InstanceCountRole
    };

    explicit ShunyaBlockListModel(const cad2d::BlockManager* mgr, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void refresh();

private:
    struct BlockRow {
        QString name;
        int item_count{0};
        int nested_count{0};
        int instance_count{0};
    };

    const cad2d::BlockManager* mgr_;
    std::vector<BlockRow> rows_;
};

class ShunyaLayerPanelController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* layerModel READ layerModel CONSTANT)
    Q_PROPERTY(QObject* blockModel READ blockModel CONSTANT)
    Q_PROPERTY(QString activeLayer READ activeLayer WRITE setActiveLayer NOTIFY activeLayerChanged)
    Q_PROPERTY(int layerCount READ layerCount NOTIFY layersChanged)
    Q_PROPERTY(int blockCount READ blockCount NOTIFY blocksChanged)
    Q_PROPERTY(int instanceCount READ instanceCount NOTIFY blocksChanged)

public:
    explicit ShunyaLayerPanelController(QObject* parent = nullptr);

    QObject* layerModel() const { return layer_model_; }
    QObject* blockModel() const { return block_model_; }
    QString activeLayer() const { return active_layer_; }
    Q_INVOKABLE void setActiveLayer(const QString& layerName);

    int layerCount() const { return static_cast<int>(layer_mgr_.layer_count()); }
    int blockCount() const { return static_cast<int>(block_mgr_.block_count()); }
    int instanceCount() const { return static_cast<int>(block_mgr_.instance_count()); }

    Q_INVOKABLE bool addLayer(
        const QString& name,
        const QString& parentLayer = QString(),
        const QString& colorHex = "#61afef",
        double weightMm = 0.25,
        const QString& pattern = "Solid"
    );
    Q_INVOKABLE bool toggleLayerVisible(const QString& name);
    Q_INVOKABLE bool toggleLayerLocked(const QString& name);

    Q_INVOKABLE bool createRectBlock(const QString& name, double widthMm, double heightMm);
    Q_INVOKABLE QString placeBlockInstance(
        const QString& blockName,
        double x,
        double y,
        double rotDeg = 0.0,
        double scale = 1.0
    );

    cad2d::LayerManager& layerManager() noexcept { return layer_mgr_; }
    cad2d::BlockManager& blockManager() noexcept { return block_mgr_; }
    void syncModels();

signals:
    void activeLayerChanged();
    void layersChanged();
    void blocksChanged();

private:
    cad2d::LayerManager layer_mgr_;
    cad2d::BlockManager block_mgr_;
    ShunyaLayerListModel* layer_model_{nullptr};
    ShunyaBlockListModel* block_model_{nullptr};
    QString active_layer_{"0"};
};

} // namespace shunya::gui
