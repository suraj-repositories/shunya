import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#21252b"

    property var controller: typeof multiPropertyController !== "undefined" ? multiPropertyController : null

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header Banner
        Rectangle {
            Layout.fillWidth: true
            height: 42
            color: "#1e2227"
            border.color: "#181a1f"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: "MULTI-PROPERTY INSPECTOR"
                        color: Theme.textPrimary
                        font.pixelSize: 11
                        font.bold: true
                    }
                    Text {
                        text: root.controller ? root.controller.selectionDescription : "No Multi-Selection"
                        color: Theme.textMuted
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }

                // Entity count pill
                Rectangle {
                    width: countText.contentWidth + 12
                    height: 20
                    radius: 10
                    color: Theme.bgActive
                    border.color: Theme.borderStrong

                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: root.controller ? (root.controller.selectedEntityCount + " entities") : "0"
                        color: Theme.textPrimary
                        font.pixelSize: 10
                        font.bold: true
                    }
                }
            }
        }

        // Demo Presets Bar
        Rectangle {
            Layout.fillWidth: true
            height: 28
            color: "#282c34"
            border.color: "#181a1f"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                Repeater {
                    model: [
                        { label: "50 Identical", id: "50_resistors_identical" },
                        { label: "50 Mixed", id: "50_resistors_divergent" },
                        { label: "R + C", id: "resistor_and_capacitor" }
                    ]

                    Rectangle {
                        height: 20
                        width: dText.contentWidth + 10
                        color: dMouse.containsMouse ? "#3e4451" : "#21252b"
                        radius: 2
                        border.color: "#3e4451"

                        Text {
                            id: dText
                            anchors.centerIn: parent
                            text: modelData.label
                            color: "#98c379"
                            font.pixelSize: 9
                        }

                        MouseArea {
                            id: dMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (root.controller) root.controller.selectDemoMultiSet(modelData.id)
                            }
                        }
                    }
                }
            }
        }

        // Intersected Properties List
        ListView {
            id: propList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.controller
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
            }

            delegate: Rectangle {
                width: propList.width
                height: 36
                color: itemMouse.containsMouse ? "#2c313a" : "transparent"
                border.color: "#282c34"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
                    spacing: 8

                    // Property Label & Category
                    ColumnLayout {
                        Layout.preferredWidth: 120
                        spacing: 1

                        Text {
                            text: model.displayName || model.propertyName
                            color: model.isReadOnly ? "#7f848e" : "#d7dae0"
                            font.pixelSize: 11
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Text {
                            text: model.category
                            color: "#5c6370"
                            font.pixelSize: 9
                        }
                    }

                    // Value Editor or Mixed Badge
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26

                        // When values diverge across multi-selection
                        Rectangle {
                            visible: model.isMixed
                            anchors.fill: parent
                            color: "#3d2b1f"
                            radius: 3
                            border.color: "#d19a66"

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                anchors.rightMargin: 6
                                spacing: 4

                                Text {
                                    text: "<mixed values>"
                                    color: "#e5c07b"
                                    font.pixelSize: 11
                                    font.italic: true
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: "Edit to Unify"
                                    color: "#61afef"
                                    font.pixelSize: 9
                                    font.underline: true
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    // Clicking mixed tag opens prompt/input
                                    if (root.controller) {
                                        root.controller.setBatchPropertyValue(model.propertyId, 10.0)
                                    }
                                }
                            }
                        }

                        // When values are identical across multi-selection
                        PropertyDelegateFactory {
                            visible: !model.isMixed
                            anchors.fill: parent
                            visualType: model.visualType
                            propertyValue: model.value
                            unitSymbol: model.unitSymbol || ""
                            allowedUnits: model.allowedUnits || []
                            readOnly: model.isReadOnly || false

                            onValueCommitted: function(newVal) {
                                if (root.controller) {
                                    root.controller.setBatchPropertyValue(model.propertyId, newVal)
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    id: itemMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    propagateComposedEvents: true
                    onClicked: function(mouse) { mouse.accepted = false }
                }
            }
        }
    }
}
