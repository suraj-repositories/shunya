import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import shunya

Rectangle {
    id: root
    color: Theme.bgCanvas

    // =========================================================================
    // Core Domain Data & Assembly State (OBJ-ASSY)
    // =========================================================================
    property var objectsList: []
    property var assemblyParts: []
    property var assemblyJoints: []
    property var assemblyWires: []
    property int selectedPartIdx: 0
    property int activeAssemblyTab: 0 // 0: Parts, 1: Joints, 2: Wires, 3: Thermal, 4: Pre-Flight
    property var preflightReport: null
    property bool showAssemblyPreflightModal: false

    readonly property var currentPart: (selectedPartIdx >= 0 && selectedPartIdx < assemblyParts.length) ? assemblyParts[selectedPartIdx] : null

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        objectsList = pipeline.getObjects();
        assemblyParts = pipeline.getAssemblyParts();
        assemblyJoints = pipeline.getAssemblyJoints();
        assemblyWires = pipeline.getAssemblyWires();
        if (selectedPartIdx >= assemblyParts.length) selectedPartIdx = Math.max(0, assemblyParts.length - 1);
        if (currentPart && vp3d) {
            vp3d.selectedObjectId = currentPart.objectId;
        }
        if (vp3d) vp3d.refreshScene();
    }

    Component.onCompleted: refreshAll()
    Connections {
        target: typeof pipeline !== "undefined" ? pipeline : null
        function onWorkspaceChanged() { root.refreshAll(); }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // =========================================================================
        // Top Header & Viewport Controls
        // =========================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            ColumnLayout {
                spacing: 2
                Text {
                    text: "3D CAD & Multi-Object Assembly Studio"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontLg
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: "Combine physical objects into kinematic mechanisms with electrical wiring and thermal contacts"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }
            }

            Item { Layout.fillWidth: true }

            // Camera Views
            RowLayout {
                spacing: 4
                Text { text: "Views:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                Repeater {
                    model: ["Iso", "Front", "Top", "Right"]
                    delegate: MonoButton {
                        text: modelData
                        compact: true
                        onClicked: vp3d.snapView(modelData)
                    }
                }
            }

            Rectangle { width: 1; height: 16; color: Theme.borderSubtle }

            MonoButton {
                text: "Wireframe"
                compact: true
                active: vp3d.wireframe
                onClicked: vp3d.wireframe = !vp3d.wireframe
            }

            MonoButton {
                text: "Thermal Overlay"
                compact: true
                active: vp3d.showThermalOverlay
                onClicked: vp3d.showThermalOverlay = !vp3d.showThermalOverlay
            }

            MonoButton {
                text: "Send to Simulator"
                variant: "primary"
                compact: true
                onClicked: {
                    if (typeof pipeline !== "undefined") {
                        var rep = pipeline.runAssemblyPreflightChecks();
                        if (!rep.ok) {
                            root.preflightReport = rep;
                            root.showAssemblyPreflightModal = true;
                        } else {
                            pipeline.sendAssemblyToSimulator();
                        }
                    }
                }
            }
        }

        // =========================================================================
        // Main 3-Pane Assembly Layout
        // =========================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            // ---------------------------------------------------------------------
            // LEFT PANE: Object Library Catalog
            // ---------------------------------------------------------------------
            Rectangle {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                radius: Theme.radiusMd
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Text {
                        text: "Object Library Catalog"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontSm
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                    }

                    Text {
                        text: "Click '+ Add' to place part instance into assembly:"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontXs
                        color: Theme.textMuted
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ListView {
                            width: parent.width
                            spacing: 4
                            model: root.objectsList

                            delegate: Rectangle {
                                width: parent.width
                                height: 48
                                radius: Theme.radiusSm
                                color: Theme.bgCanvas
                                border.color: Theme.borderSubtle
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    spacing: 6

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 1

                                        Text {
                                            text: modelData.name
                                            font.family: Theme.fontSans
                                            font.pixelSize: Theme.fontXs
                                            font.weight: Font.Medium
                                            color: Theme.textPrimary
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: modelData.shape + " (" + Number(modelData.mass).toExponential(1) + " kg)"
                                            font.family: Theme.fontMono
                                            font.pixelSize: 10
                                            color: Theme.textMuted
                                        }
                                    }

                                    MonoButton {
                                        text: "+ Add"
                                        compact: true
                                        onClicked: {
                                            if (typeof pipeline !== "undefined") {
                                                pipeline.addAssemblyPartInstance(modelData.id, modelData.name + "_Part");
                                                root.refreshAll();
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ---------------------------------------------------------------------
            // CENTER PANE: 3D Interactive Viewport
            // ---------------------------------------------------------------------
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.radiusMd
                color: Theme.bgPrimary
                border.color: Theme.borderDefault
                border.width: 1
                clip: true

                Viewport3DItem {
                    id: vp3d
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
                            if (btn === Qt.RightButton) vp3d.panBy(m.x - lx, m.y - ly);
                            else vp3d.orbitBy((m.x - lx) * 0.5, (m.y - ly) * 0.5);
                            lx = m.x; ly = m.y;
                        }
                        onWheel: (w) => vp3d.zoomBy(w.angleDelta.y > 0 ? 0.9 : 1.1)
                    }
                }

                // Viewport HUD
                Rectangle {
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    anchors.margins: 10
                    height: 24
                    width: hudTxt.implicitWidth + 14
                    radius: Theme.radiusSm
                    color: Theme.bgSurface
                    border.color: Theme.borderDefault
                    border.width: 1

                    Text {
                        id: hudTxt
                        anchors.centerIn: parent
                        text: "Assembly Parts: " + root.assemblyParts.length + " | Joints: " + root.assemblyJoints.length + " | Wires: " + root.assemblyWires.length + " | Tris: " + vp3d.triangleCount
                        font.family: Theme.fontMono
                        font.pixelSize: 10
                        color: Theme.textSecondary
                    }
                }
            }

            // ---------------------------------------------------------------------
            // RIGHT PANE: Tabbed Assembly Inspector
            // ---------------------------------------------------------------------
            Rectangle {
                Layout.preferredWidth: 360
                Layout.fillHeight: true
                radius: Theme.radiusMd
                color: Theme.bgSurface
                border.color: Theme.borderDefault
                border.width: 1
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    // Tab Selector Bar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Repeater {
                            model: ["Parts", "Joints", "Wiring", "Thermal", "PreFlight"]
                            delegate: MonoButton {
                                text: modelData
                                compact: true
                                active: root.activeAssemblyTab === index
                                onClicked: root.activeAssemblyTab = index
                            }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                    // TAB 0: PARTS & MATING SNAPS
                    ColumnLayout {
                        visible: root.activeAssemblyTab === 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 6

                        Text {
                            text: "Assembly Part Instances (" + root.assemblyParts.length + "):"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        ListView {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 120
                            clip: true
                            spacing: 3
                            model: root.assemblyParts

                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 32
                                radius: Theme.radiusSm
                                color: index === root.selectedPartIdx ? Theme.bgActive : Theme.bgCanvas
                                border.color: index === root.selectedPartIdx ? Theme.borderStrong : Theme.borderSubtle
                                border.width: 1

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 6
                                    Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; Layout.fillWidth: true; elide: Text.ElideRight }
                                    Text { text: "(" + modelData.posX.toFixed(2) + ", " + modelData.posY.toFixed(2) + ")"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.textMuted }
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: {
                                        root.selectedPartIdx = index;
                                        if (vp3d) vp3d.selectedObjectId = modelData.objectId;
                                    }
                                }
                            }
                        }

                        // Precision Mating Snaps
                        Text {
                            text: "Precision Mating Alignments:"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            MonoButton {
                                text: "Concentric Snap"
                                compact: true
                                Layout.fillWidth: true
                                onClicked: {
                                    if (root.assemblyParts.length >= 2) {
                                        pipeline.snapAssemblyConcentric(root.assemblyParts[0].instanceId, root.assemblyParts[1].instanceId);
                                        root.refreshAll();
                                    }
                                }
                            }

                            MonoButton {
                                text: "Face-to-Face"
                                compact: true
                                Layout.fillWidth: true
                                onClicked: {
                                    if (root.assemblyParts.length >= 2) {
                                        pipeline.snapAssemblyFaceToFace(root.assemblyParts[0].instanceId, root.assemblyParts[1].instanceId, 0.05);
                                        root.refreshAll();
                                    }
                                }
                            }
                        }

                        // Transform Adjuster
                        Text {
                            text: "Position Offset (X, Y, Z):"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            color: Theme.textMuted
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            MonoButton { text: "+X (1cm)"; compact: true; Layout.fillWidth: true; onClicked: if (root.currentPart) { pipeline.updateAssemblyPartTransform(root.currentPart.instanceId, root.currentPart.posX + 0.01, root.currentPart.posY, root.currentPart.posZ, 0, 0, 0, 1, 1, 1); root.refreshAll(); } }
                            MonoButton { text: "+Y (1cm)"; compact: true; Layout.fillWidth: true; onClicked: if (root.currentPart) { pipeline.updateAssemblyPartTransform(root.currentPart.instanceId, root.currentPart.posX, root.currentPart.posY + 0.01, root.currentPart.posZ, 0, 0, 0, 1, 1, 1); root.refreshAll(); } }
                            MonoButton { text: "+Z (1cm)"; compact: true; Layout.fillWidth: true; onClicked: if (root.currentPart) { pipeline.updateAssemblyPartTransform(root.currentPart.instanceId, root.currentPart.posX, root.currentPart.posY, root.currentPart.posZ + 0.01, 0, 0, 0, 1, 1, 1); root.refreshAll(); } }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            MonoButton {
                                text: "Delete Part"
                                compact: true
                                Layout.fillWidth: true
                                onClicked: {
                                    if (root.currentPart) {
                                        pipeline.removeAssemblyPart(root.currentPart.instanceId);
                                        root.refreshAll();
                                    }
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }

                    // TAB 1: KINEMATIC JOINTS
                    ColumnLayout {
                        visible: root.activeAssemblyTab === 1
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8

                        Text {
                            text: "Kinematic Mechanism Constraints:"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        GridLayout {
                            columns: 2
                            Layout.fillWidth: true
                            rowSpacing: 4
                            columnSpacing: 4

                            MonoButton {
                                text: "+ Revolute (Hinge)"
                                compact: true
                                Layout.fillWidth: true
                                onClicked: {
                                    if (root.assemblyParts.length >= 2) {
                                        pipeline.addAssemblyJoint("Arm_Hinge", "Revolute", root.assemblyParts[0].instanceId, root.assemblyParts[1].instanceId);
                                        root.refreshAll();
                                    }
                                }
                            }
                            MonoButton {
                                text: "+ Threaded Screw"
                                compact: true
                                Layout.fillWidth: true
                                onClicked: {
                                    if (root.assemblyParts.length >= 2) {
                                        pipeline.addAssemblyJoint("Bulb_Socket_Thread", "ThreadedScrew", root.assemblyParts[0].instanceId, root.assemblyParts[1].instanceId);
                                        root.refreshAll();
                                    }
                                }
                            }
                            MonoButton {
                                text: "+ Prismatic (Slider)"
                                compact: true
                                Layout.fillWidth: true
                                onClicked: {
                                    if (root.assemblyParts.length >= 2) {
                                        pipeline.addAssemblyJoint("Linear_Slide", "Prismatic", root.assemblyParts[0].instanceId, root.assemblyParts[1].instanceId);
                                        root.refreshAll();
                                    }
                                }
                            }
                            MonoButton {
                                text: "+ Ball (Spherical)"
                                compact: true
                                Layout.fillWidth: true
                                onClicked: {
                                    if (root.assemblyParts.length >= 2) {
                                        pipeline.addAssemblyJoint("Ball_Socket", "Ball", root.assemblyParts[0].instanceId, root.assemblyParts[1].instanceId);
                                        root.refreshAll();
                                    }
                                }
                            }
                        }

                        Text { text: "Active Mechanism Joints (" + root.assemblyJoints.length + "):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 3
                            model: root.assemblyJoints
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 32
                                radius: Theme.radiusSm
                                color: Theme.bgCanvas
                                border.color: Theme.borderSubtle
                                border.width: 1
                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 6
                                    Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; Layout.fillWidth: true }
                                    MonoBadge { text: modelData.kind; tone: "info" }
                                }
                            }
                        }
                    }

                    // TAB 2: ELECTRICAL WIRING
                    ColumnLayout {
                        visible: root.activeAssemblyTab === 2
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8

                        Text {
                            text: "3D Wire Routing Between Terminals:"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        MonoButton {
                            text: "+ Route AWG 22 Wire (Socket -> Bulb)"
                            compact: true
                            Layout.fillWidth: true
                            onClicked: {
                                if (root.assemblyParts.length >= 2) {
                                    pipeline.addAssemblyWire("Power_Lead", root.assemblyParts[0].instanceId, "Socket_Pin1", root.assemblyParts[1].instanceId, "Base_Contact", 22, 0.25);
                                    pipeline.addAssemblyWire("Return_Lead", root.assemblyParts[0].instanceId, "Socket_Pin2", root.assemblyParts[1].instanceId, "Screw_Shell", 22, 0.25);
                                    root.refreshAll();
                                }
                            }
                        }

                        Text { text: "Connected Wire Netlist (" + root.assemblyWires.length + "):"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        ListView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 3
                            model: root.assemblyWires
                            delegate: Rectangle {
                                width: ListView.view.width
                                height: 40
                                radius: Theme.radiusSm
                                color: Theme.bgCanvas
                                border.color: Theme.borderSubtle
                                border.width: 1
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 4; spacing: 1
                                    RowLayout {
                                        Text { text: modelData.name; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; Layout.fillWidth: true }
                                        Text { text: "R: " + (modelData.resistance_ohm * 1000).toFixed(1) + " mΩ"; font.family: Theme.fontMono; font.pixelSize: 10; color: Theme.accentPrimary }
                                    }
                                    Text { text: modelData.termA + " -> " + modelData.termB + " (AWG " + modelData.awg + ")"; font.family: Theme.fontMono; font.pixelSize: 9; color: Theme.textMuted }
                                }
                            }
                        }
                    }

                    // TAB 3: THERMAL CONTACTS
                    ColumnLayout {
                        visible: root.activeAssemblyTab === 3
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8

                        Text {
                            text: "Surface Thermal Contact Interfaces:"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        MonoButton {
                            text: "+ Add Thermal Joint (hc = 5000 W/m²K)"
                            compact: true
                            Layout.fillWidth: true
                            onClicked: {
                                if (root.assemblyParts.length >= 2) {
                                    pipeline.addAssemblyThermalContact("Bulb_Socket_Interface", root.assemblyParts[0].instanceId, root.assemblyParts[1].instanceId, 0.0002, 5000.0);
                                    root.refreshAll();
                                }
                            }
                        }

                        Text { text: "Coupled Thermal Boundary Networks"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                        Item { Layout.fillHeight: true }
                    }

                    // TAB 4: PRE-FLIGHT & SIMULATION BRIDGE
                    ColumnLayout {
                        visible: root.activeAssemblyTab === 4
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 8

                        Text {
                            text: "Assembly Verification & Co-Simulation:"
                            font.family: Theme.fontSans
                            font.pixelSize: Theme.fontXs
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                        }

                        MonoButton {
                            text: "Run Assembly Pre-Flight"
                            compact: true
                            Layout.fillWidth: true
                            onClicked: {
                                var rep = pipeline.runAssemblyPreflightChecks();
                                root.preflightReport = rep;
                                root.showAssemblyPreflightModal = true;
                            }
                        }

                        MonoButton {
                            text: "Send to Multiphysics Simulator"
                            variant: "primary"
                            compact: true
                            Layout.fillWidth: true
                            onClicked: {
                                var rep = pipeline.runAssemblyPreflightChecks();
                                if (!rep.ok) {
                                    root.preflightReport = rep;
                                    root.showAssemblyPreflightModal = true;
                                } else {
                                    pipeline.sendAssemblyToSimulator();
                                }
                            }
                        }

                        Item { Layout.fillHeight: true }
                    }
                }
            }
        }
    }

    // =========================================================================
    // Assembly Pre-Flight Modal Dialog (PH5-28.02.006)
    // =========================================================================
    Rectangle {
        visible: root.showAssemblyPreflightModal
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.65)
        z: 999

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 520)
            height: Math.min(parent.height - 40, 420)
            radius: Theme.radiusMd
            color: Theme.bgSurface
            border.color: Theme.borderStrong
            border.width: 1
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    MonoBadge {
                        text: root.preflightReport && root.preflightReport.ok ? "PRE-FLIGHT PASSED" : "PRE-FLIGHT FAILED"
                        tone: root.preflightReport && root.preflightReport.ok ? "success" : "error"
                    }
                    Text {
                        text: "Assembly Pre-Flight Diagnostics"
                        font.family: Theme.fontSans
                        font.pixelSize: Theme.fontMd
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        Layout.fillWidth: true
                    }
                    MonoButton {
                        text: "Close"
                        compact: true
                        onClicked: root.showAssemblyPreflightModal = false
                    }
                }

                Text {
                    text: root.preflightReport && root.preflightReport.ok
                        ? "All kinematic constraints, part bindings, and electrical loops verified. Ready for co-simulation."
                        : "Blocking assembly defects detected:"
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.textMuted
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ListView {
                        width: parent.width
                        spacing: 6
                        model: root.preflightReport ? root.preflightReport.issues : []
                        delegate: Rectangle {
                            width: parent.width
                            height: 48
                            radius: Theme.radiusSm
                            color: Theme.bgCanvas
                            border.color: Theme.borderError
                            border.width: 1

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 2
                                RowLayout {
                                    MonoBadge { text: modelData.ruleId; tone: "error" }
                                    Text { text: modelData.message; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textPrimary; Layout.fillWidth: true; elide: Text.ElideRight }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
