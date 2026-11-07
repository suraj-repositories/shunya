import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var controller: typeof viewportController !== "undefined" ? viewportController : null
    property var proj: ({})
    property string hoveredAxis: "none"
    property string activeAxis: "none"
    property point lastPos: Qt.point(0, 0)
    property bool isDragging: false

    function refreshProjection() {
        if (root.controller && width > 0 && height > 0) {
            root.proj = root.controller.getGizmoProjection(width, height);
            canvas.requestPaint();
        }
    }

    onWidthChanged: refreshProjection()
    onHeightChanged: refreshProjection()

    Connections {
        target: root.controller
        function onTargetTransformChanged() { root.refreshProjection(); }
        function onCameraChanged() { root.refreshProjection(); }
        function onGizmoModeChanged() { root.refreshProjection(); }
    }

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);

            if (!root.proj || !root.proj.visible) return;

            var ox = root.proj.originX;
            var oy = root.proj.originY;
            var xx = root.proj.xTipX;
            var xy = root.proj.xTipY;
            var yx = root.proj.yTipX;
            var yy = root.proj.yTipY;
            var zx = root.proj.zTipX;
            var zy = root.proj.zTipY;

            var mode = root.controller ? root.controller.gizmoMode : "translate";
            var active = root.hoveredAxis;

            function drawAxis(tipX, tipY, color, isAct, label) {
                ctx.beginPath();
                ctx.moveTo(ox, oy);
                ctx.lineTo(tipX, tipY);
                ctx.strokeStyle = isAct ? "#ffffff" : color;
                ctx.lineWidth = isAct ? 4 : 2.5;
                ctx.stroke();

                if (mode === "translate") {
                    // Draw arrowhead
                    var angle = Math.atan2(tipY - oy, tipX - ox);
                    var headLen = 10;
                    ctx.beginPath();
                    ctx.moveTo(tipX, tipY);
                    ctx.lineTo(tipX - headLen * Math.cos(angle - Math.PI / 6), tipY - headLen * Math.sin(angle - Math.PI / 6));
                    ctx.lineTo(tipX - headLen * Math.cos(angle + Math.PI / 6), tipY - headLen * Math.sin(angle + Math.PI / 6));
                    ctx.closePath();
                    ctx.fillStyle = isAct ? "#ffffff" : color;
                    ctx.fill();
                } else if (mode === "scale") {
                    // Draw cube handle
                    var boxSize = 8;
                    ctx.fillStyle = isAct ? "#ffffff" : color;
                    ctx.fillRect(tipX - boxSize / 2, tipY - boxSize / 2, boxSize, boxSize);
                }

                // Label
                ctx.font = "bold 11px sans-serif";
                ctx.fillStyle = isAct ? "#ffffff" : color;
                ctx.fillText(label, tipX + 4, tipY + 4);
            }

            // Draw axes
            drawAxis(xx, xy, "#e06c75", active === "x", "X"); // Red
            drawAxis(yx, yy, "#98c379", active === "y", "Y"); // Green
            drawAxis(zx, zy, "#61afef", active === "z", "Z"); // Blue

            // Draw Center Handle
            ctx.beginPath();
            ctx.arc(ox, oy, 6, 0, 2 * Math.PI);
            ctx.fillStyle = active === "all" ? "#e5c07b" : "#abb2bf";
            ctx.fill();
            ctx.strokeStyle = "#1e2227";
            ctx.lineWidth = 1.5;
            ctx.stroke();
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true

        onPositionChanged: function(mouse) {
            if (root.isDragging) {
                var dx = mouse.x - root.lastPos.x;
                var dy = mouse.y - root.lastPos.y;
                root.lastPos = Qt.point(mouse.x, mouse.y);
                if (root.controller && root.activeAxis !== "none") {
                    root.controller.applyGizmoDrag(root.activeAxis, dx, dy, mouse.x, mouse.y, root.width, root.height);
                }
            } else {
                if (root.controller) {
                    var h = root.controller.testGizmoHover(mouse.x, mouse.y, root.width, root.height);
                    if (root.hoveredAxis !== h) {
                        root.hoveredAxis = h;
                        canvas.requestPaint();
                    }
                }
            }
        }

        onPressed: function(mouse) {
            if (root.hoveredAxis !== "none") {
                root.activeAxis = root.hoveredAxis;
                root.isDragging = true;
                root.lastPos = Qt.point(mouse.x, mouse.y);
            }
        }

        onReleased: function() {
            root.isDragging = false;
            root.activeAxis = "none";
            canvas.requestPaint();
        }
    }

    // Mode Selector Overlay at bottom center
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        anchors.horizontalCenter: parent.horizontalCenter
        height: 32
        width: modeRow.width + 16
        radius: 6
        color: "#1e2227ee"
        border.color: "#3e4451"
        border.width: 1

        Row {
            id: modeRow
            anchors.centerIn: parent
            spacing: 6

            Repeater {
                model: [
                    { label: "Translate (W)", id: "translate" },
                    { label: "Rotate (E)", id: "rotate" },
                    { label: "Scale (R)", id: "scale" }
                ]

                Rectangle {
                    height: 24
                    width: btnText.contentWidth + 14
                    radius: 4
                    color: root.controller && root.controller.gizmoMode === modelData.id ? "#2c3e50" : (mArea.containsMouse ? "#282c34" : "transparent")
                    border.color: root.controller && root.controller.gizmoMode === modelData.id ? "#61afef" : "transparent"

                    Text {
                        id: btnText
                        anchors.centerIn: parent
                        text: modelData.label
                        color: root.controller && root.controller.gizmoMode === modelData.id ? "#61afef" : "#abb2bf"
                        font.pixelSize: 11
                        font.bold: true
                    }

                    MouseArea {
                        id: mArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.controller) root.controller.setGizmoMode(modelData.id);
                        }
                    }
                }
            }
        }
    }
}
