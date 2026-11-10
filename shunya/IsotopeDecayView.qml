import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111622"
    border.color: "#2b3245"
    border.width: 1
    radius: 6

    property string seriesName: "Uranium-238 (4n+2) Series"
    property var chainSteps: [
        { label: "U-238", mode: "α" },
        { label: "Th-234", mode: "β-" },
        { label: "Pa-234m", mode: "β-" },
        { label: "U-234", mode: "α" },
        { label: "Th-230", mode: "α" },
        { label: "Ra-226", mode: "α" },
        { label: "Rn-222", mode: "α" },
        { label: "Po-218", mode: "α" },
        { label: "Pb-214", mode: "β-" },
        { label: "Bi-214", mode: "β-" },
        { label: "Po-214", mode: "α" },
        { label: "Pb-210", mode: "β-" },
        { label: "Bi-210", mode: "β-" },
        { label: "Po-210", mode: "α" },
        { label: "Pb-206", mode: "Stable" }
    ]

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        Label {
            text: root.seriesName
            color: "#e2e8f0"
            font.bold: true
            font.pixelSize: 12
        }

        Flow {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            Repeater {
                model: root.chainSteps
                delegate: Row {
                    required property var modelData
                    spacing: 4
                    Rectangle {
                        width: 66
                        height: 28
                        radius: 4
                        color: modelData.mode === "Stable" ? "#15803d" : "#1e3a8a"
                        border.color: "#60a5fa"
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: "#ffffff"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }
                    Text {
                        visible: modelData.mode !== "Stable"
                        anchors.verticalCenter: parent.verticalCenter
                        text: "→(" + modelData.mode + ")"
                        color: "#94a3b8"
                        font.pixelSize: 10
                    }
                }
            }
        }
    }
}
