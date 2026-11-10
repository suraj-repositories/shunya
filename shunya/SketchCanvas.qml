import QtQuick
import QtQuick.Layouts
import shunya

Rectangle {
    id: root
    color: "#0d1117"
    border.color: "#30363d"
    border.width: 1
    radius: 6
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        SketchToolbar {
            Layout.fillWidth: true
            canvasNode: canvas2d
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#0d1117"
            border.color: "#282c34"
            clip: true

            ShunyaSketchCanvas2D {
                id: canvas2d
                anchors.fill: parent
            }

            SnapGlyphOverlay {
                anchors.fill: parent
                canvasNode: canvas2d
            }

            DimensionDisplayWidget {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: 8
                remainingDof: 0
                statusMode: "FullyConstrained"
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.MiddleButton | Qt.RightButton
                property bool panning: false
                property point lastPt: Qt.point(0, 0)

                onPositionChanged: function(mouse) {
                    if (panning) {
                        var dx = mouse.x - lastPt.x;
                        var dy = mouse.y - lastPt.y;
                        lastPt = Qt.point(mouse.x, mouse.y);
                        canvas2d.panBy(dx, dy);
                    } else {
                        canvas2d.onHoverMove(mouse.x, mouse.y);
                    }
                }

                onPressed: function(mouse) {
                    if (mouse.button === Qt.MiddleButton) {
                        panning = true;
                        lastPt = Qt.point(mouse.x, mouse.y);
                    } else if (mouse.button === Qt.RightButton) {
                        canvas2d.cancelDrawing();
                    } else if (mouse.button === Qt.LeftButton) {
                        canvas2d.onClickDown(mouse.x, mouse.y);
                    }
                }

                onReleased: function(mouse) {
                    if (mouse.button === Qt.MiddleButton) {
                        panning = false;
                    }
                }

                onWheel: function(wheel) {
                    var factor = wheel.angleDelta.y > 0 ? 1.15 : 0.87;
                    canvas2d.zoomAt(wheel.x, wheel.y, factor);
                }
            }

            // Bottom Coordinate & Loop Status Pill
            Rectangle {
                anchors.bottom: parent.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                height: 22
                color: "#161b22cc"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    Text {
                        text: canvas2d.cursorCoordText
                        color: "#61afef"
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: "Entities: " + canvas2d.entityCount + " | Solid Profile Loops: " + canvas2d.solidLoopCount
                        color: "#98c379"
                        font.pixelSize: 10
                        font.family: "Monospace"
                    }
                }
            }
        }
    }
}
