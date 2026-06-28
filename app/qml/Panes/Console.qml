import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import ProtoDebug

Item {
    id: root
    implicitWidth: 600
    implicitHeight: 400
    property bool _restoring: true

    readonly property var c: themeManager.colors
    readonly property color pageBg: themeManager.isDark ? "#0C1017" : "#F8F9FB"
    readonly property color surface: themeManager.isDark ? "#131920" : "#FFFFFF"
    readonly property color surfaceRaised: themeManager.isDark ? "#1A222C" : "#F3F4F6"
    readonly property color surfaceSoft: themeManager.isDark ? "#0F141B" : "#F9FAFB"
    readonly property color outline: themeManager.isDark ? "#253040" : "#D1D5DB"
    readonly property color outlineStrong: themeManager.isDark ? "#384858" : "#9CA3AF"
    readonly property color textPrimary: themeManager.isDark ? "#E8EDF5" : "#111827"
    readonly property color textSecondary: themeManager.isDark ? "#9CAAB8" : "#6B7280"
    readonly property color textMuted: themeManager.isDark ? "#607080" : "#9CA3AF"
    readonly property color accent: themeManager.isDark ? "#3B8AFF" : "#2563EB"
    readonly property color success: themeManager.isDark ? "#34D399" : "#059669"
    readonly property color danger: themeManager.isDark ? "#F87171" : "#DC2626"

    Palette {
        id: darkPalette
        text: textPrimary
        base: surfaceSoft
        button: surfaceRaised
        buttonText: textPrimary
        highlight: accent
        highlightedText: "#FFFFFF"
        window: surface
        windowText: textPrimary
        placeholderText: textMuted
    }

    Component.onCompleted: {
        var dm = typeof optHexMode !== "undefined" && optHexMode ? 1
                 : settingsManager.loadValue("terminal.displayMode", 0)
        terminal.displayMode = dm
        displayModeCombo.currentIndex = dm
        terminal.lineEnding = settingsManager.loadValue("terminal.lineEnding", 1)
        lineEndingCombo.currentIndex = terminal.lineEnding
        terminal.showTimestamp = settingsManager.loadValue("terminal.showTimestamp", false)
        timestampCheck.checked = terminal.showTimestamp
        var echoVal = settingsManager.loadValue("terminal.echo", true)
        terminal.echo = echoVal
        echoCheck.checked = echoVal
        vt100Check.checked = settingsManager.loadValue("terminal.vt100", false)
        tw.vt100emulation = vt100Check.checked
        ansiCheck.checked = settingsManager.loadValue("terminal.ansi", false)
        tw.ansiColors = ansiCheck.checked
        terminal.dataMode = settingsManager.loadValue("terminal.dataMode", 0)
        hexCheckbox.checked = terminal.dataMode === 1
        autoscrollCheck.checked = settingsManager.loadValue("terminal.autoscroll", true)
        tw.autoscroll = autoscrollCheck.checked
        _restoring = false
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 6

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 7
            color: themeManager.isDark ? "#0D131A" : "#F3F4F6"
            border.color: outline
            clip: true

            TerminalWidget {
                id: tw
                anchors.fill: parent
                anchors.margins: 1

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.IBeamCursor
                    acceptedButtons: Qt.RightButton
                    onClicked: function(mouse) {
                        if (mouse.button === Qt.RightButton)
                            ctxMenu.popup()
                    }
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 40, hintText.implicitWidth + 34)
                height: hintText.implicitHeight + 18
                radius: 7
                color: themeManager.isDark ? "#18202B" : "#FFFFFF"
                border.color: outlineStrong
                visible: linkManager && !linkManager.connected
                Text {
                    id: hintText
                    anchors.centerIn: parent
                    text: "未连接，请先在右侧配置链路并打开连接"
                    color: textSecondary
                    font.pixelSize: 12
                    elide: Text.ElideRight
                    width: parent.width - 20
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 78
            radius: 7
            color: surfaceRaised
            border.color: outline

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 28
                    spacing: 6

                    TextField {
                        id: sendField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 140
                        Layout.preferredHeight: 28
                        font: commonFonts.customMonoFont()
                        color: textPrimary
                        placeholderText: "输入发送内容"
                        placeholderTextColor: textMuted
                        enabled: linkManager && linkManager.connected
                        selectByMouse: true

                        background: Rectangle {
                            radius: 6
                            color: surfaceSoft
                            border.color: sendField.activeFocus ? accent : outline
                        }

                        cursorDelegate: Rectangle {
                            width: 1
                            height: sendField.font.pixelSize
                            color: accent
                        }

                        Keys.onReturnPressed: terminal.send(sendField.text)
                        Keys.onUpPressed: {
                            terminal.historyUp()
                            sendField.text = terminal.currentHistoryString
                        }
                        Keys.onDownPressed: {
                            terminal.historyDown()
                            sendField.text = terminal.currentHistoryString
                        }

                        onTextChanged: {
                            if (hexCheckbox.checked) {
                                var f = terminal.formatUserHex(sendField.text)
                                if (sendField.text !== f) {
                                    sendField.text = f
                                    sendField.cursorPosition = Math.min(sendField.cursorPosition + 1, sendField.length)
                                }
                                sendField.color = terminal.validateUserHex(f) ? textPrimary : danger
                            } else {
                                sendField.color = textPrimary
                            }
                        }

                        Connections {
                            target: linkManager
                            function onConnectionChanged() {
                                if (!linkManager.connected)
                                    sendField.clear()
                            }
                        }
                    }

                    CompactCheckBox {
                        id: hexCheckbox
                        text: "HEX"
                        enabled: linkManager && linkManager.connected
                        checked: terminal.dataMode === 1
                        onToggled: {
                            if (root._restoring)
                                return
                            terminal.dataMode = checked ? 1 : 0
                            settingsManager.saveValue("terminal.dataMode", checked ? 1 : 0)
                        }
                    }

                    ComboBox {
                        id: lineEndingCombo
                        Layout.preferredWidth: 124
                        Layout.preferredHeight: 28
                        enabled: linkManager && linkManager.connected
                        model: terminal.lineEndings
                        currentIndex: terminal.lineEnding
                        opacity: enabled ? 1 : 0.56
                        palette: darkPalette

                        onCurrentIndexChanged: {
                            if (root._restoring)
                                return
                            if (terminal.lineEnding !== currentIndex)
                                terminal.lineEnding = currentIndex
                            settingsManager.saveValue("terminal.lineEnding", currentIndex)
                        }

                        background: Rectangle {
                            radius: 6
                            color: surfaceSoft
                            border.color: lineEndingCombo.activeFocus ? accent : outline
                        }
                        contentItem: Text {
                            text: lineEndingCombo.currentText
                            color: lineEndingCombo.enabled ? textPrimary : textMuted
                            font.pixelSize: 11
                            leftPadding: 8
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }

                    TerminalButton {
                        id: sendBt
                        text: "发送"
                        tone: success
                        primary: true
                        Layout.preferredWidth: 54
                        enabled: linkManager && linkManager.connected
                                 && (sendField.length > 0 || terminal.lineEnding !== 0)
                        onClicked: terminal.send(sendField.text)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 26
                    spacing: 6

                    CompactCheckBox {
                        id: timestampCheck
                        text: "时间戳"
                        checked: typeof optTimestamp !== "undefined" && optTimestamp ? true : terminal.showTimestamp
                        onToggled: {
                            if (root._restoring)
                                return
                            terminal.showTimestamp = checked
                            settingsManager.saveValue("terminal.showTimestamp", checked)
                        }
                    }

                    CompactCheckBox {
                        id: echoCheck
                        text: "回显"
                        checked: terminal.echo
                        onToggled: {
                            if (root._restoring)
                                return
                            terminal.echo = checked
                            settingsManager.saveValue("terminal.echo", checked)
                        }
                    }

                    CompactCheckBox {
                        id: autoscrollCheck
                        text: "滚动"
                        checked: settingsManager.loadValue("terminal.autoscroll", true)
                        onToggled: {
                            if (root._restoring)
                                return
                            tw.autoscroll = checked
                            settingsManager.saveValue("terminal.autoscroll", checked)
                        }
                    }

                    CompactCheckBox {
                        id: vt100Check
                        text: "VT100"
                        checked: settingsManager.loadValue("terminal.vt100", false)
                        onToggled: {
                            if (root._restoring)
                                return
                            tw.vt100emulation = checked
                            settingsManager.saveValue("terminal.vt100", checked)
                        }
                    }

                    CompactCheckBox {
                        id: ansiCheck
                        text: "ANSI"
                        checked: settingsManager.loadValue("terminal.ansi", false)
                        onToggled: {
                            if (root._restoring)
                                return
                            tw.ansiColors = checked
                            settingsManager.saveValue("terminal.ansi", checked)
                        }
                    }

                    Item { Layout.fillWidth: true }

                    ComboBox {
                        id: displayModeCombo
                        Layout.preferredWidth: 122
                        Layout.preferredHeight: 26
                        palette: darkPalette
                        model: terminal.displayModes
                        currentIndex: typeof optHexMode !== "undefined" && optHexMode ? 1 : terminal.displayMode
                        displayText: "显示: " + currentText
                        onCurrentIndexChanged: terminal.displayMode = currentIndex

                        background: Rectangle {
                            radius: 6
                            color: surfaceSoft
                            border.color: displayModeCombo.activeFocus ? accent : outline
                        }
                        contentItem: Text {
                            text: displayModeCombo.displayText
                            color: textPrimary
                            font.pixelSize: 11
                            leftPadding: 8
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                        }
                    }

                    TerminalButton {
                        id: clearBtn
                        text: "清空"
                        tone: danger
                        Layout.preferredWidth: 50
                        Layout.preferredHeight: 26
                        onClicked: {
                            terminal.clear()
                            tw.clear()
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: terminal
        function onDisplayString(text) {
            if (text.length === 0)
                return
            tw.append(text)
        }
    }

    Connections {
        target: tw
        function onDataSendRequested(data) { linkManager.sendData(data) }
    }

    Menu {
        id: ctxMenu
        palette: darkPalette
        MenuItem {
            text: "复制"
            onTriggered: tw.copy()
            enabled: tw.copyAvailable
        }
        MenuItem {
            text: "全选"
            onTriggered: tw.selectAll()
        }
        MenuItem {
            text: "清空"
            onTriggered: {
                terminal.clear()
                tw.clear()
            }
        }
    }

    component TerminalButton: Button {
        id: control
        property color tone: root.accent
        property bool primary: false
        Layout.preferredWidth: Math.max(50, controlText.implicitWidth + 22)
        Layout.preferredHeight: 28
        padding: 0
        opacity: enabled ? 1 : 0.56
        focusPolicy: Qt.TabFocus
        background: Rectangle {
            radius: 6
            color: !control.enabled ? (themeManager.isDark ? "#1B222B" : "#F3F4F6")
                   : (control.primary && control.enabled ? accent
                      : (control.hovered ? (themeManager.isDark ? "#202B37" : "#F3F4F6") : (themeManager.isDark ? "#161D26" : "#F9FAFB")))
            border.color: !control.enabled ? (themeManager.isDark ? "#2B3441" : "#E5E7EB")
                                           : (control.primary ? Qt.lighter(control.tone, 1.15)
                                                              : (control.activeFocus ? root.accent : root.outline))
        }
        contentItem: Text {
            id: controlText
            text: control.text
            color: control.primary && control.enabled ? "#FFFFFF" : root.textSecondary
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component CompactCheckBox: CheckBox {
        id: check
        Layout.preferredWidth: Math.max(50, checkText.implicitWidth + 28)
        Layout.preferredHeight: 24
        palette: darkPalette
        opacity: enabled ? 1 : 0.56
        focusPolicy: Qt.TabFocus
        indicator: Rectangle {
            implicitWidth: 14
            implicitHeight: 14
            x: 0
            y: parent.height / 2 - height / 2
            radius: 4
            color: check.checked ? accent : surfaceSoft
            border.color: check.activeFocus ? (themeManager.isDark ? "#FFFFFF" : "#111827") : (check.checked ? accent : outlineStrong)
            Text {
                anchors.centerIn: parent
                text: check.checked ? "✓" : ""
                color: "#FFFFFF"
                font.pixelSize: 10
                font.weight: Font.DemiBold
            }
        }
        contentItem: Text {
            id: checkText
            text: check.text
            color: check.enabled ? textSecondary : textMuted
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            leftPadding: check.indicator.width + 6
            elide: Text.ElideRight
        }
    }
}
