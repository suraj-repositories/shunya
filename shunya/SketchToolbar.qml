import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    height: 34
    color: "#1e2227"
    border.color: "#3e4451"
    border.width: 1
    radius: 4

    property var canvasNode: null

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        spacing: 6

        Text {
            text: "2D SKETCHER"
            color: Theme.textPrimary
            font.pixelSize: 10
            font.bold: true
        }

        Rectangle { width: 1; height: 18; color: Theme.borderSubtle }

        // Drawing Tools
        Repeater {
            model: [
                { label: "Select", id: "select" },
                { label: "Line", id: "line" },
                { label: "Circle", id: "circle" },
                { label: "Rect", id: "rect" }
            ]
            Rectangle {
                height: 22
                width: tTxt.contentWidth + 12
                radius: 3
                color: root.canvasNode && root.canvasNode.activeTool === modelData.id ? Theme.btnPrimaryBg : Theme.bgSurface
                border.color: root.canvasNode && root.canvasNode.activeTool === modelData.id ? Theme.borderStrong : Theme.borderDefault
                Text {
                    id: tTxt
                    anchors.centerIn: parent
                    text: modelData.label
                    color: root.canvasNode && root.canvasNode.activeTool === modelData.id ? Theme.btnPrimaryFg : Theme.textSecondary
                    font.pixelSize: 10
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: { if (root.canvasNode) root.canvasNode.setActiveTool(modelData.id); }
                }
            }
        }

        Rectangle { width: 1; height: 18; color: "#3e4451" }

        // Toggles: Snap, Polar, Construction
        Rectangle {
            height: 22
            width: sTxt.contentWidth + 10
            radius: 3
            color: root.canvasNode && root.canvasNode.snapEnabled ? "#1b3a24" : "#282c34"
            border.color: root.canvasNode && root.canvasNode.snapEnabled ? "#98c379" : "#3e4451"
            Text {
                id: sTxt
                anchors.centerIn: parent
                text: "OSNAP"
                color: root.canvasNode && root.canvasNode.snapEnabled ? "#98c379" : "#6e7681"
                font.pixelSize: 9
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (root.canvasNode) root.canvasNode.setSnapEnabled(!root.canvasNode.snapEnabled); }
            }
        }

        Rectangle {
            height: 22
            width: pTxt.contentWidth + 10
            radius: 3
            color: root.canvasNode && root.canvasNode.polarEnabled ? "#1b3a24" : "#282c34"
            border.color: root.canvasNode && root.canvasNode.polarEnabled ? "#98c379" : "#3e4451"
            Text {
                id: pTxt
                anchors.centerIn: parent
                text: "POLAR"
                color: root.canvasNode && root.canvasNode.polarEnabled ? "#98c379" : "#6e7681"
                font.pixelSize: 9
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (root.canvasNode) root.canvasNode.setPolarEnabled(!root.canvasNode.polarEnabled); }
            }
        }

        Rectangle {
            height: 22
            width: cTxt.contentWidth + 10
            radius: 3
            color: root.canvasNode && root.canvasNode.constructionMode ? "#3e3116" : "#282c34"
            border.color: root.canvasNode && root.canvasNode.constructionMode ? "#e5c07b" : "#3e4451"
            Text {
                id: cTxt
                anchors.centerIn: parent
                text: "CONSTR"
                color: root.canvasNode && root.canvasNode.constructionMode ? "#e5c07b" : "#6e7681"
                font.pixelSize: 9
                font.bold: true
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (root.canvasNode) root.canvasNode.setConstructionMode(!root.canvasNode.constructionMode); }
            }
        }

        Item { Layout.fillWidth: true }

        // Demo / Clear buttons
        Rectangle {
            height: 22
            width: demoTxt.contentWidth + 10
            radius: 3
            color: "#282c34"
            border.color: "#3e4451"
            Text {
                id: demoTxt
                anchors.centerIn: parent
                text: "Reset Demo"
                color: "#abb2bf"
                font.pixelSize: 9
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (root.canvasNode) root.canvasNode.loadDemoSketch(); }
            }
        }

        Rectangle {
            height: 22
            width: clrTxt.contentWidth + 10
            radius: 3
            color: "#282c34"
            border.color: "#e06c7588"
            Text {
                id: clrTxt
                anchors.centerIn: parent
                text: "Clear"
                color: "#e06c75"
                font.pixelSize: 9
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: { if (root.canvasNode) root.canvasNode.clearDocument(); }
            }
        }
    }
}
