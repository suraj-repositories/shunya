import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property string currentMaterialId: ""
    property string currentMaterialName: "Select Material..."
    property var materialsList: []
    signal materialSelected(string materialId, string materialName)

    implicitWidth: 260
    implicitHeight: 32

    Component.onCompleted: {
        reloadMaterials();
    }

    function reloadMaterials() {
        if (typeof pipeline !== "undefined" && pipeline.getGroupedMaterialsCatalog) {
            root.materialsList = pipeline.getGroupedMaterialsCatalog();
        }
    }

    // Trigger button
    Rectangle {
        id: triggerBtn
        anchors.fill: parent
        radius: Theme.radiusSm
        color: btnArea.containsMouse ? Theme.bgSurfaceRaised : Theme.bgSurface
        border.color: pickerPopup.opened ? Theme.borderFocus : Theme.borderSubtle
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 6

            ShunyaIcon {
                name: "layers"
                size: 14
                color: Theme.textMuted
            }

            Text {
                Layout.fillWidth: true
                text: root.currentMaterialName.length > 0 ? root.currentMaterialName : "Select Material..."
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                color: root.currentMaterialName.length > 0 ? Theme.textPrimary : Theme.textMuted
                elide: Text.ElideRight
            }

            ShunyaIcon {
                name: pickerPopup.opened ? "chevron-up" : "chevron-down"
                size: 12
                color: Theme.textMuted
            }
        }

        MouseArea {
            id: btnArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                root.reloadMaterials();
                pickerPopup.open();
            }
        }
    }

    // Dropdown Dialog / Popup
    Popup {
        id: pickerPopup
        y: triggerBtn.height + 4
        width: Math.max(340, root.width)
        height: 380
        padding: 8
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: Theme.bgElevated
            radius: Theme.radiusMd
            border.color: Theme.borderSubtle
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 8

            // Search Header
            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                MonoInput {
                    id: searchInput
                    Layout.fillWidth: true
                    placeholderText: "Search elements, compounds, mixes..."
                    onTextEdited: (t) => filterProxy.searchText = t.toLowerCase()
                }

                MonoButton {
                    iconName: "x"
                    compact: true
                    onClicked: pickerPopup.close()
                }
            }

            // Category Filter Pills
            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                property string activeCat: "All"

                Repeater {
                    model: ["All", "Elements", "Compounds", "Project Materials", "My Mixes"]
                    delegate: Rectangle {
                        height: 22
                        width: catText.implicitWidth + 12
                        radius: Theme.radiusSm
                        color: parent.activeCat === modelData ? Theme.bgSurfaceRaised : Theme.bgCanvas
                        border.color: parent.activeCat === modelData ? Theme.borderFocus : Theme.borderSubtle
                        border.width: 1

                        Text {
                            id: catText
                            anchors.centerIn: parent
                            text: modelData
                            font.family: Theme.fontSans
                            font.pixelSize: 10
                            font.weight: parent.parent.activeCat === modelData ? Font.DemiBold : Font.Normal
                            color: parent.parent.activeCat === modelData ? Theme.textPrimary : Theme.textMuted
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                parent.parent.activeCat = modelData;
                                filterProxy.activeCategory = modelData;
                            }
                        }
                    }
                }
            }

            // Filter logic state
            QtObject {
                id: filterProxy
                property string searchText: ""
                property string activeCategory: "All"

                function matches(item) {
                    if (activeCategory !== "All" && item.category !== activeCategory) return false;
                    if (searchText.length === 0) return true;
                    var n = (item.name || "").toLowerCase();
                    var s = (item.symbol || "").toLowerCase();
                    var c = (item.category || "").toLowerCase();
                    return n.indexOf(searchText) !== -1 || s.indexOf(searchText) !== -1 || c.indexOf(searchText) !== -1;
                }
            }

            // Materials List
            ListView {
                id: matListView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: root.materialsList.filter((m) => filterProxy.matches(m))

                delegate: Rectangle {
                    id: itemDelegate
                    width: matListView.width
                    height: 44
                    radius: Theme.radiusSm
                    color: itemArea.containsMouse ? Theme.bgSurfaceRaised : (root.currentMaterialId === modelData.id ? Theme.bgSurfaceActive : Theme.bgCanvas)
                    border.color: root.currentMaterialId === modelData.id ? Theme.borderFocus : Theme.borderSubtle
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 6
                        spacing: 8

                        // Color pill
                        Rectangle {
                            width: 12
                            height: 24
                            radius: 3
                            color: modelData.colorHex || "#4a9eff"
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            RowLayout {
                                spacing: 4
                                Text {
                                    text: modelData.name
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                                MonoBadge {
                                    text: modelData.category
                                    tone: "neutral"
                                }
                            }

                            Text {
                                text: "ρ: " + (modelData.density ? Number(modelData.density).toFixed(0) + " kg/m³" : "—") +
                                      " | k: " + (modelData.thermalK ? Number(modelData.thermalK).toFixed(1) + " W/mK" : "—") +
                                      " | σ: " + (modelData.electricalSigma > 0 ? Number(modelData.electricalSigma).toExponential(1) + " S/m" : "Insulator")
                                font.family: Theme.fontMono
                                font.pixelSize: 10
                                color: Theme.textMuted
                            }
                        }
                    }

                    MouseArea {
                        id: itemArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            root.currentMaterialId = modelData.id;
                            root.currentMaterialName = modelData.name;
                            root.materialSelected(modelData.id, modelData.name);
                            pickerPopup.close();
                        }
                    }
                }
            }
        }
    }
}
