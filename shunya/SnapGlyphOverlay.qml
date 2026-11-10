import QtQuick

Item {
    id: root
    property var canvasNode: null

    Connections {
        target: root.canvasNode
        function onCursorChanged() { glyphCanvas.requestPaint(); }
    }

    Canvas {
        id: glyphCanvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            if (!root.canvasNode) return;

            // 1. Dynamic Polar Guide Ray
            if (root.canvasNode.polarActive) {
                ctx.save();
                ctx.strokeStyle = "#98c379cc";
                ctx.lineWidth = 1.2;
                ctx.setLineDash([5, 4]);
                ctx.beginPath();
                ctx.moveTo(root.canvasNode.polarOriginX, root.canvasNode.polarOriginY);
                ctx.lineTo(root.canvasNode.polarEndX, root.canvasNode.polarEndY);
                ctx.stroke();

                ctx.setLineDash([]);
                ctx.fillStyle = "#98c379";
                ctx.font = "bold 10px Monospace";
                ctx.fillText(
                    root.canvasNode.polarAngleDeg.toFixed(0) + "\u00B0",
                    root.canvasNode.polarEndX + 6,
                    root.canvasNode.polarEndY - 6
                );
                ctx.restore();
            }

            // 2. Object Snap Magnetic Glyph
            if (root.canvasNode.snapActive) {
                var sx = root.canvasNode.snapScreenX;
                var sy = root.canvasNode.snapScreenY;
                var type = root.canvasNode.snapTypeName;
                var r = 6;

                ctx.save();
                ctx.strokeStyle = "#e5c07b";
                ctx.lineWidth = 2.0;
                ctx.beginPath();

                if (type === "Endpoint") {
                    ctx.rect(sx - r, sy - r, r * 2, r * 2);
                } else if (type === "Midpoint") {
                    ctx.moveTo(sx, sy - r);
                    ctx.lineTo(sx + r, sy + r);
                    ctx.lineTo(sx - r, sy + r);
                    ctx.closePath();
                } else if (type === "Center") {
                    ctx.arc(sx, sy, r, 0, 2 * Math.PI);
                    ctx.moveTo(sx - 3, sy); ctx.lineTo(sx + 3, sy);
                    ctx.moveTo(sx, sy - 3); ctx.lineTo(sx, sy + 3);
                } else if (type === "Quadrant") {
                    ctx.moveTo(sx, sy - r);
                    ctx.lineTo(sx + r, sy);
                    ctx.lineTo(sx, sy + r);
                    ctx.lineTo(sx - r, sy);
                    ctx.closePath();
                } else if (type === "Intersection") {
                    ctx.moveTo(sx - r, sy - r); ctx.lineTo(sx + r, sy + r);
                    ctx.moveTo(sx + r, sy - r); ctx.lineTo(sx - r, sy + r);
                } else if (type === "Tangent") {
                    ctx.arc(sx, sy, r - 1, 0, 2 * Math.PI);
                    ctx.moveTo(sx - r - 2, sy - r - 1); ctx.lineTo(sx + r + 2, sy - r - 1);
                }
                ctx.stroke();

                // Snap Type Badge
                ctx.fillStyle = "#e5c07b";
                ctx.font = "bold 9px Monospace";
                ctx.fillText(type, sx + 10, sy - 8);
                ctx.restore();
            }
        }
    }
}
