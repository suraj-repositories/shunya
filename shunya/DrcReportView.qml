import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111827"
    border.color: "#374151"
    border.width: 1
    radius: 6

    property int violationCount: 0
    property var violationsModel: []

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Design Rule Check (DRC)"
                color: "#f9fafb"
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            Label {
                text: root.violationCount === 0 ? "PASS (0 violations)" : ("FAIL (" + root.violationCount + " violations)")
                color: root.violationCount === 0 ? "#34d399" : "#f87171"
                font.bold: true
                font.pixelSize: 12
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.violationsModel
            spacing: 4
            delegate: Rectangle {
                required property var modelData
                width: ListView.view.width
                height: 36
                radius: 4
                color: "#1f2937"
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    Rectangle { width: 8; height: 20; radius: 2; color: "#ef4444" }
                    Label {
                        text: modelData.message + " @ (" + modelData.x.toFixed(2) + ", " + modelData.y.toFixed(2) + ")"
                        color: "#e5e7eb"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
