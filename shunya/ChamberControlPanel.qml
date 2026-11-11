import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bgSurface
    border.color: Theme.borderDefault
    border.width: 1
    radius: Theme.radiusMd

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

        Text {
            text: "Hermetic Climatic & Vacuum Chamber"
            color: Theme.textPrimary
            font.family: Theme.fontSans
            font.bold: true
            font.pixelSize: 13
        }

        GridLayout {
            columns: 2
            columnSpacing: 12
            rowSpacing: 6
            Layout.fillWidth: true

            Text { text: "Temperature Setpoint (C):"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: 11 }
            Slider {
                id: tempSlider
                from: -40.0
                to: 125.0
                value: root.targetTemperatureC
                onMoved: root.targetTemperatureC = value
                Layout.fillWidth: true
            }

            Text { text: "Vacuum Pressure (Pa):"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: 11 }
            Text {
                text: root.pressurePa.toExponential(2) + " Pa"
                color: Theme.textPrimary
                font.family: Theme.fontMono
                font.pixelSize: 11
            }

            Text { text: "Measured DUT / Chamber:"; color: Theme.textMuted; font.family: Theme.fontSans; font.pixelSize: 11 }
            Text {
                text: root.currentTemperatureC.toFixed(1) + " C  |  " + root.humidityPct.toFixed(0) + "% RH"
                color: Theme.statusSuccess
                font.family: Theme.fontMono
                font.bold: true
                font.pixelSize: 11
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            MonoButton {
                text: "Apply Conditions"
                compact: true
                onClicked: root.applySetpoint(root.targetTemperatureC, root.pressurePa, root.humidityPct)
            }
            MonoButton {
                text: root.cyclingActive ? "Stop MIL-STD-810" : "MIL-STD-810 Shock"
                compact: true
                active: root.cyclingActive
                onClicked: {
                    root.cyclingActive = !root.cyclingActive
                    root.toggleThermalShock(root.cyclingActive)
                }
            }
        }
    }
}
