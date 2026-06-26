import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    color: pageBg

    property int monitorTabIndex: 0
    readonly property bool terminalActive: monitorTabIndex === 1
    property bool luaScriptPanelOpen: false
    readonly property bool luaScriptPanelAvailable: monitorTabIndex === 0 || monitorTabIndex === 2
    readonly property bool luaScriptPanelVisible: luaScriptPanelAvailable && luaScriptPanelOpen

    readonly property color pageBg: "#0F141B"
    readonly property color surface: "#151B23"
    readonly property color surfaceRaised: "#1A222C"
    readonly property color surfaceSoft: "#10161D"
    readonly property color outline: "#293544"
    readonly property color outlineStrong: "#3B4A5C"
    readonly property color textPrimary: "#E7EDF5"
    readonly property color textSecondary: "#AAB6C4"
    readonly property color textMuted: "#6F7D8C"
    readonly property color accent: "#2F81F7"
    readonly property color success: "#2DBA7F"
    readonly property color warning: "#E3A13B"
    readonly property color danger: "#E25D5D"
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
                    selectedTextColor: "#08121B"
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
                        placeholderText: "示例: Src:0x0101 && Len:24 || Data:*AA*"
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
                            color: "#111821"
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
                                    color: parent.pressed ? accent : (vScrollBar.hovered ? outlineStrong : "#465362")
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
                                    TableCell { text: displayHexText(model.sender || ""); width: 54; color: monitorTextColor(customTextColor, "#8CC8FF") }
                                    TableCell { text: displayHexText(model.receiver || ""); width: 54; color: monitorTextColor(customTextColor, "#8CC8FF") }
                                    TableCell { text: model.len || ""; width: 28; color: monitorTextColor(customTextColor, "#A7D38C") }
                                    TableCell { text: model.type || ""; width: 40; color: monitorTextColor(customTextColor, "#E8CC73") }
                                    TableCell { text: model.seq || ""; width: seqColumnWidth; color: monitorTextColor(customTextColor, "#7DB5FF") }
                                    TableCell { text: displayHexText(model.cmdSet || ""); width: 56; color: monitorTextColor(customTextColor, "#D6A4E8") }
                                    TableCell { text: displayHexText(model.cmdId || ""); width: 52; color: monitorTextColor(customTextColor, "#E8CC73") }
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
                                    TableCell { text: displayHexText(model.crc || ""); width: 48; color: monitorTextColor(customTextColor, "#7DB5FF") }
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
            return "#183553"
        if (customColor && String(customColor).length > 0)
            return String(customColor)
        if (hovered)
            return "#1B2633"
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
                                     : (tabControl.hovered ? "#202A36" : "transparent")
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
                                   : (control.hovered ? "#202B37" : "#161D26")
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
            color: toggle.isChecked ? "#162A3E" : "#151D26"
            border.color: toggle.activeFocus ? root.accent : (toggle.isChecked ? "#295D8E" : root.outline)
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
                color: toggle.isChecked ? root.accent : "#52606F"
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
        color: "#AFC0D0"
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
            color: dataMouse.containsMouse && dataCell.text.length > 0 ? "#1F2E3D" : "transparent"
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
        readonly property var curvePalette: ["#2F81F7", "#2DBA7F", "#E3A13B", "#C586C0", "#4FC3F7", "#F06292", "#A7D38C", "#FF8A65", "#B39DDB"]

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
                        }
                        Button {
                            width: 24
                            height: 28
                            enabled: curveModel.count > 1
                            onClicked: card.removeCurve(index)
                            background: Rectangle {
                                radius: 5
                                color: parent.enabled && parent.hovered ? "#3A2028" : "transparent"
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
                color: "#0D131A"
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
                        ctx.fillStyle = "#0D131A"
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

                        ctx.strokeStyle = "#1E2A36"
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
            if (total > 0)
                updateVisibleRange()
            else {
                visibleStartTime = 0
                visibleEndTime = 0
                visibleStartText = ""
                visibleEndText = ""
                minValue = 0
                maxValue = 0
            }
            plotCanvas.requestPaint()
        }

        function cleanHexText(text) {
            return String(text || "").trim().replace(/0x/gi, "").replace(/[^0-9A-Fa-f]/g, "")
        }

        function parseCommandByte(text) {
            var normalized = cleanHexText(text)
            if (normalized.length === 0)
                return -1
            if (!/^[0-9A-Fa-f]{1,2}$/.test(normalized))
                return -1

            var value = parseInt(normalized, 16)
            return isFinite(value) && value >= 0 && value <= 255 ? value : -1
        }

        function parseCommandWord(text) {
            var normalized = cleanHexText(text)
            if (normalized.length === 0)
                return -1
            if (!/^[0-9A-Fa-f]{1,4}$/.test(normalized))
                return -1

            var value = parseInt(normalized, 16)
            return isFinite(value) && value >= 0 && value <= 65535 ? value : -1
        }

        function frameMatchesCommand(frame, targetSet, targetId) {
            if (!frame)
                return false

            if (parseCommandByte(frame.cmdSet) === targetSet
                    && parseCommandByte(frame.cmdId) === targetId) {
                return true
            }

            var targetWord = ((targetSet & 0xff) << 8) | (targetId & 0xff)
            if (parseCommandWord(frame.cmd) === targetWord)
                return true

            return rawFrameCommandWord(frame.rawHex) === targetWord
        }

        function commandPair(cmdSet, cmdId) {
            var setValue = cmdSet & 0xff
            var idValue = cmdId & 0xff
            var label = commandText(setValue, idValue)
            return {
                valid: true,
                cmdSet: setValue,
                cmdId: idValue,
                key: hexByteText(setValue) + ":" + hexByteText(idValue),
                label: label
            }
        }

        function frameCommandPair(frame) {
            if (!frame)
                return { valid: false }

            var setValue = parseCommandByte(frame.cmdSet)
            var idValue = parseCommandByte(frame.cmdId)
            if (setValue >= 0 && idValue >= 0)
                return commandPair(setValue, idValue)

            var word = parseCommandWord(frame.cmd)
            if (word < 0)
                word = rawFrameCommandWord(frame.rawHex)
            if (word < 0)
                return { valid: false }

            return commandPair((word >> 8) & 0xff, word & 0xff)
        }

        function refreshProtocolChoices(frames) {
            var seen = ({})
            var entries = []
            for (var i = 0; i < frames.length; i++) {
                var pair = frameCommandPair(frames[i])
                if (!pair.valid || seen[pair.key])
                    continue

                seen[pair.key] = true
                entries.push(pair)
            }

            var signature = entries.map(function(item) { return item.key }).join("|")
            if (signature !== protocolChoiceSignature) {
                protocolChoiceSignature = signature
                protocolChoiceModel.clear()
                for (var e = 0; e < entries.length; e++) {
                    protocolChoiceModel.append({
                        key: entries[e].key,
                        label: entries[e].label,
                        cmdSet: entries[e].cmdSet,
                        cmdId: entries[e].cmdId
                    })
                }
            }

            if (protocolChoiceModel.count === 0) {
                selectedProtocolKey = ""
                selectedProtocolIndex = -1
                return
            }

            var preferredKey = selectedProtocolKey
            if (preferredKey.length === 0)
                preferredKey = String(settingsManager.loadValue("curve.protocolKey", ""))

            var index = protocolIndexByKey(preferredKey)
            if (index < 0)
                index = 0

            selectProtocol(index, false)
        }

        function protocolIndexByKey(key) {
            if (!key || key.length === 0)
                return -1

            for (var i = 0; i < protocolChoiceModel.count; i++) {
                if (protocolChoiceModel.get(i).key === key)
                    return i
            }
            return -1
        }

        function selectProtocol(index, save) {
            if (index < 0 || index >= protocolChoiceModel.count) {
                selectedProtocolIndex = -1
                selectedProtocolKey = ""
                return
            }

            var item = protocolChoiceModel.get(index)
            selectedProtocolIndex = index
            selectedProtocolKey = item.key
            if (save)
                settingsManager.saveValue("curve.protocolKey", item.key)
        }

        function selectedProtocol() {
            if (selectedProtocolIndex < 0 || selectedProtocolIndex >= protocolChoiceModel.count)
                return { valid: false, cmdSet: -1, cmdId: -1 }

            var item = protocolChoiceModel.get(selectedProtocolIndex)
            return {
                valid: true,
                cmdSet: item.cmdSet,
                cmdId: item.cmdId
            }
        }

        function commandText(cmdSet, cmdId) {
            return "0x" + hexByteText(cmdSet) + " / 0x" + hexByteText(cmdId)
        }

        function hexByteText(value) {
            var text = Number(value || 0).toString(16).toUpperCase()
            return text.length < 2 ? ("0" + text) : text.slice(-2)
        }

        function frameDataBytes(frame) {
            if (!frame)
                return []

            var dataBytes = parseDataBytes(frame.data)
            var rawDataBytes = rawFrameUserDataBytes(frame.rawHex)
            return rawDataBytes.length > dataBytes.length ? rawDataBytes : dataBytes
        }

        function rawFrameCommandWord(rawHex) {
            var raw = parseDataBytes(rawHex)
            if (raw.length < 14 || raw[0] !== 0xaa)
                return -1

            return raw[12] | (raw[13] << 8)
        }

        function rawFrameUserDataBytes(rawHex) {
            var raw = parseDataBytes(rawHex)
            if (raw.length < 18 || raw[0] !== 0xaa)
                return []

            var payloadLen = raw[6] | (raw[7] << 8)
            var payloadStart = 10
            var dataStart = payloadStart + 6
            var dataEnd = payloadStart + payloadLen
            if (payloadLen < 6 || dataEnd > raw.length)
                return []

            return raw.slice(dataStart, dataEnd)
        }

        function appendCurve() {
            if (curveModel.count >= maxCurves)
                return

            curveModel.append({
                offset: Math.min(63, curveModel.count * 4),
                typeIndex: defaultCurveTypeIndex(),
                hidden: false
            })
            deferPlotRefresh()
        }

        function appendDefaultCurves() {
            var floatIndex = defaultCurveTypeIndex()
            curveModel.append({ offset: 0, typeIndex: floatIndex, hidden: false })
            curveModel.append({ offset: 4, typeIndex: floatIndex, hidden: false })
        }

        function defaultCurveTypeIndex() {
            var index = dataTypeOptions.indexOf("float LE")
            return index >= 0 ? index : 0
        }

        function removeCurve(index) {
            if (curveModel.count <= 1 || index < 0 || index >= curveModel.count)
                return

            curveModel.remove(index)
            deferPlotRefresh()
        }

        function curveColor(index) {
            return curvePalette[index % curvePalette.length]
        }

        function currentSecondsPerDiv() {
            var index = Math.max(0, Math.min(secondsPerDivOptions.length - 1,
                                             Math.round(secondsPerDivSlider.value)))
            return secondsPerDivOptions[index]
        }

        function secondsPerDivLabel(value) {
            return Number(value).toString() + "s/div"
        }

        function updateVisibleRange() {
            var allRange = sampleTimeRange(curveSamples)
            if (!isFinite(allRange[0]) || !isFinite(allRange[1])) {
                visibleStartTime = 0
                visibleEndTime = 0
                minValue = 0
                maxValue = 0
                return
            }

            var allEnd = allRange[1]
            var visibleSpan = Math.max(1, currentSecondsPerDiv() * 1000 * plotTimeDivisions)

            visibleEndTime = allEnd
            visibleStartTime = allEnd - visibleSpan
            visibleStartText = formatTimeMs(visibleStartTime)
            visibleEndText = formatTimeMs(visibleEndTime)

            var yRange = visibleValueRange(curveSamples, visibleStartTime, visibleEndTime)
            minValue = yRange[0]
            maxValue = yRange[1]
        }

        function visibleValueRange(seriesList, startTime, endTime) {
            var minVal = Number.POSITIVE_INFINITY
            var maxVal = Number.NEGATIVE_INFINITY
            var count = 0

            for (var s = 0; s < seriesList.length; s++) {
                var list = seriesList[s]
                if (!list)
                    continue

                for (var i = 0; i < list.length; i++) {
                    if (list[i].t < startTime || list[i].t > endTime)
                        continue

                    minVal = Math.min(minVal, list[i].v)
                    maxVal = Math.max(maxVal, list[i].v)
                    count++
                }
            }

            if (count > 0)
                return [minVal, maxVal]

            for (var fs = 0; fs < seriesList.length; fs++) {
                var fullList = seriesList[fs]
                if (!fullList)
                    continue

                for (var fp = 0; fp < fullList.length; fp++) {
                    minVal = Math.min(minVal, fullList[fp].v)
                    maxVal = Math.max(maxVal, fullList[fp].v)
                }
            }

            return isFinite(minVal) && isFinite(maxVal) ? [minVal, maxVal] : [0, 0]
        }

        function totalSampleCount(seriesList) {
            var total = 0
            for (var i = 0; i < seriesList.length; i++)
                total += seriesList[i] ? seriesList[i].length : 0
            return total
        }

        function sampleTimeRange(seriesList) {
            var minTime = Number.POSITIVE_INFINITY
            var maxTime = Number.NEGATIVE_INFINITY
            for (var s = 0; s < seriesList.length; s++) {
                var list = seriesList[s]
                if (!list || list.length === 0)
                    continue

                minTime = Math.min(minTime, list[0].t)
                maxTime = Math.max(maxTime, list[list.length - 1].t)
            }
            return [minTime, maxTime]
        }

        function timeTextForValue(seriesList, timeValue) {
            for (var s = 0; s < seriesList.length; s++) {
                var list = seriesList[s]
                if (!list)
                    continue

                for (var i = 0; i < list.length; i++) {
                    if (list[i].t === timeValue)
                        return list[i].timeText || ""
                }
            }
            return ""
        }

        function parseDataBytes(text) {
            var clean = String(text || "").replace(/0x/gi, " ").replace(/[^0-9A-Fa-f]/g, " ").trim()
            if (clean.length === 0)
                return []

            var parts = clean.split(/\s+/)
            if (parts.length === 1 && parts[0].length > 2) {
                var compact = parts[0]
                parts = []
                for (var i = 0; i < compact.length - 1; i += 2)
                    parts.push(compact.slice(i, i + 2))
            }

            var bytes = []
            for (var j = 0; j < parts.length; j++) {
                var byteValue = parseInt(parts[j], 16)
                if (isFinite(byteValue))
                    bytes.push(byteValue & 0xff)
            }
            return bytes
        }

        function readBytes(bytes, offset, typeText) {
            var type = normalizeDataType(typeText)
            var byteCount = dataTypeSize(type)
            if (offset < 0 || offset + byteCount > bytes.length)
                return NaN

            if (type === "uint8")
                return bytes[offset]
            if (type === "int8")
                return bytes[offset] >= 128 ? bytes[offset] - 256 : bytes[offset]
            if (type === "uint16")
                return readUIntLE(bytes, offset, 2)
            if (type === "int16")
                return toSigned(readUIntLE(bytes, offset, 2), 16)
            if (type === "uint32")
                return readUInt32LE(bytes, offset)
            if (type === "int32")
                return toSigned(readUInt32LE(bytes, offset), 32)
            if (type === "uint64")
                return readUInt64LE(bytes, offset)
            if (type === "int64")
                return readInt64LE(bytes, offset)
            if (type === "floatle")
                return readFloat32LE(bytes, offset)
            if (type === "doublele")
                return readFloat64LE(bytes, offset)

            return NaN
        }

        function normalizeDataType(typeText) {
            return String(typeText || "uint8").replace(/\s+/g, "").toLowerCase()
        }

        function dataTypeSize(typeText) {
            var type = normalizeDataType(typeText)
            if (type === "uint64" || type === "int64" || type === "doublele")
                return 8
            if (type === "uint32" || type === "int32" || type === "floatle")
                return 4
            if (type === "uint16" || type === "int16")
                return 2
            return 1
        }

        function readUIntLE(bytes, offset, byteCount) {
            var value = 0
            var mul = 1
            for (var i = 0; i < byteCount; i++) {
                value += bytes[offset + i] * mul
                mul *= 256
            }
            return value
        }

        function readUInt32LE(bytes, offset) {
            return bytes[offset]
                 + bytes[offset + 1] * 256
                 + bytes[offset + 2] * 65536
                 + bytes[offset + 3] * 16777216
        }

        function readUInt64LE(bytes, offset) {
            var low = readUInt32LE(bytes, offset)
            var high = readUInt32LE(bytes, offset + 4)
            return high * 4294967296 + low
        }

        function readInt64LE(bytes, offset) {
            var low = readUInt32LE(bytes, offset)
            var high = readUInt32LE(bytes, offset + 4)
            var value = high * 4294967296 + low
            if (high >= 2147483648)
                value -= 18446744073709551616
            return value
        }

        function toSigned(value, bits) {
            var sign = Math.pow(2, bits - 1)
            var full = Math.pow(2, bits)
            return value >= sign ? value - full : value
        }

        function readFloat32LE(bytes, offset) {
            var bits = readUInt32LE(bytes, offset)
            var sign = bits >= 2147483648 ? -1 : 1
            var exponent = Math.floor(bits / 8388608) % 256
            var fraction = bits % 8388608
            if (exponent === 255)
                return fraction === 0 ? sign * Infinity : NaN
            if (exponent === 0)
                return sign * Math.pow(2, -126) * (fraction / 8388608)
            return sign * Math.pow(2, exponent - 127) * (1 + fraction / 8388608)
        }

        function readFloat64LE(bytes, offset) {
            var low = readUInt32LE(bytes, offset)
            var high = readUInt32LE(bytes, offset + 4)
            var sign = high >= 2147483648 ? -1 : 1
            var exponent = Math.floor(high / 1048576) % 2048
            var fractionHigh = high % 1048576
            var fraction = fractionHigh * 4294967296 + low
            if (exponent === 2047)
                return fraction === 0 ? sign * Infinity : NaN
            if (exponent === 0)
                return sign * Math.pow(2, -1022) * (fraction / 4503599627370496)
            return sign * Math.pow(2, exponent - 1023) * (1 + fraction / 4503599627370496)
        }

        function timestampMs(text, fallbackIndex) {
            var match = /^(\d+):(\d+):(\d+)(?:\.(\d+))?/.exec(String(text || ""))
            if (!match)
                return fallbackIndex

            var ms = match[4] ? Number((match[4] + "000").slice(0, 3)) : 0
            return (Number(match[1]) * 3600 + Number(match[2]) * 60 + Number(match[3])) * 1000 + ms
        }

        function formatTimeMs(value) {
            if (!isFinite(value))
                return ""

            var totalMs = Math.max(0, Math.round(value))
            var ms = totalMs % 1000
            var totalSeconds = Math.floor(totalMs / 1000)
            var seconds = totalSeconds % 60
            var totalMinutes = Math.floor(totalSeconds / 60)
            var minutes = totalMinutes % 60
            var hours = Math.floor(totalMinutes / 60) % 24
            return ("0" + hours).slice(-2)
                    + ":" + ("0" + minutes).slice(-2)
                    + ":" + ("0" + seconds).slice(-2)
                    + "." + ("00" + ms).slice(-3)
        }

        function formatNumber(value) {
            if (!isFinite(value))
                return "--"
            if (Math.abs(value) >= 1000 || (value !== 0 && Math.abs(value) < 0.01))
                return Number(value).toExponential(2)
            return Number(value).toFixed(Math.abs(value) >= 100 ? 1 : 2).replace(/\.?0+$/, "")
        }

        function shortTime(text) {
            var value = String(text || "")
            return value.length > 8 ? value.slice(0, 12) : value
        }
    }

    component CompactLabel: Text {
        color: root.textSecondary
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignRight
    }

    component CurveTag: Rectangle {
        property alias text: tagText.text
        property color accentColor: root.accent
        width: 28
        height: 28
        radius: 6
        color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.15)
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.58)

        Text {
            id: tagText
            anchors.centerIn: parent
            color: root.textPrimary
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }
    }

    component ValueBadge: Rectangle {
        property string label: ""
        property string value: ""
        property color accentColor: root.accent
        Layout.preferredWidth: 76
        Layout.preferredHeight: 28
        radius: 6
        color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.13)
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.38)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            spacing: 4

            Text {
                text: label
                color: root.textSecondary
                font.pixelSize: 10
                Layout.preferredWidth: 24
                elide: Text.ElideRight
            }
            Text {
                text: value
                color: root.textPrimary
                font.pixelSize: 11
                font.family: "Consolas"
                font.weight: Font.DemiBold
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
    }
}
