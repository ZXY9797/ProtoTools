import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    color: themeManager.isDark ? "#0C1017" : "#F8F9FB"

    property int monitorTabIndex: 0
    readonly property bool terminalActive: monitorTabIndex === 1
    property bool luaScriptPanelOpen: false
    readonly property bool luaScriptPanelAvailable: monitorTabIndex === 0 || monitorTabIndex === 2
    readonly property bool luaScriptPanelVisible: luaScriptPanelAvailable && luaScriptPanelOpen

    // 主题颜色别名
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
    readonly property color warning: themeManager.isDark ? "#FBBF24" : "#D97706"
    readonly property color danger: themeManager.isDark ? "#F87171" : "#DC2626"
    readonly property int rowNumberColumnWidth: 48
    readonly property int seqColumnWidth: 46
    readonly property int fixedTableWidth: 554
    property string dataPreviewTitle: "Data"
    property string dataPreviewMeta: ""
    property string dataPreviewText: ""

    onTerminalActiveChanged: {
        linkManager.terminalMode = terminalActive
    }

    onLuaScriptPanelAvailableChanged: {
        if (!luaScriptPanelAvailable)
            luaScriptPanelOpen = false
    }

    Connections {
        target: monitorModel
        function onExportFinished(path) {
            root.dataPreviewTitle = "导出完成"
            root.dataPreviewMeta = path
            root.dataPreviewText = path
            dataPreviewPopup.open()
        }
        function onExportFailed(message) {
            root.dataPreviewTitle = "导出失败"
            root.dataPreviewMeta = ""
            root.dataPreviewText = message
            dataPreviewPopup.open()
        }
    }

    Popup {
        id: dataPreviewPopup
        width: Math.min(root.width - 40, 720)
        height: Math.min(root.height - 90, 320)
        x: Math.max(20, (root.width - width) / 2)
        y: Math.max(54, (root.height - height) / 2)
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0
        z: 1000

        background: Rectangle {
            radius: 8
            color: root.surfaceRaised
            border.color: root.outlineStrong
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: root.dataPreviewTitle
                    color: root.textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: root.dataPreviewMeta
                    color: root.textMuted
                    font.pixelSize: 10
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                }

                MonitorButton {
                    text: "关闭"
                    tone: root.textSecondary
                    Layout.preferredWidth: 58
                    onClicked: dataPreviewPopup.close()
                }
            }

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                ScrollBar.vertical.policy: ScrollBar.AsNeeded
                ScrollBar.horizontal.policy: ScrollBar.AsNeeded
                background: Rectangle {
                    radius: 6
                    color: root.surfaceSoft
                    border.color: root.outline
                }

                TextArea {
                    text: root.dataPreviewText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.WrapAnywhere
                    color: root.textPrimary
                    selectedTextColor: themeManager.isDark ? "#08121B" : "#FFFFFF"
                    selectionColor: root.accent
                    font.pixelSize: 12
                    font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                    background: null
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 6
        spacing: 6

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 38
            radius: 7
            color: surfaceRaised
            border.color: outline

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 4

                Repeater {
                    model: ["协议监控", "终端", "协议曲线"]
                    TabButton {
                        text: modelData
                        active: index === monitorTabIndex
                        accentColor: index === 0 ? root.accent : (index === 1 ? root.success : root.warning)
                        onClicked: monitorTabIndex = index
                    }
                }

                Item { Layout.fillWidth: true }

                MonitorButton {
                    text: root.luaScriptPanelOpen ? "隐藏Lua" : "Lua脚本"
                    tone: root.luaScriptPanelOpen ? root.warning : root.accent
                    primary: root.luaScriptPanelOpen
                    visible: root.luaScriptPanelAvailable
                    Layout.preferredWidth: 82
                    onClicked: root.luaScriptPanelOpen = !root.luaScriptPanelOpen
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6
            visible: monitorTabIndex === 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                radius: 7
                color: surfaceRaised
                border.color: outline

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 6

                    MonitorButton {
                        id: pauseBtn
                        text: monitorModel.paused ? "继续" : "暂停"
                        tone: monitorModel.paused ? root.success : root.warning
                        primary: true
                        Layout.preferredWidth: 74
                        onClicked: monitorModel.paused = !monitorModel.paused
                    }

                    MonitorButton {
                        text: "清空"
                        tone: root.danger
                        onClicked: monitorModel.clear()
                    }

                    MonitorButton {
                        text: "导出"
                        tone: root.textSecondary
                        enabled: monitorModel.count > 0
                        onClicked: monitorModel.exportRawHex(linkManager.linkType, root.currentLinkExportInfo())
                    }

                    TextField {
                        id: filterField
                        Layout.fillWidth: true
                        Layout.minimumWidth: 110
                        Layout.preferredHeight: 28
                        placeholderText: "示例: Src:0x10 && Len:24 || Data:*AA*"
                        placeholderTextColor: textMuted
                        color: textPrimary
                        selectionColor: accent
                        selectedTextColor: "#FFFFFF"
                        font.pixelSize: 12
                        onTextChanged: monitorModel.filterText = text
                        background: Rectangle {
                            radius: 6
                            color: surfaceSoft
                            border.color: filterField.activeFocus ? accent : outline
                        }
                    }

                    PillToggle {
                        text: "自动滚动"
                        isChecked: tableList.autoScroll
                        onClicked: tableList.autoScroll = !tableList.autoScroll
                    }

                    PillToggle {
                        text: "命令回显"
                        isChecked: monitorModel.commandEchoEnabled
                        onClicked: monitorModel.commandEchoEnabled = !monitorModel.commandEchoEnabled
                    }
                }
            }

            SplitView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                orientation: Qt.Vertical

                Rectangle {
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: 230
                    radius: 7
                    color: surface
                    border.color: outline
                    clip: true

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28
                            color: themeManager.isDark ? "#0F1620" : "#F3F4F6"
                            border.color: outline

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: 6
                                spacing: 0
                                HeaderCell { text: "#"; width: rowNumberColumnWidth }
                                HeaderCell { text: "时间"; width: 96 }
                                HeaderCell { text: "方向"; width: 32 }
                                HeaderCell { text: "Src"; width: 54 }
                                HeaderCell { text: "Dst"; width: 54 }
                                HeaderCell { text: "Len"; width: 28 }
                                HeaderCell { text: "Type"; width: 40 }
                                HeaderCell { text: "Seq"; width: seqColumnWidth }
                                HeaderCell { text: "CmdSet"; width: 56 }
                                HeaderCell { text: "CmdID"; width: 52 }
                                HeaderCell { text: "Data"; width: dataColumnWidth(tableList.width) }
                                HeaderCell { text: "CRC16"; width: 48 }
                            }
                        }

                        ListView {
                            id: tableList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            model: monitorModel
                            currentIndex: -1
                            boundsBehavior: Flickable.StopAtBounds

                            property bool autoScroll: true

                            onCountChanged: {
                                if (autoScroll && count > 0)
                                    positionViewAtEnd()
                            }
                            onMovementStarted: autoScroll = false
                            onMovementEnded: {
                                autoScroll = (contentY + height >= contentHeight - 50)
                                scrollFadeTimer.restart()
                            }

                            ScrollBar.vertical: ScrollBar {
                                id: vScrollBar
                                width: 10
                                policy: ScrollBar.AlwaysOn
                                opacity: (tableList.moving || scrollFadeTimer.running || vScrollBar.hovered) ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 140 } }
                                contentItem: Rectangle {
                                    implicitWidth: vScrollBar.hovered ? 8 : 5
                                    radius: 4
                                    color: parent.pressed ? accent : (vScrollBar.hovered ? outlineStrong : (themeManager.isDark ? "#384858" : "#CBD5E1"))
                                }
                                background: Rectangle { implicitWidth: 10; color: "transparent" }
                            }

                            Timer { id: scrollFadeTimer; interval: 1200; repeat: false }

                            delegate: Rectangle {
                                width: tableList.width
                                height: 27
                                color: monitorRowColor(model.rowColor, index, rowMouse.containsMouse, ListView.isCurrentItem)
                                property string customTextColor: model.textColor || ""

                                MouseArea {
                                    id: rowMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: tableList.currentIndex = index
                                }

                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: 6
                                    spacing: 0
                                    TableCell { text: monitorRowNumber(index); width: rowNumberColumnWidth; color: monitorTextColor(customTextColor, textMuted) }
                                    TableCell { text: model.timestamp || ""; width: 96; color: monitorTextColor(customTextColor, textSecondary) }
                                    TableCell {
                                        text: model.direction || ""
                                        width: 32
                                        color: monitorTextColor(customTextColor, (model.direction === "→" || model.direction === "TX") ? warning : success)
                                    }
                                    TableCell { text: displayHexText(model.sender || ""); width: 54; color: monitorTextColor(customTextColor, themeManager.isDark ? "#60A5FA" : "#3B82F6") }
                                    TableCell { text: displayHexText(model.receiver || ""); width: 54; color: monitorTextColor(customTextColor, themeManager.isDark ? "#60A5FA" : "#3B82F6") }
                                    TableCell { text: model.len || ""; width: 28; color: monitorTextColor(customTextColor, themeManager.isDark ? "#86EFAC" : "#10B981") }
                                    TableCell { text: model.type || ""; width: 40; color: monitorTextColor(customTextColor, themeManager.isDark ? "#FCD34D" : "#F59E0B") }
                                    TableCell { text: model.seq || ""; width: seqColumnWidth; color: monitorTextColor(customTextColor, themeManager.isDark ? "#93C5FD" : "#60A5FA") }
                                    TableCell { text: displayHexText(model.cmdSet || ""); width: 56; color: monitorTextColor(customTextColor, themeManager.isDark ? "#C4B5FD" : "#8B5CF6") }
                                    TableCell { text: displayHexText(model.cmdId || ""); width: 52; color: monitorTextColor(customTextColor, themeManager.isDark ? "#FCD34D" : "#F59E0B") }
                                    DataCell {
                                        text: displayHexText(model.data || "")
                                        width: dataColumnWidth(tableList.width)
                                        color: monitorTextColor(customTextColor, textPrimary)
                                        onClicked: root.openDataPreview({
                                            timestamp: model.timestamp || "",
                                            sender: model.sender || "",
                                            receiver: model.receiver || "",
                                            seq: model.seq || "",
                                            cmdSet: model.cmdSet || "",
                                            cmdId: model.cmdId || "",
                                            data: model.data || ""
                                        })
                                    }
                                    TableCell { text: displayHexText(model.crc || ""); width: 48; color: monitorTextColor(customTextColor, themeManager.isDark ? "#93C5FD" : "#60A5FA") }
                                }
                            }
                        }
                    }
                }

            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: monitorTabIndex === 1
            Loader {
                anchors.fill: parent
                active: monitorTabIndex === 1
                sourceComponent: terminalComponent
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: monitorTabIndex === 2

            MetricPlotCard {
                anchors.fill: parent
            }
        }

    }

    Component { id: terminalComponent; Console {} }

    function displayHexText(value) {
        return String(value || "").replace(/0X/g, "0x")
    }

    function monitorRowNumber(rowIndex) {
        var value = Number(rowIndex || 0) % 1000
        if (value < 0)
            value = 0
        var text = value.toString()
        while (text.length < 3)
            text = "0" + text
        return text
    }

    function exportHex(value, minDigits) {
        var text = Number(value || 0).toString(16).toUpperCase()
        while (text.length < minDigits)
            text = "0" + text
        return "0x" + text
    }

    function currentLinkExportInfo() {
        var type = String(linkManager.linkType || "LINK")
        if (type === "UART")
            return String(uartDriver.portName || "UART") + "_" + String(uartDriver.baudRate || "")
        if (type === "USB")
            return "VID" + exportHex(usbDriver.vendorId, 4) + "_PID" + exportHex(usbDriver.productId, 4)
        if (type === "CAN")
            return "CH" + String(canDriver.channel || 0)
                    + "_TX" + exportHex(canDriver.txId, 0)
                    + "_RX" + exportHex(canDriver.rxId, 0)
        if (type === "BLE")
            return String(linkManager.connectedLinkName || "BLE")
        return type
    }

    function openDataPreview(frame) {
        var data = displayHexText(frame && frame.data ? frame.data : "")
        if (data.length <= 0)
            return

        var src = displayHexText(frame.sender || "-")
        var dst = displayHexText(frame.receiver || "-")
        var set = displayHexText(frame.cmdSet || "-")
        var id = displayHexText(frame.cmdId || "-")
        root.dataPreviewTitle = "Data"
        root.dataPreviewMeta = (frame.timestamp || "-")
                + "  " + src + " -> " + dst
                + "  Seq " + (frame.seq || "-")
                + "  CmdSet " + set
                + "  CmdID " + id
        root.dataPreviewText = data
        dataPreviewPopup.open()
    }

    function dataColumnWidth(totalWidth) {
        return Math.max(70, totalWidth - fixedTableWidth - 18)
    }

    function monitorRowColor(customColor, rowIndex, hovered, current) {
        if (current)
            return themeManager.isDark ? "#1A3050" : "#DBEAFE"
        if (customColor && String(customColor).length > 0)
            return String(customColor)
        if (hovered)
            return themeManager.isDark ? "#162030" : "#F3F4F6"
        return rowIndex % 2 === 0 ? surface : surfaceSoft
    }

    function monitorTextColor(customColor, fallbackColor) {
        return customColor && String(customColor).length > 0 ? String(customColor) : fallbackColor
    }

    component TabButton: Button {
        id: tabControl
        property bool active: false
        property color accentColor: root.accent
        Layout.preferredHeight: 30
        Layout.preferredWidth: Math.max(72, tabText.implicitWidth + 24)
        padding: 0
        focusPolicy: Qt.TabFocus
        background: Rectangle {
            radius: 6
            color: tabControl.active ? Qt.rgba(tabControl.accentColor.r, tabControl.accentColor.g, tabControl.accentColor.b, 0.17)
                                     : (tabControl.hovered ? (themeManager.isDark ? "#1A2535" : "#F3F4F6") : "transparent")
            border.color: tabControl.active ? Qt.rgba(tabControl.accentColor.r, tabControl.accentColor.g, tabControl.accentColor.b, 0.58)
                                            : (tabControl.activeFocus ? root.accent : "transparent")
            Rectangle {
                width: parent.width - 12
                height: 2
                radius: 1
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 2
                color: tabControl.accentColor
                visible: tabControl.active
            }
        }
        contentItem: Text {
            id: tabText
            text: tabControl.text
            color: tabControl.active ? root.textPrimary : root.textSecondary
            font.pixelSize: 12
            font.weight: tabControl.active ? Font.DemiBold : Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component MonitorButton: Button {
        id: control
        property color tone: root.accent
        property bool primary: false
        Layout.preferredWidth: Math.max(54, buttonText.implicitWidth + 22)
        Layout.preferredHeight: 28
        padding: 0
        focusPolicy: Qt.TabFocus
        background: Rectangle {
            radius: 6
            color: control.primary ? (control.hovered ? Qt.lighter(control.tone, 1.08) : control.tone)
                                   : (control.hovered ? (themeManager.isDark ? "#1A2535" : "#F3F4F6") : (themeManager.isDark ? "#131920" : "#F9FAFB"))
            border.color: control.primary ? Qt.lighter(control.tone, 1.15) : (control.activeFocus ? root.accent : root.outline)
        }
        contentItem: Text {
            id: buttonText
            text: control.text
            color: control.primary ? "#FFFFFF" : root.textSecondary
            font.pixelSize: 12
            font.weight: Font.Medium
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component PillToggle: Button {
        id: toggle
        property bool isChecked: false
        Layout.preferredWidth: Math.max(104, label.implicitWidth + 46)
        Layout.preferredHeight: 28
        padding: 0
        checkable: false
        focusPolicy: Qt.TabFocus
        background: Rectangle {
            radius: 14
            color: toggle.isChecked ? (themeManager.isDark ? "#0D2D4A" : "#DBEAFE") : (themeManager.isDark ? "#131920" : "#F3F4F6")
            border.color: toggle.activeFocus ? root.accent : (toggle.isChecked ? (themeManager.isDark ? "#1E5A94" : "#93C5FD") : root.outline)
        }
        contentItem: Item {
            Rectangle {
                id: track
                width: 24
                height: 12
                radius: 6
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                color: toggle.isChecked ? root.accent : (themeManager.isDark ? "#384858" : "#CBD5E1")
                Rectangle {
                    width: 10
                    height: 10
                    radius: 5
                    anchors.verticalCenter: parent.verticalCenter
                    x: toggle.isChecked ? 13 : 1
                    color: "#FFFFFF"
                    Behavior on x { NumberAnimation { duration: 120 } }
                }
            }
            Text {
                id: label
                anchors.left: track.right
                anchors.leftMargin: 6
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: toggle.text
                color: toggle.isChecked ? root.textPrimary : root.textSecondary
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }
    }

    component HeaderCell: Text {
        color: themeManager.isDark ? "#8899AA" : "#6B7280"
        font.pixelSize: 12
        font.weight: Font.DemiBold
        verticalAlignment: Text.AlignVCenter
        leftPadding: 2
        rightPadding: 4
        height: parent ? parent.height : 28
        elide: Text.ElideRight
    }

    component TableCell: Text {
        property int cellPadding: 3
        font.pixelSize: 13
        font.family: "Consolas"
        verticalAlignment: Text.AlignVCenter
        leftPadding: cellPadding
        height: parent ? parent.height : 27
        elide: Text.ElideRight
    }

    component DataCell: Item {
        id: dataCell
        property string text: ""
        property color color: root.textPrimary
        signal clicked()

        height: parent ? parent.height : 27
        clip: true

        function byteTokens(value) {
            var clean = String(value || "").trim()
            if (clean.length <= 0)
                return []
            return clean.split(/\s+/)
        }

        Rectangle {
            anchors.fill: parent
            anchors.leftMargin: 1
            anchors.rightMargin: 2
            anchors.topMargin: 3
            anchors.bottomMargin: 3
            radius: 4
            color: dataMouse.containsMouse && dataCell.text.length > 0 ? (themeManager.isDark ? "#1A2535" : "#EFF6FF") : "transparent"
            border.color: dataMouse.containsMouse && dataCell.text.length > 0 ? root.outlineStrong : "transparent"
        }

        Row {
            anchors.fill: parent
            anchors.leftMargin: 3
            anchors.rightMargin: 3
            spacing: 2
            clip: true

            Repeater {
                model: dataCell.byteTokens(dataCell.text)
                Text {
                    text: modelData
                    color: dataCell.color
                    font.pixelSize: 13
                    font.family: "Consolas"
                    height: dataCell.height
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        MouseArea {
            id: dataMouse
            anchors.fill: parent
            enabled: dataCell.text.length > 0
            hoverEnabled: enabled
            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: dataCell.clicked()
        }
    }

    component MetricPlotCard: Rectangle {
        id: card
        radius: 7
        color: surfaceRaised
        border.color: outline
        clip: true

        property var curveSamples: []
        property var latestValues: []
        property int sampleCount: 0
        property real minValue: 0
        property real maxValue: 0
        property real visibleStartTime: 0
        property real visibleEndTime: 0
        property string visibleStartText: ""
        property string visibleEndText: ""
        property string filterStatus: "等待协议监控数据"
        property bool filterValid: false
        property int matchedFrameCount: 0
        property real lastPlotRefreshMs: 0
        property bool plotConfigPaused: false
        property string protocolChoiceSignature: ""
        property string selectedProtocolKey: ""
        property int selectedProtocolIndex: -1
        readonly property int maxPlotFps: 60
        readonly property real minPlotIntervalMs: 1000 / maxPlotFps
        readonly property int plotTimeDivisions: 4
        readonly property int maxCurves: 9
        readonly property var secondsPerDivOptions: [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50, 100]
        readonly property var dataTypeOptions: ["uint8", "int8", "uint16", "int16", "uint32", "int32", "uint64", "int64", "float LE", "double LE"]
        readonly property var curvePalette: ["#2563EB", "#059669", "#D97706", "#7C3AED", "#0891B2", "#E11D48", "#65A30D", "#EA580C", "#7C3AED"]

        ListModel {
            id: curveModel
        }

        ListModel {
            id: protocolChoiceModel
        }

        Timer {
            id: plotRefreshTimer
            interval: 16
            repeat: false
            onTriggered: card.rebuildSamples()
        }

        Timer {
            id: plotConfigRefreshTimer
            interval: 450
            repeat: false
            onTriggered: {
                plotRefreshTimer.stop()
                card.resumePlotRefresh()
            }
        }

        Connections {
            target: monitorModel
            function onCountChanged() { card.schedulePlotRefresh(false) }
        }

        Connections {
            target: root
            function onMonitorTabIndexChanged() {
                if (root.monitorTabIndex === 2)
                    card.schedulePlotRefresh(true)
            }
        }

        Component.onCompleted: {
            if (curveModel.count === 0)
                card.appendDefaultCurves()
            card.schedulePlotRefresh(true)
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 32
                spacing: 6

                Text {
                    text: "协议曲线"
                    color: textPrimary
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 70
                    elide: Text.ElideRight
                }

                ValueBadge {
                    label: "曲线"
                    value: curveModel.count + "/" + card.maxCurves
                    accentColor: root.accent
                }

                ValueBadge {
                    label: "样本"
                    value: card.sampleCount.toString()
                    accentColor: root.textSecondary
                }

                Item { Layout.fillWidth: true }

                MonitorButton {
                    text: "新增曲线"
                    tone: root.success
                    primary: false
                    enabled: curveModel.count < card.maxCurves
                    Layout.preferredWidth: 76
                    onClicked: card.appendCurve()
                }

                CompactLabel { text: "s/div" }
                Slider {
                    id: secondsPerDivSlider
                    Layout.preferredWidth: 156
                    Layout.preferredHeight: 28
                    from: 0
                    to: card.secondsPerDivOptions.length - 1
                    value: 6
                    stepSize: 1
                    snapMode: Slider.SnapAlways
                    onValueChanged: card.refreshViewportOnly()
                    background: Rectangle {
                        x: secondsPerDivSlider.leftPadding
                        y: secondsPerDivSlider.topPadding + secondsPerDivSlider.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 4
                        width: secondsPerDivSlider.availableWidth
                        height: implicitHeight
                        radius: 2
                        color: surfaceSoft

                        Rectangle {
                            width: secondsPerDivSlider.visualPosition * parent.width
                            height: parent.height
                            color: accent
                            radius: 2
                        }
                    }
                    handle: Rectangle {
                        x: secondsPerDivSlider.leftPadding + secondsPerDivSlider.visualPosition * (secondsPerDivSlider.availableWidth - width)
                        y: secondsPerDivSlider.topPadding + secondsPerDivSlider.availableHeight / 2 - height / 2
                        implicitWidth: 16
                        implicitHeight: 16
                        radius: 8
                        color: secondsPerDivSlider.pressed ? Qt.lighter(accent, 1.2) : accent
                        border.color: Qt.lighter(accent, 1.3)
                    }
                }

                Text {
                    text: card.secondsPerDivLabel(card.currentSecondsPerDiv())
                    color: textSecondary
                    font.pixelSize: 11
                    font.family: "Consolas"
                    Layout.preferredWidth: 72
                    horizontalAlignment: Text.AlignRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                spacing: 6

                CompactLabel {
                    text: "协议"
                    Layout.preferredWidth: 32
                }

                ComboBox {
                    id: protocolSelector
                    Layout.preferredWidth: 170
                    Layout.preferredHeight: 28
                    enabled: protocolChoiceModel.count > 0
                    model: protocolChoiceModel
                    textRole: "label"
                    currentIndex: card.selectedProtocolIndex
                    font.pixelSize: 11
                    font.family: "Consolas"
                    Connections {
                        target: protocolSelector.popup
                        function onVisibleChanged() {
                            if (protocolSelector.popup.visible)
                                card.beginPlotConfigPause()
                            else if (card.plotConfigPaused)
                                card.deferPlotRefresh()
                        }
                    }
                    onActivated: function(index) {
                        card.selectProtocol(index, true)
                        card.deferPlotRefresh()
                    }
                    contentItem: Text {
                        text: protocolSelector.enabled ? protocolSelector.displayText : "暂无协议"
                        color: protocolSelector.enabled ? textPrimary : textMuted
                        font: protocolSelector.font
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                        leftPadding: 10
                        rightPadding: 24
                    }
                    background: Rectangle {
                        radius: 5
                        color: surfaceSoft
                        border.color: protocolSelector.activeFocus ? accent
                                      : (protocolSelector.enabled ? outline : outlineStrong)
                    }
                    popup: Popup {
                        y: protocolSelector.height + 2
                        width: protocolSelector.width
                        implicitHeight: contentItem.implicitHeight + 2
                        padding: 1
                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: protocolSelector.popup.visible ? protocolSelector.delegateModel : null
                            currentIndex: protocolSelector.highlightedIndex
                            delegate: Rectangle {
                                width: protocolSelector.width
                                height: protocolSelector.implicitItemHeight
                                color: index === protocolSelector.currentIndex ? accent : (hovered ? surfaceRaised : surface)
                                required property int index
                                required property string modelData
                                property bool hovered: indexMouse.containsMouse
                                Text {
                                    anchors.centerIn: parent
                                    text: modelData
                                    color: index === protocolSelector.currentIndex ? "#FFFFFF" : textPrimary
                                    font.pixelSize: 11
                                }
                                MouseArea {
                                    id: indexMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: {
                                        protocolSelector.currentIndex = index
                                        protocolSelector.popup.close()
                                    }
                                }
                            }
                        }
                        background: Rectangle {
                            radius: 5
                            color: surface
                            border.color: outline
                        }
                    }
                }

                Text {
                    text: card.filterStatus
                    color: !card.filterValid ? root.warning
                         : (card.matchedFrameCount > 0 ? root.success : root.textMuted)
                    font.pixelSize: 11
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }
            }

            Flow {
                id: curveControls
                Layout.fillWidth: true
                Layout.preferredHeight: implicitHeight
                spacing: 8

                Repeater {
                    model: curveModel
                    delegate: Row {
                        width: 352
                        height: 28
                        spacing: 5

                        CurveTag { text: "C" + (index + 1); accentColor: card.curveColor(index) }
                        CheckBox {
                            id: hideCurveCheck
                            width: 54
                            height: 28
                            checked: hidden === true
                            onToggled: {
                                if (curveModel.count > index && curveModel.get(index).hidden !== checked) {
                                    curveModel.setProperty(index, "hidden", checked)
                                    card.deferPlotRefresh()
                                }
                            }
                            indicator: Rectangle {
                                width: 14
                                height: 14
                                x: 0
                                anchors.verticalCenter: parent.verticalCenter
                                radius: 3
                                color: hideCurveCheck.checked ? root.warning : surfaceSoft
                                border.color: hideCurveCheck.checked ? root.warning : outlineStrong
                                Text {
                                    anchors.centerIn: parent
                                    text: hideCurveCheck.checked ? "x" : ""
                                    color: "#FFFFFF"
                                    font.pixelSize: 9
                                    font.weight: Font.DemiBold
                                }
                            }
                            contentItem: Text {
                                text: "隐藏"
                                color: hideCurveCheck.checked ? root.warning : root.textSecondary
                                font.pixelSize: 11
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: 18
                                elide: Text.ElideRight
                            }
                        }
                        CompactLabel { text: "偏移"; width: 28; height: parent.height }
                        SpinBox {
                            width: 58
                            height: 28
                            from: 0
                            to: 63
                            value: offset
                            editable: true
                            onActiveFocusChanged: {
                                if (activeFocus)
                                    card.beginPlotConfigPause()
                                else if (card.plotConfigPaused)
                                    card.deferPlotRefresh()
                            }
                            onValueChanged: {
                                if (curveModel.count > index && curveModel.get(index).offset !== value) {
                                    curveModel.setProperty(index, "offset", value)
                                    card.deferPlotRefresh()
                                }
                            }
                            background: Rectangle {
                                color: surfaceSoft
                                border.color: activeFocus ? accent : outline
                                radius: 5
                            }
                        }
                        CompactLabel { text: "类型"; width: 28; height: parent.height }
                        ComboBox {
                            id: curveTypeCombo
                            width: 104
                            height: 28
                            model: card.dataTypeOptions
                            currentIndex: typeIndex
                            font.pixelSize: 11
                            Connections {
                                target: curveTypeCombo.popup
                                function onVisibleChanged() {
                                    if (curveTypeCombo.popup.visible)
                                        card.beginPlotConfigPause()
                                    else if (card.plotConfigPaused)
                                        card.deferPlotRefresh()
                                }
                            }
                            onActivated: function(selectedIndex) {
                                if (curveModel.count > index && curveModel.get(index).typeIndex !== selectedIndex) {
                                    curveModel.setProperty(index, "typeIndex", selectedIndex)
                                    card.deferPlotRefresh()
                                }
                            }
                            background: Rectangle { radius: 5; color: surfaceSoft; border.color: curveTypeCombo.activeFocus ? accent : outline }
                            contentItem: Text {
                                text: curveTypeCombo.displayText
                                color: textPrimary
                                font.pixelSize: 11
                                leftPadding: 8
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            popup: Popup {
                                y: curveTypeCombo.height + 2
                                width: curveTypeCombo.width
                                implicitHeight: contentItem.implicitHeight + 2
                                padding: 1
                                contentItem: ListView {
                                    clip: true
                                    implicitHeight: contentHeight
                                    model: curveTypeCombo.popup.visible ? curveTypeCombo.delegateModel : null
                                    currentIndex: curveTypeCombo.highlightedIndex
                                    delegate: Rectangle {
                                        width: curveTypeCombo.width
                                        height: curveTypeCombo.implicitItemHeight
                                        color: index === curveTypeCombo.currentIndex ? accent : (hovered ? surfaceRaised : surface)
                                        required property int index
                                        required property string modelData
                                        property bool hovered: indexMouse.containsMouse
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: index === curveTypeCombo.currentIndex ? "#FFFFFF" : textPrimary
                                            font.pixelSize: 11
                                        }
                                        MouseArea {
                                            id: indexMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                curveTypeCombo.currentIndex = index
                                                curveTypeCombo.popup.close()
                                            }
                                        }
                                    }
                                }
                                background: Rectangle {
                                    radius: 5
                                    color: surface
                                    border.color: outline
                                }
                            }
                        }
                        Button {
                            width: 24
                            height: 28
                            enabled: curveModel.count > 1
                            onClicked: card.removeCurve(index)
                            background: Rectangle {
                                radius: 5
                                color: parent.enabled && parent.hovered ? (themeManager.isDark ? "#3A2028" : "#FEE2E2") : "transparent"
                                border.color: parent.enabled ? root.danger : root.outline
                            }
                            contentItem: Text {
                                text: "x"
                                color: parent.enabled ? root.danger : root.textMuted
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                color: themeManager.isDark ? "#0A0F18" : "#FFFFFF"
                border.color: outline
                clip: true

                Canvas {
                    id: plotCanvas
                    anchors.fill: parent
                    anchors.margins: 1
                    antialiasing: true

                    onPaint: {
                        var ctx = getContext("2d")
                        var w = width
                        var h = height
                        ctx.fillStyle = themeManager.isDark ? "#0A0F18" : "#FFFFFF"
                        ctx.fillRect(0, 0, w, h)

                        if (card.plotConfigPaused) {
                            ctx.fillStyle = root.textMuted
                            ctx.font = "12px sans-serif"
                            ctx.textAlign = "center"
                            ctx.textBaseline = "middle"
                            ctx.fillText("配置切换中，暂停绘制", w / 2, h / 2)
                            return
                        }

                        var left = 42
                        var right = 14
                        var top = 12
                        var bottom = 24
                        var plotW = Math.max(1, w - left - right)
                        var plotH = Math.max(1, h - top - bottom)

                        ctx.strokeStyle = themeManager.isDark ? "#162030" : "#E5E7EB"
                        ctx.lineWidth = 1
                        for (var gx = 0; gx <= 4; gx++) {
                            var x = left + plotW * gx / 4
                            ctx.beginPath()
                            ctx.moveTo(x, top)
                            ctx.lineTo(x, top + plotH)
                            ctx.stroke()
                        }
                        for (var gy = 0; gy <= 3; gy++) {
                            var y = top + plotH * gy / 3
                            ctx.beginPath()
                            ctx.moveTo(left, y)
                            ctx.lineTo(left + plotW, y)
                            ctx.stroke()
                        }

                        ctx.fillStyle = root.textMuted
                        ctx.font = "10px Consolas"
                        ctx.textAlign = "right"
                        ctx.textBaseline = "middle"
                        ctx.fillText(card.formatNumber(card.maxValue), left - 6, top + 2)
                        ctx.fillText(card.formatNumber(card.minValue), left - 6, top + plotH - 2)

                        var seriesList = card.curveSamples
                        var totalCount = card.totalSampleCount(seriesList)
                        if (totalCount === 0) {
                            ctx.fillStyle = root.textMuted
                            ctx.font = "12px sans-serif"
                            ctx.textAlign = "center"
                            ctx.textBaseline = "middle"
                            ctx.fillText("暂无可绘制数据", w / 2, h / 2)
                            return
                        }

                        var t0 = card.visibleStartTime
                        var t1 = card.visibleEndTime
                        if (t1 <= t0)
                            t1 = t0 + 1

                        var range = card.maxValue - card.minValue
                        if (range === 0)
                            range = 1

                        for (var s = 0; s < seriesList.length; s++) {
                            var list = seriesList[s]
                            if (!list || list.length === 0)
                                continue

                            ctx.strokeStyle = card.curveColor(s)
                            ctx.lineWidth = 2
                            var started = false
                            ctx.beginPath()

                            for (var i = 0; i < list.length; i++) {
                                if (list[i].t < t0 || list[i].t > t1)
                                    continue

                                var sx = left + ((list[i].t - t0) / (t1 - t0)) * plotW
                                var sy = top + plotH - ((list[i].v - card.minValue) / range) * plotH
                                if (!started) {
                                    ctx.moveTo(sx, sy)
                                    started = true
                                } else {
                                    ctx.lineTo(sx, sy)
                                }
                            }
                            if (started)
                                ctx.stroke()

                            if (list.length <= 120) {
                                ctx.fillStyle = card.curveColor(s)
                                for (var p = 0; p < list.length; p++) {
                                    if (list[p].t < t0 || list[p].t > t1)
                                        continue

                                    var px = left + ((list[p].t - t0) / (t1 - t0)) * plotW
                                    var py = top + plotH - ((list[p].v - card.minValue) / range) * plotH
                                    ctx.beginPath()
                                    ctx.arc(px, py, 2, 0, Math.PI * 2)
                                    ctx.fill()
                                }
                            }
                        }

                        ctx.fillStyle = root.textMuted
                        ctx.textAlign = "left"
                        ctx.textBaseline = "alphabetic"
                        ctx.fillText(card.shortTime(card.formatTimeMs(t0)), left, h - 7)
                        ctx.textAlign = "right"
                        ctx.fillText(card.shortTime(card.formatTimeMs(t1)), w - right, h - 7)
                    }
                }
            }
        }

        function schedulePlotRefresh(immediate) {
            if (root.monitorTabIndex !== 2)
                return

            if (immediate) {
                plotConfigRefreshTimer.stop()
                plotConfigPaused = false
                plotRefreshTimer.stop()
                rebuildSamples()
                return
            }

            if (plotConfigPaused || plotConfigRefreshTimer.running)
                return

            var now = Date.now()
            var elapsed = now - lastPlotRefreshMs
            if (elapsed >= minPlotIntervalMs) {
                plotRefreshTimer.stop()
                rebuildSamples()
                return
            }

            plotRefreshTimer.interval = Math.max(1, Math.ceil(minPlotIntervalMs - elapsed))
            if (!plotRefreshTimer.running)
                plotRefreshTimer.start()
        }

        function beginPlotConfigPause() {
            if (root.monitorTabIndex !== 2)
                return

            plotRefreshTimer.stop()
            plotConfigRefreshTimer.stop()
            if (plotConfigPaused)
                return

            plotConfigPaused = true
            curveSamples = []
            latestValues = []
            sampleCount = 0
            matchedFrameCount = 0
            filterValid = false
            filterStatus = "配置切换中，暂停绘制"
            visibleStartTime = 0
            visibleEndTime = 0
            visibleStartText = ""
            visibleEndText = ""
            minValue = 0
            maxValue = 0
            plotCanvas.requestPaint()
        }

        function deferPlotRefresh() {
            if (root.monitorTabIndex !== 2)
                return

            beginPlotConfigPause()
            plotRefreshTimer.stop()
            plotConfigRefreshTimer.stop()
            plotConfigRefreshTimer.start()
        }

        function resumePlotRefresh() {
            if (root.monitorTabIndex !== 2)
                return

            plotConfigPaused = false
            rebuildSamples()
        }

        function refreshViewportOnly() {
            if (root.monitorTabIndex !== 2)
                return

            if (sampleCount > 0) {
                updateVisibleRange()
                plotCanvas.requestPaint()
                return
            }

            deferPlotRefresh()
        }

        function rebuildSamples() {
            lastPlotRefreshMs = Date.now()
            var frames = monitorModel.frames(5000)
            var parsed = []
            var latest = []
            var total = 0
            var matched = 0
            var latestDataBytes = 0
            var lastPlotTime = Number.NEGATIVE_INFINITY
            refreshProtocolChoices(frames)
            var selection = selectedProtocol()
            var targetSet = selection.cmdSet
            var targetId = selection.cmdId

            for (var curve = 0; curve < curveModel.count; curve++) {
                parsed.push([])
                latest.push(Number.NaN)
            }

            filterValid = selection.valid
            if (!filterValid) {
                matchedFrameCount = 0
                filterStatus = frames.length > 0 ? "请选择协议监控中的协议" : "协议监控暂无可选协议"
                applySampleResult(parsed, latest, 0)
                return
            }

            for (var i = 0; i < frames.length; i++) {
                var frame = frames[i]
                if (!frameMatchesCommand(frame, targetSet, targetId))
                    continue

                matched++
                var bytes = frameDataBytes(frame)
                latestDataBytes = bytes.length
                var timeValue = timestampMs(frame.timestamp, i)
                if (!isFinite(timeValue))
                    timeValue = i
                if (timeValue <= lastPlotTime)
                    timeValue = lastPlotTime + 1
                lastPlotTime = timeValue

                for (var c = 0; c < curveModel.count; c++) {
                    var config = curveModel.get(c)
                    if (config.hidden)
                        continue

                    var value = readBytes(bytes, config.offset, dataTypeOptions[config.typeIndex] || dataTypeOptions[0])
                    if (!isFinite(value))
                        continue

                    parsed[c].push({ t: timeValue, v: value, timeText: frame.timestamp || "" })
                    latest[c] = value
                    total++
                }
            }

            matchedFrameCount = matched
            if (matched === 0)
                filterStatus = "扫描 " + frames.length + " 帧，未匹配 "
                        + commandText(targetSet, targetId)
            else if (total === 0)
                filterStatus = "匹配 " + matched + " 帧，Data " + latestDataBytes + "B，未取到有效数值"
            else
                filterStatus = "扫描 " + frames.length + " 帧，匹配 " + matched
                        + " 帧，Data " + latestDataBytes + "B，绘制 " + total + " 点"

            applySampleResult(parsed, latest, total)
        }

        function applySampleResult(parsed, latest, total) {
            curveSamples = parsed
            latestValues = latest
            sampleCount = total
            updateVisibleRange()
            plotCanvas.requestPaint()
        }

        function updateVisibleRange() {
            if (sampleCount === 0) {
                visibleStartTime = 0
                visibleEndTime = 0
                return
            }

            var latestTime = 0
            for (var c = 0; c < curveSamples.length; c++) {
                var list = curveSamples[c]
                if (list.length > 0) {
                    var lastT = list[list.length - 1].t
                    if (lastT > latestTime)
                        latestTime = lastT
                }
            }

            var spd = currentSecondsPerDiv()
            var span = spd * plotTimeDivisions
            visibleEndTime = latestTime
            visibleStartTime = latestTime - span
        }

        function currentSecondsPerDiv() {
            var idx = Math.round(secondsPerDivSlider.value)
            if (idx < 0) idx = 0
            if (idx >= secondsPerDivOptions.length) idx = secondsPerDivOptions.length - 1
            return secondsPerDivOptions[idx]
        }

        function secondsPerDivLabel(spd) {
            if (spd < 1) return (spd * 1000).toFixed(0) + " ms"
            if (spd < 60) return spd.toFixed(spd < 10 ? 1 : 0) + " s"
            return (spd / 60).toFixed(1) + " min"
        }

        function totalSampleCount(seriesList) {
            var count = 0
            for (var s = 0; s < seriesList.length; s++)
                count += seriesList[s].length
            return count
        }

        function curveColor(index) {
            return curvePalette[index % curvePalette.length]
        }

        function formatNumber(value) {
            if (Math.abs(value) >= 1000000) return (value / 1000000).toFixed(1) + "M"
            if (Math.abs(value) >= 1000) return (value / 1000).toFixed(1) + "K"
            if (Math.abs(value) < 0.01 && value !== 0) return value.toExponential(1)
            return value.toFixed(2)
        }

        function formatTimeMs(ms) {
            return ms
        }

        function shortTime(ms) {
            if (ms < 1000) return ms.toFixed(0) + "ms"
            if (ms < 60000) return (ms / 1000).toFixed(1) + "s"
            return (ms / 60000).toFixed(1) + "m"
        }

        function timestampMs(timestamp, frameIndex) {
            if (!timestamp) return frameIndex
            var parts = timestamp.split(/[:.]/)
            if (parts.length >= 3) {
                var h = parseInt(parts[0]) || 0
                var m = parseInt(parts[1]) || 0
                var s = parseInt(parts[2]) || 0
                var ms = parseInt(parts[3]) || 0
                return ((h * 3600 + m * 60) * 1000 + s * 1000 + ms)
            }
            return frameIndex
        }

        function frameMatchesCommand(frame, targetSet, targetId) {
            var fs = String(frame.cmdSet || "")
            var fi = String(frame.cmdId || "")
            var ts = String(targetSet || "")
            var ti = String(targetId || "")
            if (ts === "0x00" && ti === "0x00") return true
            return fs === ts && fi === ti
        }

        function frameDataBytes(frame) {
            var hex = String(frame.data || "").trim()
            if (hex.length === 0) return []
            hex = hex.replace(/0x/gi, "").replace(/[\s,;:_-]+/g, "")
            var bytes = []
            for (var i = 0; i + 1 < hex.length; i += 2)
                bytes.push(parseInt(hex.substring(i, i + 2), 16))
            return bytes
        }

        function readBytes(bytes, offset, type) {
            if (offset < 0 || offset >= bytes.length) return Number.NaN
            switch (type) {
            case "uint8": return bytes[offset]
            case "int8": return bytes[offset] > 127 ? bytes[offset] - 256 : bytes[offset]
            case "uint16":
                if (offset + 1 >= bytes.length) return Number.NaN
                return bytes[offset] | (bytes[offset + 1] << 8)
            case "int16":
                if (offset + 1 >= bytes.length) return Number.NaN
                var u16 = bytes[offset] | (bytes[offset + 1] << 8)
                return u16 > 32767 ? u16 - 65536 : u16
            case "uint32":
                if (offset + 3 >= bytes.length) return Number.NaN
                return (bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24)) >>> 0
            case "int32":
                if (offset + 3 >= bytes.length) return Number.NaN
                return bytes[offset] | (bytes[offset + 1] << 8) | (bytes[offset + 2] << 16) | (bytes[offset + 3] << 24)
            case "float LE":
                if (offset + 3 >= bytes.length) return Number.NaN
                var buf = new ArrayBuffer(4)
                var view = new DataView(buf)
                view.setUint8(0, bytes[offset])
                view.setUint8(1, bytes[offset + 1])
                view.setUint8(2, bytes[offset + 2])
                view.setUint8(3, bytes[offset + 3])
                return view.getFloat32(0, true)
            case "double LE":
                if (offset + 7 >= bytes.length) return Number.NaN
                var buf2 = new ArrayBuffer(8)
                var view2 = new DataView(buf2)
                for (var b = 0; b < 8; b++) view2.setUint8(b, bytes[offset + b])
                return view2.getFloat64(0, true)
            default:
                return bytes[offset]
            }
        }

        function commandText(cmdSet, cmdId) {
            return "CmdSet " + cmdSet + " CmdID " + cmdId
        }

        function refreshProtocolChoices(frames) {
            var seen = {}
            protocolChoiceModel.clear()
            for (var i = 0; i < frames.length; i++) {
                var f = frames[i]
                var key = String(f.cmdSet || "") + "|" + String(f.cmdId || "")
                if (seen[key]) continue
                seen[key] = true
                var label = String(f.cmdSet || "") + " / " + String(f.cmdId || "")
                if (f.cmdSet && f.cmdId) {
                    var setNum = parseInt(String(f.cmdSet).replace(/0x/gi, ""), 16)
                    var idNum = parseInt(String(f.cmdId).replace(/0x/gi, ""), 16)
                    if (!isNaN(setNum) && !isNaN(idNum))
                        label = "0x" + setNum.toString(16).toUpperCase() + " / 0x" + idNum.toString(16).toUpperCase()
                }
                protocolChoiceModel.append({ label: label, cmdSet: f.cmdSet || "", cmdId: f.cmdId || "" })
            }

            if (protocolChoiceModel.count > 0 && selectedProtocolIndex < 0) {
                selectedProtocolIndex = 0
            } else if (protocolChoiceModel.count === 0) {
                selectedProtocolIndex = -1
            }
        }

        function selectProtocol(index, userTriggered) {
            if (index < 0 || index >= protocolChoiceModel.count) {
                selectedProtocolIndex = -1
                selectedProtocolKey = ""
                return
            }
            selectedProtocolIndex = index
            var item = protocolChoiceModel.get(index)
            selectedProtocolKey = item.cmdSet + "|" + item.cmdId
        }

        function selectedProtocol() {
            if (selectedProtocolIndex < 0 || selectedProtocolIndex >= protocolChoiceModel.count)
                return { valid: false, cmdSet: "", cmdId: "" }
            var item = protocolChoiceModel.get(selectedProtocolIndex)
            return { valid: true, cmdSet: item.cmdSet, cmdId: item.cmdId }
        }

        function appendDefaultCurves() {
            curveModel.append({ offset: 0, typeIndex: 0, hidden: false })
            curveModel.append({ offset: 4, typeIndex: 0, hidden: false })
        }

        function appendCurve() {
            if (curveModel.count >= maxCurves) return
            var lastOffset = 0
            var lastType = 0
            if (curveModel.count > 0) {
                var last = curveModel.get(curveModel.count - 1)
                lastOffset = last.offset + 4
                lastType = last.typeIndex
            }
            curveModel.append({ offset: lastOffset, typeIndex: lastType, hidden: false })
        }

        function removeCurve(index) {
            if (curveModel.count <= 1) return
            curveModel.remove(index)
        }
    }

    component ValueBadge: Rectangle {
        property string label: ""
        property string value: ""
        property color accentColor: root.accent
        height: 20
        radius: 10
        color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.12)
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.3)
        implicitWidth: Math.max(60, badgeLabel.implicitWidth + badgeValue.implicitWidth + 20)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 4

            Text {
                id: badgeLabel
                text: label
                color: root.textSecondary
                font.pixelSize: 10
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                id: badgeValue
                text: value
                color: accentColor
                font.pixelSize: 10
                font.weight: Font.DemiBold
                font.family: "Consolas"
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    component CurveTag: Rectangle {
        property string text: ""
        property color accentColor: root.accent
        width: 28
        height: 28
        radius: 6
        color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.15)
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.4)

        Text {
            anchors.centerIn: parent
            text: curveTag.text
            color: accentColor
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.family: "Consolas"
        }
    }

    component CompactLabel: Text {
        color: root.textSecondary
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
