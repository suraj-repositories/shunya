import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#111827"
    border.color: "#374151"
    border.width: 1
    radius: 6

    property real targetTemperatureC: 25.0
    property real currentTemperatureC: 25.0
    property real pressurePa: 101325.0
    property real humidityPct: 50.0
    property bool cyclingActive: false

    signal applySetpoint(real temperatureC, real pressurePa, real humidityPct)
    signal toggleThermalShock(bool active)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Label {
            text: "Hermetic Climatic & Vacuum Chamber"
            color: "#f9fafb"
            font.bold: true
            font.pixelSize: 13
        }

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 6
            Layout.fillWidth: true

            Label { text: "Temperature Setpoint (°C):"; color: "#9ca3af"; font.pixelSize: 11 }
            Slider {
                id: tempSlider
                from: -40.0
                to: 125.0
                value: root.targetTemperatureC
                onMoved: root.targetTemperatureC = value
                Layout.fillWidth: true
            }

            Label { text: "Vacuum Pressure (Pa):"; color: "#9ca3af"; font.pixelSize: 11 }
            Label {
                text: root.pressurePa.toExponential(2) + " Pa"
                color: "#60a5fa"
                font.pixelSize: 11
            }

            Label { text: "Measured DUT / Chamber:"; color: "#9ca3af"; font.pixelSize: 11 }
            Label {
                text: root.currentTemperatureC.toFixed(1) + " °C  |  " + root.humidityPct.toFixed(0) + "% RH"
                color: "#34d399"
                font.bold: true
                font.pixelSize: 11
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "Apply Conditions"
                onClicked: root.applySetpoint(root.targetTemperatureC, root.pressurePa, root.humidityPct)
            }
            Button {
                text: root.cyclingActive ? "Stop MIL-STD-810" : "MIL-STD-810 Shock"
                onClicked: {
                    root.cyclingActive = !root.cyclingActive
                    root.toggleThermalShock(root.cyclingActive)
                }
            }
        }
    }
}
