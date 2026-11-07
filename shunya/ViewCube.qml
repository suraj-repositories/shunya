import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    width: 100
    height: 125
    radius: 8
    color: "#1e2227dd"
    border.color: "#3e4451"
    border.width: 1

    property var controller: typeof viewportController !== "undefined" ? viewportController : null

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 4

        // Title & Projection Toggle
        RowLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                text: "VIEW"
                color: "#61afef"
                font.pixelSize: 10
                font.bold: true
                Layout.fillWidth: true
            }

            Rectangle {
                width: 44
                height: 16
                radius: 3
                color: projMouse.containsMouse ? "#3e4451" : "#282c34"
                border.color: "#4b5263"

                Text {
                    anchors.centerIn: parent
                    text: root.controller && root.controller.isOrthographic ? "ORTHO" : "PERS"
                    color: root.controller && root.controller.isOrthographic ? "#98c379" : "#e5c07b"
                    font.pixelSize: 8
                    font.bold: true
                }

                MouseArea {
                    id: projMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (root.controller) root.controller.toggleProjection();
                    }
                }
            }
        }

        // 3x3 Mini Cube Face Grid
        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: 3
            rows: 3
            rowSpacing: 2
            columnSpacing: 2

            // Row 1
            Item { Layout.fillWidth: true; Layout.fillHeight: true }
            CubeFaceButton { text: "TOP"; faceId: "Top" }
            Item { Layout.fillWidth: true; Layout.fillHeight: true }

            // Row 2
            CubeFaceButton { text: "LFT"; faceId: "Left" }
            CubeFaceButton { text: "FRT"; faceId: "Front" }
            CubeFaceButton { text: "RGT"; faceId: "Right" }

            // Row 3
            CubeFaceButton { text: "ISO"; faceId: "Isometric" }
            CubeFaceButton { text: "BTM"; faceId: "Bottom" }
            CubeFaceButton { text: "BCK"; faceId: "Back" }
        }
    }

    component CubeFaceButton : Rectangle {
        id: btn
        property string text: ""
        property string faceId: ""

        Layout.fillWidth: true
        Layout.fillHeight: true
        radius: 3
        color: {
            if (root.controller && root.controller.currentView === btn.faceId) return "#2c3e50";
            if (fMouse.containsMouse) return "#3b4048";
            return "#282c34";
        }
        border.color: {
            if (root.controller && root.controller.currentView === btn.faceId) return "#61afef";
            return "#3e4451";
        }

        Text {
            anchors.centerIn: parent
            text: btn.text
            color: (root.controller && root.controller.currentView === btn.faceId) ? "#61afef" : "#abb2bf"
            font.pixelSize: 9
            font.bold: true
        }

        MouseArea {
            id: fMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.controller) root.controller.snapToView(btn.faceId);
            }
        }
    }
}
