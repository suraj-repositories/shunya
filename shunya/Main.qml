import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

ApplicationWindow {
    id: window
    width: 1280
    height: 800
    minimumWidth: 800
    minimumHeight: 600
    visible: true
    title: qsTr("Shunya Virtual Engineering & Multiphysics Studio")

    property bool lightMode: Application.styleHints.colorScheme === Qt.Light
    property color bgDark: "#181818"
    property color panelDark: "#222222"
    property color borderDark: "#333333"
    property color textDark: "#e0e0e0"
    property color accentColor: "#00b4d8"
    property color accentGreen: "#06d6a0"
    property color accentRed: "#ef476f"

    property string currentPreset: "Modeling"
    property double simTime: 0.0
    property string simStatus: "Stopped"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // Top Toolbar
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 48
            color: window.panelDark

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 12

                Label {
                    text: "SHUNYA"
                    font.bold: true
                    font.pixelSize: 16
                    color: window.accentColor
                }

                Rectangle { width: 1; height: 24; color: window.borderDark }

                // Preset selector
                RowLayout {
                    spacing: 4
                    Repeater {
                        model: ["Modeling", "Schematic", "Simulation", "Results", "Scripting"]
                        Button {
                            id: presetBtn
                            text: modelData
                            checked: window.currentPreset === modelData
                            onClicked: window.currentPreset = modelData
                            contentItem: Text {
                                text: presetBtn.text
                                color: presetBtn.checked ? window.accentColor : window.textDark
                                font.bold: presetBtn.checked
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                implicitWidth: 80
                                implicitHeight: 28
                                radius: 4
                                color: presetBtn.checked ? "#2c3e50" : "transparent"
                                border.color: presetBtn.checked ? window.accentColor : "transparent"
                            }
                        }
                    }
                }

                Rectangle { width: 1; height: 24; color: window.borderDark }

                // Simulation Controls
                Button {
                    text: "\u25B6 Run"
                    contentItem: Text { text: parent.text; color: window.accentGreen; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 70; implicitHeight: 28; radius: 4; color: "#1b3a2b" }
                    onClicked: { window.simStatus = "Running"; }
                }

                Button {
                    text: "\u23F8 Pause"
                    contentItem: Text { text: parent.text; color: "#ffd166"; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 70; implicitHeight: 28; radius: 4; color: "#3a341b" }
                    onClicked: { window.simStatus = "Paused"; }
                }

                Button {
                    text: "\u23F9 Stop"
                    contentItem: Text { text: parent.text; color: window.accentRed; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { implicitWidth: 70; implicitHeight: 28; radius: 4; color: "#3a1b22" }
                    onClicked: { window.simStatus = "Stopped"; }
                }

                Item { Layout.fillWidth: true }

                // Telemetry badge
                Rectangle {
                    implicitWidth: 180
                    implicitHeight: 28
                    radius: 4
                    color: "#161b22"
                    border.color: window.borderDark
                    RowLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        Label { text: "Time: " + window.simTime.toFixed(3) + " s"; color: "#8b949e"; font.pixelSize: 11 }
                        Label { text: "[" + window.simStatus + "]"; color: window.accentColor; font.bold: true; font.pixelSize: 11 }
                    }
                }
            }
        }

        // =====================================================================
        // Main Workspace Area (3 Columns)
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 1

            // Left Panel: Project Tree & Components
            Rectangle {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                color: window.panelDark

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label { text: "PROJECT EXPLORER"; font.bold: true; font.pixelSize: 11; color: "#8b949e" }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: ListModel {
                            ListElement { name: "Root Scene"; iconText: "\u25BC" }
                            ListElement { name: "  \u2022 Resistor_R1 (100 \u03A9)"; iconText: "" }
                            ListElement { name: "  \u2022 Capacitor_C1 (10 \u00B5F)"; iconText: "" }
                            ListElement { name: "  \u2022 VoltageSource_V1 (5 V)"; iconText: "" }
                            ListElement { name: "Experiments"; iconText: "\u25B6" }
                            ListElement { name: "Run Results"; iconText: "\u25B6" }
                        }
                        delegate: Item {
                            width: parent.width
                            height: 28
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                Label { text: model.name; color: window.textDark; font.pixelSize: 13 }
                            }
                        }
                    }
                }
            }

            // Center Panel: Viewport / Canvas
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: window.bgDark

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#121212"
                        radius: 6
                        border.color: window.borderDark

                        Label {
                            anchors.centerIn: parent
                            text: "[" + window.currentPreset + " Viewport Canvas]\n3D Scientific Mesh & 2D Schematic Ready"
                            horizontalAlignment: Text.AlignHCenter
                            color: "#6e7681"
                            font.pixelSize: 16
                        }
                    }
                }
            }

            // Right Panel: Property Inspector
            Rectangle {
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                color: window.panelDark

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label { text: "PROPERTY INSPECTOR"; font.bold: true; font.pixelSize: 11; color: "#8b949e" }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: ListModel {
                            ListElement { propName: "Element ID"; propVal: "Resistor_R1" }
                            ListElement { propName: "Resistance"; propVal: "100.0 \u03A9" }
                            ListElement { propName: "Tolerance"; propVal: "\u00B11.0 %" }
                            ListElement { propName: "Power Rating"; propVal: "0.25 W" }
                            ListElement { propName: "Fidelity Tier"; propVal: "L2 (Validated)" }
                        }
                        delegate: Rectangle {
                            width: parent.width
                            height: 36
                            color: "transparent"
                            border.color: "#2d333b"
                            radius: 4
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                Label { text: model.propName; color: "#8b949e"; font.pixelSize: 12 }
                                Item { Layout.fillWidth: true }
                                Label { text: model.propVal; color: window.textDark; font.bold: true; font.pixelSize: 12 }
                            }
                        }
                    }
                }
            }
        }

        // =====================================================================
        // Bottom Status & Diagnostics Drawer
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#161b22"
            border.color: window.borderDark

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                spacing: 16

                Label { text: "\u25CF System Ready"; color: window.accentGreen; font.pixelSize: 11 }
                Label { text: "Solvers: RK45 Adaptive / Sparse CSR MNA"; color: "#8b949e"; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Label { text: "C++20 Core Baseline | Zero Leakage Interface"; color: "#6e7681"; font.pixelSize: 11 }
            }
        }
    }
}
