import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    implicitHeight: grid.implicitHeight
    implicitWidth: grid.implicitWidth
    property bool _restoring: true
    property int controlHeight: 26
    property int minComboWidth: 92
    property var arbitrationBitrates: [125000, 250000, 500000, 800000, 1000000]
    property var dataBitrates: [1000000, 2000000, 4000000, 5000000, 8000000]

    function parseNumber(text, fallbackValue) {
        var s = (text || "").trim()
        if (s.length === 0) return fallbackValue
        var v = s.indexOf("0x") === 0 || s.indexOf("0X") === 0
                ? parseInt(s.substring(2), 16)
                : parseInt(s, 10)
        return isNaN(v) ? fallbackValue : v
    }

    function hexText(value) {
        return "0x" + Number(value).toString(16).toUpperCase()
    }

    function normalizeHexPrefix(value) {
        return String(value || "").replace(/0X/g, "0x")
    }

    function bitrateLabel(value) {
        return value >= 1000000 ? (value / 1000000) + "M" : (value / 1000) + "K"
    }

    function bitrateIndex(values, value, fallbackIndex) {
        for (var i = 0; i < values.length; i++) {
            if (values[i] === value)
                return i
        }
        return fallbackIndex
    }

    function defaultPayloadSize(format) {
        return format === 0 ? 8 : 64
    }

    function applyPayloadSize(format) {
        var payloadSize = defaultPayloadSize(format)
        if (canDriver)
            canDriver.framePayloadSize = payloadSize
        settingsManager.saveValue("setup.can.framePayloadSize", payloadSize)
    }

    Component.onCompleted: {
        deviceIndexField.text = settingsManager.loadValue("setup.can.deviceIndex", 0).toString()
        channelField.text = settingsManager.loadValue("setup.can.channel", 0).toString()
        txIdField.text = normalizeHexPrefix(settingsManager.loadValue("setup.can.txId", "0x100"))
        rxIdField.text = normalizeHexPrefix(settingsManager.loadValue("setup.can.rxId", "0x200"))
        formatCombo.currentIndex = Math.max(0, Math.min(formatCombo.count - 1, settingsManager.loadValue("setup.can.frameFormat", 1)))
        arbBitrateCombo.currentIndex = bitrateIndex(arbitrationBitrates, settingsManager.loadValue("setup.can.arbitrationBitrate", 1000000), 4)
        dataBitrateCombo.currentIndex = bitrateIndex(dataBitrates, settingsManager.loadValue("setup.can.dataBitrate", 1000000), 0)
        extendedCheck.checked = settingsManager.loadValue("setup.can.extended", false)
        brsCheck.checked = settingsManager.loadValue("setup.can.brs", true)

        if (canDriver) {
            canDriver.libraryPath = ""
            canDriver.deviceTypeIndex = settingsManager.loadValue("setup.can.deviceTypeIndex", canDriver.deviceTypeIndex)
            canDriver.deviceIndex = parseNumber(deviceIndexField.text, 0)
            canDriver.channel = parseNumber(channelField.text, 0)
            canDriver.txId = parseNumber(txIdField.text, 0x100)
            canDriver.rxId = parseNumber(rxIdField.text, 0x200)
            canDriver.frameFormat = formatCombo.currentIndex
            canDriver.arbitrationBitrate = arbitrationBitrates[arbBitrateCombo.currentIndex]
            canDriver.dataBitrate = dataBitrates[dataBitrateCombo.currentIndex]
            canDriver.framePayloadSize = defaultPayloadSize(formatCombo.currentIndex)
            canDriver.extended = extendedCheck.checked
            canDriver.brs = brsCheck.checked
            canDriver.transmitType = 0
            canDriver.abitTiming = settingsManager.loadValue("setup.can.abitTiming", 101166)
            canDriver.dbitTiming = settingsManager.loadValue("setup.can.dbitTiming", 101166)
            canDriver.pollIntervalMs = 5
            canDriver.interFrameDelayMs = 0
        }
        settingsManager.saveValue("setup.can.libraryPath", "")
        settingsManager.saveValue("setup.can.framePayloadSize", defaultPayloadSize(formatCombo.currentIndex))
        _restoring = false
    }

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        columns: 2
        rowSpacing: 3
        columnSpacing: 7

        Label { text: "索引/通道"; color: themeManager.isDark ? "#AEB7C2" : "#6B7280"; font.pixelSize: 11 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 5
            TextField {
                id: deviceIndexField
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 10
                color: themeManager.isDark ? "#D4D4D4" : "#111827"
                validator: IntValidator { bottom: 0; top: 32 }
                background: Rectangle { color: themeManager.isDark ? "#22272E" : "#F9FAFB"; border.color: deviceIndexField.activeFocus ? (themeManager.isDark ? "#7AB7FF" : "#2563EB") : (themeManager.isDark ? "#3A424C" : "#D1D5DB"); radius: 6 }
                onTextEdited: {
                    if (root._restoring || !canDriver) return
                    var v = root.parseNumber(text, 0)
                    canDriver.deviceIndex = v
                    settingsManager.saveValue("setup.can.deviceIndex", v)
                }
            }
            TextField {
                id: channelField
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 10
                color: themeManager.isDark ? "#D4D4D4" : "#111827"
                validator: IntValidator { bottom: 0; top: 16 }
                background: Rectangle { color: themeManager.isDark ? "#22272E" : "#F9FAFB"; border.color: channelField.activeFocus ? (themeManager.isDark ? "#7AB7FF" : "#2563EB") : (themeManager.isDark ? "#3A424C" : "#D1D5DB"); radius: 6 }
                onTextEdited: {
                    if (root._restoring || !canDriver) return
                    var v = root.parseNumber(text, 0)
                    canDriver.channel = v
                    settingsManager.saveValue("setup.can.channel", v)
                }
            }
        }

        Label { text: "TX/RX ID"; color: themeManager.isDark ? "#AEB7C2" : "#6B7280"; font.pixelSize: 11 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 5
            TextField {
                id: txIdField
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 10
                font.family: "Menlo"
                color: themeManager.isDark ? "#D4D4D4" : "#111827"
                background: Rectangle { color: themeManager.isDark ? "#22272E" : "#F9FAFB"; border.color: txIdField.activeFocus ? (themeManager.isDark ? "#7AB7FF" : "#2563EB") : (themeManager.isDark ? "#3A424C" : "#D1D5DB"); radius: 6 }
                onTextEdited: {
                    if (root._restoring || !canDriver) return
                    canDriver.txId = root.parseNumber(text, 0x100)
                    settingsManager.saveValue("setup.can.txId", text)
                }
            }
            TextField {
                id: rxIdField
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 10
                font.family: "Menlo"
                color: themeManager.isDark ? "#D4D4D4" : "#111827"
                background: Rectangle { color: themeManager.isDark ? "#22272E" : "#F9FAFB"; border.color: rxIdField.activeFocus ? (themeManager.isDark ? "#7AB7FF" : "#2563EB") : (themeManager.isDark ? "#3A424C" : "#D1D5DB"); radius: 6 }
                onTextEdited: {
                    if (root._restoring || !canDriver) return
                    canDriver.rxId = root.parseNumber(text, 0x200)
                    settingsManager.saveValue("setup.can.rxId", text)
                }
            }
        }

        Label { text: "帧类型"; color: themeManager.isDark ? "#AEB7C2" : "#6B7280"; font.pixelSize: 11 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ComboBox {
                id: formatCombo
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 10
                model: ["CAN", "CAN FD"]
                enabled: !linkManager.connected && !linkManager.connecting
                background: Rectangle { color: themeManager.isDark ? "#22272E" : "#F9FAFB"; border.color: formatCombo.activeFocus ? (themeManager.isDark ? "#7AB7FF" : "#2563EB") : (themeManager.isDark ? "#3A424C" : "#D1D5DB"); radius: 6 }
                contentItem: Text {
                    text: formatCombo.currentText
                    color: themeManager.isDark ? "#D4D4D4" : "#111827"
                    font: formatCombo.font
                    leftPadding: 6
                    rightPadding: 6
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onActivated: function(index) {
                    if (root._restoring || !canDriver) return
                    canDriver.frameFormat = index
                    settingsManager.saveValue("setup.can.frameFormat", index)
                    root.applyPayloadSize(index)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                spacing: 8
                CheckBox {
                    id: brsCheck
                    text: "BRS"
                    implicitHeight: root.controlHeight
                    font.pixelSize: 10
                    checked: true
                    enabled: formatCombo.currentIndex === 1 && !linkManager.connected && !linkManager.connecting
                    onCheckedChanged: {
                        if (root._restoring || !canDriver) return
                        canDriver.brs = checked
                        settingsManager.saveValue("setup.can.brs", checked)
                    }
                }
                CheckBox {
                    id: extendedCheck
                    text: "EXT ID"
                    implicitHeight: root.controlHeight
                    font.pixelSize: 10
                    enabled: !linkManager.connected && !linkManager.connecting
                    onCheckedChanged: {
                        if (root._restoring || !canDriver) return
                        canDriver.extended = checked
                        settingsManager.saveValue("setup.can.extended", checked)
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }

        Label { text: "速率"; color: themeManager.isDark ? "#AEB7C2" : "#6B7280"; font.pixelSize: 11 }
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            ComboBox {
                id: arbBitrateCombo
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 10
                model: root.arbitrationBitrates.map(function(v) { return root.bitrateLabel(v) })
                enabled: !linkManager.connected && !linkManager.connecting
                background: Rectangle { color: themeManager.isDark ? "#22272E" : "#F9FAFB"; border.color: arbBitrateCombo.activeFocus ? (themeManager.isDark ? "#7AB7FF" : "#2563EB") : (themeManager.isDark ? "#3A424C" : "#D1D5DB"); radius: 6 }
                contentItem: Text {
                    text: arbBitrateCombo.currentText
                    color: themeManager.isDark ? "#D4D4D4" : "#111827"
                    font: arbBitrateCombo.font
                    leftPadding: 6
                    rightPadding: 6
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onActivated: function(index) {
                    if (root._restoring || !canDriver) return
                    var value = root.arbitrationBitrates[index]
                    canDriver.arbitrationBitrate = value
                    settingsManager.saveValue("setup.can.arbitrationBitrate", value)
                }
            }
            ComboBox {
                id: dataBitrateCombo
                visible: formatCombo.currentIndex === 1
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 10
                model: root.dataBitrates.map(function(v) { return root.bitrateLabel(v) })
                enabled: formatCombo.currentIndex === 1 && !linkManager.connected && !linkManager.connecting
                background: Rectangle { color: themeManager.isDark ? "#22272E" : "#F9FAFB"; border.color: dataBitrateCombo.activeFocus ? (themeManager.isDark ? "#7AB7FF" : "#2563EB") : (themeManager.isDark ? "#3A424C" : "#D1D5DB"); radius: 6 }
                contentItem: Text {
                    text: dataBitrateCombo.currentText
                    color: themeManager.isDark ? "#D4D4D4" : "#111827"
                    font: dataBitrateCombo.font
                    leftPadding: 6
                    rightPadding: 6
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onActivated: function(index) {
                    if (root._restoring || !canDriver) return
                    var value = root.dataBitrates[index]
                    canDriver.dataBitrate = value
                    settingsManager.saveValue("setup.can.dataBitrate", value)
                }
            }
            Item {
                visible: formatCombo.currentIndex !== 1
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
            }
        }

    }
}
