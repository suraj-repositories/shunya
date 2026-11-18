import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root
    property alias text: input.text
    property string placeholderText: ""
    property bool readOnly: false
    property bool monoFont: false
    property bool clearable: false
    signal editingFinished()
    signal textEdited(string newText)

    implicitHeight: 30
    implicitWidth: 160
    radius: Theme.radiusSm
    color: Theme.bgInput
    border.width: input.activeFocus ? 1.5 : 1
    border.color: input.activeFocus ? Theme.borderFocus : Theme.borderDefault

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: (root.clearable && input.text.length > 0) ? 24 : 8
        verticalAlignment: TextInput.AlignVCenter
        color: Theme.textPrimary
        selectionColor: Theme.borderStrong
        selectedTextColor: Theme.textInverse
        font.family: root.monoFont ? Theme.fontMono : Theme.fontSans
        font.pixelSize: Theme.fontSm
        readOnly: root.readOnly
        clip: true
        onEditingFinished: root.editingFinished()
        onTextEdited: root.textEdited(input.text)

        Keys.onEscapePressed: (event) => {
            if (root.clearable || input.text.length > 0) {
                input.text = "";
                root.textEdited("");
                event.accepted = true;
            }
        }

        Text {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            text: root.placeholderText
            color: Theme.textSubtle
            font: input.font
            visible: input.text.length === 0 && !input.activeFocus
            elide: Text.ElideRight
        }
    }

    Text {
        id: clearBtn
        anchors.right: parent.right
        anchors.rightMargin: 6
        anchors.verticalCenter: parent.verticalCenter
        visible: root.clearable && input.text.length > 0
        text: "✕"
        font.family: Theme.fontSans
        font.pixelSize: 11
        color: clearMa.containsMouse ? Theme.textPrimary : Theme.textMuted

        MouseArea {
            id: clearMa
            anchors.fill: parent
            anchors.margins: -4
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                input.text = "";
                root.textEdited("");
            }
        }
    }
}
