#include "icon_provider.hpp"

#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace shunya::gui {

ShunyaIconProvider::ShunyaIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage ShunyaIconProvider::requestImage(const QString& id, QSize* size, const QSize& requestedSize) {
    const QStringList parts = id.split('/');
    const QString name = parts.isEmpty() ? QStringLiteral("info") : parts[0];
    QString hex = (parts.size() >= 2 && !parts[1].isEmpty()) ? parts[1] : QStringLiteral("FAFAFA");
    if (!hex.startsWith('#')) {
        hex.prepend('#');
    }

    const int w = (requestedSize.width() > 0) ? requestedSize.width() : 24;
    const int h = (requestedSize.height() > 0) ? requestedSize.height() : 24;
    if (size) {
        *size = QSize(w, h);
    }

    QFile f(QStringLiteral(":/icons/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly)) {
        f.setFileName(QStringLiteral(":/icons/info.svg"));
        if (!f.open(QIODevice::ReadOnly)) {
            QImage fallback(w, h, QImage::Format_ARGB32_Premultiplied);
            fallback.fill(Qt::transparent);
            return fallback;
        }
    }

    QByteArray svg = f.readAll();
    svg.replace("#000000", hex.toUtf8());
    svg.replace("currentColor", hex.toUtf8());

    QSvgRenderer renderer(svg);
    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    if (renderer.isValid()) {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&p, QRectF(0, 0, w, h));
    }
    return img;
}

} // namespace shunya::gui
