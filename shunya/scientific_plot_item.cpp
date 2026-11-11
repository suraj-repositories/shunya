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
    extra_series_.clear();
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

void ShunyaScientificPlotItem::setMultiSeries(const QVariantList& seriesList) {
    extra_series_.clear();
    data_.clear();
    std::vector<core::domain::PlotPoint2D> all_pts;

    for (const auto& sv : seriesList) {
        const auto sm = sv.toMap();
        PlotSeries s;
        s.name = sm.value("name").toString();
        s.color = QColor(sm.value("color", "#2563EB").toString());
        const auto pts = sm.value("points").toList();
        s.data.reserve(static_cast<size_t>(pts.size()));
        for (const auto& pv : pts) {
            const auto pm = pv.toMap();
            core::domain::PlotPoint2D p{pm.value("x").toDouble(), pm.value("y").toDouble()};
            if (std::isfinite(p.x) && std::isfinite(p.y)) {
                s.data.push_back(p);
                all_pts.push_back(p);
            }
        }
        if (!s.data.empty()) {
            extra_series_.push_back(std::move(s));
        }
    }

    if (!extra_series_.empty()) {
        data_ = extra_series_[0].data;
        range_ = core::domain::PlotAxisCalculator::compute_auto_range(all_pts, 0.08);
    } else {
        range_ = core::domain::PlotRange{0.0, 1.0, 0.0, 1.0};
    }
    auto_scale_ = true;
    rebuild_decimation();
    emit autoScaleChanged();
    emit dataChanged();
    emit rangeChanged();
    update();
}

QVariantList ShunyaScientificPlotItem::legendItems() const {
    QVariantList out;
    if (extra_series_.empty()) {
        if (!data_.empty()) {
            out.append(QVariantMap{{"name", demo_signal_}, {"color", line_color_.name()}});
        }
        return out;
    }
    for (const auto& s : extra_series_) {
        out.append(QVariantMap{{"name", s.name}, {"color", s.color.name()}});
    }
    return out;
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
    for (auto& s : extra_series_) {
        core::domain::MinMaxDecimator::decimate(
            s.data,
            range_.x_min,
            range_.x_max,
            target_px,
            s.decimated
        );
    }
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
    auto* root = oldNode ? oldNode : new QSGNode();

    auto sync_series_node = [&](QSGGeometryNode* n,
                                const std::vector<core::domain::PlotPoint2D>& pts,
                                const QColor& col) {
        if (!n->geometry()) {
            auto* g = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
            n->setGeometry(g);
            n->setFlag(QSGNode::OwnsGeometry);
        }
        if (!n->material()) {
            auto* m = new QSGFlatColorMaterial();
            n->setMaterial(m);
            n->setFlag(QSGNode::OwnsMaterial);
        }
        auto* geom = n->geometry();
        auto* mat = static_cast<QSGFlatColorMaterial*>(n->material());
        mat->setColor(col);
        if (plot_mode_ == "scatter") {
            geom->setDrawingMode(QSGGeometry::DrawPoints);
            geom->setLineWidth(3.5f);
        } else {
            geom->setDrawingMode(QSGGeometry::DrawLineStrip);
            geom->setLineWidth(1.75f);
        }
        const int count = static_cast<int>(pts.size());
        geom->allocate(count);
        const double w = width();
        const double h = height();
        const double rx = std::max(1e-12, range_.width());
        const double ry = std::max(1e-12, range_.height());
        auto* verts = geom->vertexDataAsPoint2D();
        for (int i = 0; i < count; ++i) {
            const float sx = static_cast<float>(((pts[i].x - range_.x_min) / rx) * w);
            const float sy = static_cast<float>((1.0 - (pts[i].y - range_.y_min) / ry) * h);
            verts[i].set(sx, sy);
        }
        n->markDirty(QSGNode::DirtyGeometry | QSGNode::DirtyMaterial);
    };

    if (geometry_dirty_ || root->childCount() == 0) {
        const int needed = extra_series_.empty() ? 1 : static_cast<int>(extra_series_.size());
        while (root->childCount() > needed) {
            auto* ch = root->lastChild();
            root->removeChildNode(ch);
            delete ch;
        }
        while (root->childCount() < needed) {
            root->appendChildNode(new QSGGeometryNode());
        }

        if (extra_series_.empty()) {
            sync_series_node(static_cast<QSGGeometryNode*>(root->firstChild()), decimated_, line_color_);
        } else {
            QSGNode* cur = root->firstChild();
            for (const auto& s : extra_series_) {
                sync_series_node(static_cast<QSGGeometryNode*>(cur), s.decimated, s.color);
                cur = cur->nextSibling();
            }
        }
        geometry_dirty_ = false;
    }

    return root;
}

} // namespace shunya::gui
