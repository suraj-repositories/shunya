import QtQuick
import QtQuick.Controls

Item {
    id: root

    property double value: 0.0
    property string unitSymbol: ""
    property var allowedUnits: []
    property string formula: ""
    property var minValue: null
    property var maxValue: null
    property bool readOnly: false

    signal valueModified(double newValue)
    signal formulaModified(string newFormula)
    signal unitModified(string newUnit)

    implicitHeight: 28
    implicitWidth: 160

    property bool isFormula: textInput.text.trim().startsWith("=") || (formula !== "" && formula !== undefined)
    property bool hasError: false
    property string errorMessage: ""
    property string previewText: ""

    Row {
        anchors.fill: parent
        spacing: 4

        // Numeric or formula text field
        Rectangle {
            id: inputContainer
            width: (unitMenuButton.visible) ? parent.width - unitMenuButton.width - 4 : parent.width
            height: parent.height
            color: root.readOnly ? "#1c1e24" : (root.hasError ? "#3d1b1b" : "#21252b")
            radius: 3
            border.color: root.hasError ? "#e06c75" : (textInput.activeFocus ? "#61afef" : "#3e4451")
            border.width: textInput.activeFocus ? 2 : 1

            TextInput {
                id: textInput
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                verticalAlignment: TextInput.AlignVCenter
                color: root.readOnly ? "#7f848e" : (root.isFormula ? "#98c379" : "#abb2bf")
                font.family: "Segoe UI, -apple-system, sans-serif"
                font.pixelSize: 12
                font.bold: root.isFormula
                selectByMouse: true
                readOnly: root.readOnly

                text: {
                    if (root.formula !== "" && root.formula !== undefined) {
                        return root.formula
                    }
                    return (Math.abs(root.value) < 1e-4 && root.value !== 0) 
                        ? root.value.toExponential(3) 
                        : Number(root.value.toFixed(4)).toString()
                }

                onTextEdited: {
                    root.updateFromText(text)
                }

                onAccepted: {
                    root.commitValue()
                }

                onActiveFocusChanged: {
                    if (!activeFocus) {
                        root.commitValue()
                    }
                }
            }
        }

        // Unit selector dropdown button
        Rectangle {
            id: unitMenuButton
            visible: root.allowedUnits && root.allowedUnits.length > 1
            width: Math.max(36, unitLabel.contentWidth + 14)
            height: parent.height
            color: unitMouseArea.containsMouse ? "#3b4048" : "#2c313a"
            radius: 3
            border.color: "#3e4451"
            border.width: 1

            Row {
                anchors.centerIn: parent
                spacing: 2
                Text {
                    id: unitLabel
                    text: root.unitSymbol !== "" ? root.unitSymbol : (root.allowedUnits.length > 0 ? root.allowedUnits[0] : "")
                    color: "#98c379"
                    font.pixelSize: 11
                    font.bold: true
                }
                Text {
                    text: "▾"
                    color: "#abb2bf"
                    font.pixelSize: 8
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: unitMouseArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: unitMenu.open()
            }

            Menu {
                id: unitMenu
                y: unitMenuButton.height

                Repeater {
                    model: root.allowedUnits
                    MenuItem {
                        text: modelData
                        highlighted: modelData === root.unitSymbol
                        onTriggered: {
                            if (typeof quantityController !== "undefined") {
                                var converted = quantityController.convertUnit(root.value, root.unitSymbol, modelData)
                                var num = parseFloat(converted)
                                if (!isNaN(num)) {
                                    root.value = num
                                }
                            }
                            root.unitSymbol = modelData
                            root.unitModified(modelData)
                        }
                    }
                }
            }
        }
    }

    function updateFromText(str) {
        var trimmed = str.trim()
        if (trimmed.startsWith("=")) {
            root.isFormula = true
            if (typeof quantityController !== "undefined") {
                var evalRes = quantityController.evaluateExpression(trimmed)
                if (evalRes.success) {
                    root.hasError = false
                    root.errorMessage = ""
                    root.previewText = "= " + evalRes.value
                } else {
                    root.hasError = true
                    root.errorMessage = evalRes.error
                    root.previewText = "Error: " + evalRes.error
                }
            }
            return
        }

        root.isFormula = false
        if (typeof quantityController !== "undefined") {
            var res = quantityController.parseQuantity(trimmed, root.unitSymbol)
            if (res.success) {
                root.hasError = false
                root.errorMessage = ""
                if (res.unit !== "" && res.unit !== root.unitSymbol) {
                    root.previewText = "SI: " + res.canonicalValue + " " + res.dimension
                } else {
                    root.previewText = ""
                }
            } else {
                root.hasError = true
                root.errorMessage = res.error
            }
        }
    }

    function commitValue() {
        var trimmed = textInput.text.trim()
        if (trimmed.startsWith("=")) {
            root.formula = trimmed
            root.formulaModified(trimmed)
            if (typeof quantityController !== "undefined") {
                var evalRes = quantityController.evaluateExpression(trimmed)
                if (evalRes.success) {
                    root.value = evalRes.value
                    root.valueModified(evalRes.value)
                }
            }
            return
        }

        root.formula = ""
        root.formulaModified("")

        if (typeof quantityController !== "undefined") {
            var parseRes = quantityController.parseQuantity(trimmed, root.unitSymbol)
            if (parseRes.success) {
                root.value = parseRes.displayValue
                if (parseRes.unit !== "" && parseRes.unit !== root.unitSymbol) {
                    root.unitSymbol = parseRes.unit
                    root.unitModified(parseRes.unit)
                }
                root.valueModified(parseRes.canonicalValue)
                return
            }
        }

        var num = parseFloat(trimmed)
        if (!isNaN(num)) {
            if (root.minValue !== null && num < root.minValue) num = root.minValue
            if (root.maxValue !== null && num > root.maxValue) num = root.maxValue
            root.value = num
            root.valueModified(num)
        }
    }
}
