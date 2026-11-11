import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import shunya

Rectangle {
    id: root
    color: Theme.bgCanvas

    property var objectsList: []
    property var materialsList: []
    property int selectedIdx: 0
    readonly property var selectedObj: (selectedIdx >= 0 && selectedIdx < objectsList.length) ? objectsList[selectedIdx] : null
    property string newName: "Copper_Test_Bar"
    property string newShape: "Cylinder"
    property double newDimX: 0.05
    property double newDimY: 0.25
    property double newDimZ: 0.05
    property double newRadius: 0.004
    property double newTempK: 298.15
    property string feedbackMsg: ""

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        objectsList = pipeline.getObjects();
        materialsList = pipeline.getMaterials();
        if (selectedIdx >= objectsList.length) selectedIdx = Math.max(0, objectsList.length - 1);
        if (preview3d) preview3d.refreshScene();
        if (typeof propertyTreeModel !== "undefined" && selectedObj) {
            propertyTreeModel.inspectObjectProperties(selectedObj.name, selectedObj);
        }
    }

    function currentObj() {
        return (selectedIdx >= 0 && selectedIdx < objectsList.length) ? objectsList[selectedIdx] : null;
    }

    Component.onCompleted: refreshAll()
    Connections {
        target: typeof pipeline !== "undefined" ? pipeline : null
        function onWorkspaceChanged() { root.refreshAll(); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ColumnLayout {
                spacing: 2
                Text { text: "Physical Objects & Multi-Region Modeling"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXl; font.weight: Font.DemiBold; color: Theme.textPrimary }
                Text { text: "Create material-backed objects with live mass, density, electrical resistance, and thermal capacitance"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
            }
            Item { Layout.fillWidth: true }
            Repeater {
                model: ["Copper Wire", "Iron Plate", "Aluminum Frame", "Lithium Foil", "Insulating Spacer"]
                delegate: MonoButton {
                    text: "+ " + modelData
                    compact: true
                    onClicked: {
                        var r = pipeline.addPresetObject(modelData);
                        root.feedbackMsg = r.ok ? ("Added preset " + modelData) : ("Error: " + r.error);
                        root.refreshAll();
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // LEFT: Objects List + Creator
            Rectangle {
                Layout.preferredWidth: 320
                Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8

                    Text { text: "New Physical Object"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    MonoInput { Layout.fillWidth: true; text: root.newName; placeholderText: "Object Name"; onTextEdited: (t) => root.newName = t }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        Repeater {
                            model: ["Box", "Cylinder", "Sphere", "Cone"]
                            delegate: MonoButton {
                                text: modelData; compact: true; active: root.newShape === modelData
                                onClicked: root.newShape = modelData
                            }
                        }
                    }

                    ComboBox {
                        id: matPicker
                        Layout.fillWidth: true
                        model: root.materialsList
                        textRole: "name"
                    }

                    GridLayout {
                        columns: 2; columnSpacing: 6; rowSpacing: 6; Layout.fillWidth: true
                        Text { text: "Dim X / W (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: String(root.newDimX); onTextEdited: (t) => root.newDimX = parseFloat(t) || 0.05 }
                        Text { text: "Dim Y / Len (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: String(root.newDimY); onTextEdited: (t) => root.newDimY = parseFloat(t) || 0.25 }
                        Text { text: "Dim Z / Radius (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.newShape === "Box" ? String(root.newDimZ) : String(root.newRadius); onTextEdited: (t) => { var v = parseFloat(t) || 0.01; root.newDimZ = v; root.newRadius = v; } }
                    }

                    MonoButton {
                        Layout.fillWidth: true
                        text: "Create Material Object"
                        variant: "primary"
                        iconName: "plus"
                        onClicked: {
                            var mid = (matPicker.currentIndex >= 0 && matPicker.currentIndex < root.materialsList.length) ? root.materialsList[matPicker.currentIndex].id : "";
                            var r = pipeline.createObject(root.newName, "Custom", root.newShape, root.newDimX, root.newDimY, root.newDimZ, root.newRadius, mid, root.newTempK, 101325.0);
                            root.feedbackMsg = r.ok ? ("Created '" + root.newName + "'") : ("Error: " + r.error);
                            root.refreshAll();
                        }
                    }

                    Text { visible: root.feedbackMsg !== ""; text: root.feedbackMsg; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }

                    Text { text: "Project Objects (" + root.objectsList.length + ")"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                        model: root.objectsList
                        delegate: Rectangle {
                            width: ListView.view.width; height: 50; radius: Theme.radiusSm
                            color: index === root.selectedIdx ? Theme.bgActive : Theme.bgCanvas
                            border.color: index === root.selectedIdx ? Theme.borderStrong : Theme.borderSubtle; border.width: 1
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 8; spacing: 6
                                ColumnLayout {
                                    Layout.fillWidth: true; spacing: 2
                                    Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.Medium; color: Theme.textPrimary; elide: Text.ElideRight; Layout.fillWidth: true }
                                    Text { text: modelData.shape + "  |  " + Number(modelData.mass).toExponential(2) + " kg"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                }
                                MonoButton { iconName: "trash"; compact: true; onClicked: pipeline.deleteObject(modelData.id) }
                            }
                            MouseArea { anchors.fill: parent; z: -1; onClicked: { root.selectedIdx = index; preview3d.selectedObjectId = modelData.id; } }
                        }
                    }
                }
            }

            // CENTER: Selected Object Live Properties & Regions
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: root.selectedObj ? root.selectedObj.name : "No Object Selected"; font.family: Theme.fontSans; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.fillWidth: true }
                        MonoBadge { text: root.selectedObj && root.selectedObj.hasConductivePath ? "Conductive Path" : "Insulating / Non-Conductive"; tone: root.selectedObj && root.selectedObj.hasConductivePath ? "success" : "neutral" }
                        MonoBadge { text: root.selectedObj && root.selectedObj.isSolid ? "Solid Phase" : "Melted Warning"; tone: root.selectedObj && root.selectedObj.isSolid ? "neutral" : "error" }
                    }

                    GridLayout {
                        visible: root.selectedObj !== null
                        columns: 2; columnSpacing: 16; rowSpacing: 6; Layout.fillWidth: true
                        Text { text: "Volume (m3)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.volume).toExponential(4) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Surface Area (m2)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.surfaceArea).toExponential(4) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Effective Density (kg/m3)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.density).toFixed(2) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Calculated Mass (kg)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.mass).toFixed(6) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; font.weight: Font.Bold; color: Theme.textPrimary }
                        Text { text: "Axial Electrical R (Ohm)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.axialR).toExponential(4) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Thermal Mass m*Cp (J/K)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.thermalMass).toFixed(3) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                    }

                    Text { text: "Material Regions"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                        model: root.selectedObj ? root.selectedObj.regions : []
                        delegate: Rectangle {
                            width: ListView.view.width; height: 32; radius: Theme.radiusSm
                            color: Theme.bgCanvas; border.color: Theme.borderSubtle; border.width: 1
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 8
                                Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; font.weight: Font.Medium; color: Theme.textPrimary; Layout.preferredWidth: 110 }
                                Text { text: modelData.materialName; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textSecondary; Layout.fillWidth: true; elide: Text.ElideRight }
                                Text { text: (modelData.volumeFraction * 100).toFixed(0) + "% vol"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            }
                        }
                    }
                }
            }

            // RIGHT: Real 3D Preview of Workspace Objects
            Rectangle {
                Layout.preferredWidth: 340
                Layout.fillHeight: true
                color: Theme.bgCanvas; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                Viewport3DItem {
                    id: preview3d
                    anchors.fill: parent; anchors.margins: 2
                    darkMode: Theme.isDarkMode
                    MouseArea {
                        anchors.fill: parent
                        property double lx: 0; property double ly: 0
                        onPressed: (m) => { lx = m.x; ly = m.y; }
                        onPositionChanged: (m) => { preview3d.orbitBy((m.x - lx) * 0.5, (m.y - ly) * 0.5); lx = m.x; ly = m.y; }
                        onWheel: (w) => preview3d.zoomBy(w.angleDelta.y > 0 ? 0.9 : 1.1)
                    }
                }
                Text {
                    anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 10
                    text: "3D Object Preview (" + preview3d.meshCount + " meshes, " + preview3d.triangleCount + " tris)"
                    font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted
                }
            }
        }
    }
}
