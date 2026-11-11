import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import shunya

Rectangle {
    id: root
    color: Theme.bgCanvas

    property var objectsList: []
    property int selectedIdx: 0
    property var meshInfo: ({})
    readonly property var selectedObj: (selectedIdx >= 0 && selectedIdx < objectsList.length) ? objectsList[selectedIdx] : null

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        objectsList = pipeline.getObjects();
        if (objectsList.length > 0) {
            if (selectedIdx >= objectsList.length) selectedIdx = 0;
            vp3d.selectedObjectId = objectsList[selectedIdx].id;
            meshInfo = vp3d.inspectSelectedMesh();
        } else {
            meshInfo = ({});
        }
        vp3d.refreshScene();
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
        anchors.fill: parent; anchors.margins: 12; spacing: 10

        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Text { text: "3D CAD & Assembly Viewport"; font.family: Theme.fontSans; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold; color: Theme.textPrimary }
            Item { Layout.preferredWidth: 8 }
            Repeater {
                model: ["Iso", "Front", "Top", "Right"]
                delegate: MonoButton { text: modelData; compact: true; onClicked: vp3d.snapView(modelData) }
            }
            MonoButton { text: "Wireframe"; compact: true; active: vp3d.wireframe; onClicked: vp3d.wireframe = !vp3d.wireframe }
            MonoButton { text: "Thermal Overlay"; compact: true; active: vp3d.showThermalOverlay; onClicked: vp3d.showThermalOverlay = !vp3d.showThermalOverlay }
            MonoButton { text: "Reset Cam"; iconName: "reset"; compact: true; onClicked: vp3d.resetCamera() }
            Item { Layout.fillWidth: true }
            MonoButton { text: "+ Cube"; compact: true; onClicked: pipeline.addPresetObject("Iron Plate") }
            MonoButton { text: "+ Cylinder"; compact: true; onClicked: pipeline.addPresetObject("Copper Wire") }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10

            // Interactive 3D Shaded Mesh Viewport
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: Theme.bgPrimary; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd
                clip: true

                Viewport3DItem {
                    id: vp3d
                    anchors.fill: parent
                    darkMode: Theme.isDarkMode
                    MouseArea {
                        anchors.fill: parent
                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                        property double lx: 0; property double ly: 0; property int btn: 0
                        onPressed: (m) => { lx = m.x; ly = m.y; btn = m.button; }
                        onPositionChanged: (m) => {
                            if (btn === Qt.RightButton) vp3d.panBy(m.x - lx, m.y - ly);
                            else vp3d.orbitBy((m.x - lx) * 0.5, (m.y - ly) * 0.5);
                            lx = m.x; ly = m.y;
                        }
                        onWheel: (w) => vp3d.zoomBy(w.angleDelta.y > 0 ? 0.9 : 1.1)
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.margins: 10
                    height: 24; width: hudTxt.implicitWidth + 14; radius: Theme.radiusSm
                    color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1
                    Text {
                        id: hudTxt
                        anchors.centerIn: parent
                        text: "Drag: Orbit | Right-Drag: Pan | Scroll: Zoom  |  Yaw: " + vp3d.cameraYaw.toFixed(0) + "  Pitch: " + vp3d.cameraPitch.toFixed(0) + "  Tris: " + vp3d.triangleCount
                        font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textSecondary
                    }
                }
            }

            // Right Inspector: Transform Gizmo & Watertight Mesh Analytics
            Rectangle {
                Layout.preferredWidth: 330; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 8
                    property var obj: root.currentObj()

                    Text { text: "Scene Objects (" + root.objectsList.length + ")"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    ListView {
                        Layout.fillWidth: true; Layout.preferredHeight: 140; clip: true; spacing: 3
                        model: root.objectsList
                        delegate: Rectangle {
                            width: ListView.view.width; height: 34; radius: Theme.radiusSm
                            color: index === root.selectedIdx ? Theme.bgActive : Theme.bgCanvas
                            border.color: index === root.selectedIdx ? Theme.borderStrong : Theme.borderSubtle; border.width: 1
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 6
                                Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; Layout.fillWidth: true; elide: Text.ElideRight }
                                Text { text: modelData.shape; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textMuted }
                            }
                            MouseArea {
                                anchors.fill: parent
                                onClicked: {
                                    root.selectedIdx = index;
                                    vp3d.selectedObjectId = modelData.id;
                                    root.meshInfo = vp3d.inspectSelectedMesh();
                                }
                            }
                        }
                    }

                    Text { text: "Transform (Move / Rotate / Scale)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    GridLayout {
                        visible: root.selectedObj !== null
                        columns: 3; columnSpacing: 4; rowSpacing: 4; Layout.fillWidth: true
                        Text { text: "Pos (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoButton { text: "X +0.05"; compact: true; onClicked: if (root.selectedObj) pipeline.updateObjectTransform(root.selectedObj.id, root.selectedObj.posX + 0.05, root.selectedObj.posY, root.selectedObj.posZ, root.selectedObj.rotX, root.selectedObj.rotY, root.selectedObj.rotZ, root.selectedObj.scaleX, root.selectedObj.scaleY, root.selectedObj.scaleZ) }
                        MonoButton { text: "Y +0.05"; compact: true; onClicked: if (root.selectedObj) pipeline.updateObjectTransform(root.selectedObj.id, root.selectedObj.posX, root.selectedObj.posY + 0.05, root.selectedObj.posZ, root.selectedObj.rotX, root.selectedObj.rotY, root.selectedObj.rotZ, root.selectedObj.scaleX, root.selectedObj.scaleY, root.selectedObj.scaleZ) }
                        Text { text: "Rot (deg):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoButton { text: "Yaw +15"; compact: true; onClicked: if (root.selectedObj) pipeline.updateObjectTransform(root.selectedObj.id, root.selectedObj.posX, root.selectedObj.posY, root.selectedObj.posZ, root.selectedObj.rotX, root.selectedObj.rotY + 15, root.selectedObj.rotZ, root.selectedObj.scaleX, root.selectedObj.scaleY, root.selectedObj.scaleZ) }
                        MonoButton { text: "Roll +15"; compact: true; onClicked: if (root.selectedObj) pipeline.updateObjectTransform(root.selectedObj.id, root.selectedObj.posX, root.selectedObj.posY, root.selectedObj.posZ, root.selectedObj.rotX, root.selectedObj.rotY, root.selectedObj.rotZ + 15, root.selectedObj.scaleX, root.selectedObj.scaleY, root.selectedObj.scaleZ) }
                        Text { text: "Scale:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoButton { text: "1.2x"; compact: true; onClicked: if (root.selectedObj) pipeline.updateObjectTransform(root.selectedObj.id, root.selectedObj.posX, root.selectedObj.posY, root.selectedObj.posZ, root.selectedObj.rotX, root.selectedObj.rotY, root.selectedObj.rotZ, root.selectedObj.scaleX * 1.2, root.selectedObj.scaleY * 1.2, root.selectedObj.scaleZ * 1.2) }
                        MonoButton { text: "Reset"; compact: true; onClicked: if (root.selectedObj) pipeline.updateObjectTransform(root.selectedObj.id, 0, 0, 0, 0, 0, 0, 1, 1, 1) }
                    }

                    Text { text: "Watertight Mesh & Geometry Inspector"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    GridLayout {
                        columns: 2; columnSpacing: 12; rowSpacing: 4; Layout.fillWidth: true
                        Text { text: "Vertices / Tris:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        Text { text: (root.meshInfo.vertices || 0) + " / " + (root.meshInfo.triangles || 0); font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }
                        Text { text: "2-Manifold Watertight:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoBadge { text: root.meshInfo.watertight ? "Watertight" : "Open/Invalid"; tone: root.meshInfo.watertight ? "success" : "warning" }
                        Text { text: "Volume / Mass:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        Text { text: Number(root.meshInfo.volume || 0).toExponential(3) + " m3 / " + Number(root.meshInfo.mass || 0).toFixed(4) + " kg"; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        enabled: root.selectedObj !== null
                        MonoButton { Layout.fillWidth: true; text: "Duplicate"; compact: true; onClicked: if (root.selectedObj) pipeline.duplicateObject(root.selectedObj.id) }
                        MonoButton { Layout.fillWidth: true; text: "Delete"; compact: true; onClicked: if (root.selectedObj) pipeline.deleteObject(root.selectedObj.id) }
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
