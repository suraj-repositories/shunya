#include "colormap_legend_controller.hpp"

#include <QVariantMap>
#include <algorithm>

namespace shunya::gui {

ShunyaColormapLegendController::ShunyaColormapLegendController(QObject* parent)
    : QObject(parent)
{
    updateProbeByValue(345.0);
}

void ShunyaColormapLegendController::setColormapName(const QString& name) {
    auto parsed = core::domain::colormap_type_from_string(name.toStdString());
    if (parsed && *parsed != type_) {
        type_ = *parsed;
        if (probe_active_) {
            probe_result_ = core::domain::ColormapTables::probe_scalar(
                type_, probe_result_.raw_value, min_val_, max_val_, unit_symbol_.toStdString()
            );
            emit probeChanged();
        }
        emit colormapChanged();
    }
}

void ShunyaColormapLegendController::setMinValue(double v) {
    if (min_val_ != v) {
        min_val_ = v;
        emit rangeChanged();
    }
}

void ShunyaColormapLegendController::setMaxValue(double v) {
    if (max_val_ != v) {
        max_val_ = v;
        emit rangeChanged();
    }
}

void ShunyaColormapLegendController::setUnitSymbol(const QString& unit) {
    if (unit_symbol_ != unit) {
        unit_symbol_ = unit;
        emit rangeChanged();
    }
}

QVariantList ShunyaColormapLegendController::getGradientStops(int numStops) const {
    QVariantList stops;
    int count = std::max(2, numStops);
    for (int i = 0; i < count; ++i) {
        double t = static_cast<double>(i) / static_cast<double>(count - 1);
        auto rgb = core::domain::ColormapTables::sample_rgb8(type_, t);
        QVariantMap stop;
        stop["pos"] = 1.0 - t; // Top = max (t=1), Bottom = min (t=0)
        stop["t"] = t;
        stop["color"] = QString::fromStdString(rgb.to_hex());
        stops.append(stop);
    }
    return stops;
}

void ShunyaColormapLegendController::updateProbeByNormalized(double normT) {
    double clamped = std::clamp(normT, 0.0, 1.0);
    double val = min_val_ + clamped * (max_val_ - min_val_);
    updateProbeByValue(val);
}

void ShunyaColormapLegendController::updateProbeByValue(double value) {
    probe_active_ = true;
    probe_result_ = core::domain::ColormapTables::probe_scalar(
        type_, value, min_val_, max_val_, unit_symbol_.toStdString()
    );
    emit probeChanged();
}

void ShunyaColormapLegendController::clearProbe() {
    if (probe_active_) {
        probe_active_ = false;
        emit probeChanged();
    }
}

} // namespace shunya::gui
