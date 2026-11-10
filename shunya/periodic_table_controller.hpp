#pragma once

#include "shunya/matdb/periodic_heatmap_filter.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace shunya::gui {

class ShunyaPeriodicTableController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString activeProperty READ activeProperty WRITE setActiveProperty NOTIFY gridChanged)
    Q_PROPERTY(QString blockFilter READ blockFilter WRITE setBlockFilter NOTIFY gridChanged)
    Q_PROPERTY(bool hideFBlock READ hideFBlock WRITE setHideFBlock NOTIFY gridChanged)
    Q_PROPERTY(int selectedAtomicNumber READ selectedAtomicNumber WRITE setSelectedAtomicNumber NOTIFY selectionChanged)
    Q_PROPERTY(QVariantList cells READ cells NOTIFY gridChanged)
    Q_PROPERTY(QVariantMap selectedElementDetails READ selectedElementDetails NOTIFY selectionChanged)
    Q_PROPERTY(double minRange READ minRange NOTIFY gridChanged)
    Q_PROPERTY(double maxRange READ maxRange NOTIFY gridChanged)
    Q_PROPERTY(QString rangeUnit READ rangeUnit NOTIFY gridChanged)

public:
    explicit ShunyaPeriodicTableController(QObject* parent = nullptr);
    ~ShunyaPeriodicTableController() override = default;

    QString activeProperty() const { return active_property_; }
    Q_INVOKABLE void setActiveProperty(const QString& propName);

    QString blockFilter() const { return block_filter_; }
    Q_INVOKABLE void setBlockFilter(const QString& block);

    bool hideFBlock() const { return hide_f_block_; }
    Q_INVOKABLE void setHideFBlock(bool hide);

    int selectedAtomicNumber() const { return selected_z_; }
    Q_INVOKABLE void setSelectedAtomicNumber(int z);

    QVariantList cells() const { return cells_cache_; }
    QVariantMap selectedElementDetails() const;

    double minRange() const { return range_stats_.min_value; }
    double maxRange() const { return range_stats_.max_value; }
    QString rangeUnit() const { return QString::fromStdString(range_stats_.unit); }

signals:
    void gridChanged();
    void selectionChanged();

private:
    void rebuild_grid();

    QString active_property_{"FamilyCategory"};
    QString block_filter_{"All"};
    bool hide_f_block_{false};
    int selected_z_{29};
    matdb::PropertyRangeStats range_stats_;
    QVariantList cells_cache_;
};

} // namespace shunya::gui
