#pragma once

#include <QQuickPaintedItem>
#include <QColor>
#include <QString>
#include <QVariantList>
#include <vector>

#include "shunya/core/domain/scientific_plot.hpp"

namespace shunya::gui {

class ShunyaScientificPlotItem : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(double xMin READ xMin NOTIFY rangeChanged)
    Q_PROPERTY(double xMax READ xMax NOTIFY rangeChanged)
    Q_PROPERTY(double yMin READ yMin NOTIFY rangeChanged)
    Q_PROPERTY(double yMax READ yMax NOTIFY rangeChanged)
    Q_PROPERTY(int pointCount READ pointCount NOTIFY dataChanged)
    Q_PROPERTY(int decimatedCount READ decimatedCount NOTIFY rangeChanged)
    Q_PROPERTY(QString plotMode READ plotMode WRITE setPlotMode NOTIFY plotModeChanged)
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY lineColorChanged)
    Q_PROPERTY(bool autoScale READ autoScale WRITE setAutoScale NOTIFY autoScaleChanged)
    Q_PROPERTY(bool crosshairValid READ crosshairValid NOTIFY crosshairChanged)
    Q_PROPERTY(double crosshairDataX READ crosshairDataX NOTIFY crosshairChanged)
    Q_PROPERTY(double crosshairDataY READ crosshairDataY NOTIFY crosshairChanged)
    Q_PROPERTY(double crosshairNormX READ crosshairNormX NOTIFY crosshairChanged)
    Q_PROPERTY(double crosshairNormY READ crosshairNormY NOTIFY crosshairChanged)
    Q_PROPERTY(QString demoSignal READ demoSignal WRITE loadDemoSignal NOTIFY dataChanged)
    Q_PROPERTY(QVariantList legendItems READ legendItems NOTIFY dataChanged)
    Q_PROPERTY(QVariantList ticksX READ getTicksX NOTIFY rangeChanged)
    Q_PROPERTY(QVariantList ticksY READ getTicksY NOTIFY rangeChanged)

public:
    explicit ShunyaScientificPlotItem(QQuickItem* parent = nullptr);
    ~ShunyaScientificPlotItem() override = default;

    double xMin() const { return range_.x_min; }
    double xMax() const { return range_.x_max; }
    double yMin() const { return range_.y_min; }
    double yMax() const { return range_.y_max; }
    int pointCount() const { return static_cast<int>(data_.size()); }
    int decimatedCount() const { return static_cast<int>(decimated_.size()); }

    QString plotMode() const { return plot_mode_; }
    Q_INVOKABLE void setPlotMode(const QString& mode);

    QColor lineColor() const { return line_color_; }
    Q_INVOKABLE void setLineColor(const QColor& color);

    bool autoScale() const { return auto_scale_; }
    Q_INVOKABLE void setAutoScale(bool enabled);

    bool crosshairValid() const { return crosshair_valid_; }
    double crosshairDataX() const { return crosshair_pt_.x; }
    double crosshairDataY() const { return crosshair_pt_.y; }
    double crosshairNormX() const { return crosshair_norm_x_; }
    double crosshairNormY() const { return crosshair_norm_y_; }
    QString demoSignal() const { return demo_signal_; }

    Q_INVOKABLE void loadDemoSignal(const QString& signalType);
    Q_INVOKABLE void setMultiSeries(const QVariantList& seriesList);
    QVariantList legendItems() const;
    Q_INVOKABLE void zoomAt(double normX, double normY, double factor);
    Q_INVOKABLE void panBy(double deltaNormX, double deltaNormY);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void updateCrosshair(double pixelX, double pixelY);
    Q_INVOKABLE void clearCrosshair();
    Q_INVOKABLE QVariantList getTicksX() const;
    Q_INVOKABLE QVariantList getTicksY() const;

signals:
    void rangeChanged();
    void dataChanged();
    void plotModeChanged();
    void lineColorChanged();
    void autoScaleChanged();
    void crosshairChanged();

protected:
    void paint(QPainter* painter) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;

private:
    void rebuild_decimation();

    struct PlotSeries {
        QString name;
        QColor color;
        std::vector<core::domain::PlotPoint2D> data;
        std::vector<core::domain::PlotPoint2D> decimated;
    };
    std::vector<PlotSeries> extra_series_;

    std::vector<core::domain::PlotPoint2D> data_;
    std::vector<core::domain::PlotPoint2D> decimated_;
    core::domain::PlotRange range_;

    QString plot_mode_ = "line"; // "line" or "scatter"
    QString demo_signal_ = "damped_sine";
    QColor line_color_{37, 99, 235}; // #2563EB scientific chart primary blue
    bool auto_scale_ = true;

    bool crosshair_valid_ = false;
    core::domain::PlotPoint2D crosshair_pt_{};
    double crosshair_norm_x_ = 0.0;
    double crosshair_norm_y_ = 0.0;
    bool geometry_dirty_ = true;
};

} // namespace shunya::gui
