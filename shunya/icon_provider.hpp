#pragma once

#include <QQuickImageProvider>
#include <QImage>
#include <QSize>
#include <QString>

namespace shunya::gui {

/**
 * @brief Renders monochrome SVG icons from qrc:/icons/<name>.svg dynamically recolored
 *        for Light & Dark monochrome Theme states via QSvgRenderer.
 *        Image URI format: "image://icons/<icon_name>/<hex_color_without_hash>"
 *        Example: "image://icons/battery/FAFAFA" or "image://icons/save/0A0A0A"
 */
class ShunyaIconProvider : public QQuickImageProvider {
public:
    ShunyaIconProvider();
    QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;
};

} // namespace shunya::gui
