#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include "shunya/core/domain/colormap_tables.hpp"

namespace shunya::gui {

class ShunyaColormapLegendController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString colormapName READ colormapName WRITE setColormapName NOTIFY colormapChanged)
    Q_PROPERTY(double minValue READ minValue WRITE setMinValue NOTIFY rangeChanged)
    Q_PROPERTY(double maxValue READ maxValue WRITE setMaxValue NOTIFY rangeChanged)
    Q_PROPERTY(QString unitSymbol READ unitSymbol WRITE setUnitSymbol NOTIFY rangeChanged)
    Q_PROPERTY(bool probeActive READ probeActive NOTIFY probeChanged)
    Q_PROPERTY(double probeValue READ probeValue NOTIFY probeChanged)
    Q_PROPERTY(double probeNormalized READ probeNormalized NOTIFY probeChanged)
    Q_PROPERTY(QString probeLabel READ probeLabel NOTIFY probeChanged)
    Q_PROPERTY(QString probeColorHex READ probeColorHex NOTIFY probeChanged)

public:
    explicit ShunyaColormapLegendController(QObject* parent = nullptr);
    ~ShunyaColormapLegendController() override = default;

    QString colormapName() const { return QString::fromStdString(std::string(core::domain::colormap_type_to_string(type_))); }
    Q_INVOKABLE void setColormapName(const QString& name);

    double minValue() const { return min_val_; }
    Q_INVOKABLE void setMinValue(double v);

    double maxValue() const { return max_val_; }
    Q_INVOKABLE void setMaxValue(double v);

    QString unitSymbol() const { return unit_symbol_; }
    Q_INVOKABLE void setUnitSymbol(const QString& unit);

    bool probeActive() const { return probe_active_; }
    double probeValue() const { return probe_result_.raw_value; }
    double probeNormalized() const { return probe_result_.normalized; }
    QString probeLabel() const { return QString::fromStdString(probe_result_.formatted_label); }
    QString probeColorHex() const { return QString::fromStdString(probe_result_.color.to_hex()); }

    Q_INVOKABLE QVariantList getGradientStops(int numStops = 17) const;
    Q_INVOKABLE void updateProbeByNormalized(double normT);
    Q_INVOKABLE void updateProbeByValue(double value);
    Q_INVOKABLE void clearProbe();

signals:
    void colormapChanged();
    void rangeChanged();
    void probeChanged();

private:
    core::domain::ColormapType type_{core::domain::ColormapType::Viridis};
    double min_val_{293.15};
    double max_val_{450.0};
    QString unit_symbol_{"K"};
    bool probe_active_{false};
    core::domain::FieldProbeResult probe_result_{};
};

} // namespace shunya::gui
