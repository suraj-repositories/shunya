import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111622"
    border.color: "#2b3245"
    border.width: 1
    radius: 6

    property double totalThicknessMm: 1.60
    property double computedZ0Ohm: 50.0
    property var layersModel: [
        { name: "F.Cu",      kind: "Signal",     thicknessMm: 0.035, color: "#ef4444" },
        { name: "Prepreg 1", kind: "Prepreg",    thicknessMm: 0.200, color: "#15803d" },
        { name: "In1.GND",   kind: "Ground",     thicknessMm: 0.035, color: "#f59e0b" },
        { name: "Core FR4",  kind: "Core",       thicknessMm: 1.060, color: "#166534" },
        { name: "In2.PWR",   kind: "Power",      thicknessMm: 0.035, color: "#eab308" },
        { name: "Prepreg 2", kind: "Prepreg",    thicknessMm: 0.200, color: "#15803d" },
        { name: "B.Cu",      kind: "Signal",     thicknessMm: 0.035, color: "#3b82f6" }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "PCB Stackup Editor"
                color: "#f8fafc"
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            Label {
                text: "Total: " + root.totalThicknessMm.toFixed(2) + " mm | Z0: " + root.computedZ0Ohm.toFixed(1) + " Ω"
                color: "#38bdf8"
                font.pixelSize: 12
                font.bold: true
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.layersModel
            spacing: 3
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: Math.max(22, Math.min(48, 18 + modelData.thicknessMm * 28))
                color: modelData.color
                radius: 3
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    Label {
                        text: modelData.name + " (" + modelData.kind + ")"
                        color: "#ffffff"
                        font.bold: true
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }
                    Label {
                        text: modelData.thicknessMm.toFixed(3) + " mm"
                        color: "#f8fafc"
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
