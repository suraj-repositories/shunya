import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Dialog {
    id: root
    title: "Saved Mixture Experiments Manager"
    width: 680
    height: 480
    modal: true
    anchors.centerIn: parent

    background: Rectangle {
        color: Theme.bgSurface
        border.color: Theme.borderDefault
        border.width: 1
        radius: Theme.radiusMd
    }

    signal loadExperimentRequested(var experiment)

    property var experimentList: []
    property string searchQuery: ""
    property string toastText: ""
    property var lastDeletedId: ""

    function refresh() {
        if (typeof pipeline === "undefined") return;
        experimentList = pipeline.listSavedExperimentFiles();
    }

    onOpened: refresh()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 10

        // Search & Filter Toolbar
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            MonoInput {
                Layout.fillWidth: true
                placeholder: "Search saved experiments by name, author, or scenario..."
                text: root.searchQuery
                onTextEdited: (t) => root.searchQuery = t
            }

            MonoButton {
                text: "Refresh"
                compact: true
                onClicked: root.refresh()
            }
        }

        // Notification banner
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 28
            radius: Theme.radiusSm
            color: Qt.rgba(Theme.statusSuccess.r, Theme.statusSuccess.g, Theme.statusSuccess.b, 0.15)
            border.color: Theme.statusSuccess
            border.width: 1
            visible: root.toastText.length > 0

            RowLayout {
                anchors.fill: parent
                anchors.margins: 6
                Text {
                    text: root.toastText
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontXs
                    color: Theme.statusSuccess
                    Layout.fillWidth: true
                }
            }
        }

        // Experiment List View
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 6

                Repeater {
                    model: {
                        var q = root.searchQuery.toLowerCase().trim();
                        var list = root.experimentList || [];
                        if (q.length === 0) return list;
                        var res = [];
                        for (var i = 0; i < list.length; ++i) {
                            var item = list[i];
                            if (item.name.toLowerCase().indexOf(q) !== -1 ||
                                item.author.toLowerCase().indexOf(q) !== -1 ||
                                item.scenario.toLowerCase().indexOf(q) !== -1) {
                                res.push(item);
                            }
                        }
                        return res;
                    }

                    delegate: Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 64
                        radius: Theme.radiusSm
                        color: Theme.bgElevated
                        border.color: Theme.borderDefault
                        border.width: 1

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 8

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                RowLayout {
                                    spacing: 6
                                    Text {
                                        text: modelData.name
                                        font.family: Theme.fontSans
                                        font.pixelSize: Theme.fontSm
                                        font.weight: Font.DemiBold
                                        color: Theme.textPrimary
                                    }
                                    MonoBadge {
                                        text: modelData.scenario
                                        tone: "neutral"
                                    }
                                }

                                Text {
                                    text: "Author: " + modelData.author + "  •  Modified: " + modelData.modifiedAt
                                    font.family: Theme.fontSans
                                    font.pixelSize: Theme.fontXs
                                    color: Theme.textMuted
                                }
                            }

                            // Actions
                            MonoButton {
                                text: "Load"
                                compact: true
                                onClicked: {
                                    root.loadExperimentRequested(modelData);
                                    root.close();
                                }
                            }

                            MonoButton {
                                text: "Duplicate"
                                compact: true
                                variant: "outline"
                                onClicked: {
                                    if (typeof pipeline !== "undefined") {
                                        pipeline.duplicateExperimentFile(modelData.id);
                                        root.refresh();
                                        root.toastText = "Duplicated " + modelData.name;
                                    }
                                }
                            }

                            MonoButton {
                                text: "Trash"
                                compact: true
                                variant: "outline"
                                onClicked: {
                                    if (typeof pipeline !== "undefined") {
                                        pipeline.deleteExperimentFile(modelData.id);
                                        root.refresh();
                                        root.toastText = "Moved " + modelData.name + " to trash";
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
