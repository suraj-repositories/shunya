import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#21252b"

    property var model: typeof propertyTreeModel !== "undefined" ? propertyTreeModel : null

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Inspector Header
        Rectangle {
            Layout.fillWidth: true
            height: 38
            color: "#1e2227"
            border.color: "#181a1f"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 8
                spacing: 8

                Text {
                    text: root.model ? root.model.inspectedTitle : "Property Inspector"
                    color: "#d7dae0"
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }

                // Demo object switcher button
                Rectangle {
                    width: 65
                    height: 22
                    color: demoMouse.containsMouse ? "#3b4048" : "#2c313a"
                    radius: 3
                    border.color: "#3e4451"

                    Text {
                        anchors.centerIn: parent
                        text: "Switch Demo"
                        color: "#98c379"
                        font.pixelSize: 10
                    }

                    MouseArea {
                        id: demoMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.model) {
                                if (root.model.inspectedTitle.indexOf("Resistor") >= 0) {
                                    root.model.inspectDemoObject("Body");
                                } else {
                                    root.model.inspectDemoObject("Resistor");
                                }
                            }
                        }
                    }
                }
            }
        }

        // Search Filter Bar
        Rectangle {
            Layout.fillWidth: true
            height: 32
            color: "#282c34"
            border.color: "#181a1f"
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 6

                ShunyaIcon { name: "search"; size: 12; muted: true }

                TextInput {
                    id: searchInput
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font.pixelSize: 11
                    selectByMouse: true

                    Text {
                        text: "Filter properties..."
                        color: Theme.textSubtle
                        font.pixelSize: 11
                        font.italic: true
                        visible: parent.text.length === 0 && !parent.activeFocus
                    }

                    onTextChanged: {
                        if (root.model) {
                            root.model.filterText = text
                        }
                    }
                }

                ShunyaIcon {
                    name: "close"
                    size: 11
                    color: clearMouse.containsMouse ? Theme.statusError : Theme.textMuted
                    visible: searchInput.text.length > 0
                    Layout.alignment: Qt.AlignVCenter

                    MouseArea {
                        id: clearMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            searchInput.text = ""
                            if (root.model) root.model.filterText = ""
                        }
                    }
                }
            }
        }

        // Property Item List
        ListView {
            id: propertyList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.model
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                active: true
                policy: ScrollBar.AsNeeded
            }

            delegate: Item {
                width: propertyList.width
                height: model.isCategory ? 26 : 34

                // 1. Category Header Row
                Rectangle {
                    visible: model.isCategory
                    anchors.fill: parent
                    color: catMouse.containsMouse ? "#2d323b" : "#282c34"
                    border.color: "#181a1f"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        spacing: 6

                        ShunyaIcon {
                            name: model.isExpanded ? "chevron-down" : "chevron-right"
                            size: 12
                            muted: true
                        }

                        Text {
                            text: model.categoryDisplayName || model.category
                            color: "#d7dae0"
                            font.pixelSize: 11
                            font.bold: true
                            Layout.fillWidth: true
                        }
                    }

                    MouseArea {
                        id: catMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.model) {
                                root.model.toggleCategory(model.category)
                            }
                        }
                    }
                }

                // 2. Property Value Row
                Rectangle {
                    visible: !model.isCategory
                    anchors.fill: parent
                    color: propMouse.containsMouse ? "#2c313a" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 8
                        spacing: 8

                        // Property Label
                        Row {
                            Layout.preferredWidth: 120
                            Layout.fillHeight: true
                            spacing: 4

                            Text {
                                text: model.displayName || model.propertyName
                                color: model.isReadOnly ? "#7f848e" : "#abb2bf"
                                font.pixelSize: 11
                                font.bold: false
                                elide: Text.ElideRight
                                width: parent.width - (model.formula ? 16 : 0)
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            // Formula Indicator
                            Text {
                                text: "ƒ"
                                color: "#98c379"
                                font.pixelSize: 11
                                font.bold: true
                                visible: model.formula !== "" && model.formula !== undefined
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        // Property Editor Delegate Factory
                        PropertyDelegateFactory {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 26
                            visualType: model.visualType
                            propertyValue: model.value
                            unitSymbol: model.unitSymbol || ""
                            allowedUnits: model.allowedUnits || []
                            formula: model.formula || ""
                            minValue: model.minValue
                            maxValue: model.maxValue
                            enumOptions: model.enumOptions || []
                            readOnly: model.isReadOnly || false

                            onValueCommitted: function(newVal) {
                                if (root.model) {
                                    root.model.setPropertyValue(model.propertyId, newVal)
                                }
                            }

                            onFormulaCommitted: function(newForm) {
                                if (root.model) {
                                    root.model.setPropertyFormula(model.propertyId, newForm)
                                }
                            }

                            onUnitCommitted: function(newUnit) {
                                if (root.model) {
                                    root.model.setPropertyUnit(model.propertyId, newUnit)
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: propMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                        onClicked: function(mouse) { mouse.accepted = false }
                    }
                }
            }
        }
    }
}
