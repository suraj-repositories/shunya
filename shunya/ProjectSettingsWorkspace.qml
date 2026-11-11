import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import QtQuick.Dialogs

Rectangle {
    id: root
    color: Theme.bgCanvas

    property var valReport: ({})
    property string statusFeedback: ""

    function refreshAll() {
        if (typeof pipeline === "undefined") return;
        nameEdit.text = pipeline.projectName;
        authorEdit.text = pipeline.projectAuthor;
        valReport = pipeline.validateWorkspaceReport();
    }

    Component.onCompleted: refreshAll()
    Connections {
        target: typeof pipeline !== "undefined" ? pipeline : null
        function onWorkspaceChanged() { root.valReport = pipeline.validateWorkspaceReport(); }
    }

    FileDialog {
        id: saveBundleDlg
        title: "Save Shunya v3.0 Engineering Bundle"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Shunya v3 Project (*.shunya3.json)", "All Files (*)"]
        onAccepted: {
            var r = pipeline.saveProjectBundle(selectedFile.toString());
            root.statusFeedback = r.ok ? ("Saved bundle: " + r.path) : ("Save failed: " + r.error);
        }
    }

    FileDialog {
        id: loadBundleDlg
        title: "Load Shunya v3.0 Engineering Bundle"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Shunya v3 Project (*.shunya3.json)", "All Files (*)"]
        onAccepted: {
            var r = pipeline.loadProjectBundle(selectedFile.toString());
            root.statusFeedback = r.ok ? ("Loaded bundle: " + r.path) : ("Load failed: " + r.error);
            root.refreshAll();
        }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 16

        Text { text: "Project Settings, Appearance & Persistence"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXl; font.weight: Font.DemiBold; color: Theme.textPrimary }

        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 16

            // LEFT: Appearance & Single-Project Bundle Metadata
            Rectangle {
                Layout.preferredWidth: 440; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 12

                    Text { text: "Appearance & Theme (Monochrome Black & White)"; font.family: Theme.fontSans; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        MonoButton {
                            text: "Dark Mode (High Contrast)"
                            iconName: "moon"
                            active: Theme.isDarkMode
                            onClicked: Theme.isDarkMode = true
                        }
                        MonoButton {
                            text: "Light Mode (Paper)"
                            iconName: "sun"
                            active: !Theme.isDarkMode
                            onClicked: Theme.isDarkMode = false
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                    Text { text: "Active Single-Project Manifest"; font.family: Theme.fontSans; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    Text { text: "Project Name"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                    MonoInput { id: nameEdit; Layout.fillWidth: true }
                    Text { text: "Lead Engineer / Author"; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textMuted }
                    MonoInput { id: authorEdit; Layout.fillWidth: true }
                    MonoButton {
                        text: "Update Project Metadata"
                        variant: "primary"
                        onClicked: {
                            pipeline.updateProjectMetadata(nameEdit.text, authorEdit.text, "Phase 3 Chemistry Engineering Project");
                            root.statusFeedback = "Updated project metadata.";
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }

                    Text { text: "Project Serialization & Starter Workflow"; font.family: Theme.fontSans; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 8
                        MonoButton { text: "Save Bundle (.shunya3.json)"; iconName: "save"; onClicked: saveBundleDlg.open() }
                        MonoButton { text: "Open Bundle..."; iconName: "open"; onClicked: loadBundleDlg.open() }
                    }
                    MonoButton {
                        Layout.fillWidth: true
                        text: "Seed Complete Lithium Battery Workflow Demo"
                        iconName: "battery"
                        onClicked: {
                            var r = pipeline.seedLithiumBatteryDemo();
                            root.statusFeedback = r.ok ? "Seeded Elements -> LiFePO4 Materials -> Objects -> Battery -> Circuit." : ("Error: " + r.error);
                            root.refreshAll();
                        }
                    }
                    Text { visible: root.statusFeedback !== ""; text: root.statusFeedback; font.family: Theme.fontSans; font.pixelSize: Theme.fontXs; color: Theme.textSecondary; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Item { Layout.fillHeight: true }
                }
            }

            // RIGHT: Integrity & Validation Summary
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true
                color: Theme.bgSurface; border.color: Theme.borderDefault; border.width: 1; radius: Theme.radiusMd

                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 16; spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Workspace Referential Integrity & Readiness"; font.family: Theme.fontSans; font.pixelSize: Theme.fontMd; font.weight: Font.DemiBold; color: Theme.textPrimary; Layout.fillWidth: true }
                        MonoBadge { text: root.valReport.isValid ? "Workspace Valid" : "Issues Detected"; tone: root.valReport.isValid ? "success" : "error" }
                    }

                    GridLayout {
                        columns: 2; columnSpacing: 20; rowSpacing: 6; Layout.fillWidth: true
                        Text { text: "Chemical Elements Loaded:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: typeof pipeline !== "undefined" ? String(pipeline.elementCount) : "0"; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Materials in Registry:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: typeof pipeline !== "undefined" ? String(pipeline.materialCount) : "0"; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Physical Objects:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: typeof pipeline !== "undefined" ? String(pipeline.objectCount) : "0"; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Generated Components:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: typeof pipeline !== "undefined" ? String(pipeline.componentCount) : "0"; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Battery Designs:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: typeof pipeline !== "undefined" ? String(pipeline.batteryCount) : "0"; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                        Text { text: "Circuit Documents:"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; color: Theme.textMuted }
                        Text { text: typeof pipeline !== "undefined" ? String(pipeline.circuitCount) : "0"; font.family: Theme.fontMono; font.pixelSize: Theme.fontSm; color: Theme.textPrimary }
                    }

                    Text { text: "Diagnostic Report"; font.family: Theme.fontSans; font.pixelSize: Theme.fontSm; font.weight: Font.DemiBold; color: Theme.textPrimary }
                    ListView {
                        Layout.fillWidth: true; Layout.fillHeight: true; clip: true; spacing: 4
                        model: (root.valReport.errors || []).concat(root.valReport.warnings || [])
                        delegate: MonoBadge { text: modelData; tone: index < (root.valReport.errors || []).length ? "error" : "warning" }
                    }
                }
            }
        }
    }
}
