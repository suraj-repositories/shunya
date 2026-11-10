import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0f172a"
    border.color: "#334155"
    border.width: 1
    radius: 6

    property string footprintName: "SOIC-8"
    property double bodyWidthMm: 3.9
    property double bodyHeightMm: 4.9
    property var padsModel: []

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 6

        Label {
            text: "IPC-7351 Footprint • " + root.footprintName
            color: "#f8fafc"
            font.bold: true
            font.pixelSize: 12
        }

        Canvas {
            id: fpCanvas
            Layout.fillWidth: true
            Layout.fillHeight: true

            onPaint: {
                const ctx = getContext("2d");
                ctx.reset();
                const cx = width * 0.5;
                const cy = height * 0.5;
                const px = Math.min(width, height) / Math.max(10.0, root.bodyHeightMm * 2.2);

                // Silkscreen body
                ctx.strokeStyle = "#facc15";
                ctx.lineWidth = 1.5;
                ctx.strokeRect(cx - 0.5 * root.bodyWidthMm * px,
                               cy - 0.5 * root.bodyHeightMm * px,
                               root.bodyWidthMm * px,
                               root.bodyHeightMm * px);

                // SMD/THT pads
                ctx.fillStyle = "#ef4444";
                for (let i = 0; i < root.padsModel.length; ++i) {
                    const p = root.padsModel[i];
                    const pw = p.w * px;
                    const ph = p.h * px;
                    ctx.fillRect(cx + p.x * px - 0.5 * pw,
                                 cy - p.y * px - 0.5 * ph,
                                 pw, ph);
                }
            }
        }
    }
}
