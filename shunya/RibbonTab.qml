import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    property var groups: []

    signal actionTriggered(string actionId)

    RowLayout {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 6

        Repeater {
            model: root.groups
            RibbonGroup {
                title: modelData.title
                actions: modelData.actions
                onActionTriggered: (actionId) => root.actionTriggered(actionId)
            }
        }

        Item { Layout.fillWidth: true } // spacer
    }
}
