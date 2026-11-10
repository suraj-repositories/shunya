import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#0f172a"
    border.color: "#334155"
    border.width: 1
    radius: 6

    property real inletSpeedMs: 10.0
    property real angleOfAttackDeg: 5.0
    property real liftCoeffCl: 0.0
    property real dragCoeffCd: 0.0

    signal startTunnelRun(real inletSpeedMs, real angleOfAttackDeg)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Aerodynamic Wind Tunnel Test Section"
                color: "#f8fafc"
                font.bold: true
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            Label {
                text: "CL: " + root.liftCoeffCl.toFixed(3) + "  |  CD: " + root.dragCoeffCd.toFixed(3)
                color: "#38bdf8"
                font.bold: true
                font.pixelSize: 12
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#020617"
            border.color: "#1e293b"
            radius: 4

            Label {
                anchors.centerIn: parent
                text: "Inlet U = " + root.inletSpeedMs.toFixed(1) + " m/s   |   AoA = " + root.angleOfAttackDeg.toFixed(1) + " deg"
                color: "#94a3b8"
                font.pixelSize: 12
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Label { text: "AoA (deg):"; color: "#cbd5e1"; font.pixelSize: 11 }
            Slider {
                from: -15.0
                to: 15.0
                value: root.angleOfAttackDeg
                onMoved: root.angleOfAttackDeg = value
                Layout.fillWidth: true
            }
            Button {
                text: "Run Sweep"
                onClicked: root.startTunnelRun(root.inletSpeedMs, root.angleOfAttackDeg)
            }
        }
    }
}
