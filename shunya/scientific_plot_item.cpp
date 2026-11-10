#include "scientific_plot_item.hpp"

#include <QVariantMap>
#include <algorithm>
#include <cmath>

namespace shunya::gui {

ShunyaScientificPlotItem::ShunyaScientificPlotItem(QQuickItem* parent)
    : QQuickItem(parent)
{
    setFlag(QQuickItem::ItemHasContents, true);
    loadDemoSignal("damped_sine");
}

void ShunyaScientificPlotItem::setPlotMode(const QString& mode) {
    if (plot_mode_ != mode) {
        plot_mode_ = mode;
        geometry_dirty_ = true;
        emit plotModeChanged();
        update();
    }
}

void ShunyaScientificPlotItem::setLineColor(const QColor& color) {
    if (line_color_ != color) {
        line_color_ = color;
        geometry_dirty_ = true;
        emit lineColorChanged();
        update();
    }
}

void ShunyaScientificPlotItem::setAutoScale(bool enabled) {
    if (auto_scale_ != enabled) {
        auto_scale_ = enabled;
        emit autoScaleChanged();
        if (auto_scale_) resetView();
    }
}

void ShunyaScientificPlotItem::loadDemoSignal(const QString& signalType) {
    demo_signal_ = signalType;
    data_.clear();

    if (signalType == "chirp_500k") {
        const size_t N = 500000;
        data_.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            double t = static_cast<double>(i) / 50000.0; // 0 to 10s
            double freq = 0.5 + t * 2.5;
            double y = std::sin(2.0 * 3.14159265358979323846 * freq * t) * std::exp(-0.08 * t);
            if (i == 250000) y = 2.4; // Transient spike to prove min-max peak preservation
            data_.push_back({t, y});
        }
    } else if (signalType == "step_response") {
        const size_t N = 4000;
        data_.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            double t = static_cast<double>(i) * 0.0025; // 0 to 10ms
            double y = 0.0;
            if (t >= 1.0) {
                double tau = t - 1.0;
                y = 5.0 * (1.0 - std::exp(-0.8 * tau) * (std::cos(4.0 * tau) + 0.2 * std::sin(4.0 * tau)));
            }
            data_.push_back({t, y});
        }
    } else {
        // Default: damped_sine (10,000 pts)
        const size_t N = 10000;
        data_.reserve(N);
        for (size_t i = 0; i < N; ++i) {
            double t = static_cast<double>(i) * 0.001;
            double y = 3.3 * std::exp(-0.35 * t) * std::sin(2.0 * 3.14159265358979323846 * 1.5 * t);
            data_.push_back({t, y});
        }
    }

    range_ = core::domain::PlotAxisCalculator::compute_auto_range(data_, 0.08);
    rebuild_decimation();
    emit dataChanged();
    emit rangeChanged();
    update();
}

void ShunyaScientificPlotItem::zoomAt(double normX, double normY, double factor) {
    auto_scale_ = false;
    range_ = core::domain::PlotAxisCalculator::zoom_around(range_, normX, 1.0 - normY, factor);
    rebuild_decimation();
    emit autoScaleChanged();
    emit rangeChanged();
    update();
}

void ShunyaScientificPlotItem::panBy(double deltaNormX, double deltaNormY) {
    auto_scale_ = false;
    range_ = core::domain::PlotAxisCalculator::pan_by_normalized(range_, deltaNormX, -deltaNormY);
    rebuild_decimation();
    emit autoScaleChanged();
    emit rangeChanged();
    update();
}

void ShunyaScientificPlotItem::resetView() {
    auto_scale_ = true;
    range_ = core::domain::PlotAxisCalculator::compute_auto_range(data_, 0.08);
    rebuild_decimation();
    emit autoScaleChanged();
    emit rangeChanged();
    update();
}

