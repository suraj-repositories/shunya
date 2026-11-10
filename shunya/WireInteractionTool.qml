import QtQuick
import QtQuick.Controls

Item {
    id: root
    property bool active: false
    property double snapGridMm: 2.54
    property double startXmm: 0.0
    property double startYmm: 0.0
    property double endXmm: 0.0
    property double endYmm: 0.0
    property bool horizontalFirst: true

    signal wireCommitted(double sx, double sy, double ex, double ey, bool hFirst)

    Canvas {
        id: previewCanvas
        anchors.fill: parent
        visible: root.active
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            if (!root.active) return;
            ctx.strokeStyle = "#38bdf8";
            ctx.setLineDash([4, 4]);
            ctx.lineWidth = 2.0;
            ctx.beginPath();
            ctx.moveTo(root.startXmm, root.startYmm);
            if (root.horizontalFirst) {
                ctx.lineTo(root.endXmm, root.startYmm);
            } else {
                ctx.lineTo(root.startXmm, root.endYmm);
            }
            ctx.lineTo(root.endXmm, root.endYmm);
            ctx.stroke();
        }
    }
}
