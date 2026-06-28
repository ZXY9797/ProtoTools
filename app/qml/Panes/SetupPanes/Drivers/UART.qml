import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

/**
 * UART 配置面板
 *
 * 直接绑定 uartDriver 的 Q_PROPERTY，无需间接 setUiDriverProperty。
 * 端口列表支持热插拔轮询（ConnectionManager 每秒检测）。
 */
ColumnLayout {
    id: root
    spacing: 7

    property bool _restoring: true
    property int controlHeight: 30
    property int labelWidth: 52
    property int actionWidth: 32
    property var baudRates: ["9600", "19200", "38400", "57600", "115200", "256000", "460800", "921600", "1000000", "2000000"]

    readonly property color surface: themeManager.isDark ? "#131920" : "#FFFFFF"
    readonly property color surfaceHover: themeManager.isDark ? "#1A222C" : "#F3F4F6"
    readonly property color fieldBg: themeManager.isDark ? "#0F141B" : "#F9FAFB"
    readonly property color outline: themeManager.isDark ? "#253040" : "#D1D5DB"
    readonly property color outlineActive: themeManager.isDark ? "#3B8AFF" : "#2563EB"
    readonly property color textPrimary: themeManager.isDark ? "#E8EDF5" : "#111827"
    readonly property color textSecondary: themeManager.isDark ? "#9CAAB8" : "#6B7280"
    readonly property color textMuted: themeManager.isDark ? "#607080" : "#9CA3AF"
    readonly property color accent: themeManager.isDark ? "#3B8AFF" : "#2563EB"
    readonly property color success: themeManager.isDark ? "#34D399" : "#059669"

    function syncPortSelection(saveSetting) {
        var wanted = settingsManager.loadValue("setup.uart.portName", "")
        var index = -1
        if (wanted) {
            for (var i = 0; i < portCombo.count; i++) {
                if (portCombo.textAt(i) === wanted) {
                    index = i
                    break
                }
            }
        }
        if (index < 0 && uartDriver.portName)
            index = portCombo.find(uartDriver.portName)
        if (index < 0 && portCombo.count > 0)
            index = 0

        if (index >= 0) {
            portCombo.currentIndex = index
            var selected = portCombo.textAt(index)
            uartDriver.portName = selected
            if (saveSetting)
                settingsManager.saveValue("setup.uart.portName", selected)
        } else {
            portCombo.currentIndex = -1
            uartDriver.portName = ""
        }
    }

    Component.onCompleted: {
        syncPortSelection(false)
        var savedBaud = settingsManager.loadValue("setup.uart.baudRate", 115200)
        baudCombo.editText = savedBaud.toString()
        _restoring = false
    }

    Connections {
        target: uartDriver
        function onPortsChanged() {
            if (root._restoring)
                return
            if (portCombo.find(uartDriver.portName) < 0)
                root.syncPortSelection(true)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        FieldLabel { text: "端口" }

        ComboBox {
            id: portCombo
            model: uartDriver.availablePorts
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 1
            implicitHeight: root.controlHeight
            font.pixelSize: 12
            focusPolicy: Qt.TabFocus
            background: FieldBackground { focused: portCombo.activeFocus; hovered: portCombo.hovered }
            contentItem: Text {
                text: portCombo.currentText || "无可用端口"
                color: portCombo.currentText ? root.textPrimary : root.textMuted
                font: portCombo.font
                leftPadding: 9
                rightPadding: 26
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
            onCurrentTextChanged: {
                if (root._restoring)
                    return
                if (currentText) {
                    uartDriver.portName = currentText
                    settingsManager.saveValue("setup.uart.portName", currentText)
                }
            }
        }

        IconButton {
            text: "↻"
            tooltip: "刷新端口"
            onClicked: linkManager.refreshPorts()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        FieldLabel { text: "波特率" }

        ComboBox {
            id: baudCombo
            model: root.baudRates
            editable: true
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 1
            implicitHeight: root.controlHeight
            font.pixelSize: 12
            focusPolicy: Qt.TabFocus
            background: FieldBackground { focused: baudCombo.activeFocus; hovered: baudCombo.hovered }
            contentItem: TextInput {
                text: baudCombo.editText
                color: root.textPrimary
                font: baudCombo.font
                verticalAlignment: Text.AlignVCenter
                leftPadding: 9
                rightPadding: 26
                selectByMouse: true
                selectedTextColor: themeManager.isDark ? "#08121B" : "#FFFFFF"
                selectionColor: root.outlineActive
                validator: IntValidator { bottom: 300; top: 9216000 }
                onTextEdited: {
                    var v = parseInt(text)
                    if (!isNaN(v)) {
                        uartDriver.baudRate = v
                        settingsManager.saveValue("setup.uart.baudRate", v)
                    }
                }
            }
            onCurrentTextChanged: {
                if (root._restoring)
                    return
                if (currentIndex >= 0) {
                    var baud = parseInt(currentText)
                    uartDriver.baudRate = baud
                    settingsManager.saveValue("setup.uart.baudRate", baud)
                }
            }
        }

        Item { Layout.preferredWidth: root.actionWidth; Layout.preferredHeight: root.controlHeight }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 6

        FieldLabel { text: "参数" }

        Rectangle {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            implicitHeight: root.controlHeight
            radius: 7
            color: root.surface
            border.color: root.outline

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 7
                anchors.rightMargin: 7
                spacing: 5

                MiniCombo {
                    id: dataBitsCombo
                    model: ["8", "7"]
                    currentIndex: uartDriver.dataBits === 7 ? 1 : 0
                    Layout.preferredWidth: 54
                    onCurrentTextChanged: uartDriver.dataBits = parseInt(currentText)
                }

                SubLabel { text: "校验" }

                MiniCombo {
                    id: parityCombo
                    model: ["N", "E", "O"]
                    Layout.preferredWidth: 54
                    Component.onCompleted: {
                        var idx = find(uartDriver.parity)
                        if (idx >= 0)
                            currentIndex = idx
                    }
                    onCurrentTextChanged: uartDriver.parity = currentText
                }

                SubLabel { text: "停止" }

                MiniCombo {
                    id: stopBitsCombo
                    model: ["1", "2"]
                    currentIndex: uartDriver.stopBits === 2 ? 1 : 0
                    Layout.preferredWidth: 54
                    onCurrentTextChanged: uartDriver.stopBits = parseInt(currentText)
                }

                Item { Layout.fillWidth: true }
            }
        }

        Item { Layout.preferredWidth: root.actionWidth; Layout.preferredHeight: root.controlHeight }
    }

    StatusStrip {
        connected: linkManager.connected && linkManager.linkType === "UART"
        text: connected ? "已连接  " + uartDriver.portName + " @ " + uartDriver.baudRate
                        : "选择端口和参数后点击「打开连接」"
    }

    component FieldLabel: Text {
        Layout.preferredWidth: root.labelWidth
        Layout.alignment: Qt.AlignVCenter
        color: root.textSecondary
        font.pixelSize: 12
        font.weight: Font.Medium
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component SubLabel: Text {
        color: root.textMuted
        font.pixelSize: 10
        Layout.alignment: Qt.AlignVCenter
        verticalAlignment: Text.AlignVCenter
    }

    component FieldBackground: Rectangle {
        property bool focused: false
        property bool hovered: false
        radius: 7
        color: hovered ? root.surfaceHover : root.fieldBg
        border.width: 1
        border.color: focused ? root.outlineActive : root.outline
    }

    component IconButton: Button {
        id: iconButton
        property string tooltip: ""
        Layout.preferredWidth: root.actionWidth
        Layout.preferredHeight: root.controlHeight
        focusPolicy: Qt.TabFocus
        contentItem: Text {
            text: iconButton.text
            color: iconButton.down ? "#FFFFFF" : (iconButton.hovered ? root.textPrimary : root.textSecondary)
            font.pixelSize: 15
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 7
            color: iconButton.down ? (themeManager.isDark ? "#236EA9" : "#1D4ED8") : (iconButton.hovered ? root.surfaceHover : root.surface)
            border.color: iconButton.activeFocus ? root.outlineActive : root.outline
        }
        ToolTip.visible: hovered && tooltip.length > 0
        ToolTip.text: tooltip
    }

    component MiniCombo: ComboBox {
        id: miniCombo
        Layout.preferredHeight: 22
        implicitHeight: 22
        font.pixelSize: 11
        focusPolicy: Qt.TabFocus
        background: Rectangle {
            radius: 5
            color: miniCombo.hovered ? (themeManager.isDark ? "#26303A" : "#E5E7EB") : root.fieldBg
            border.color: miniCombo.activeFocus ? root.outlineActive : root.outline
        }
        contentItem: Text {
            text: miniCombo.currentText
            color: root.textPrimary
            font: miniCombo.font
            leftPadding: 7
            rightPadding: 14
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component StatusStrip: Rectangle {
        property bool connected: false
        property alias text: statusText.text
        Layout.fillWidth: true
        implicitHeight: 28
        radius: 7
        color: connected ? (themeManager.isDark ? "#102E29" : "#ECFDF5") : root.surface
        border.color: connected ? root.success : root.outline

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 7

            Rectangle {
                width: 7
                height: 7
                radius: 4
                color: parent.parent.connected ? root.success : root.textMuted
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                id: statusText
                Layout.fillWidth: true
                color: parent.parent.connected ? (themeManager.isDark ? "#9FF2DD" : "#059669") : root.textMuted
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
}
