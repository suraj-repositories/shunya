import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string objectId: ""
    property var regionsList: []
    property int selectedRegionIndex: 0
    signal regionSelected(string regionId, int index)
    signal regionsChanged()

    implicitWidth: 320
    implicitHeight: 380

    onObjectIdChanged: reloadRegions()

    function reloadRegions() {
        if (!root.objectId || typeof pipeline === "undefined") return;
        root.regionsList = pipeline.getObjectRegions(root.objectId);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        // Header and + Add Region Action
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "Material Regions & Volumetric Slices"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                Layout.fillWidth: true
            }

            MonoButton {
                text: "+ Add Region"
                iconName: "plus"
                compact: true
                onClicked: {
                    var addRes = pipeline.addObjectRegion(root.objectId, "New Region", "elem:Al", 0.25, "Structural", true);
                    if (addRes.ok) {
                        root.reloadRegions();
                        root.regionsChanged();
                    }
                }
            }
        }

        // Live Volume Distribution Bar (Asserting sum V_i == 100%)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: "Volume Distribution (100% Total)"
                    font.family: Theme.fontSans
                    font.pixelSize: 10
                    color: Theme.textMuted
                    Layout.fillWidth: true
                }
                Text {
                    text: {
                        var sum = 0;
                        for (var i = 0; i < root.regionsList.length; ++i) {
                            sum += (root.regionsList[i].volumeFraction || 0);
                        }
                        return (sum * 100).toFixed(1) + "%";
                    }
                    font.family: Theme.fontMono
                    font.pixelSize: 10
                    color: Theme.accentPrimary
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 12
                radius: 4
                color: Theme.bgCanvas
                border.color: Theme.borderSubtle
                border.width: 1
                clip: true

                Row {
                    anchors.fill: parent
                    spacing: 1

                    Repeater {
                        model: root.regionsList
                        delegate: Rectangle {
                            height: parent.height
                            width: Math.max(2, (modelData.volumeFraction || 0) * parent.width)
                            color: modelData.colorHex || Theme.accentPrimary
                            opacity: modelData.isVisible ? 1.0 : 0.3

                            ToolTip.visible: segHover.containsMouse
                            ToolTip.text: modelData.name + ": " + ((modelData.volumeFraction || 0) * 100).toFixed(1) + "%"

                            MouseArea {
                                id: segHover
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    root.selectedRegionIndex = index;
                                    root.regionSelected(modelData.id, index);
                                }
                            }
                        }
                    }
                }
            }
        }

        // Region List
        ListView {
            id: regionListView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 6
            model: root.regionsList

            delegate: Rectangle {
                id: regionCard
                width: regionListView.width
                implicitHeight: cardLayout.implicitHeight + 12
                radius: Theme.radiusSm
                color: root.selectedRegionIndex === index ? Theme.bgSurfaceActive : Theme.bgSurface
                border.color: root.selectedRegionIndex === index ? Theme.borderFocus : Theme.borderSubtle
                border.width: 1

                ColumnLayout {
                    id: cardLayout
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 6

                    // Row 1: Color indicator, Name, Role badge, Display toggles (Eye, Solo, Lock), Trash
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        Rectangle {
                            width: 10
                            height: 18
                            radius: 2
                            color: modelData.colorHex || "#4a9eff"
                        }

                        Text {
                            text: modelData.name
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        MonoBadge {
                            text: modelData.role || "Structural"
                            tone: modelData.isConductive ? "success" : "neutral"
                        }

                        // Eye (Visibility Toggle)
                        MonoButton {
                            iconName: modelData.isVisible ? "eye" : "eye-off"
                            compact: true
                            onClicked: {
                                pipeline.updateObjectRegion(root.objectId, modelData.id, { "isVisible": !modelData.isVisible });
                                root.reloadRegions();
                                root.regionsChanged();
                            }
                        }

                        // Lock Toggle
                        MonoButton {
                            iconName: modelData.isLocked ? "lock" : "unlock"
                            compact: true
                            onClicked: {
                                pipeline.updateObjectRegion(root.objectId, modelData.id, { "isLocked": !modelData.isLocked });
                                root.reloadRegions();
                                root.regionsChanged();
                            }
                        }

                        // Trash
                        MonoButton {
                            iconName: "trash"
                            compact: true
                            visible: root.regionsList.length > 1
                            onClicked: {
                                pipeline.removeObjectRegion(root.objectId, modelData.id);
                                root.reloadRegions();
                                root.regionsChanged();
                            }
                        }
                    }

                    // Row 2: Material Picker Combo & Volume Fraction
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        MaterialPickerCombo {
                            Layout.fillWidth: true
                            currentMaterialId: modelData.materialId || ""
                            currentMaterialName: modelData.materialName || "Select Material..."
                            onMaterialSelected: (mid, mname) => {
                                pipeline.updateObjectRegion(root.objectId, modelData.id, { "materialId": mid });
                                root.reloadRegions();
                                root.regionsChanged();
                            }
                        }

                        // Volume percentage badge / control
                        RowLayout {
                            spacing: 2
                            MonoButton {
                                text: "-"
                                compact: true
                                onClicked: {
                                    var newV = Math.max(0.05, (modelData.volumeFraction || 0.1) - 0.05);
                                    pipeline.updateObjectRegion(root.objectId, modelData.id, { "volumeFraction": newV });
                                    root.reloadRegions();
                                    root.regionsChanged();
                                }
                            }
                            MonoBadge {
                                text: ((modelData.volumeFraction || 0) * 100).toFixed(0) + "% vol"
                                tone: "neutral"
                            }
                            MonoButton {
                                text: "+"
                                compact: true
                                onClicked: {
                                    var newV = Math.min(0.95, (modelData.volumeFraction || 0.1) + 0.05);
                                    pipeline.updateObjectRegion(root.objectId, modelData.id, { "volumeFraction": newV });
                                    root.reloadRegions();
                                    root.regionsChanged();
                                }
                            }
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    z: -1
                    onClicked: {
                        root.selectedRegionIndex = index;
                        root.regionSelected(modelData.id, index);
                    }
                }
            }
        }
    }
}
