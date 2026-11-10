import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111622"
    border.color: "#2b3245"
    border.width: 1
    radius: 6

    property string alloyName: "Ti-6Al-4V (Grade 5)"
    property var elementsModel: [
        { symbol: "Ti", pct: 90.0 },
        { symbol: "Al", pct: 6.0 },
        { symbol: "V",  pct: 4.0 }
    ]
    property double computedDensityKgM3: 4430.0

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Alloy Composer • " + root.alloyName
                color: "#f8fafc"
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            Label {
                text: "ρ = " + root.computedDensityKgM3.toFixed(1) + " kg/m³"
                color: "#38bdf8"
                font.pixelSize: 12
                font.bold: true
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.elementsModel
            spacing: 4
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 30
                radius: 4
                color: "#1e293b"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    Label {
                        text: modelData.symbol
                        color: "#f1f5f9"
                        font.bold: true
                        Layout.preferredWidth: 40
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        height: 8
                        radius: 4
                        color: "#0f172a"
                        Rectangle {
                            width: parent.width * Math.min(1.0, modelData.pct / 100.0)
                            height: parent.height
                            radius: 4
                            color: "#3b82f6"
                        }
                    }
                    Label {
                        text: modelData.pct.toFixed(1) + " wt%"
                        color: "#cbd5e1"
                        font.pixelSize: 11
                    }
                }
            }
        }
    }
}