void ShunyaScientificPlotItem::updateCrosshair(double pixelX, double pixelY) {
    Q_UNUSED(pixelY)
    double w = width();
    if (w <= 0.0 || data_.empty() || !range_.is_valid()) return;

    double norm_x = std::clamp(pixelX / w, 0.0, 1.0);
    double target_x = range_.x_min + norm_x * range_.width();

    core::domain::PlotPoint2D pt;
    if (core::domain::PlotAxisCalculator::find_nearest_x(data_, target_x, pt)) {
        crosshair_valid_ = true;
        crosshair_pt_ = pt;
        crosshair_norm_x_ = std::clamp((pt.x - range_.x_min) / range_.width(), 0.0, 1.0);
        crosshair_norm_y_ = std::clamp(1.0 - (pt.y - range_.y_min) / range_.height(), 0.0, 1.0);
        emit crosshairChanged();
    }
}

void ShunyaScientificPlotItem::clearCrosshair() {
    if (crosshair_valid_) {
        crosshair_valid_ = false;
        emit crosshairChanged();
    }
}

QVariantList ShunyaScientificPlotItem::getTicksX() const {
    QVariantList list;
    auto ticks = core::domain::PlotAxisCalculator::generate_ticks(range_.x_min, range_.x_max, 7);
    for (const auto& t : ticks) {
        QVariantMap m;
        m["value"] = t.value;
        m["norm"] = t.normalized_pos;
        m["label"] = QString::fromStdString(t.label);
        list.append(m);
    }
    return list;
}

QVariantList ShunyaScientificPlotItem::getTicksY() const {
    QVariantList list;
    auto ticks = core::domain::PlotAxisCalculator::generate_ticks(range_.y_min, range_.y_max, 6);
    for (const auto& t : ticks) {
        QVariantMap m;
        m["value"] = t.value;
        m["norm"] = 1.0 - t.normalized_pos; // Screen Y grows downward
        m["label"] = QString::fromStdString(t.label);
        list.append(m);
    }
    return list;
}

void ShunyaScientificPlotItem::rebuild_decimation() {
    size_t target_px = static_cast<size_t>(std::max(120.0, width()));
    core::domain::MinMaxDecimator::decimate(
        data_,
        range_.x_min,
        range_.x_max,
        target_px,
        decimated_
    );
    geometry_dirty_ = true;
}

void ShunyaScientificPlotItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        rebuild_decimation();
        emit rangeChanged();
        update();
    }
}

QSGNode* ShunyaScientificPlotItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) {
    auto* node = static_cast<QSGGeometryNode*>(oldNode);
    if (!node) {
        node = new QSGGeometryNode();
        auto* geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geom->setDrawingMode(plot_mode_ == "scatter" ? QSGGeometry::DrawPoints : QSGGeometry::DrawLineStrip);
        geom->setLineWidth(1.5f);
        node->setGeometry(geom);
        node->setFlag(QSGNode::OwnsGeometry);

        auto* mat = new QSGFlatColorMaterial();
        mat->setColor(line_color_);
        node->setMaterial(mat);
        node->setFlag(QSGNode::OwnsMaterial);
        geometry_dirty_ = true;
    }

    if (geometry_dirty_) {
        auto* geom = node->geometry();
        auto* mat = static_cast<QSGFlatColorMaterial*>(node->material());
        mat->setColor(line_color_);

        if (plot_mode_ == "scatter") {
            geom->setDrawingMode(QSGGeometry::DrawPoints);
            geom->setLineWidth(3.5f);
        } else {
            geom->setDrawingMode(QSGGeometry::DrawLineStrip);
            geom->setLineWidth(1.5f);
        }

        const int count = static_cast<int>(decimated_.size());
        geom->allocate(count);

        double w = width();
        double h = height();
        double rx = std::max(1e-12, range_.width());
        double ry = std::max(1e-12, range_.height());

        auto* verts = geom->vertexDataAsPoint2D();
        for (int i = 0; i < count; ++i) {
            float sx = static_cast<float>(((decimated_[i].x - range_.x_min) / rx) * w);
            float sy = static_cast<float>((1.0 - (decimated_[i].y - range_.y_min) / ry) * h);
            verts[i].set(sx, sy);
        }

        node->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
        geometry_dirty_ = false;
    }

    return node;
}

} // namespace shunya::gui
