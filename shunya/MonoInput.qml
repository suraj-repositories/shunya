import QtQuick
import QtQuick.Controls.Basic

Rectangle {
    id: root
    property alias text: input.text
    property string placeholderText: ""
    property bool readOnly: false
    property bool monoFont: false
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
        anchors.rightMargin: 8
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
}
