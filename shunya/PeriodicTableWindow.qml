import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: Theme.bgSurface
    width: 1080
    height: 620

    signal elementActivated(int atomicNumber)
    property var controller: (typeof periodicTableController !== "undefined") ? periodicTableController : null

    focus: true
    Keys.onPressed: (event) => {
        if (!root.controller) return;
        var currentZ = root.controller.selectedAtomicNumber;
        if (event.key === Qt.Key_Left && currentZ > 1) {
            root.controller.setSelectedAtomicNumber(currentZ - 1);
            root.elementActivated(currentZ - 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Right && currentZ < 118) {
            root.controller.setSelectedAtomicNumber(currentZ + 1);
            root.elementActivated(currentZ + 1);
            event.accepted = true;
        } else if (event.key === Qt.Key_Up && currentZ > 8) {
            var prevZ = Math.max(1, currentZ - (currentZ > 56 ? 32 : (currentZ > 18 ? 18 : 8)));
            root.controller.setSelectedAtomicNumber(prevZ);
            root.elementActivated(prevZ);
            event.accepted = true;
        } else if (event.key === Qt.Key_Down && currentZ < 110) {
            var nextZ = Math.min(118, currentZ + (currentZ > 56 ? 32 : (currentZ > 18 ? 18 : 8)));
            root.controller.setSelectedAtomicNumber(nextZ);
            root.elementActivated(nextZ);
            event.accepted = true;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Label {
                text: "Periodic Table of Elements"
                color: Theme.textPrimary
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontLg
                font.bold: true
            }

            ComboBox {
                id: propCombo
                implicitHeight: 36
                implicitWidth: 170
                model: [
                    "FamilyCategory",
                    "BlockCategory",
                    "AtomicMass",
                    "Electronegativity",
                    "FirstIonizationEnergy",
                    "CovalentRadius",
                    "MeltingPointK",
                    "DensityGcm3",
                    "ThermalConductivity"
                ]
                background: Rectangle {
                    implicitHeight: 36
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: propCombo.activeFocus ? Theme.borderFocus : Theme.borderDefault
                    border.width: 1
                }
                contentItem: Text {
                    text: propCombo.displayText
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 10
                    rightPadding: 26
                    elide: Text.ElideRight
                }
                indicator: Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "▾"
                    font.pixelSize: 12
                    color: Theme.textMuted
                }
                popup: Popup {
                    y: propCombo.height + 2
                    width: propCombo.width
                    implicitHeight: contentItem.implicitHeight + 4
                    padding: 2
                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: propCombo.popup.visible ? propCombo.delegateModel : null
                        currentIndex: propCombo.highlightedIndex
                    }
                    background: Rectangle {
                        color: Theme.bgElevated
                        border.color: Theme.borderDefault
                        border.width: 1
                        radius: Theme.radiusSm
                    }
                }
                delegate: ItemDelegate {
                    width: propCombo.width
                    height: 32
                    highlighted: propCombo.highlightedIndex === index
                    contentItem: Text {
                        text: modelData
                        color: highlighted ? Theme.textPrimary : Theme.textSecondary
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSm
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    background: Rectangle {
                        color: highlighted ? Theme.bgActive : (hovered ? Theme.bgHover : "transparent")
                    }
                }
                onActivated: {
                    if (root.controller) {
                        root.controller.setActiveProperty(currentText);
                    }
                }
            }

            ComboBox {
                id: blockCombo
                implicitHeight: 36
                implicitWidth: 80
                model: ["All", "s", "p", "d", "f"]
                background: Rectangle {
                    implicitHeight: 36
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: blockCombo.activeFocus ? Theme.borderFocus : Theme.borderDefault
                    border.width: 1
                }
                contentItem: Text {
                    text: blockCombo.displayText
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 10
                    rightPadding: 20
                }
                indicator: Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: "▾"
                    font.pixelSize: 12
                    color: Theme.textMuted
                }
                popup: Popup {
                    y: blockCombo.height + 2
                    width: blockCombo.width
                    implicitHeight: contentItem.implicitHeight + 4
                    padding: 2
                    contentItem: ListView {
                        clip: true
                        implicitHeight: contentHeight
                        model: blockCombo.popup.visible ? blockCombo.delegateModel : null
                        currentIndex: blockCombo.highlightedIndex
                    }
                    background: Rectangle {
                        color: Theme.bgElevated
                        border.color: Theme.borderDefault
                        border.width: 1
                        radius: Theme.radiusSm
                    }
                }
                delegate: ItemDelegate {
                    width: blockCombo.width
                    height: 30
                    highlighted: blockCombo.highlightedIndex === index
                    contentItem: Text {
                        text: modelData
                        color: highlighted ? Theme.textPrimary : Theme.textSecondary
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSm
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 8
                    }
                    background: Rectangle {
                        color: highlighted ? Theme.bgActive : (hovered ? Theme.bgHover : "transparent")
                    }
                }
                onActivated: {
                    if (root.controller) {
                        root.controller.setBlockFilter(currentText);
                    }
                }
            }

            CheckBox {
                id: fBlockCheck
                checked: root.controller ? root.controller.hideFBlock : false
                indicator: Rectangle {
                    implicitWidth: 18
                    implicitHeight: 18
                    x: fBlockCheck.leftPadding
                    anchors.verticalCenter: parent.verticalCenter
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: fBlockCheck.checked ? Theme.borderFocus : Theme.borderDefault
                    border.width: 1

                    Rectangle {
                        width: 10
                        height: 10
                        anchors.centerIn: parent
                        radius: 2
                        color: Theme.textPrimary
                        visible: fBlockCheck.checked
                    }
                }
                contentItem: Text {
                    text: "Hide f-block"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: fBlockCheck.indicator.width + 6
                }
                onToggled: {
                    if (root.controller) {
                        root.controller.setHideFBlock(checked);
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        HeatmapLegend {
            Layout.fillWidth: true
            visible: root.controller &&
                     root.controller.activeProperty !== "FamilyCategory" &&
                     root.controller.activeProperty !== "BlockCategory"
            propertyTitle: root.controller ? root.controller.activeProperty : "Property"
            minValue: root.controller ? root.controller.minRange : 0.0
            maxValue: root.controller ? root.controller.maxRange : 1.0
            unitLabel: root.controller ? root.controller.rangeUnit : ""
        }

        Item {
            id: gridContainer
            Layout.fillWidth: true
            Layout.fillHeight: true

            property real cellW: (width - 17 * 4) / 18.0
            property real cellH: (height - 8 * 4) / 9.0

            Repeater {
                model: root.controller ? root.controller.cells : []
                delegate: ElementCell {
                    required property var modelData
                    x: modelData.col * (gridContainer.cellW + 4)
                    y: modelData.row * (gridContainer.cellH + 4)
                    width: gridContainer.cellW
                    height: gridContainer.cellH
                    visible: modelData.visible
                    atomicNumber: modelData.z
                    symbol: modelData.symbol
                    elementName: modelData.name
                    atomicMass: modelData.mass
                    cellColor: modelData.colorHex
                    valueLabel: modelData.valueLabel
                    isHighlighted: modelData.highlighted
                    isSelected: root.controller && root.controller.selectedAtomicNumber === modelData.z
                    onCellClicked: (z) => {
                        if (root.controller) {
                            root.controller.setSelectedAtomicNumber(z);
                        }
                        root.elementActivated(z);
                    }
                }
            }
        }
    }
}
