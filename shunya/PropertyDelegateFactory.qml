import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string visualType: "number"
    property var propertyValue: null
    property string unitSymbol: ""
    property var allowedUnits: []
    property string formula: ""
    property var minValue: null
    property var maxValue: null
    property var enumOptions: []
    property bool readOnly: false

    signal valueCommitted(var newValue)
    signal formulaCommitted(string newFormula)
    signal unitCommitted(string newUnit)

    implicitHeight: 28
    implicitWidth: 180

    Loader {
        id: delegateLoader
        anchors.fill: parent
        sourceComponent: {
            switch (root.visualType) {
                case "number":
                case "quantity":
                    return quantityComponent
                case "vec3":
                    return vec3Component
                case "bool":
                    return boolComponent
                case "enum":
                    return enumComponent
                case "color":
                    return colorComponent
                case "text":
                default:
                    return textComponent
            }
        }
    }

    // Number & Quantity Editor
    Component {
        id: quantityComponent
        QuantitySpinBox {
            anchors.fill: parent
            value: typeof root.propertyValue === "number" ? root.propertyValue : 0.0
            unitSymbol: root.unitSymbol
            allowedUnits: root.allowedUnits
            formula: root.formula
            minValue: root.minValue
            maxValue: root.maxValue
            readOnly: root.readOnly
            onValueModified: function(v) { root.valueCommitted(v) }
            onFormulaModified: function(f) { root.formulaCommitted(f) }
            onUnitModified: function(u) { root.unitCommitted(u) }
        }
    }

    // Vector3 Coordinate Editor (X, Y, Z)
    Component {
        id: vec3Component
        Row {
            anchors.fill: parent
            spacing: 2

            Repeater {
                model: [
                    { label: "X", idx: 0, color: "#e06c75" },
                    { label: "Y", idx: 1, color: "#98c379" },
                    { label: "Z", idx: 2, color: "#61afef" }
                ]

                Item {
                    width: (parent.width - 4) / 3
                    height: parent.height

                    Rectangle {
                        id: labelBadge
                        width: 14
                        height: parent.height - 2
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        color: modelData.color
                        radius: 2
                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            color: "#1e1e1e"
                            font.pixelSize: 10
                            font.bold: true
                        }
                    }

                    Rectangle {
                        anchors.left: labelBadge.right
                        anchors.leftMargin: 2
                        anchors.right: parent.right
                        height: parent.height
                        color: "#21252b"
                        radius: 2
                        border.color: vInput.activeFocus ? "#61afef" : "#3e4451"

                        TextInput {
                            id: vInput
                            anchors.fill: parent
                            anchors.margins: 2
                            verticalAlignment: TextInput.AlignVCenter
                            color: "#abb2bf"
                            font.pixelSize: 11
                            selectByMouse: true
                            readOnly: root.readOnly

                            text: {
                                var arr = root.propertyValue
                                if (Array.isArray(arr) && arr.length > modelData.idx) {
                                    return Number(arr[modelData.idx].toFixed(3)).toString()
                                }
                                return "0"
                            }

                            onAccepted: {
                                var arr = root.propertyValue
                                if (!Array.isArray(arr)) arr = [0, 0, 0]
                                var newArr = [arr[0] || 0, arr[1] || 0, arr[2] || 0]
                                var num = parseFloat(text)
                                if (!isNaN(num)) {
                                    newArr[modelData.idx] = num
                                    root.valueCommitted(newArr)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Boolean Toggle Switch
    Component {
        id: boolComponent
        Item {
            anchors.fill: parent
            Rectangle {
                width: 38
                height: 20
                anchors.verticalCenter: parent.verticalCenter
                radius: 10
                color: root.propertyValue ? "#98c379" : "#3e4451"

                Rectangle {
                    width: 16
                    height: 16
                    radius: 8
                    anchors.verticalCenter: parent.verticalCenter
                    x: root.propertyValue ? parent.width - width - 2 : 2
                    color: "#ffffff"
                    Behavior on x { NumberAnimation { duration: 100 } }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: !root.readOnly
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.valueCommitted(!root.propertyValue)
                    }
                }
            }
        }
    }

    // Enumeration ComboBox
    Component {
        id: enumComponent
        ComboBox {
            anchors.fill: parent
            model: root.enumOptions
            currentIndex: {
                var idx = root.enumOptions.indexOf(String(root.propertyValue))
                return idx >= 0 ? idx : 0
            }
            enabled: !root.readOnly
            onActivated: function(index) {
                root.valueCommitted(root.enumOptions[index])
            }
        }
    }

    // Color Input
    Component {
        id: colorComponent
        Row {
            anchors.fill: parent
            spacing: 4

            Rectangle {
                width: 24
                height: parent.height - 4
                anchors.verticalCenter: parent.verticalCenter
                radius: 2
                color: String(root.propertyValue).startsWith("#") ? String(root.propertyValue) : "#5c6370"
                border.color: "#3e4451"
                border.width: 1
            }

            Rectangle {
                width: parent.width - 28
                height: parent.height
                color: "#21252b"
                radius: 2
                border.color: "#3e4451"

                TextInput {
                    anchors.fill: parent
                    anchors.margins: 4
                    verticalAlignment: TextInput.AlignVCenter
                    color: "#abb2bf"
                    font.pixelSize: 12
                    text: String(root.propertyValue || "#ffffff")
                    readOnly: root.readOnly
                    onAccepted: root.valueCommitted(text)
                }
            }
        }
    }

    // Plain Text Input
    Component {
        id: textComponent
        Rectangle {
            anchors.fill: parent
            color: root.readOnly ? "#1c1e24" : "#21252b"
            radius: 2
            border.color: tInput.activeFocus ? "#61afef" : "#3e4451"

            TextInput {
                id: tInput
                anchors.fill: parent
                anchors.margins: 4
                verticalAlignment: TextInput.AlignVCenter
                color: root.readOnly ? "#7f848e" : "#abb2bf"
                font.pixelSize: 12
                text: String(root.propertyValue || "")
                readOnly: root.readOnly
                onAccepted: root.valueCommitted(text)
                onActiveFocusChanged: {
                    if (!activeFocus) root.valueCommitted(text)
                }
            }
        }
    }
}
