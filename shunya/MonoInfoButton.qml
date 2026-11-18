import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic

Item {
    id: root
    implicitWidth: 18
    implicitHeight: 18

    property string title: "Information"
    property string description: ""
    property string formula: ""
    property string parameterRange: ""
    property string helpAnchor: "mixing-lab-overview"

    signal helpRequested(string anchor)

    Rectangle {
        id: iconCircle
        anchors.fill: parent
        radius: width / 2
        color: mouseArea.containsMouse ? Theme.bgHover : "transparent"
        border.color: mouseArea.containsMouse ? Theme.borderFocus : Theme.borderSubtle
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "ⓘ"
            font.family: Theme.fontSans
            font.pixelSize: 11
            color: mouseArea.containsMouse ? Theme.accentPrimary : Theme.textMuted
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: popover.visible = !popover.visible
    }

    Popup {
        id: popover
        parent: root
        x: (parent.width - width) / 2
        y: parent.height + 4
        width: 260
        padding: 10
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: Theme.bgElevated
            border.color: Theme.borderDefault
            border.width: 1
            radius: Theme.radiusSm
        }

        contentItem: ColumnLayout {
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text {
                    text: root.title
                    font.family: Theme.fontSans
                    font.pixelSize: Theme.fontSm
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                    Layout.fillWidth: true
                }
                Text {
                    text: "✕"
                    font.pixelSize: 10
                    color: Theme.textMuted
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: popover.close()
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: Theme.borderSubtle
            }

            Text {
                text: root.description
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                visible: root.description.length > 0
            }

            // Formula box
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: formulaText.implicitHeight + 8
                color: Theme.bgCanvas
                radius: Theme.radiusSm
                border.color: Theme.borderSubtle
                border.width: 1
                visible: root.formula.length > 0

                Text {
                    id: formulaText
                    anchors.fill: parent
                    anchors.margins: 4
                    text: root.formula
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontXs
                    font.weight: Font.DemiBold
                    color: Theme.accentPrimary
                    wrapMode: Text.WordWrap
                }
            }

            // Valid range
            Text {
                text: "Range: " + root.parameterRange
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontXs
                color: Theme.textMuted
                visible: root.parameterRange.length > 0
            }

            // Learn more link
            Text {
                text: "Learn more in Scientific Help Center →"
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontXs
                font.weight: Font.Medium
                color: linkMouse.containsMouse ? Theme.accentHover : Theme.accentPrimary
                Layout.topMargin: 2

                MouseArea {
                    id: linkMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        popover.close();
                        root.helpRequested(root.helpAnchor);
                        // Forward to top-level app or window if available
                        if (typeof appWindow !== "undefined" && appWindow.openHelpCenter) {
                            appWindow.openHelpCenter(root.helpAnchor);
                        }
                    }
                }
            }
        }
    }
}
