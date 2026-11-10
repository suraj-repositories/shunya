import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111622"
    border.color: "#2b3245"
    border.width: 1
    radius: 4
    implicitHeight: 36

    property double minValue: 0.0
    property double maxValue: 100.0
    property string unitLabel: ""
    property string propertyTitle: "Heatmap"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 10

        Label {
            text: root.propertyTitle
            color: "#e2e8f0"
            font.pixelSize: 11
            font.bold: true
        }
        Label {
            text: root.minValue.toFixed(2) + (root.unitLabel !== "" ? (" " + root.unitLabel) : "")
            color: "#93c5fd"
            font.pixelSize: 11
        }
        Rectangle {
            Layout.fillWidth: true
            height: 12
            radius: 3
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#3b4cc0" }
                GradientStop { position: 0.5; color: "#dddcdc" }
                GradientStop { position: 1.0; color: "#b40426" }
            }
        }
        Label {
            text: root.maxValue.toFixed(2) + (root.unitLabel !== "" ? (" " + root.unitLabel) : "")
            color: "#fca5a5"
            font.pixelSize: 11
        }
    }
}
