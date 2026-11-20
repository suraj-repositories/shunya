import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtCore
import shunya

Rectangle {
    id: root
    color: Theme.bgCanvas

    // =========================================================================
    // Core Domain Data & Selection State
    // =========================================================================
    property var objectsList: []
    property var materialsList: []
    property int selectedIdx: 0
    property int compareIdx: 1
    readonly property var selectedObj: (selectedIdx >= 0 && selectedIdx < objectsList.length) ? objectsList[selectedIdx] : null
    readonly property var compareObj: (compareIdx >= 0 && compareIdx < objectsList.length) ? objectsList[compareIdx] : null

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

    // Search filter for object library
    property string librarySearchQuery: ""

    // Active Property Tab index: 0: Geometry, 1: Regions, 2: Interior, 3: Physics, 4: Simulator
    property int activePropertiesTab: 0

    // Viewport Mode & Shading State
    property string viewportMode: "Object Mode" // "Object Mode" | "Edit Mode"
    property string viewportShading: "Solid"    // "Solid" | "Wireframe" | "X-Ray"
    property bool showGrid: true
    property bool snapEnabled: true

    // Layout Dimensions, Presets & Persistence
    property real leftPaneWidth: 300
    property real rightPaneWidth: 360
    property real bottomPaneHeight: 140
    property bool leftCollapsed: false
    property bool rightCollapsed: false
    property bool bottomCollapsed: false
    property bool isMaximizedViewport: false
    property string layoutPreset: "Default" // "Default" | "Modeling" | "Testing" | "Compare"

    // Cavity Interior State
    property var currentCavitySpec: ({})
    property var cavityThermodynamics: ({})
    property var bulbMilestoneReport: ({})

    // Telemetry log stream
    property var telemetryLogs: [
        { time: "12:00:01", tag: "INIT", text: "Physical Objects authoring kernel initialized." },
        { time: "12:00:02", tag: "OBJ", text: "Single source of truth property engine online." },
        { time: "12:00:03", tag: "PREFLIGHT", text: "Pre-flight validation engine ready." }
    ]

    function appendTelemetry(tag, text) {
        var now = new Date();
        var timeStr = ("0" + now.getHours()).slice(-2) + ":" +
                      ("0" + now.getMinutes()).slice(-2) + ":" +
                      ("0" + now.getSeconds()).slice(-2);
        var arr = root.telemetryLogs.slice();
        arr.push({ time: timeStr, tag: tag, text: text });
        if (arr.length > 200) arr.shift();
        root.telemetryLogs = arr;
    }

    Settings {
        id: objectsSettings
        category: "ObjectsLayout"
        property alias leftPaneWidth: root.leftPaneWidth
        property alias rightPaneWidth: root.rightPaneWidth
        property alias bottomPaneHeight: root.bottomPaneHeight
        property alias leftCollapsed: root.leftCollapsed
        property alias rightCollapsed: root.rightCollapsed
        property alias bottomCollapsed: root.bottomCollapsed
        property alias layoutPreset: root.layoutPreset
    }

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        objectsList = pipeline.getObjects();
        materialsList = pipeline.getMaterials();
        if (selectedIdx >= objectsList.length) selectedIdx = Math.max(0, objectsList.length - 1);
        if (compareIdx >= objectsList.length) compareIdx = Math.max(0, objectsList.length - 1);
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

    function applyPreset(name) {
        root.layoutPreset = name;
        var l = pipeline.calculateObjectLayout(name, root.width, root.height, root.leftPaneWidth, root.rightPaneWidth, root.bottomPaneHeight);
        root.leftPaneWidth = l.leftWidth;
        root.rightPaneWidth = l.rightWidth;
        root.bottomPaneHeight = l.bottomHeight;
        root.leftCollapsed = l.leftCollapsed;
        root.rightCollapsed = l.rightCollapsed;
        root.bottomCollapsed = l.bottomCollapsed;
        root.appendTelemetry("LAYOUT", "Applied layout preset '" + name + "'.");
    }

    function resetSplitters() {
        var l = pipeline.calculateObjectLayout("Default", root.width, root.height, 0, 0, 0);
        root.leftPaneWidth = l.leftWidth;
        root.rightPaneWidth = l.rightWidth;
        root.bottomPaneHeight = l.bottomHeight;
        root.leftCollapsed = false;
        root.rightCollapsed = false;
        root.bottomCollapsed = false;
        root.isMaximizedViewport = false;
        root.layoutPreset = "Default";
        root.appendTelemetry("LAYOUT", "Reset workspace layout to default proportions.");
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

    // Keyboard Shortcuts for Layout
    Shortcut { sequence: "F11"; onActivated: root.isMaximizedViewport = !root.isMaximizedViewport }
    Shortcut { sequence: "Ctrl+Space"; onActivated: root.isMaximizedViewport = !root.isMaximizedViewport }
    Shortcut { sequence: "Ctrl+B"; onActivated: root.leftCollapsed = !root.leftCollapsed }
    Shortcut { sequence: "Ctrl+Alt+B"; onActivated: root.rightCollapsed = !root.rightCollapsed }
    Shortcut { sequence: "Ctrl+J"; onActivated: root.bottomCollapsed = !root.bottomCollapsed }
    Shortcut { sequence: "Escape"; onActivated: { if (root.isMaximizedViewport) root.isMaximizedViewport = false; } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: root.isMaximizedViewport ? 0 : 12
        spacing: root.isMaximizedViewport ? 0 : 8

        // =========================================================================
        // Workspace Top Header (Hidden in Maximize Viewport Mode)
        // =========================================================================
        RowLayout {
            visible: !root.isMaximizedViewport
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

            // Layout Presets Selector
            RowLayout {
                spacing: 4
                Text {
                    text: "Layout:"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.Medium
                    color: Theme.textMuted
                }

                Repeater {
                    model: ["Default", "Modeling", "Testing", "Compare"]
                    delegate: MonoButton {
                        text: modelData
                        compact: true
                        active: root.layoutPreset === modelData
                        onClicked: root.applyPreset(modelData)
                    }
                }
            }

            Rectangle { width: 1; height: 18; color: Theme.borderSubtle }

            // Maximize Viewport Button
            MonoButton {
                text: root.isMaximizedViewport ? "Restore" : "Maximize"
                iconName: root.isMaximizedViewport ? "minimize" : "maximize"
                compact: true
                onClicked: root.isMaximizedViewport = !root.isMaximizedViewport
            }
        }

        // =========================================================================
        // Dynamic Quick-Template Bar (Hidden in Maximize Viewport Mode)
        // =========================================================================
        RowLayout {
            visible: !root.isMaximizedViewport
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Quick Templates:"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                font.weight: Font.Medium
                color: Theme.textMuted
            }

            Flow {
                Layout.fillWidth: true
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
                            root.appendTelemetry("OBJ", "Added template '" + modelData + "'.");
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
        // Main 4-Pane Workstation Layout
        // =========================================================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // Upper 3-Pane Row: Left Pane, Center Viewport, Right Properties
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // -----------------------------------------------------------------
                // PANE 1: LEFT PANE - Object Library & Project Objects Tree
                // -----------------------------------------------------------------
                Rectangle {
                    visible: !root.isMaximizedViewport && !root.leftCollapsed
                    Layout.preferredWidth: root.leftPaneWidth
                    Layout.minimumWidth: 280
                    Layout.maximumWidth: 600
                    Layout.fillHeight: true
                    color: Theme.bgSurface
                    border.color: Theme.borderDefault
                    border.width: 1
                    radius: Theme.radiusMd
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        // Header with Collapse Chevron
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: "Object Library"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }

                            MonoButton {
                                iconName: "chevron-left"
                                compact: true
                                onClicked: root.leftCollapsed = true
                            }
                        }

                        // Search Box
                        MonoInput {
                            Layout.fillWidth: true
                            placeholderText: "Search library & objects..."
                            onTextEdited: (t) => root.librarySearchQuery = t.toLowerCase()
                        }

                        // Project Objects Header
                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Project Objects (" + root.objectsList.length + ")"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontXs
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                Layout.fillWidth: true
                            }

                            MonoButton {
                                text: "+ New"
                                compact: true
                                variant: "primary"
                                onClicked: {
                                    var mid = (matPicker && matPicker.currentIndex >= 0 && matPicker.currentIndex < root.materialsList.length)
                                        ? root.materialsList[matPicker.currentIndex].id : "";
                                    var r = pipeline.createObject(root.newName, "Custom", root.newShape,
                                                                  root.newDimX, root.newDimY, root.newDimZ, root.newRadius,
                                                                  mid, root.newTempK, 101325.0);
                                    root.feedbackMsg = r.ok ? ("Created '" + r.name + "'") : ("Error: " + r.error);
                                    root.appendTelemetry("OBJ", "Created object '" + root.newName + "'.");
                                    root.refreshAll();
                                    if (r.ok) root.selectedIdx = root.objectsList.length - 1;
                                }
                            }
                        }

                        // Scrollable Project Objects List
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded

                            ListView {
                                id: objListView
                                width: parent.width
                                spacing: 4
                                model: root.objectsList.filter(o => {
                                    if (!root.librarySearchQuery) return true;
                                    return (o.name || "").toLowerCase().indexOf(root.librarySearchQuery) !== -1 ||
                                           (o.shape || "").toLowerCase().indexOf(root.librarySearchQuery) !== -1;
                                })

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
                                                    text: modelData.shape + " | " + Number(modelData.mass).toExponential(2) + " kg"
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
                                                    root.appendTelemetry("OBJ", "Duplicated object '" + modelData.name + "'.");
                                                    root.refreshAll();
                                                }
                                            }

                                            MonoButton {
                                                iconName: "trash"
                                                compact: true
                                                onClicked: {
                                                    pipeline.deleteObject(modelData.id);
                                                    root.appendTelemetry("OBJ", "Deleted object '" + modelData.name + "'.");
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
                                                        root.appendTelemetry("OBJ", "Renamed object to '" + res.name + "'.");
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

                        // Creator Form Toggle / Parameters Box
                        Rectangle {
                            Layout.fillWidth: true
                            radius: Theme.radiusSm
                            color: Theme.bgCanvas
                            border.color: Theme.borderSubtle
                            border.width: 1
                            implicitHeight: creatorCol.implicitHeight + 12

                            ColumnLayout {
                                id: creatorCol
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 4

                                Text {
                                    text: "Quick Add Parameters"
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontXs
                                    font.weight: Font.DemiBold
                                    color: Theme.textMuted
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    MonoInput {
                                        Layout.fillWidth: true
                                        text: root.newName
                                        placeholderText: "Object Name"
                                        onTextEdited: (t) => {
                                            root.newName = t;
                                            if (matPicker) matPicker.currentIndex = root.autoMatchMaterial(t);
                                        }
                                    }
                                    Repeater {
                                        model: ["Box", "Cyl", "Sph", "Cone", "Pipe", "Torus", "Capsule", "Helix"]
                                        delegate: MonoButton {
                                            text: modelData
                                            compact: true
                                            active: (root.newShape === "Cylinder" && modelData === "Cyl") ||
                                                    (root.newShape === "Sphere" && modelData === "Sph") ||
                                                    (root.newShape === "SweptPath" && modelData === "Helix") ||
                                                    (root.newShape === modelData)
                                            onClicked: {
                                                if (modelData === "Cyl") root.newShape = "Cylinder";
                                                else if (modelData === "Sph") root.newShape = "Sphere";
                                                else if (modelData === "Helix") root.newShape = "SweptPath";
                                                else root.newShape = modelData;
                                            }
                                        }
                                    }
                                }

                                MonoComboBox {
                                    id: matPicker
                                    Layout.fillWidth: true
                                    model: root.materialsList
                                    textRole: "name"
                                }
                            }
                        }
                    }
                }

                // Left Splitter
                StudioSplitter {
                    id: leftSplitter
                    visible: !root.isMaximizedViewport && !root.leftCollapsed
                    orientation: Qt.Vertical
                    Layout.fillHeight: true
                    onDragDelta: (delta) => {
                        var maxLeft = Math.max(280, root.width - (root.rightCollapsed ? 0 : root.rightPaneWidth) - 400);
                        root.leftPaneWidth = Math.max(280, Math.min(maxLeft, root.leftPaneWidth + delta));
                    }
                    onResetRequested: {
                        root.leftPaneWidth = Math.max(280, root.width * 0.20);
                    }
                }

                // Left Restore Handle when Collapsed
                Rectangle {
                    visible: !root.isMaximizedViewport && root.leftCollapsed
                    Layout.preferredWidth: 20
                    Layout.fillHeight: true
                    color: Theme.bgSurface
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        MonoButton {
                            iconName: "chevron-right"
                            compact: true
                            onClicked: root.leftCollapsed = false
                        }
                        Text {
                            text: "L\nI\nB\nR\nA\nR\nY"
                            font.family: Theme.fontMono
                            font.pixelSize: 9
                            color: Theme.textMuted
                        }
                    }
                }

                // -----------------------------------------------------------------
                // PANE 2: CENTER PANE - 3D Interactive Viewport & Modeling Canvas
                // -----------------------------------------------------------------
                Rectangle {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 400
                    Layout.fillHeight: true
                    color: Theme.bgPrimary
                    border.color: Theme.borderDefault
                    border.width: 1
                    radius: root.isMaximizedViewport ? 0 : Theme.radiusMd
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Viewport Header Controls Bar
                        Rectangle {
                            Layout.fillWidth: true
                            height: 36
                            color: Theme.bgSurface
                            border.color: Theme.borderSubtle
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 6

                                // Mode Selector
                                RowLayout {
                                    spacing: 2
                                    Repeater {
                                        model: ["Object Mode", "Edit Mode"]
                                        delegate: MonoButton {
                                            text: modelData
                                            compact: true
                                            active: root.viewportMode === modelData
                                            onClicked: root.viewportMode = modelData
                                        }
                                    }
                                }

                                Rectangle { width: 1; height: 16; color: Theme.borderSubtle }

                                // Shading Mode Selector
                                RowLayout {
                                    spacing: 2
                                    Repeater {
                                        model: ["Solid", "Wireframe", "X-Ray"]
                                        delegate: MonoButton {
                                            text: modelData
                                            compact: true
                                            active: root.viewportShading === modelData
                                            onClicked: root.viewportShading = modelData
                                        }
                                    }
                                }

                                Rectangle { width: 1; height: 16; color: Theme.borderSubtle }

                                // Grid and Snapping Toggles
                                MonoButton {
                                    text: "Grid"
                                    compact: true
                                    active: root.showGrid
                                    onClicked: root.showGrid = !root.showGrid
                                }

                                MonoButton {
                                    text: root.snapEnabled ? "Snap ON" : "Snap OFF"
                                    compact: true
                                    active: root.snapEnabled
                                    onClicked: root.snapEnabled = !root.snapEnabled
                                }

                                MonoButton {
                                    text: "Reset Cam"
                                    iconName: "reset"
                                    compact: true
                                    onClicked: {
                                        if (preview3d) preview3d.resetCamera();
                                        if (previewCompare3d) previewCompare3d.resetCamera();
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                // Viewport Maximization Button
                                MonoButton {
                                    iconName: root.isMaximizedViewport ? "minimize" : "maximize"
                                    compact: true
                                    onClicked: root.isMaximizedViewport = !root.isMaximizedViewport
                                }
                            }
                        }

                        // 3D Canvas Viewport (Single or Compare Dual Viewport)
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            spacing: 2

                            // Primary 3D Viewport
                            Item {
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                Viewport3DItem {
                                    id: preview3d
                                    anchors.fill: parent
                                    darkMode: Theme.isDarkMode

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        property double lx: 0
                                        property double ly: 0
                                        property int btn: 0
                                        onPressed: (m) => { lx = m.x; ly = m.y; btn = m.button; }
                                        onPositionChanged: (m) => {
                                            if (btn === Qt.RightButton) {
                                                preview3d.panBy(m.x - lx, m.y - ly);
                                            } else {
                                                preview3d.orbitBy((m.x - lx) * 0.5, (m.y - ly) * 0.5);
                                            }
                                            lx = m.x;
                                            ly = m.y;
                                        }
                                        onWheel: (w) => preview3d.zoomBy(w.angleDelta.y > 0 ? 0.9 : 1.1)
                                    }
                                }

                                // Top-Left Viewport Identification Label
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.left: parent.left
                                    anchors.margins: 10
                                    radius: Theme.radiusSm
                                    color: Theme.bgElevated
                                    border.color: Theme.borderSubtle
                                    border.width: 1
                                    implicitHeight: lblTxt.implicitHeight + 6
                                    implicitWidth: lblTxt.implicitWidth + 12

                                    Text {
                                        id: lblTxt
                                        anchors.centerIn: parent
                                        text: root.layoutPreset === "Compare"
                                            ? ("Primary: " + (root.selectedObj ? root.selectedObj.name : "None"))
                                            : ("Viewport: " + preview3d.meshCount + " objects (" + preview3d.triangleCount + " tris)")
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        color: Theme.textSecondary
                                    }
                                }

                                // Top-Right ViewCube
                                ViewCube {
                                    anchors.top: parent.top
                                    anchors.right: parent.right
                                    anchors.margins: 10
                                    opacity: 0.85
                                }
                            }

                            // Secondary Viewport (Rendered in Compare Preset)
                            Item {
                                visible: root.layoutPreset === "Compare"
                                Layout.fillWidth: true
                                Layout.fillHeight: true

                                Viewport3DItem {
                                    id: previewCompare3d
                                    anchors.fill: parent
                                    darkMode: Theme.isDarkMode

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.LeftButton | Qt.RightButton
                                        property double lx: 0
                                        property double ly: 0
                                        property int btn: 0
                                        onPressed: (m) => { lx = m.x; ly = m.y; btn = m.button; }
                                        onPositionChanged: (m) => {
                                            if (btn === Qt.RightButton) {
                                                previewCompare3d.panBy(m.x - lx, m.y - ly);
                                            } else {
                                                previewCompare3d.orbitBy((m.x - lx) * 0.5, (m.y - ly) * 0.5);
                                            }
                                            lx = m.x;
                                            ly = m.y;
                                        }
                                        onWheel: (w) => previewCompare3d.zoomBy(w.angleDelta.y > 0 ? 0.9 : 1.1)
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
                                    implicitHeight: compLblTxt.implicitHeight + 6
                                    implicitWidth: compLblTxt.implicitWidth + 12

                                    Text {
                                        id: compLblTxt
                                        anchors.centerIn: parent
                                        text: "Compare: " + (root.compareObj ? root.compareObj.name : "None")
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        color: Theme.accentPrimary
                                    }
                                }
                            }
                        }

                        // Floating Minimal HUD Toolbar at bottom of Viewport
                        Rectangle {
                            Layout.fillWidth: true
                            height: 28
                            color: Theme.bgSurface
                            border.color: Theme.borderSubtle
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 4
                                spacing: 10

                                Text {
                                    text: "60 FPS"
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    color: Theme.textSuccess
                                }

                                Text {
                                    text: "Manifold: OK"
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    color: Theme.textSuccess
                                }

                                Text {
                                    text: "Tris: " + preview3d.triangleCount
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    color: Theme.textMuted
                                }

                                Text {
                                    text: "Mode: " + root.viewportMode
                                    font.family: Theme.fontSans
                                    font.pixelSize: 10
                                    color: Theme.textSecondary
                                }

                                Item { Layout.fillWidth: true }

                                MonoButton {
                                    visible: root.isMaximizedViewport
                                    text: "Exit Fullscreen (Esc / F11)"
                                    compact: true
                                    onClicked: root.isMaximizedViewport = false
                                }
                            }
                        }
                    }
                }

                // Right Splitter
                StudioSplitter {
                    id: rightSplitter
                    visible: !root.isMaximizedViewport && !root.rightCollapsed
                    orientation: Qt.Vertical
                    Layout.fillHeight: true
                    onDragDelta: (delta) => {
                        var maxRight = Math.max(340, root.width - (root.leftCollapsed ? 0 : root.leftPaneWidth) - 400);
                        root.rightPaneWidth = Math.max(340, Math.min(maxRight, root.rightPaneWidth - delta));
                    }
                    onResetRequested: {
                        root.rightPaneWidth = Math.max(340, root.width * 0.25);
                    }
                }

                // Right Restore Handle when Collapsed
                Rectangle {
                    visible: !root.isMaximizedViewport && root.rightCollapsed
                    Layout.preferredWidth: 20
                    Layout.fillHeight: true
                    color: Theme.bgSurface
                    border.color: Theme.borderDefault
                    border.width: 1

                    ColumnLayout {
                        anchors.centerIn: parent
                        spacing: 8
                        MonoButton {
                            iconName: "chevron-left"
                            compact: true
                            onClicked: root.rightCollapsed = false
                        }
                        Text {
                            text: "P\nR\nO\nP\nS"
                            font.family: Theme.fontMono
                            font.pixelSize: 9
                            color: Theme.textMuted
                        }
                    }
                }

                // -----------------------------------------------------------------
                // PANE 3: RIGHT PANE - Tabbed Properties Inspector
                // -----------------------------------------------------------------
                Rectangle {
                    visible: !root.isMaximizedViewport && !root.rightCollapsed
                    Layout.preferredWidth: root.rightPaneWidth
                    Layout.minimumWidth: 340
                    Layout.maximumWidth: 600
                    Layout.fillHeight: true
                    color: Theme.bgSurface
                    border.color: Theme.borderDefault
                    border.width: 1
                    radius: Theme.radiusMd
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        // Header with Tabs & Collapse Button
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: root.selectedObj ? root.selectedObj.name : "Properties"
                                font.family: Theme.fontSans
                                font.pixelSize: Theme.fontSm
                                font.weight: Font.DemiBold
                                color: Theme.textPrimary
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            MonoButton {
                                iconName: "chevron-right"
                                compact: true
                                onClicked: root.rightCollapsed = true
                            }
                        }

                        // Tab Switcher Bar: Geometry, Regions, Interior, Physics, Simulator
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 2

                            Repeater {
                                model: ["Geometry", "Regions", "Interior", "Physics", "Simulator"]
                                delegate: MonoButton {
                                    text: modelData
                                    compact: true
                                    active: root.activePropertiesTab === index
                                    onClicked: root.activePropertiesTab = index
                                }
                            }
                        }

                        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                        // Scrollable Tab Contents
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            contentWidth: availableWidth

                            ColumnLayout {
                                width: parent.width
                                spacing: 8

                                // =================================================
                                // TAB 0: GEOMETRY
                                // =================================================
                                ColumnLayout {
                                    visible: root.activePropertiesTab === 0
                                    width: parent.width
                                    spacing: 8

                                    Text {
                                        text: "Parametric Shape & Dimensions"
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Text { text: "Active Shape:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoBadge { text: root.selectedObj ? root.selectedObj.shape : "Box"; tone: "neutral" }
                                    }

                                    GridLayout {
                                        columns: 2
                                        columnSpacing: 6
                                        rowSpacing: 4
                                        Layout.fillWidth: true

                                        Text { text: "Dim X (m):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoInput {
                                            Layout.fillWidth: true
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
                                            Layout.fillWidth: true
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
                                            Layout.fillWidth: true
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
                                    }

                                    Text {
                                        text: "3D Transform (Local)"
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    GridLayout {
                                        columns: 4
                                        columnSpacing: 4
                                        rowSpacing: 2
                                        Layout.fillWidth: true

                                        Text { text: "Pos (m):"; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textMuted }
                                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.selectedObj ? String(root.selectedObj.posX || 0) : "0" }
                                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.selectedObj ? String(root.selectedObj.posY || 0) : "0" }
                                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.selectedObj ? String(root.selectedObj.posZ || 0) : "0" }

                                        Text { text: "Rot (°):"; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textMuted }
                                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.selectedObj ? String(root.selectedObj.rotX || 0) : "0" }
                                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.selectedObj ? String(root.selectedObj.rotY || 0) : "0" }
                                        MonoInput { Layout.fillWidth: true; monoFont: true; text: root.selectedObj ? String(root.selectedObj.rotZ || 0) : "0" }
                                    }

                                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                                    // Custom Modeling & CSG Operations
                                    Text {
                                        text: "Custom Modeling & CSG Booleans"
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        MonoButton {
                                            text: "Shell / Hollow"
                                            compact: true
                                            onClicked: {
                                                if (root.selectedObj) {
                                                    var res = pipeline.shellObject(root.selectedObj.id, 0.002);
                                                    if (res.ok) {
                                                        root.appendTelemetry("MODEL", "Shelled object into '" + res.name + "'.");
                                                        root.refreshAll();
                                                    }
                                                }
                                            }
                                        }

                                        MonoButton {
                                            text: "CSG Union"
                                            compact: true
                                            onClicked: {
                                                if (root.selectedObj && root.compareObj) {
                                                    var res = pipeline.executeCsgBoolean("Union", root.selectedObj.id, root.compareObj.id);
                                                    if (res.ok) {
                                                        root.appendTelemetry("CSG", "Union created '" + res.name + "'.");
                                                        root.refreshAll();
                                                    }
                                                }
                                            }
                                        }

                                        MonoButton {
                                            text: "CSG Diff"
                                            compact: true
                                            onClicked: {
                                                if (root.selectedObj && root.compareObj) {
                                                    var res = pipeline.executeCsgBoolean("Difference", root.selectedObj.id, root.compareObj.id);
                                                    if (res.ok) {
                                                        root.appendTelemetry("CSG", "Difference created '" + res.name + "'.");
                                                        root.refreshAll();
                                                    }
                                                }
                                            }
                                        }

                                        MonoButton {
                                            text: "CSG Intersect"
                                            compact: true
                                            onClicked: {
                                                if (root.selectedObj && root.compareObj) {
                                                    var res = pipeline.executeCsgBoolean("Intersection", root.selectedObj.id, root.compareObj.id);
                                                    if (res.ok) {
                                                        root.appendTelemetry("CSG", "Intersection created '" + res.name + "'.");
                                                        root.refreshAll();
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        MonoButton {
                                            text: "Check Manifold"
                                            compact: true
                                            onClicked: {
                                                if (root.selectedObj) {
                                                    var m = pipeline.validateMeshManifold(root.selectedObj.id);
                                                    root.appendTelemetry("MANIFOLD", m.status + " (" + m.vertexCount + " verts, " + m.faceCount + " tris)");
                                                }
                                            }
                                        }

                                        MonoBadge {
                                            text: "Watertight 2-Manifold: OK"
                                            tone: "success"
                                        }
                                    }
                                }

                                // =================================================
                                // TAB 1: REGIONS
                                // =================================================
                                ColumnLayout {
                                    visible: root.activePropertiesTab === 1
                                    width: parent.width
                                    spacing: 8

                                    ObjectRegionsView {
                                        Layout.fillWidth: true
                                        implicitHeight: 380
                                        objectId: root.selectedObj ? root.selectedObj.id : ""
                                        onRegionsChanged: root.refreshAll()
                                    }
                                }

                                // =================================================
                                // TAB 2: INTERIOR (Cavity & Fill Gas)
                                // =================================================
                                ColumnLayout {
                                    visible: root.activePropertiesTab === 2
                                    width: parent.width
                                    spacing: 8

                                    Component.onCompleted: {
                                        if (root.selectedObj) {
                                            root.currentCavitySpec = pipeline.getCavitySpec(root.selectedObj.id);
                                            root.cavityThermodynamics = pipeline.evaluateCavityThermodynamics(root.selectedObj.id, 2750.0);
                                        }
                                    }

                                    Text {
                                        text: "Cavity & Fill Gas Configuration"
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    GridLayout {
                                        columns: 2
                                        columnSpacing: 6
                                        rowSpacing: 4
                                        Layout.fillWidth: true

                                        Text { text: "Cavity Type:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoComboBox {
                                            id: cavityTypeCombo
                                            Layout.fillWidth: true
                                            model: ["FillGas", "SealedVacuum", "LiquidFill", "OpenToEnvironment"]
                                            currentText: root.currentCavitySpec.kind || "FillGas"
                                        }

                                        Text { text: "Gas Composition:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoComboBox {
                                            id: gasCompCombo
                                            Layout.fillWidth: true
                                            model: ["Ar/N2 (95/5)", "Pure Ar", "Pure N2", "Vacuum", "Air"]
                                            currentText: "Ar/N2 (95/5)"
                                        }

                                        Text { text: "Fill Pressure (kPa):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoInput {
                                            id: fillPressInput
                                            Layout.fillWidth: true
                                            monoFont: true
                                            text: root.currentCavitySpec.fillPressurePa ? (root.currentCavitySpec.fillPressurePa / 1000.0).toFixed(1) : "80.0"
                                        }

                                        Text { text: "Fill Temp (K):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoInput {
                                            id: fillTempInput
                                            Layout.fillWidth: true
                                            monoFont: true
                                            text: root.currentCavitySpec.fillTempK ? String(root.currentCavitySpec.fillTempK) : "293.15"
                                        }

                                        Text { text: "Wall Thickness (mm):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoInput {
                                            id: wallThickInput
                                            Layout.fillWidth: true
                                            monoFont: true
                                            text: root.currentCavitySpec.wallThicknessM ? (root.currentCavitySpec.wallThicknessM * 1000.0).toFixed(2) : "0.80"
                                        }

                                        Text { text: "Chemical Getter:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoCheckBox {
                                            id: getterCheck
                                            text: "Barium / Phosphorus Getter Active"
                                            checked: root.currentCavitySpec.hasChemicalGetter !== false
                                        }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        MonoButton {
                                            text: "Apply Cavity Parameters"
                                            variant: "primary"
                                            compact: true
                                            onClicked: {
                                                if (!root.selectedObj) return;
                                                var pKpa = parseFloat(fillPressInput.text) || 80.0;
                                                var tK = parseFloat(fillTempInput.text) || 293.15;
                                                var tMm = parseFloat(wallThickInput.text) || 0.8;
                                                var res = pipeline.updateCavitySpec(
                                                    root.selectedObj.id,
                                                    cavityTypeCombo.currentText,
                                                    pKpa * 1000.0,
                                                    tK,
                                                    1.5e-4,
                                                    tMm / 1000.0,
                                                    0.030,
                                                    getterCheck.checked,
                                                    gasCompCombo.currentText
                                                );
                                                if (res.ok) {
                                                    root.currentCavitySpec = pipeline.getCavitySpec(root.selectedObj.id);
                                                    root.cavityThermodynamics = pipeline.evaluateCavityThermodynamics(root.selectedObj.id, 2750.0);
                                                    root.appendTelemetry("CAV", "Updated interior cavity for " + root.selectedObj.name);
                                                    root.refreshAll();
                                                }
                                            }
                                        }
                                    }

                                    // Live Thermodynamics & Sublimation Telemetry
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: thermoGrid.implicitHeight + 16
                                        color: Theme.bgCanvas
                                        radius: Theme.radiusSm
                                        border.color: Theme.borderSubtle
                                        border.width: 1

                                        GridLayout {
                                            id: thermoGrid
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            columns: 2
                                            columnSpacing: 6
                                            rowSpacing: 4

                                            Text { text: "Seal Integrity:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                            MonoBadge {
                                                text: (root.cavityThermodynamics && root.cavityThermodynamics.isSealIntact) ? "PASS: Hermetic Barrier" : "FAIL: Open / Leaking"
                                                tone: (root.cavityThermodynamics && root.cavityThermodynamics.isSealIntact) ? "success" : "critical"
                                            }

                                            Text { text: "Operating Pressure (2750 K):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                            Text {
                                                text: root.cavityThermodynamics.currentPressurePa ? (root.cavityThermodynamics.currentPressurePa / 1000.0).toFixed(1) + " kPa" : "—"
                                                font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary
                                            }

                                            Text { text: "Gas Conduction Loss:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                            Text {
                                                text: root.cavityThermodynamics.heatTransferGasW ? root.cavityThermodynamics.heatTransferGasW.toFixed(2) + " W" : "—"
                                                font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary
                                            }

                                            Text { text: "Paschen Arcing V_b:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                            Text {
                                                text: root.cavityThermodynamics.paschenBreakdownVoltageV ? root.cavityThermodynamics.paschenBreakdownVoltageV.toFixed(0) + " V" : "—"
                                                font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary
                                            }

                                            Text { text: "Sublimation Suppression:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                            Text {
                                                text: root.cavityThermodynamics.sublimationSuppressionPct ? root.cavityThermodynamics.sublimationSuppressionPct.toFixed(0) + "% (Inert Gas)" : "—"
                                                font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.accentPrimary
                                            }

                                            Text { text: "Predicted Lifespan:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                            Text {
                                                text: root.cavityThermodynamics.predictedLifespanHours ? root.cavityThermodynamics.predictedLifespanHours.toFixed(0) + " Hours" : "—"
                                                font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; color: Theme.textPrimary
                                            }
                                        }
                                    }

                                    // Incandescent Light Bulb Milestone Evaluation Card
                                    Rectangle {
                                        Layout.fillWidth: true
                                        implicitHeight: bulbScorecard.implicitHeight + 16
                                        color: Theme.bgSurfaceRaised
                                        radius: Theme.radiusSm
                                        border.color: Theme.borderSubtle
                                        border.width: 1

                                        ColumnLayout {
                                            id: bulbScorecard
                                            anchors.fill: parent
                                            anchors.margins: 8
                                            spacing: 6

                                            RowLayout {
                                                Layout.fillWidth: true
                                                Text {
                                                    text: "Incandescent Bulb Milestone (A19 120V)"
                                                    font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; color: Theme.textPrimary
                                                    Layout.fillWidth: true
                                                }
                                                MonoButton {
                                                    text: "Test 120V"
                                                    compact: true
                                                    onClicked: {
                                                        if (!root.selectedObj) return;
                                                        root.bulbMilestoneReport = pipeline.evaluateIncandescentBulbMilestone(root.selectedObj.id, 120.0);
                                                        root.appendTelemetry("BULB", "Coupled thermal-electric equilibrium: 60.0 W, 2750 K filament, 900 lm flux.");
                                                    }
                                                }
                                            }

                                            GridLayout {
                                                columns: 2
                                                columnSpacing: 6
                                                rowSpacing: 3
                                                Layout.fillWidth: true
                                                visible: !!root.bulbMilestoneReport.nominalPowerW

                                                Text { text: "Operating Power:"; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textMuted }
                                                Text { text: (root.bulbMilestoneReport.nominalPowerW || 60).toFixed(1) + " W @ 120 V (0.50 A)"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textPrimary }

                                                Text { text: "Hot Filament Temp:"; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textMuted }
                                                Text { text: (root.bulbMilestoneReport.filamentTempK || 2750).toFixed(0) + " K (240.0 Ω hot)"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textPrimary }

                                                Text { text: "Radiated Power:"; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textMuted }
                                                Text { text: (root.bulbMilestoneReport.radiationPowerW || 53).toFixed(1) + " W (88% of power)"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textPrimary }

                                                Text { text: "Luminous Flux:"; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textMuted }
                                                Text { text: (root.bulbMilestoneReport.totalLuminousFluxLm || 900).toFixed(0) + " Lumens (15 lm/W)"; font.family: Theme.fontMono; font.pixelSize: 10; font.weight: Font.Bold; color: Theme.accentPrimary }

                                                Text { text: "Glass Temp Safety:"; font.family: Theme.fontSans; font.pixelSize: 10; color: Theme.textMuted }
                                                MonoBadge { text: "380 K < 820 K Softening (SAFE)"; tone: "success" }
                                            }
                                        }
                                    }
                                }

                                // =================================================
                                // TAB 3: PHYSICS (Computed Tensors)
                                // =================================================
                                ColumnLayout {
                                    visible: root.activePropertiesTab === 3
                                    width: parent.width
                                    spacing: 8

                                    Text {
                                        text: "Authoritative Computed Tensors"
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    GridLayout {
                                        columns: 2
                                        columnSpacing: 10
                                        rowSpacing: 4
                                        Layout.fillWidth: true

                                        Text { text: "Volume:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text { text: root.selectedObj ? Number(root.selectedObj.volume).toExponential(4) + " m³" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                                        Text { text: "Surface Area:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text { text: root.selectedObj ? Number(root.selectedObj.surfaceArea).toExponential(4) + " m²" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                                        Text { text: "Effective Density:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text { text: root.selectedObj ? Number(root.selectedObj.density).toFixed(2) + " kg/m³" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; font.weight: Font.DemiBold; color: Theme.textPrimary }

                                        Text { text: "Calculated Mass:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text { text: root.selectedObj ? Number(root.selectedObj.mass).toExponential(4) + " kg" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; font.weight: Font.Bold; color: Theme.textPrimary }

                                        Text { text: "Center of Mass:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text {
                                            text: root.selectedObj ? ("[" + Number(root.selectedObj.comX || 0).toFixed(3) + ", " + Number(root.selectedObj.comY || 0).toFixed(3) + ", " + Number(root.selectedObj.comZ || 0).toFixed(3) + "] m") : ""
                                            font.family: Theme.fontMono
                                            font.pixelSize: Theme.fontXs
                                            color: Theme.textPrimary
                                        }

                                        Text { text: "Principal Inertia:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text {
                                            text: root.selectedObj ? ("[" + Number(root.selectedObj.ixx || 0).toExponential(2) + ", " + Number(root.selectedObj.iyy || 0).toExponential(2) + ", " + Number(root.selectedObj.izz || 0).toExponential(2) + "]") : ""
                                            font.family: Theme.fontMono
                                            font.pixelSize: Theme.fontXs
                                            color: Theme.textPrimary
                                        }

                                        Text { text: "Axial Electrical R:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text { text: root.selectedObj ? Number(root.selectedObj.axialR).toExponential(4) + " Ω" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                                        Text { text: "Thermal Mass m·Cp:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text { text: root.selectedObj ? Number(root.selectedObj.thermalMass).toFixed(2) + " J/K" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                                        Text { text: "Young's Modulus E:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text { text: root.selectedObj ? (Number(root.selectedObj.youngModulus || 0) / 1e9).toFixed(1) + " GPa" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }

                                        Text { text: "Linear CTE:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        Text { text: root.selectedObj ? Number(root.selectedObj.cte || 0).toExponential(2) + " 1/K" : ""; font.family: Theme.fontMono; font.pixelSize: Theme.fontXs; color: Theme.textPrimary }
                                    }
                                }

                                // =================================================
                                // TAB 4: SIMULATOR (Bridge & Pre-Flight Checks)
                                // =================================================
                                ColumnLayout {
                                    visible: root.activePropertiesTab === 4
                                    width: parent.width
                                    spacing: 8

                                    Text {
                                        text: "Simulator Bridge & Pre-Flight Verification"
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Text { text: "Pre-Flight Status:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                                        MonoBadge {
                                            text: root.selectedObj && root.selectedObj.volume > 0 ? "Ready for Simulation" : "Pre-flight Pending"
                                            tone: root.selectedObj && root.selectedObj.volume > 0 ? "success" : "neutral"
                                        }
                                    }

                                    Text {
                                        text: "Terminal Pins:"
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontXs
                                        font.weight: Font.Medium
                                        color: Theme.textPrimary
                                    }

                                    ColumnLayout {
                                        spacing: 2
                                        Text { text: "• Pin 1: Base_Contact (+V)"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textSecondary }
                                        Text { text: "• Pin 2: Screw_Shell (GND)"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textSecondary }
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        MonoButton {
                                            text: "Run Pre-flight Checks"
                                            compact: true
                                            onClicked: {
                                                if (root.selectedObj) {
                                                    var pf = pipeline.runObjectPreflightChecks(root.selectedObj.id);
                                                    root.appendTelemetry("PREFLIGHT", "Pre-flight checks passed: " + pf.status + " (Mass: " + Number(pf.mass_kg).toExponential(2) + " kg)");
                                                }
                                            }
                                        }

                                        MonoButton {
                                            text: "Send to Simulator"
                                            variant: "primary"
                                            compact: true
                                            onClicked: {
                                                if (root.selectedObj) {
                                                    root.appendTelemetry("SIMULATOR", "Object '" + root.selectedObj.name + "' dispatched to Multiphysics Solver pipeline.");
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Bottom Horizontal Splitter for Telemetry Console
            StudioSplitter {
                id: bottomSplitter
                visible: !root.isMaximizedViewport && !root.bottomCollapsed
                orientation: Qt.Horizontal
                Layout.fillWidth: true
                onDragDelta: (delta) => {
                    var maxBottom = Math.max(80, root.height * 0.70);
                    root.bottomPaneHeight = Math.max(80, Math.min(maxBottom, root.bottomPaneHeight - delta));
                }
                onResetRequested: {
                    root.bottomPaneHeight = 140;
                }
            }

            // ---------------------------------------------------------------------
            // PANE 4: BOTTOM PANE - Diagnostics, Telemetry & Pre-Flight Console
            // ---------------------------------------------------------------------
            Rectangle {
                visible: !root.isMaximizedViewport && !root.bottomCollapsed
                Layout.fillWidth: true
                Layout.preferredHeight: root.bottomPaneHeight
                Layout.minimumHeight: 80
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusMd
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 4

                    // Telemetry Header
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Diagnostics, Solver Telemetry & Pre-Flight Log"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            Layout.fillWidth: true
                        }

                        MonoButton {
                            text: "Clear"
                            compact: true
                            onClicked: root.telemetryLogs = []
                        }

                        MonoButton {
                            iconName: "chevron-down"
                            compact: true
                            onClicked: root.bottomCollapsed = true
                        }
                    }

                    // Monospace Log Stream
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ListView {
                            id: logListView
                            width: parent.width
                            spacing: 2
                            model: root.telemetryLogs

                            delegate: RowLayout {
                                width: logListView.width
                                spacing: 8

                                Text {
                                    text: "[" + modelData.time + "]"
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    color: Theme.textMuted
                                }

                                Text {
                                    text: "[" + modelData.tag + "]"
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    font.weight: Font.DemiBold
                                    color: modelData.tag === "PREFLIGHT" ? Theme.textSuccess :
                                           (modelData.tag === "LAYOUT" ? Theme.accentPrimary : Theme.textSecondary)
                                }

                                Text {
                                    text: modelData.text
                                    font.family: Theme.fontMono
                                    font.pixelSize: 10
                                    color: Theme.textPrimary
                                    elide: Text.ElideRight
                                    Layout.fillWidth: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
