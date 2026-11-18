import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Item {
    id: root
    property var model: []
    property int currentIndex: 0
    property string currentText: (model && model.length > currentIndex && currentIndex >= 0) ? model[currentIndex] : ""
    property string placeholderText: ""
    property alias count: combo.count
    signal activated(int index, string value)

    implicitHeight: 36
    implicitWidth: 160

    ComboBox {
        id: combo
        anchors.fill: parent
        model: root.model
        currentIndex: root.currentIndex

        onActivated: (index) => {
            root.currentIndex = index;
            root.activated(index, combo.textAt(index));
        }

        background: Rectangle {
            implicitHeight: 36
            radius: Theme.radiusSm
            color: combo.activeFocus ? Theme.bgSurface : Theme.bgInput
            border.color: combo.activeFocus ? Theme.borderFocus : (combo.hovered ? Theme.borderStrong : Theme.borderDefault)
            border.width: combo.activeFocus ? 1.5 : 1
        }

        contentItem: Text {
            text: combo.displayText || root.placeholderText
            font.family: Theme.fontSans
            font.pixelSize: Theme.fontSm
            color: combo.displayText ? Theme.textPrimary : Theme.textSubtle
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 28
            elide: Text.ElideRight
        }

        indicator: Text {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            font.family: Theme.fontSans
            font.pixelSize: 12
            color: combo.down ? Theme.textPrimary : Theme.textMuted
            rotation: combo.popup.visible ? 180 : 0
            Behavior on rotation {
                NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
            }
        }

        popup: Popup {
            y: combo.height + 3
            width: Math.max(combo.width, 140)
            implicitHeight: Math.min(240, contentItem.implicitHeight + 6)
            padding: 3

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: combo.popup.visible ? combo.delegateModel : null
                currentIndex: combo.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                    contentItem: Rectangle {
                        implicitWidth: 4
                        radius: 2
                        color: Theme.borderDefault
                    }
                }
            }

            background: Rectangle {
                color: Theme.bgElevated
                border.color: Theme.borderDefault
                border.width: 1
                radius: Theme.radiusSm
            }
        }

        delegate: ItemDelegate {
            id: itemDel
            width: combo.width
            height: 32
            highlighted: combo.highlightedIndex === index

            contentItem: Text {
                text: typeof modelData === "string" ? modelData : (modelData.text || "")
                color: itemDel.highlighted ? Theme.textPrimary : Theme.textSecondary
                font.family: Theme.fontSans
                font.pixelSize: Theme.fontSm
                verticalAlignment: Text.AlignVCenter
                leftPadding: 10
                rightPadding: 10
                elide: Text.ElideRight
            }

            background: Rectangle {
                color: itemDel.highlighted ? Theme.bgActive : (itemDel.hovered ? Theme.bgHover : "transparent")
                radius: 2
            }
        }
    }
}
