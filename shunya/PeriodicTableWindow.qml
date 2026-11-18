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

            MonoComboBox {
                id: propCombo
                Layout.preferredWidth: 170
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
                onActivated: (idx, val) => {
                    if (root.controller) {
                        root.controller.setActiveProperty(val);
                    }
                }
            }

            MonoComboBox {
                id: blockCombo
                Layout.preferredWidth: 80
                model: ["All", "s", "p", "d", "f"]
                onActivated: (idx, val) => {
                    if (root.controller) {
                        root.controller.setBlockFilter(val);
                    }
                }
            }

            MonoCheckBox {
                id: fBlockCheck
                text: "Hide f-block"
                checked: root.controller ? root.controller.hideFBlock : false
                onToggled: (chk) => {
                    if (root.controller) {
                        root.controller.setHideFBlock(chk);
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
