import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

/**
 * BLE 配置面板
 *
 * 流程: 扫描设备 → 选择设备 → 打开连接 → 选择服务/特征 → 收发。
 */
Item {
    id: root
    implicitHeight: layout.implicitHeight
    implicitWidth: layout.implicitWidth

    property var ble: bleDriver
    property int controlHeight: 30
    property int labelWidth: 52
    property int actionWidth: 32

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
    readonly property color warning: themeManager.isDark ? "#FBBF24" : "#D97706"
    readonly property color danger: themeManager.isDark ? "#F87171" : "#DC2626"
    property string deviceFilterText: settingsManager.loadValue("ble.deviceFilter", "")
    property var filteredDeviceIndexes: []

    ListModel { id: filteredDeviceModel }

    Component.onCompleted: refreshFilteredDevices()

    onDeviceFilterTextChanged: {
        settingsManager.saveValue("ble.deviceFilter", deviceFilterText)
        refreshFilteredDevices()
    }

    Connections {
        target: root.ble
        function onDevicesChanged() { root.refreshFilteredDevices() }
        function onDeviceIndexChanged() { root.syncDeviceCurrentIndex() }
    }

    function normalizedDeviceText(text) {
        return String(text || "").toLowerCase().replace(/[\s:_-]/g, "")
    }

    function deviceNameAt(index) {
        var names = root.ble ? root.ble.deviceNames : []
        return index >= 0 && index < names.length ? names[index] : ""
    }

    function deviceAddressAt(index) {
        var addresses = root.ble ? root.ble.deviceAddresses : []
        return index >= 0 && index < addresses.length ? addresses[index] : ""
    }

    function deviceLabelAt(index) {
        var name = deviceNameAt(index)
        var address = deviceAddressAt(index)
        if (name.length <= 0)
            name = "BLE"
        return address.length > 0 ? (name + "  " + address) : name
    }

    function deviceMatchesFilter(index) {
        var filter = normalizedDeviceText(deviceFilterText)
        if (filter.length <= 0)
            return true
        return normalizedDeviceText(deviceNameAt(index)).indexOf(filter) >= 0
            || normalizedDeviceText(deviceAddressAt(index)).indexOf(filter) >= 0
    }

    function filteredIndexForDevice(deviceIndex) {
        for (var i = 0; i < filteredDeviceIndexes.length; i++) {
            if (filteredDeviceIndexes[i] === deviceIndex)
                return i
        }
        return -1
    }

    function syncDeviceCurrentIndex() {
        if (deviceCombo)
            deviceCombo.currentIndex = filteredIndexForDevice(root.ble ? root.ble.deviceIndex : -1)
    }

    function refreshFilteredDevices() {
        filteredDeviceModel.clear()
        var indexes = []
        var count = root.ble ? root.ble.deviceCount : 0
        for (var i = 0; i < count; i++) {
            if (!deviceMatchesFilter(i))
                continue
            indexes.push(i)
            filteredDeviceModel.append({ label: deviceLabelAt(i) })
        }
        filteredDeviceIndexes = indexes
        syncDeviceCurrentIndex()
    }

    ColumnLayout {
        id: layout
        anchors.fill: parent
        spacing: 7

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            FieldLabel { text: "过滤" }

            TextField {
                id: deviceFilterField
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                text: root.deviceFilterText
                placeholderText: "名称或MAC"
                placeholderTextColor: root.textMuted
                color: root.textPrimary
                selectByMouse: true
                font.pixelSize: 12
                enabled: !linkManager.connected && !linkManager.connecting
                background: FieldBackground { focused: deviceFilterField.activeFocus; hovered: deviceFilterField.hovered }
                onTextEdited: root.deviceFilterText = text
            }

            Item { Layout.preferredWidth: root.actionWidth; Layout.preferredHeight: root.controlHeight }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            FieldLabel {
                text: "设备"
                opacity: (linkManager.connected || linkManager.connecting) ? 0.55 : 1
            }

            ComboBox {
                id: deviceCombo
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 12
                focusPolicy: Qt.TabFocus
                model: filteredDeviceModel
                textRole: "label"
                enabled: !linkManager.connected && !linkManager.connecting && filteredDeviceModel.count > 0
                opacity: enabled ? 1 : 0.58
                background: FieldBackground { focused: deviceCombo.activeFocus; hovered: deviceCombo.hovered }
                contentItem: Text {
                    text: deviceCombo.currentText || "没有发现设备"
                    color: deviceCombo.currentText ? root.textPrimary : root.textMuted
                    font: deviceCombo.font
                    leftPadding: 9
                    rightPadding: 26
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onActivated: (index) => {
                    if (root.ble && index >= 0 && index < root.filteredDeviceIndexes.length)
                        root.ble.selectDevice(root.filteredDeviceIndexes[index])
                }
            }

            IconButton {
                text: "↻"
                tooltip: root.ble && root.ble.scanning ? "停止扫描" : "扫描设备"
                enabled: !linkManager.connected && !linkManager.connecting
                scanning: root.ble ? root.ble.scanning : false
                onClicked: {
                    if (root.ble) {
                        if (root.ble.scanning)
                            root.ble.stopScan()
                        else
                            root.ble.startScan()
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.ble && root.ble.deviceConnected && root.ble.serviceNames.length > 1

            FieldLabel { text: "服务" }

            ComboBox {
                id: serviceCombo
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 12
                focusPolicy: Qt.TabFocus
                model: root.ble ? root.ble.serviceNames : []
                background: FieldBackground { focused: serviceCombo.activeFocus; hovered: serviceCombo.hovered }
                contentItem: Text {
                    text: serviceCombo.currentText || "选择服务..."
                    color: serviceCombo.currentText ? root.textPrimary : root.textMuted
                    font: serviceCombo.font
                    leftPadding: 9
                    rightPadding: 26
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onCurrentIndexChanged: {
                    if (root.ble)
                        root.ble.selectService(currentIndex)
                }
            }

            Item { Layout.preferredWidth: root.actionWidth; Layout.preferredHeight: root.controlHeight }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.ble && root.ble.deviceConnected && root.ble.characteristicNames.length > 1

            FieldLabel { text: "TX UUID" }

            ComboBox {
                id: txUuidCombo
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 11
                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                focusPolicy: Qt.TabFocus
                model: root.ble ? root.ble.characteristicNames : []
                currentIndex: root.ble ? root.ble.txCharacteristicIndex : 0
                background: FieldBackground { focused: txUuidCombo.activeFocus; hovered: txUuidCombo.hovered }
                contentItem: Text {
                    text: txUuidCombo.currentText || "选择 TX UUID..."
                    color: txUuidCombo.currentText ? root.textPrimary : root.textMuted
                    font: txUuidCombo.font
                    leftPadding: 9
                    rightPadding: 26
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onCurrentIndexChanged: {
                    if (root.ble && currentIndex !== root.ble.txCharacteristicIndex)
                        root.ble.txCharacteristicIndex = currentIndex
                }
            }

            Item { Layout.preferredWidth: root.actionWidth; Layout.preferredHeight: root.controlHeight }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: root.ble && root.ble.deviceConnected && root.ble.characteristicNames.length > 1

            FieldLabel { text: "RX UUID" }

            ComboBox {
                id: rxUuidCombo
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
                implicitHeight: root.controlHeight
                font.pixelSize: 11
                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                focusPolicy: Qt.TabFocus
                model: root.ble ? root.ble.characteristicNames : []
                currentIndex: root.ble ? root.ble.rxCharacteristicIndex : 0
                background: FieldBackground { focused: rxUuidCombo.activeFocus; hovered: rxUuidCombo.hovered }
                contentItem: Text {
                    text: rxUuidCombo.currentText || "选择 RX UUID..."
                    color: rxUuidCombo.currentText ? root.textPrimary : root.textMuted
                    font: rxUuidCombo.font
                    leftPadding: 9
                    rightPadding: 26
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }
                onCurrentIndexChanged: {
                    if (root.ble && currentIndex !== root.ble.rxCharacteristicIndex)
                        root.ble.rxCharacteristicIndex = currentIndex
                }
            }

            Item { Layout.preferredWidth: root.actionWidth; Layout.preferredHeight: root.controlHeight }
        }

        StatusStrip {
            visible: root.ble && !root.ble.adapterAvailable
            stateColor: root.danger
            text: "未检测到蓝牙适配器，请检查系统蓝牙或外接适配器"
        }

        StatusStrip {
            visible: root.ble && root.ble.adapterAvailable && root.ble.deviceCount < 1
            stateColor: root.ble && root.ble.scanning ? root.warning : root.textMuted
            text: root.ble && root.ble.scanning ? "正在扫描 BLE 设备..." : "点击刷新按钮搜索 BLE 设备"
        }

        StatusStrip {
            visible: root.ble && !root.ble.deviceConnected && root.ble.deviceCount > 0 && !root.ble.scanning
            stateColor: root.textMuted
            text: "选择设备后点击「打开连接」"
        }

        StatusStrip {
            visible: root.ble && root.ble.deviceConnected
            stateColor: root.success
            text: "BLE 已连接"
        }

        Item { Layout.fillHeight: true }
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
        property bool scanning: false
        Layout.preferredWidth: root.actionWidth
        Layout.preferredHeight: root.controlHeight
        focusPolicy: Qt.TabFocus
        opacity: enabled ? 1 : 0.55
        contentItem: Text {
            text: iconButton.text
            color: iconButton.scanning ? root.success
                                      : (iconButton.down ? "#FFFFFF"
                                                         : (iconButton.hovered ? root.textPrimary : root.textSecondary))
            font.pixelSize: 15
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            RotationAnimation on rotation {
                running: iconButton.scanning
                from: 0
                to: 360
                duration: 1500
                loops: Animation.Infinite
            }
        }
        background: Rectangle {
            radius: 7
            color: iconButton.scanning ? (themeManager.isDark ? "#102E29" : "#ECFDF5")
                  : (iconButton.down ? (themeManager.isDark ? "#236EA9" : "#1D4ED8") : (iconButton.hovered ? root.surfaceHover : root.surface))
            border.color: iconButton.activeFocus ? root.outlineActive
                         : (iconButton.scanning ? root.success : root.outline)
        }
        ToolTip.visible: hovered && tooltip.length > 0
        ToolTip.text: tooltip
    }

    component StatusStrip: Rectangle {
        property color stateColor: root.textMuted
        property alias text: statusText.text
        Layout.fillWidth: true
        implicitHeight: 28
        radius: 7
        color: root.surface
        border.color: root.outline

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 7

            Rectangle {
                width: 7
                height: 7
                radius: 4
                color: parent.parent.stateColor
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                id: statusText
                Layout.fillWidth: true
                color: root.textSecondary
                font.pixelSize: 10
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }
}
