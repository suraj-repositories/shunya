import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import shunya

Rectangle {
    id: root
    color: Theme.bgCanvas

    property var entityList: []
    property var materialsList: []
    property double extrudeDepth: 0.02

    function refreshAll() {
        entityList = sketchCanvas.getEntities();
        if (typeof pipeline !== "undefined") materialsList = pipeline.getMaterials();
    }

    Component.onCompleted: refreshAll()

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 12; spacing: 10

        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Text { text: "2D CAD & Profile Editor"; font.family: Theme.fontSans; font.pixelSize: Theme.fontLg; font.weight: Font.DemiBold; color: Theme.textPrimary }
            Item { Layout.preferredWidth: 12 }
            Repeater {
                model: [
                    {id: "select", lbl: "Select"},
                    {id: "line", lbl: "Line"},
                    {id: "rect", lbl: "Rectangle"},
                    {id: "circle", lbl: "Circle"},
                    {id: "polygon", lbl: "Polygon (Hex)"},
                    {id: "path", lbl: "Path"}
                ]
                delegate: MonoButton {
                    text: modelData.lbl; compact: true; active: sketchCanvas.activeTool === modelData.id
                    onClicked: sketchCanvas.setActiveTool(modelData.id)
                }
            }
            MonoButton { visible: sketchCanvas.activeTool === "path"; text: "Finish Path"; variant: "primary"; compact: true; onClicked: sketchCanvas.finishPath() }
            MonoButton { text: sketchCanvas.constructionMode ? "Construction" : "Solid"; compact: true; onClicked: sketchCanvas.setConstructionMode(!sketchCanvas.constructionMode) }
            Item { Layout.fillWidth: true }
            MonoButton { text: "Clear"; compact: true; onClicked: { sketchCanvas.clearDocument(); root.refreshAll(); } }
            MonoButton { text: "Demo Plate"; compact: true; onClicked: { sketchCanvas.loadDemoSketch(); root.refreshAll(); } }
        }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10

            // Left: Sketch Canvas
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: Theme.bgPrimary; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd
                clip: true

                ShunyaSketchCanvas2D {
                    id: sketchCanvas
                    anchors.fill: parent
                    darkMode: Theme.isDarkMode
                    onDocumentChanged: root.refreshAll()
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onPositionChanged: (m) => sketchCanvas.onHoverMove(m.x, m.y)
                        onClicked: (m) => { sketchCanvas.onClickDown(m.x, m.y); root.refreshAll(); }
                        onDoubleClicked: sketchCanvas.finishPath()
                        onWheel: (w) => sketchCanvas.zoomAt(w.x, w.y, w.angleDelta.y > 0 ? 1.12 : 0.89)
                    }
                }

                Rectangle {
                    anchors.bottom: parent.bottom; anchors.left: parent.left; anchors.margins: 8
                    height: 24; width: coordTxt.implicitWidth + 14; radius: Theme.radiusSm
                    color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1
                    Text { id: coordTxt; anchors.centerIn: parent; text: sketchCanvas.cursorCoordText + "  |  Loops: " + sketchCanvas.solidLoopCount; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textPrimary }
                }
            }

            // Right: Entities, Transforms, Material & Extrude to 3D Physical Object
            Rectangle {
                Layout.preferredWidth: 320; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 10; spacing: 8
                    Text { text: "2D Entities & Properties (" + root.entityList.length + ")"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 4
                        enabled: sketchCanvas.selectedEntityId !== ""
                        MonoButton { text: "+X"; compact: true; onClicked: sketchCanvas.transformEntity(sketchCanvas.selectedEntityId, 0.5, 0, 0, 1) }
                        MonoButton { text: "Rot 45"; compact: true; onClicked: sketchCanvas.transformEntity(sketchCanvas.selectedEntityId, 0, 0, 45, 1) }
                        MonoButton { text: "1.2x"; compact: true; onClicked: sketchCanvas.transformEntity(sketchCanvas.selectedEntityId, 0, 0, 0, 1.2) }
                        MonoButton { text: "Dup"; compact: true; onClicked: sketchCanvas.duplicateEntity(sketchCanvas.selectedEntityId) }
                        MonoButton { iconName: "trash"; compact: true; onClicked: sketchCanvas.deleteEntity(sketchCanvas.selectedEntityId) }
                    }

                    RowLayout {
                        Layout.fillWidth: true; spacing: 6
                        ComboBox { id: mat2dCombo; Layout.fillWidth: true; model: root.materialsList; textRole: "name" }
                        MonoButton {
                            text: "Extrude -> Object"
                            variant: "primary"; compact: true
                            onClicked: {
                                if (mat2dCombo.currentIndex >= 0 && mat2dCombo.currentIndex < root.materialsList.length && typeof pipeline !== "undefined") {
                                    var mid = root.materialsList[mat2dCombo.currentIndex].id;
                                    pipeline.createObject("Extruded_2D_Plate", "Extruded2D", "Box", 0.15, 0.10, root.extrudeDepth, 0.05, mid, 298.15, 101325.0);
                                }
                            }
                        }
                    }

                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                        model: root.entityList
                        delegate: Rectangle {
                            width: ListView.view.width; height: 40; radius: Theme.radiusSm
                            color: modelData.id === sketchCanvas.selectedEntityId ? Theme.bgActive : Theme.bgCanvas
                            border.color: modelData.id === sketchCanvas.selectedEntityId ? Theme.borderStrong : Theme.borderSubtle; border.width: 1
                            RowLayout {
                                anchors.fill: parent; anchors.margins: 8; spacing: 6
                                Text { text: modelData.type + " #" + modelData.id; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; font.weight: Font.Medium; color: Theme.textPrimary; Layout.fillWidth: true }
                                Text { text: modelData.isClosed ? ("A=" + Number(modelData.areaSqM).toFixed(2)) : "Open"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textMuted }
                            }
                            MouseArea { anchors.fill: parent; onClicked: sketchCanvas.setSelectedEntityId(modelData.id) }
                        }
                    }
                }
            }
        }
    }
}
