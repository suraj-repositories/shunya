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

    // Form inputs for new object creation
    property string newName: "Copper_Test_Bar"
    property string newShape: "Cylinder"
    property double newDimX: 0.05
    property double newDimY: 0.25
    property double newDimZ: 0.05
    property double newRadius: 0.004
    property double newTempK: 298.15
    property string feedbackMsg: ""

    // Inline rename state
    property string renamingObjId: ""
    property string renameCandidateText: ""

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        objectsList = pipeline.getObjects();
        materialsList = pipeline.getMaterials();
        if (selectedIdx >= objectsList.length) selectedIdx = Math.max(0, objectsList.length - 1);
        if (preview3d) {
            preview3d.refreshScene();
            if (selectedObj) {
                preview3d.selectedObjectId = selectedObj.id;
            }
        }
        if (typeof propertyTreeModel !== "undefined" && selectedObj) {
            propertyTreeModel.inspectObjectProperties(selectedObj.name, selectedObj);
        }
    }

    function autoMatchMaterial(name) {
        if (!materialsList || materialsList.length === 0) return 0;
        var lower = name.toLowerCase();
        for (var i = 0; i < materialsList.length; ++i) {
            var mname = (materialsList[i].name || "").toLowerCase();
            if (lower.indexOf("copper") !== -1 && mname.indexOf("copper") !== -1) return i;
            if (lower.indexOf("tungsten") !== -1 && mname.indexOf("tungsten") !== -1) return i;
            if (lower.indexOf("iron") !== -1 && mname.indexOf("iron") !== -1) return i;
            if (lower.indexOf("aluminum") !== -1 && mname.indexOf("aluminum") !== -1) return i;
            if (lower.indexOf("silicon") !== -1 && mname.indexOf("silicon") !== -1) return i;
            if (lower.indexOf("glass") !== -1 && mname.indexOf("glass") !== -1) return i;
            if (lower.indexOf("nichrome") !== -1 && mname.indexOf("nichrome") !== -1) return i;
        }
        return 0;
    }

    Component.onCompleted: {
        refreshAll();
        if (matPicker) {
            matPicker.currentIndex = autoMatchMaterial(root.newName);
        }
    }

    Connections {
        target: typeof pipeline !== "undefined" ? pipeline : null
        function onWorkspaceChanged() { root.refreshAll(); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // =========================================================================
        // Header & Dynamic Template Library
        // =========================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                spacing: 2
                Text {
                    text: "Physical Objects & Multi-Region Modeling"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXl
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: "Author material-backed 3D objects with single-source-of-truth computed mass, inertia, and physical tensors"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    color: Theme.textMuted
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Templates:"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                font.weight: Font.Medium
                color: Theme.textMuted
            }

            Flow {
                Layout.maximumWidth: 640
                spacing: 4

                Repeater {
                    model: [
                        "Copper Wire",
                        "Tungsten Filament",
                        "Iron Plate",
                        "Aluminum Frame",
                        "Glass Envelope",
                        "Resistor Load",
                        "Lithium Foil",
                        "Insulating Spacer"
                    ]
                    delegate: MonoButton {
                        text: "+ " + modelData
                        compact: true
                        onClicked: {
                            var r = pipeline.addPresetObject(modelData);
                            root.feedbackMsg = r.ok ? ("Added template '" + r.name + "'") : ("Error: " + r.error);
                            root.refreshAll();
                            if (r.ok) {
                                root.selectedIdx = root.objectsList.length - 1;
                            }
                        }
                    }
                }
            }
        }

        // =========================================================================
        // 3-Pane Body: Left (Library/Creator), Middle (Live Properties), Right (3D Viewport)
        // =========================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // ---------------------------------------------------------------------
            // LEFT CARD: Object Creator & Project Objects List
            // ---------------------------------------------------------------------
            Rectangle {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Text {
                        text: "New Physical Object"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSm
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    MonoInput {
                        Layout.fillWidth: true
                        text: root.newName
                        placeholderText: "Object Name (e.g. Copper_Test_Bar)"
                        onTextEdited: (t) => {
                            root.newName = t;
                            if (matPicker) matPicker.currentIndex = root.autoMatchMaterial(t);
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: ["Box", "Cylinder", "Sphere", "Cone"]
                            delegate: MonoButton {
                                text: modelData
                                compact: true
                                active: root.newShape === modelData
                                onClicked: root.newShape = modelData
                            }
                        }
                    }

                    MonoComboBox {
                        id: matPicker
                        Layout.fillWidth: true
                        model: root.materialsList
                        textRole: "name"
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 6
                        rowSpacing: 4
                        Layout.fillWidth: true

                        Text { text: "Dim X / W (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: String(root.newDimX); onTextEdited: (t) => root.newDimX = parseFloat(t) || 0.05 }

                        Text { text: "Dim Y / Len (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput { Layout.fillWidth: true; monoFont: true; text: String(root.newDimY); onTextEdited: (t) => root.newDimY = parseFloat(t) || 0.25 }

                        Text { text: "Dim Z / Radius (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        MonoInput {
                            Layout.fillWidth: true
                            monoFont: true
                            text: root.newShape === "Box" ? String(root.newDimZ) : String(root.newRadius)
                            onTextEdited: (t) => {
                                var v = parseFloat(t) || 0.01;
                                root.newDimZ = v;
                                root.newRadius = v;
                            }
                        }
                    }

                    MonoButton {
                        Layout.fillWidth: true
                        text: "Create Material Object"
                        variant: "primary"
                        iconName: "plus"
                        onClicked: {
                            var mid = (matPicker.currentIndex >= 0 && matPicker.currentIndex < root.materialsList.length) ? root.materialsList[matPicker.currentIndex].id : "";
                            var r = pipeline.createObject(root.newName, "Custom", root.newShape, root.newDimX, root.newDimY, root.newDimZ, root.newRadius, mid, root.newTempK, 101325.0);
                            root.feedbackMsg = r.ok ? ("Created '" + r.name + "'") : ("Error: " + r.error);
                            root.refreshAll();
                            if (r.ok) {
                                root.selectedIdx = root.objectsList.length - 1;
                            }
                        }
                    }

                    Text {
                        visible: root.feedbackMsg !== ""
                        text: root.feedbackMsg
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXs
                        color: root.feedbackMsg.indexOf("Error") !== -1 ? Theme.accentError : Theme.textSecondary
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: Theme.borderSubtle
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Project Objects (" + root.objectsList.length + ")"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontSm
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                        }
                    }

                    // Scrollable Project Objects List (prevents clipping)
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.vertical.policy: ScrollBar.AsNeeded

                        ListView {
                            id: objListView
                            width: parent.width
                            spacing: 4
                            model: root.objectsList

                            delegate: Rectangle {
                                width: objListView.width
                                height: root.renamingObjId === modelData.id ? 72 : 54
                                radius: Theme.radiusSm
                                color: index === root.selectedIdx ? Theme.bgActive : Theme.bgCanvas
                                border.color: index === root.selectedIdx ? Theme.borderStrong : Theme.borderSubtle
                                border.width: 1

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 4

                                    // Normal Mode
                                    RowLayout {
                                        visible: root.renamingObjId !== modelData.id
                                        Layout.fillWidth: true
                                        spacing: 6

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 1

                                            Text {
                                                text: modelData.name
                                                font.family: Theme.fontSans
                                                font.pixelSize: Theme.fontSm
                                                font.weight: Font.Medium
                                                color: Theme.textPrimary
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: modelData.shape + "  |  " + Number(modelData.mass).toExponential(2) + " kg" + "  |  " + Number(modelData.density).toFixed(0) + " kg/m³"
                                                font.family: Theme.fontMono
                                                font.pixelSize: Theme.fontXs
                                                color: Theme.textMuted
                                            }
                                        }

                                        MonoButton {
                                            iconName: "edit"
                                            compact: true
                                            onClicked: {
                                                root.renamingObjId = modelData.id;
                                                root.renameCandidateText = modelData.name;
                                            }
                                        }

                                        MonoButton {
                                            iconName: "copy"
                                            compact: true
                                            onClicked: {
                                                pipeline.duplicateObject(modelData.id);
                                                root.refreshAll();
                                            }
                                        }

                                        MonoButton {
                                            iconName: "trash"
                                            compact: true
                                            onClicked: {
                                                pipeline.deleteObject(modelData.id);
                                                root.refreshAll();
                                            }
                                        }
                                    }

                                    // Rename Mode
                                    RowLayout {
                                        visible: root.renamingObjId === modelData.id
                                        Layout.fillWidth: true
                                        spacing: 4

                                        MonoInput {
                                            Layout.fillWidth: true
                                            text: root.renameCandidateText
                                            onTextEdited: (t) => root.renameCandidateText = t
                                        }

                                        MonoButton {
                                            iconName: "check"
                                            compact: true
                                            variant: "primary"
                                            onClicked: {
                                                var res = pipeline.renameObject(modelData.id, root.renameCandidateText);
                                                if (res.ok) {
                                                    root.renamingObjId = "";
                                                    root.refreshAll();
                                                } else {
                                                    root.feedbackMsg = "Rename Error: " + res.error;
                                                }
                                            }
                                        }

                                        MonoButton {
                                            iconName: "x"
                                            compact: true
                                            onClicked: {
                                                root.renamingObjId = "";
                                            }
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    z: -1
                                    onClicked: {
                                        root.selectedIdx = index;
                                        if (preview3d) preview3d.selectedObjectId = modelData.id;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------------------
            // CENTER CARD: Selected Object Live Properties & Dimensional Controls
            // ---------------------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    // Object Title & Badges
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: root.selectedObj ? root.selectedObj.name : "No Object Selected"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontLg
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                        }

                        MonoBadge {
                            text: root.selectedObj ? root.selectedObj.shape : "Primitive"
                            tone: "neutral"
                        }

                        MonoBadge {
                            text: root.selectedObj && root.selectedObj.hasConductivePath ? "Conductive Path" : "Insulating / Non-Conductive"
                            tone: root.selectedObj && root.selectedObj.hasConductivePath ? "success" : "neutral"
                        }

                        MonoBadge {
                            text: root.selectedObj && root.selectedObj.isSolid ? "Solid Phase" : "Melted Warning"
                            tone: root.selectedObj && root.selectedObj.isSolid ? "neutral" : "error"
                        }
                    }

                    // Live Dimensional Controls for Selected Object (Single Source of Truth)
                    Rectangle {
                        visible: root.selectedObj !== null
                        Layout.fillWidth: true
                        color: Theme.bgCanvas
                        border.color: Theme.borderSubtle
                        border.width: 1
                        radius: Theme.radiusSm
                        implicitHeight: dimRow.implicitHeight + 14

                        RowLayout {
                            id: dimRow
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10

                            Text {
                                text: "Live Geometry:"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                                color: Theme.textMuted
                            }

                            Text { text: "Dim X (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            MonoInput {
                                Layout.preferredWidth: 70
                                monoFont: true
                                text: root.selectedObj ? String(root.selectedObj.dimX) : "0.05"
                                onTextEdited: (t) => {
                                    if (!root.selectedObj) return;
                                    var vx = parseFloat(t) || 0.01;
                                    pipeline.updateObject(root.selectedObj.id, root.selectedObj.name, root.selectedObj.shape,
                                                          vx, root.selectedObj.dimY, root.selectedObj.dimZ, root.selectedObj.radius,
                                                          root.selectedObj.tempK, root.selectedObj.pressurePa, root.selectedObj.regions);
                                    root.refreshAll();
                                }
                            }

                            Text { text: "Dim Y / Len (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            MonoInput {
                                Layout.preferredWidth: 70
                                monoFont: true
                                text: root.selectedObj ? String(root.selectedObj.dimY) : "0.25"
                                onTextEdited: (t) => {
                                    if (!root.selectedObj) return;
                                    var vy = parseFloat(t) || 0.01;
                                    pipeline.updateObject(root.selectedObj.id, root.selectedObj.name, root.selectedObj.shape,
                                                          root.selectedObj.dimX, vy, root.selectedObj.dimZ, root.selectedObj.radius,
                                                          root.selectedObj.tempK, root.selectedObj.pressurePa, root.selectedObj.regions);
                                    root.refreshAll();
                                }
                            }

                            Text { text: "Dim Z / Rad (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                            MonoInput {
                                Layout.preferredWidth: 70
                                monoFont: true
                                text: root.selectedObj ? (root.selectedObj.shape === "Box" ? String(root.selectedObj.dimZ) : String(root.selectedObj.radius)) : "0.01"
                                onTextEdited: (t) => {
                                    if (!root.selectedObj) return;
                                    var vz = parseFloat(t) || 0.005;
                                    pipeline.updateObject(root.selectedObj.id, root.selectedObj.name, root.selectedObj.shape,
                                                          root.selectedObj.dimX, root.selectedObj.dimY, vz, vz,
                                                          root.selectedObj.tempK, root.selectedObj.pressurePa, root.selectedObj.regions);
                                    root.refreshAll();
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }

                    // Authoritative Physical Property Tensors Grid
                    GridLayout {
                        visible: root.selectedObj !== null
                        columns: 4
                        columnSpacing: 14
                        rowSpacing: 6
                        Layout.fillWidth: true

                        Text { text: "Volume (m³):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.volume).toExponential(4) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }

                        Text { text: "Surface Area (m²):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.surfaceArea).toExponential(4) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }

                        Text { text: "Effective Density (kg/m³):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.density).toFixed(2) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }

                        Text { text: "Calculated Mass (kg):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.mass).toExponential(4) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; font.weight: Font.Bold; color: Theme.textPrimary }

                        Text { text: "Center of Mass (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text {
                            text: root.selectedObj ? ("[" + Number(root.selectedObj.comX || 0).toFixed(3) + ", " + Number(root.selectedObj.comY || 0).toFixed(3) + ", " + Number(root.selectedObj.comZ || 0).toFixed(3) + "]") : ""
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontXs
                            color: Theme.textPrimary
                        }

                        Text { text: "Principal Inertia (kg·m²):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text {
                            text: root.selectedObj ? ("[" + Number(root.selectedObj.ixx || 0).toExponential(2) + ", " + Number(root.selectedObj.iyy || 0).toExponential(2) + ", " + Number(root.selectedObj.izz || 0).toExponential(2) + "]") : ""
                            font.family: Theme.fontMono
                            font.pixelSize: Theme.fontXs
                            color: Theme.textPrimary
                        }

                        Text { text: "Axial Electrical R (Ω):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.axialR).toExponential(4) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }

                        Text { text: "Thermal Mass m·Cp (J/K):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.thermalMass).toFixed(2) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }

                        Text { text: "Young's Modulus E (GPa):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? (Number(root.selectedObj.youngModulus || 0) / 1e9).toFixed(1) + " GPa" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }

                        Text { text: "Linear CTE (1/K):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: root.selectedObj ? Number(root.selectedObj.cte || 0).toExponential(2) : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                    }

                    Text {
                        text: "Material Regions & Sub-Bodies"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSm
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 4
                        model: root.selectedObj ? root.selectedObj.regions : []
                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 36
                            radius: Theme.radiusSm
                            color: Theme.bgCanvas
                            border.color: Theme.borderSubtle
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 8

                                Text {
                                    text: modelData.name
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.Medium
                                    color: Theme.textPrimary
                                    Layout.preferredWidth: 120
                                }

                                Text {
                                    text: modelData.materialName
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.textSecondary
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                MonoBadge {
                                    text: (modelData.volumeFraction * 100).toFixed(0) + "% vol"
                                    tone: "neutral"
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------------------
            // RIGHT CARD: Real 3D Preview of Workspace Objects
            // ---------------------------------------------------------------------
            Rectangle {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                color: Theme.bgCanvas
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd

                Viewport3DItem {
                    id: preview3d
                    anchors.fill: parent
                    anchors.margins: 2
                    darkMode: Theme.isDarkMode

                    MouseArea {
                        anchors.fill: parent
                        property double lx: 0
                        property double ly: 0
                        onPressed: (m) => { lx = m.x; ly = m.y; }
                        onPositionChanged: (m) => {
                            preview3d.orbitBy((m.x - lx) * 0.5, (m.y - ly) * 0.5);
                            lx = m.x;
                            ly = m.y;
                        }
                        onWheel: (w) => preview3d.zoomBy(w.angleDelta.y > 0 ? 0.9 : 1.1)
                    }
                }

                Rectangle {
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.margins: 10
                    radius: Theme.radiusSm
                    color: Theme.bgElevated
                    border.color: Theme.borderSubtle
                    border.width: 1
                    implicitHeight: previewBadgeText.implicitHeight + 6
                    implicitWidth: previewBadgeText.implicitWidth + 12

                    Text {
                        id: previewBadgeText
                        anchors.centerIn: parent
                        text: "3D Preview: " + preview3d.meshCount + " objects (" + preview3d.triangleCount + " tris)"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXs
                        color: Theme.textSecondary
                    }
                }
            }
        }
    }
}
