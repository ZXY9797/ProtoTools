import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../Widgets" as W

Rectangle {
    id: root
    color: pageBg

    property int currentTab: 0  // 0=固件升级, 1=协议编辑, 2=快捷指令, 3=协议校验
    property bool _restoring: true
    property int controlHeight: 26
    property int upgradeActionButtonWidth: 72
    property int compactAddressFieldWidth: 68
    property int compactCountFieldWidth: 58
    property int compactBrowseButtonWidth: 32
    property real requiredMinimumHeight: Math.ceil(36 + upgradeScroll.topPadding + upgradeContentColumn.implicitHeight + upgradeScroll.bottomPadding)
    property var protocolCheckResult: ({})
    property bool protocolCheckHasInput: false
    property bool protocolCheckOk: false
    property string protocolCheckError: ""
    property var responsePending: ({})
    property var editorResponseFrame: ({})
    property string editorResponseStatus: "发送协议后显示完整回包"
    property var quickResponseFrame: ({})
    property string quickResponseStatus: "发送快捷指令后显示当前回包"
    readonly property int responseTimeoutMs: 3000
    property color pageBg: themeManager.isDark ? "#0C1017" : "#F8F9FB"
    property color surface: themeManager.isDark ? "#131920" : "#FFFFFF"
    property color surfaceRaised: themeManager.isDark ? "#1A222C" : "#F3F4F6"
    property color surfaceSoft: themeManager.isDark ? "#0F141B" : "#F9FAFB"
    property color outline: themeManager.isDark ? "#253040" : "#D1D5DB"
    property color outlineStrong: themeManager.isDark ? "#384858" : "#9CA3AF"
    property color textPrimary: themeManager.isDark ? "#E8EDF5" : "#111827"
    property color textSecondary: themeManager.isDark ? "#9CAAB8" : "#6B7280"
    property color textMuted: themeManager.isDark ? "#607080" : "#9CA3AF"
    property color accent: themeManager.isDark ? "#3B8AFF" : "#2563EB"
    property color accentHover: themeManager.isDark ? "#509AFF" : "#3B82F6"
    property color success: themeManager.isDark ? "#34D399" : "#059669"
    property color warning: themeManager.isDark ? "#FBBF24" : "#D97706"
    property color danger: themeManager.isDark ? "#F87171" : "#DC2626"

    function loadTextValue(key, defaultValue) {
        var value = settingsManager.loadValue(key, defaultValue)
        if (value === undefined || value === null)
            return defaultValue
        var text = value.toString()
        return text.length > 0 ? displayHexText(text) : defaultValue
    }

    function displayHexText(value) {
        return String(value || "").replace(/0X/g, "0x")
    }

    // 表单参数保存
    Component.onCompleted: {
        srcField.text = loadTextValue("protoeditor.src", "0x10")
        dstField.text = loadTextValue("protoeditor.dst", "0x05")
        seqField.text = loadTextValue("protoeditor.seq", "0005")
        cmdsetField.text = loadTextValue("protoeditor.cmdset", "0x00")
        cmdidField.text = loadTextValue("protoeditor.cmdid", "0x01")
        dataField.text = loadTextValue("protoeditor.data", "01 02 03 04 05 06 07 08")
        typeCombo.currentIndex = settingsManager.loadValue("protoeditor.type", 0)
        currentTab = 0
        _restoring = false
        updatePreview()
    }

    ListModel { id: historyModel }

    Timer {
        id: editorResponseTimeout
        interval: root.responseTimeoutMs
        repeat: false
        onTriggered: root.handleResponseTimeout("editor")
    }

    Timer {
        id: quickResponseTimeout
        interval: root.responseTimeoutMs
        repeat: false
        onTriggered: root.handleResponseTimeout("quick")
    }

    // ========== 历史弹出列表 ==========
    Popup {
        id: historyPopup
        parent: root
        x: root.width - width - 14
        y: root.height - 36 - height
        width: Math.max(400, root.width * 0.55)
        height: Math.min(300, historyModel.count * 20 + 44)
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: root.surfaceRaised
            border.color: root.outlineStrong
            border.width: 1
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 24; color: surface
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 6
                    Text { text: "发送历史（点击填入表单）"; color: root.accentHover; font.pixelSize: 11; font.weight: Font.Medium }
                    Item { Layout.fillWidth: true }
                    Text { text: historyModel.count + " 条"; color: textMuted; font.pixelSize: 10 }
                    Rectangle {
                        width: 42; height: 18; radius: 5
                        color: popClearMouse.containsMouse ? (themeManager.isDark ? "#7F1D1D" : "#FEE2E2") : (themeManager.isDark ? "#2D1520" : "#FEF2F2")
                        border.color: popClearMouse.containsMouse ? danger : (themeManager.isDark ? "#5B2135" : "#FECDD3")
                        Text { anchors.centerIn: parent; text: "清空"; color: popClearMouse.containsMouse ? (themeManager.isDark ? "#FECDD3" : "#DC2626") : (themeManager.isDark ? "#FECDD3" : "#6B7280"); font.pixelSize: 10; font.weight: Font.Medium }
                        MouseArea { id: popClearMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { historyModel.clear(); historyPopup.close() } }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 18; color: surfaceSoft
                Row { anchors.fill: parent; spacing: 0
                    PopHdr { text: "#"; width: 24 } PopHdr { text: "时间"; width: 56 }
                    PopHdr { text: "Src"; width: 50 } PopHdr { text: "Dst"; width: 50 }
                    PopHdr { text: "Seq"; width: 36 } PopHdr { text: "Set"; width: 36 }
                    PopHdr { text: "ID"; width: 36 } PopHdr { text: "Data"; width: Math.max(50, historyPopup.width - 294) }
                }
            }
            ListView {
                id: popupList; Layout.fillWidth: true; Layout.fillHeight: true
                clip: true; model: historyModel; currentIndex: -1
                delegate: Rectangle {
                    width: popupList.width; height: 20
                    color: popMouse.containsMouse ? (themeManager.isDark ? "#1A3050" : "#DBEAFE") : (index % 2 === 0 ? root.surfaceRaised : root.surfaceSoft)
                    Row { anchors.fill: parent; spacing: 0
                        PopCell { text: (index + 1).toString(); width: 24; color: textMuted }
                        PopCell { text: model.timestamp || ""; width: 56; color: textMuted }
                        PopCell { text: root.displayHexText(model.src || ""); width: 50; color: themeManager.isDark ? "#9CDCFE" : "#3B82F6" }
                        PopCell { text: root.displayHexText(model.dst || ""); width: 50; color: themeManager.isDark ? "#9CDCFE" : "#3B82F6" }
                        PopCell { text: model.seq || ""; width: 36; color: themeManager.isDark ? "#569CD6" : "#60A5FA" }
                        PopCell { text: root.displayHexText(model.cmdset || ""); width: 36; color: themeManager.isDark ? "#C586C0" : "#8B5CF6" }
                        PopCell { text: root.displayHexText(model.cmdid || ""); width: 36; color: themeManager.isDark ? "#DCDCAA" : "#F59E0B" }
                        PopCell { text: root.displayHexText(model.data || ""); width: Math.max(50, popupList.width - 294); color: textPrimary; elide: Text.ElideRight }
                    }
                    MouseArea {
                        id: popMouse; anchors.fill: parent; hoverEnabled: true
                        onClicked: {
                            fillEditor(model.src, model.dst, model.seq, model.cmdset, model.cmdid, model.data)
                            historyPopup.close()
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: addQuickCmdPopup
        parent: root
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(360, root.width - 24)
        x: Math.max(12, (root.width - width) / 2)
        y: Math.max(52, (root.height - height) / 2)
        padding: 0

        background: Rectangle {
            color: root.surfaceRaised
            border.color: root.outlineStrong
            border.width: 1
            radius: 10
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 10
                color: "#0F1620"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "加入快捷指令"
                    color: root.textPrimary
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                spacing: 8

                Text {
                    text: "名称"
                    color: root.textSecondary
                    font.pixelSize: 10
                    font.weight: Font.Medium
                }
                TextField {
                    id: quickNameField
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredWidth: 1
                    implicitHeight: 30
                    selectByMouse: true
                    color: root.textPrimary
                    selectedTextColor: "#FFFFFF"
                    selectionColor: root.accentHover
                    font.pixelSize: 12
                    background: Rectangle {
                        color: root.surfaceSoft
                        border.color: quickNameField.activeFocus ? root.accentHover : root.outlineStrong
                        radius: 6
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 8
                    rowSpacing: 6
                    AddQuickLabel { text: "Src" }
                    AddQuickValue { text: normalizedEditorCommand().src }
                    AddQuickLabel { text: "Dst" }
                    AddQuickValue { text: normalizedEditorCommand().dst }
                    AddQuickLabel { text: "Set" }
                    AddQuickValue { text: normalizedEditorCommand().cmdset }
                    AddQuickLabel { text: "ID" }
                    AddQuickValue { text: normalizedEditorCommand().cmdid }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: Math.max(34, quickDataText.implicitHeight + 14)
                    radius: 6
                    color: root.surface
                    border.color: root.outline

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8
                        anchors.topMargin: 7
                        anchors.bottomMargin: 7
                        spacing: 8
                        Text {
                            text: "Data"
                            color: root.textMuted
                            font.pixelSize: 10
                            Layout.preferredWidth: 32
                            Layout.alignment: Qt.AlignTop
                        }
                        Text {
                            id: quickDataText
                            Layout.fillWidth: true
                            text: normalizedEditorCommand().data.length > 0 ? normalizedEditorCommand().data : "无数据"
                            color: normalizedEditorCommand().data.length > 0 ? root.textSecondary : root.textMuted
                            font.pixelSize: 10
                            font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "取消"
                        implicitWidth: 62
                        implicitHeight: 28
                        background: Rectangle { radius: 6; color: root.surfaceSoft; border.color: root.outlineStrong }
                        contentItem: Text { text: parent.text; color: root.textSecondary; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: addQuickCmdPopup.close()
                    }
                    Button {
                        text: "添加"
                        implicitWidth: 66
                        implicitHeight: 28
                        background: Rectangle { radius: 6; color: parent.hovered ? root.accentHover : root.accent }
                        contentItem: Text { text: parent.text; color: "#FFFFFF"; font.pixelSize: 11; font.weight: Font.Medium; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                        onClicked: addCurrentEditorToQuickCommands()
                    }
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ========== Tab Bar ==========
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: surface

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                spacing: 6

                ToolTab { label: "固件升级"; tabIndex: 0; accentColor: themeManager.isDark ? "#48D1B5" : "#059669" }
                ToolTab { label: "协议编辑"; tabIndex: 1; accentColor: themeManager.isDark ? "#7AB7FF" : "#3B82F6" }
                ToolTab { label: "快捷指令"; tabIndex: 2; accentColor: themeManager.isDark ? "#C58AF9" : "#8B5CF6" }
                ToolTab { label: "协议校验"; tabIndex: 3; accentColor: themeManager.isDark ? "#D4A83B" : "#D97706" }
                Item { Layout.fillWidth: true }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: outline
            }
        }

        // ========== Tab 1: 协议编辑 ==========
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            visible: root.currentTab === 1

            ColumnLayout {
                anchors.fill: parent; spacing: 0

                SplitView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    orientation: Qt.Vertical

                    handle: Rectangle {
                        implicitHeight: 4
                        color: SplitHandle.pressed ? root.accent : (SplitHandle.hovered ? root.accent : root.outline)
                    }

                    // ---- 表单区 ----
                    ColumnLayout {
                        id: editorFormPane
                        SplitView.fillWidth: true
                        SplitView.minimumHeight: Math.max(168, editorFormContent.implicitHeight + 14)
                        SplitView.preferredHeight: editorFormContent.implicitHeight + 14
                        spacing: 0

                        ColumnLayout {
                            id: editorFormContent
                            Layout.fillWidth: true
                            Layout.leftMargin: 7; Layout.rightMargin: 7; Layout.topMargin: 6; Layout.bottomMargin: 6
                            spacing: 4

                            GridLayout {
                                Layout.fillWidth: true
                                columns: 4
                                columnSpacing: 7
                                rowSpacing: 5

                                ProtoLabel { text: "Src" }
                                ProtoTextField {
                                    id: srcField
                                    text: "0x10"
                                    onTextChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.src", text) }
                                }
                                ProtoLabel { text: "Dst" }
                                ProtoTextField {
                                    id: dstField
                                    text: "0x05"
                                    onTextChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.dst", text) }
                                }

                                ProtoLabel { text: "Seq" }
                                ProtoTextField {
                                    id: seqField
                                    text: "0005"
                                    onTextChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.seq", text) }
                                }
                                ProtoLabel { text: "Type" }
                                ProtoCombo {
                                    id: typeCombo
                                    model: ["REQ", "ACK"]
                                    onCurrentIndexChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.type", currentIndex) }
                                }

                                ProtoLabel { text: "Set" }
                                ProtoTextField {
                                    id: cmdsetField
                                    text: "0x01"
                                    onTextChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.cmdset", text) }
                                }
                                ProtoLabel { text: "ID" }
                                ProtoTextField {
                                    id: cmdidField
                                    text: "0xA0"
                                    onTextChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.cmdid", text) }
                                }
                            }

                            // Data
                            RowLayout {
                                Layout.fillWidth: true; spacing: 7
                                ProtoLabel {
                                    text: "Data"
                                    Layout.alignment: Qt.AlignTop
                                    topPadding: 4
                                    verticalAlignment: Text.AlignTop
                                }
                                ScrollView {
                                    Layout.fillWidth: true; Layout.preferredHeight: 38; clip: true
                                    background: Rectangle { color: root.surfaceSoft; border.color: root.outlineStrong; radius: 5 }
                                    TextArea {
                                        id: dataField; text: "01 02 03 04 05 06 07 08"
                                        font.pixelSize: 10; font.family: "Menlo"
                                        color: textPrimary; wrapMode: TextArea.Wrap; selectByMouse: true; background: null
                                        onTextChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.data", text) }
                                    }
                                }
                            }

                            // CRC
                            RowLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: "BCC"; color: "#AAAAAA"; font.pixelSize: 11; Layout.preferredWidth: 36; verticalAlignment: Text.AlignVCenter }
                                Text { id: bccDisplay; text: "auto"; color: textMuted; font.pixelSize: 10; font.family: "Menlo" }
                                Item { width: 8; height: 1 }
                                Text { text: "CRC16"; color: "#AAAAAA"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                                Text { id: crc16Display; text: "auto"; color: textMuted; font.pixelSize: 10; font.family: "Menlo" }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "加入快捷"
                                    Layout.preferredWidth: 70
                                    implicitHeight: 22
                                    leftPadding: 0
                                    rightPadding: 0
                                    font.pixelSize: 9
                                    font.weight: Font.Medium
                                    background: Rectangle {
                                        radius: 6
                                        color: parent.hovered ? accent : (themeManager.isDark ? "#1C3147" : "#DBEAFE")
                                        border.color: parent.hovered ? (themeManager.isDark ? "#5DAAFF" : "#93C5FD") : (themeManager.isDark ? "#31506D" : "#93C5FD")
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: parent.hovered ? "#FFFFFF" : (themeManager.isDark ? "#BFD7F2" : "#1D4ED8")
                                        font: parent.font
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }
                                    onClicked: openAddQuickCommandPopup()
                                }
                            }

                            // Hex 预览
                            ColumnLayout {
                                Layout.fillWidth: true; Layout.topMargin: 1; spacing: 2
                                Text { text: "预览 Hex (可选中复制)"; color: textMuted; font.pixelSize: 9 }
                                ScrollView {
                                    id: hexPreviewScroll
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 50
                                    Layout.minimumHeight: 46
                                    Layout.maximumHeight: 56
                                    clip: true
                                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                    background: Rectangle { color: surface; border.color: root.outline; radius: 5 }
                                    TextArea {
                                        id: hexPreview; text: ""; readOnly: true; selectByMouse: true
                                        color: success; font.pixelSize: 9; font.family: "Menlo"
                                        width: hexPreviewScroll.availableWidth
                                        wrapMode: TextArea.Wrap
                                        leftPadding: 8
                                        rightPadding: 8
                                        topPadding: 5
                                        bottomPadding: 5
                                        background: null
                                    }
                                }
                            }

                        }
                    }

                    // ---- 完整回包区 ----
                    W.ResponseFrameBox {
                        SplitView.fillWidth: true
                        SplitView.minimumHeight: 108
                        SplitView.preferredHeight: 150
                        title: "完整回包"
                        placeholderText: root.editorResponseStatus
                        frame: root.editorResponseFrame
                        accentColor: root.accentHover
                        surface: root.surface
                        surfaceRaised: root.surfaceRaised
                        surfaceSoft: root.surfaceSoft
                        outline: root.outline
                        outlineStrong: root.outlineStrong
                        textPrimary: root.textPrimary
                        textSecondary: root.textSecondary
                        textMuted: root.textMuted
                        successColor: root.success
                        dangerColor: root.danger
                    }
                }

                // ---- 底部发送栏 ----
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 30; color: surface
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6
                        Button {
                            text: "📤 发送"; font.pixelSize: 11; padding: 3; leftPadding: 14; rightPadding: 14
                            background: Rectangle { radius: 3; color: themeManager.isDark ? "#0E639C" : "#2563EB" }
                            contentItem: Text { text: parent.text; color: "#FFFFFF"; font: parent.font }
                            onClicked: sendFrame()
                        }
                        Text { id: lastSendInfo; text: "最近: —"; color: textMuted; font.pixelSize: 10 }
                        Item { Layout.fillWidth: true }
                        Button {
                            id: historyBtn
                            text: "📜 历史 (" + historyModel.count + ")"; font.pixelSize: 10; padding: 2; leftPadding: 8; rightPadding: 8
                            background: Rectangle { radius: 5; color: historyPopup.visible ? "#20344B" : root.surfaceSoft; border.color: root.outlineStrong }
                            contentItem: Text { text: parent.text; color: historyModel.count > 0 ? "#CCCCCC" : "#555555"; font: parent.font }
                            onClicked: { if (historyModel.count > 0) historyPopup.visible = !historyPopup.visible }
                        }
                        Item { width: 2 }
                    }
                }
            }
        }

        // ========== Tab 2: 快捷指令 ==========
        W.QuickCmd {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentTab === 2
            responseFrame: root.quickResponseFrame
            responseStatusText: root.quickResponseStatus

            onSend: function(cmd) {
                sendQuickCommand(cmd)
            }
            onEdit: function(cmd) {
                fillEditor(cmd.src, cmd.dst, cmd.seq, cmd.cmdset, cmd.cmdid, cmd.data)
                root.currentTab = 1
            }
        }

        // ========== Tab 3: 协议校验 ==========
        ScrollView {
            id: protocolCheckScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentTab === 3
            clip: true
            leftPadding: 8
            rightPadding: 8
            topPadding: 8
            bottomPadding: 8

            ColumnLayout {
                width: protocolCheckScroll.availableWidth
                spacing: 8

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 4

                    Text {
                        text: "输入协议二进制"
                        color: "#C9D1D9"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        Layout.maximumHeight: 50
                        clip: true
                        background: Rectangle {
                            color: surface
                            border.color: protocolCheckInput.activeFocus ? accent : outline
                            radius: 6
                        }
                        TextArea {
                            id: protocolCheckInput
                            color: textPrimary
                            selectionColor: accent
                            selectedTextColor: "#FFFFFF"
                            placeholderText: "AA 01 ..."
                            placeholderTextColor: textMuted
                            font.pixelSize: 11
                            font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                            wrapMode: TextArea.Wrap
                            selectByMouse: true
                            background: null
                            onTextChanged: validateProtocolInput()
                            Component.onCompleted: validateProtocolInput()
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Button {
                        id: protocolCheckButton
                        text: "解析"
                        implicitHeight: 28
                        Layout.preferredWidth: 86
                        onClicked: validateProtocolInput()
                        contentItem: Text {
                            text: protocolCheckButton.text
                            color: "#FFFFFF"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: protocolCheckButton.down ? (themeManager.isDark ? "#1D5E99" : "#1D4ED8") : (protocolCheckButton.hovered ? (themeManager.isDark ? "#2878C6" : "#3B82F6") : accent)
                            border.color: themeManager.isDark ? "#55A9F8" : "#93C5FD"
                        }
                    }
                    Button {
                        id: protocolClearButton
                        text: "清空"
                        implicitHeight: 28
                        Layout.preferredWidth: 72
                        onClicked: protocolCheckInput.clear()
                        contentItem: Text {
                            text: protocolClearButton.text
                            color: textPrimary
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: protocolClearButton.down ? (themeManager.isDark ? "#303944" : "#E5E7EB") : (protocolClearButton.hovered ? (themeManager.isDark ? "#35404C" : "#F3F4F6") : surface)
                            border.color: outlineStrong
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: protocolCheckStatus.implicitWidth + 14
                        implicitHeight: 22
                        radius: 5
                        color: !root.protocolCheckHasInput ? surfaceRaised : (root.protocolCheckOk ? (themeManager.isDark ? "#123F36" : "#ECFDF5") : (themeManager.isDark ? "#3A1F24" : "#FEE2E2"))
                        border.color: !root.protocolCheckHasInput ? outlineStrong : (root.protocolCheckOk ? (themeManager.isDark ? "#2C927D" : "#059669") : (themeManager.isDark ? "#A04452" : "#DC2626"))
                        Text {
                            id: protocolCheckStatus
                            anchors.centerIn: parent
                            text: !root.protocolCheckHasInput ? "等待输入" : (root.protocolCheckOk ? "协议正确" : "协议错误")
                            color: !root.protocolCheckHasInput ? textMuted : (root.protocolCheckOk ? success : danger)
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.protocolCheckError.length > 0
                    text: root.protocolCheckError
                    color: root.protocolCheckOk ? "#8FA2B6" : "#FF8C9B"
                    font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 5
                    visible: root.protocolCheckHasInput

                    CheckCard { label: "Src"; value: checkValue("sender") }
                    CheckCard { label: "Dst"; value: checkValue("receiver") }

                    CheckCard { label: "Seq"; value: checkValue("seq") }
                    CheckCard { label: "Type"; value: checkValue("type") }

                    CheckCard { label: "CmdSet"; value: checkValue("cmdSet") }
                    CheckCard { label: "CmdID"; value: checkValue("cmdId") }

                    CheckCard { label: "CmdType"; value: checkValue("cmdType") }
                    CheckCard { label: "Payload"; value: checkValue("payloadLen") }

                    CheckCard {
                        label: "BCC"
                        value: checkPairColoredValue("headCrc", "headCrcCalc")
                        richValue: true
                    }
                    CheckCard {
                        label: "CRC16"
                        value: checkPairColoredValue("crc", "crcCalc")
                        richValue: true
                    }

                    CheckCard {
                        Layout.columnSpan: 2
                        label: "Data"
                        value: checkValue("data")
                        wrapValue: true
                    }
                }
            }
        }

        ScrollView {
            id: upgradeScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.currentTab === 0
            clip: true
            leftPadding: 8
            rightPadding: 8
            topPadding: 8
            bottomPadding: 8
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                id: upgradeContentColumn
                width: upgradeScroll.availableWidth
                spacing: 8

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: compactUpgradeConfig.implicitHeight + 14
                    radius: 8
                    color: root.surfaceRaised
                    border.color: root.outline

                    ColumnLayout {
                        id: compactUpgradeConfig
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 7
                        spacing: 7

                        Text {
                            text: "目标固件"
                            color: root.textPrimary
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 28

                            TextField {
                                id: compactFirmwarePathField
                                anchors.left: parent.left
                                anchors.right: browseFirmwareButton.left
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                height: 28
                                implicitHeight: 28
                                leftPadding: 8
                                rightPadding: 8
                                clip: true
                                font.pixelSize: 11
                                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                                color: root.textPrimary
                                text: firmwareUpgrade.firmwarePath
                                enabled: !firmwareUpgrade.running
                                placeholderText: "Firmware"
                                placeholderTextColor: root.textMuted
                                selectByMouse: true
                                background: Rectangle {
                                    color: root.surfaceSoft
                                    border.color: compactFirmwarePathField.activeFocus ? root.accent : root.outlineStrong
                                    radius: 6
                                }
                                onTextEdited: firmwareUpgrade.firmwarePath = text
                                Connections {
                                    target: firmwareUpgrade
                                    function onFirmwarePathChanged() {
                                        if (!compactFirmwarePathField.activeFocus)
                                            compactFirmwarePathField.text = firmwareUpgrade.firmwarePath
                                    }
                                }
                            }

                            Button {
                                id: browseFirmwareButton
                                text: "..."
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: root.compactBrowseButtonWidth
                                height: 28
                                implicitWidth: root.compactBrowseButtonWidth
                                implicitHeight: 28
                                padding: 0
                                enabled: !firmwareUpgrade.running
                                onClicked: firmwareUpgrade.browseFirmware(compactFirmwarePathField.text)
                                contentItem: Item {
                                    implicitWidth: root.compactBrowseButtonWidth
                                    implicitHeight: 28

                                    Row {
                                        anchors.centerIn: parent
                                        spacing: 2

                                        Repeater {
                                            model: 3
                                            Rectangle {
                                                width: 3
                                                height: 3
                                                radius: 1.5
                                                color: browseFirmwareButton.enabled ? root.textSecondary : root.textMuted
                                                opacity: browseFirmwareButton.enabled ? 0.82 : 0.4
                                            }
                                        }
                                    }
                                }
                                background: Rectangle {
                                    radius: 7
                                    color: !browseFirmwareButton.enabled ? "#151B22"
                                        : (browseFirmwareButton.hovered ? "#243447" : root.surfaceSoft)
                                    border.color: browseFirmwareButton.activeFocus ? root.accentHover
                                        : (browseFirmwareButton.hovered ? root.accent : root.outlineStrong)
                                    border.width: 1
                                }
                            }
                        }

                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 26

                            RowLayout {
                                anchors.left: parent.left
                                anchors.right: stressCountLabel.left
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                height: 26
                                clip: true
                                spacing: 5

                                Text { text: "Src/Dst"; color: root.textSecondary; font.pixelSize: 10; Layout.alignment: Qt.AlignVCenter }
                            TextField {
                                id: compactSrcField
                                Layout.minimumWidth: root.compactAddressFieldWidth
                                Layout.preferredWidth: root.compactAddressFieldWidth
                                Layout.maximumWidth: root.compactAddressFieldWidth
                                implicitHeight: 26
                                leftPadding: 5
                                rightPadding: 5
                                font.pixelSize: 11
                                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                                text: settingsManager.loadValue("upgrade.src", "0x10")
                                color: root.textPrimary
                                enabled: !firmwareUpgrade.running
                                background: Rectangle { color: root.surfaceSoft; border.color: compactSrcField.activeFocus ? root.accent : root.outlineStrong; radius: 6 }
                                onTextEdited: { firmwareUpgrade.srcAddr = parseHexField(text); settingsManager.saveValue("upgrade.src", text) }
                                Component.onCompleted: firmwareUpgrade.srcAddr = parseHexField(text)
                            }
                            TextField {
                                id: compactDstField
                                Layout.minimumWidth: root.compactAddressFieldWidth
                                Layout.preferredWidth: root.compactAddressFieldWidth
                                Layout.maximumWidth: root.compactAddressFieldWidth
                                implicitHeight: 26
                                leftPadding: 5
                                rightPadding: 5
                                font.pixelSize: 11
                                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                                text: settingsManager.loadValue("upgrade.dst", "0x05")
                                color: root.textPrimary
                                enabled: !firmwareUpgrade.running
                                background: Rectangle { color: root.surfaceSoft; border.color: compactDstField.activeFocus ? root.accent : root.outlineStrong; radius: 6 }
                                onTextEdited: { firmwareUpgrade.dstAddr = parseHexField(text); settingsManager.saveValue("upgrade.dst", text) }
                                Component.onCompleted: firmwareUpgrade.dstAddr = parseHexField(text)
                            }
                            }
                            Text {
                                id: stressCountLabel
                                text: "压测次数"
                                anchors.right: compactStressCountField.left
                                anchors.rightMargin: 5
                                anchors.verticalCenter: parent.verticalCenter
                                color: root.textSecondary
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignRight
                            }
                            TextField {
                                id: compactStressCountField
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                width: root.compactCountFieldWidth
                                height: 26
                                implicitHeight: 26
                                leftPadding: 5
                                rightPadding: 5
                                font.pixelSize: 11
                                text: settingsManager.loadValue("upgrade.stressCount", 10).toString()
                                color: root.textPrimary
                                enabled: !firmwareUpgrade.running
                                maximumLength: 4
                                validator: IntValidator { bottom: 1; top: 9999 }
                                horizontalAlignment: Text.AlignHCenter
                                background: Rectangle { color: root.surfaceSoft; border.color: compactStressCountField.activeFocus ? root.accent : root.outlineStrong; radius: 6 }
                                onTextEdited: settingsManager.saveValue("upgrade.stressCount", Math.max(1, Math.min(9999, parseInt(text) || 1)))
                                Component.onCompleted: {
                                    var v = Math.max(1, Math.min(9999, parseInt(text) || 10))
                                    text = v.toString()
                                    settingsManager.saveValue("upgrade.stressCount", v)
                                }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        id: compactProgressPanel
                        Layout.fillWidth: true
                        implicitHeight: 60
                        radius: 10
                        color: themeManager.isDark ? "#18212A" : "#FFFFFF"
                        border.color: firmwareUpgrade.running ? (themeManager.isDark ? "#376FA7" : "#3B82F6") : root.outline
                        property real percent: Math.max(0, Math.min(100, firmwareUpgrade.progress))

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 8

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Text { text: "升级进度"; color: root.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }

                                // 圆形进度指示器
                                Rectangle {
                                    implicitWidth: 28
                                    implicitHeight: 28
                                    radius: 14
                                    color: "transparent"
                                    border.width: 3
                                    border.color: themeManager.isDark ? "#253040" : "#E5E7EB"

                                    Rectangle {
                                        width: 28
                                        height: 28
                                        radius: 14
                                        color: "transparent"
                                        border.width: 3
                                        border.color: firmwareUpgrade.running ? accent : (themeManager.isDark ? "#384858" : "#D1D5DB")
                                    }

                                    // 进度弧形效果（用遮罩实现）
                                    Rectangle {
                                        width: 28
                                        height: 28
                                        radius: 14
                                        color: "transparent"
                                        border.width: 3
                                        border.color: firmwareUpgrade.running ? accent : "transparent"
                                        visible: compactProgressPanel.percent > 0
                                    }

                                    Text {
                                        anchors.centerIn: parent
                                        text: Math.round(compactProgressPanel.percent)
                                        color: root.textPrimary
                                        font.pixelSize: 8
                                        font.weight: Font.Bold
                                        font.family: "Consolas"
                                    }
                                }
                            }

                            // 现代进度条
                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 8
                                radius: 4
                                color: themeManager.isDark ? "#1E2A38" : "#F1F5F9"

                                Rectangle {
                                    id: progressFill
                                    width: parent.width * compactProgressPanel.percent / 100
                                    height: parent.height
                                    radius: 4
                                    visible: compactProgressPanel.percent > 0

                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0.0; color: firmwareUpgrade.running ? "#38C8FF" : (themeManager.isDark ? "#536B83" : "#CBD5E1") }
                                        GradientStop { position: 0.5; color: firmwareUpgrade.running ? accent : (themeManager.isDark ? "#43576D" : "#9CA3AF") }
                                        GradientStop { position: 1.0; color: firmwareUpgrade.running ? "#2563EB" : (themeManager.isDark ? "#3B5068" : "#8CA0B4") }
                                    }
                                    Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

                                    // 发光效果
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        height: 3
                                        radius: 2
                                        color: "#FFFFFF"
                                        opacity: firmwareUpgrade.running ? 0.3 : 0.1
                                    }

                                    // 闪光动画
                                    Rectangle {
                                        width: 24
                                        height: parent.height
                                        radius: 4
                                        opacity: firmwareUpgrade.running ? 0.4 : 0
                                        gradient: Gradient {
                                            orientation: Gradient.Horizontal
                                            GradientStop { position: 0.0; color: "transparent" }
                                            GradientStop { position: 0.5; color: "#FFFFFF" }
                                            GradientStop { position: 1.0; color: "transparent" }
                                        }
                                        x: Math.max(0, progressFill.width - width - 2)
                                        SequentialAnimation on x {
                                            loops: Animation.Infinite
                                            running: firmwareUpgrade.running
                                            NumberAnimation { from: -24; to: progressFill.width + 24; duration: 1500; easing.type: Easing.InOutQuad }
                                        }
                                    }
                                }
                            }

                            // 状态文字
                            RowLayout {
                                Layout.fillWidth: true
                                Text {
                                    text: firmwareUpgrade.running ? "正在升级..." : (compactProgressPanel.percent >= 100 ? "升级完成" : "准备就绪")
                                    color: firmwareUpgrade.running ? accent : (compactProgressPanel.percent >= 100 ? success : textMuted)
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: Math.round(compactProgressPanel.percent) + "%"
                                    color: root.textPrimary
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    font.family: "Consolas"
                                }
                            }
                        }
                    }

                    DigitalSuccessCounter {
                        count: firmwareUpgrade.upgradeSuccessCount
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: compactActionColumn.implicitHeight + 12
                    radius: 8
                    color: root.surfaceRaised
                    border.color: root.outline

                    ColumnLayout {
                        id: compactActionColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 6
                        spacing: 6

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 4
                            uniformCellWidths: true
                            columnSpacing: 6
                            rowSpacing: 4

                            Button {
                                id: compactQueryVersionButton
                                text: "查询版本"
                                enabled: (linkManager.connected || linkManager.linkType === "CAN") && !firmwareUpgrade.running
                                onClicked: firmwareUpgrade.queryDevice()
                                Layout.fillWidth: true
                                Layout.minimumWidth: root.upgradeActionButtonWidth
                                Layout.preferredWidth: root.upgradeActionButtonWidth
                                implicitHeight: 28
                                contentItem: Text {
                                    text: compactQueryVersionButton.text
                                    color: compactQueryVersionButton.enabled ? "#F7FBFF" : "#6E7782"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: !compactQueryVersionButton.enabled ? (themeManager.isDark ? "#252C34" : "#E5E7EB") : (compactQueryVersionButton.down ? (themeManager.isDark ? "#1D5E99" : "#1D4ED8") : (compactQueryVersionButton.hovered ? (themeManager.isDark ? "#2878C6" : "#3B82F6") : accent))
                                    border.color: !compactQueryVersionButton.enabled ? (themeManager.isDark ? "#343D47" : "#D1D5DB") : (themeManager.isDark ? "#55A9F8" : "#93C5FD")
                                }
                            }
                            Button {
                                id: compactStartUpgradeButton
                                text: "单次升级"
                                enabled: (linkManager.connected || linkManager.linkType === "CAN") && !firmwareUpgrade.running && firmwareUpgrade.firmwarePath.length > 0
                                onClicked: firmwareUpgrade.startUpgrade()
                                Layout.fillWidth: true
                                Layout.minimumWidth: root.upgradeActionButtonWidth
                                Layout.preferredWidth: root.upgradeActionButtonWidth
                                implicitHeight: 28
                                contentItem: Text {
                                    text: compactStartUpgradeButton.text
                                    color: compactStartUpgradeButton.enabled ? "#F7FBFF" : "#6E7782"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: !compactStartUpgradeButton.enabled ? (themeManager.isDark ? "#252C34" : "#E5E7EB") : (compactStartUpgradeButton.down ? (themeManager.isDark ? "#16856F" : "#047857") : (compactStartUpgradeButton.hovered ? (themeManager.isDark ? "#23A78C" : "#10B981") : (themeManager.isDark ? "#1E967E" : "#059669")))
                                    border.color: !compactStartUpgradeButton.enabled ? (themeManager.isDark ? "#343D47" : "#D1D5DB") : (themeManager.isDark ? "#48D1B5" : "#34D399")
                                }
                            }
                            Button {
                                id: compactStressUpgradeButton
                                text: "升级压测"
                                enabled: (linkManager.connected || linkManager.linkType === "CAN") && !firmwareUpgrade.running && firmwareUpgrade.firmwarePath.length > 0
                                onClicked: {
                                    var count = Math.max(1, Math.min(9999, parseInt(compactStressCountField.text) || 1))
                                    compactStressCountField.text = count.toString()
                                    settingsManager.saveValue("upgrade.stressCount", count)
                                    firmwareUpgrade.startStressUpgrade(count)
                                }
                                Layout.fillWidth: true
                                Layout.minimumWidth: root.upgradeActionButtonWidth
                                Layout.preferredWidth: root.upgradeActionButtonWidth
                                implicitHeight: 28
                                contentItem: Text {
                                    text: compactStressUpgradeButton.text
                                    color: compactStressUpgradeButton.enabled ? "#F7FBFF" : "#6E7782"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: !compactStressUpgradeButton.enabled ? (themeManager.isDark ? "#252C34" : "#E5E7EB") : (compactStressUpgradeButton.down ? (themeManager.isDark ? "#7A5B10" : "#B45309") : (compactStressUpgradeButton.hovered ? (themeManager.isDark ? "#A77E19" : "#D97706") : (themeManager.isDark ? "#8C6815" : "#F59E0B")))
                                    border.color: !compactStressUpgradeButton.enabled ? (themeManager.isDark ? "#343D47" : "#D1D5DB") : (themeManager.isDark ? "#D4A83B" : "#D97706")
                                }
                            }
                            Button {
                                id: compactCancelUpgradeButton
                                text: "取消"
                                enabled: firmwareUpgrade.running
                                onClicked: firmwareUpgrade.cancel()
                                Layout.fillWidth: true
                                Layout.minimumWidth: root.upgradeActionButtonWidth
                                Layout.preferredWidth: root.upgradeActionButtonWidth
                                implicitHeight: 28
                                contentItem: Text {
                                    text: compactCancelUpgradeButton.text
                                    color: compactCancelUpgradeButton.enabled ? "#F7FBFF" : "#6E7782"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: !compactCancelUpgradeButton.enabled ? (themeManager.isDark ? "#252C34" : "#E5E7EB") : (compactCancelUpgradeButton.down ? (themeManager.isDark ? "#8B3232" : "#B91C1C") : (compactCancelUpgradeButton.hovered ? (themeManager.isDark ? "#B84646" : "#EF4444") : (themeManager.isDark ? "#9D3A3A" : "#DC2626")))
                                    border.color: !compactCancelUpgradeButton.enabled ? (themeManager.isDark ? "#343D47" : "#D1D5DB") : (themeManager.isDark ? "#E06B6B" : "#F87171")
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: firmwareUpgrade.status
                            color: firmwareUpgrade.status.indexOf("error:") === 0 ? "#F44747" : root.textSecondary
                            font.pixelSize: 10
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: compactDeviceInfoColumn.implicitHeight + 12
                    color: root.surfaceRaised
                    border.color: root.outline
                    radius: 8

                    ColumnLayout {
                        id: compactDeviceInfoColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 6
                        spacing: 5

                        RowLayout {
                            Layout.fillWidth: true
                            Text { text: "版本信息"; color: root.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: firmwareUpgrade.deviceRuntime.length > 0 ? firmwareUpgrade.deviceRuntime : "未查询"
                                color: firmwareUpgrade.deviceRuntime === "bootloader" ? root.warning : root.success
                                font.pixelSize: 10
                                font.weight: Font.Medium
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 6
                            rowSpacing: 5

                            VersionPill { label: "APP"; value: firmwareUpgrade.deviceAppVersion.length > 0 ? firmwareUpgrade.deviceAppVersion : "-"; accentColor: themeManager.isDark ? "#7AB7FF" : "#3B82F6" }
                            VersionPill { label: "BL"; value: firmwareUpgrade.deviceBootloaderVersion.length > 0 ? firmwareUpgrade.deviceBootloaderVersion : "-"; accentColor: themeManager.isDark ? "#B6C8E8" : "#6B7280" }
                            InfoField {
                                label: "模块"
                                value: firmwareUpgrade.deviceModule.length > 0 ? firmwareUpgrade.deviceModule : "-"
                                mono: true
                                labelWidth: 24
                                valueFontSizeMode: Text.HorizontalFit
                                valueElide: Text.ElideNone
                            }
                            InfoField { label: "模块ID"; value: firmwareUpgrade.deviceModuleId.length > 0 ? firmwareUpgrade.deviceModuleId : "-"; mono: true }
                            InfoField { label: "硬件版本"; value: firmwareUpgrade.deviceHardwareVersion.length > 0 ? firmwareUpgrade.deviceHardwareVersion : "-"; mono: true }
                            InfoField { label: "升级模式"; value: firmwareUpgrade.deviceModes.length > 0 ? firmwareUpgrade.deviceModes : "-"; mono: true }
                            InfoField { Layout.columnSpan: 2; label: "SN"; value: firmwareUpgrade.deviceSerialNumber.length > 0 ? firmwareUpgrade.deviceSerialNumber : "-"; mono: true }
                        }
                    }
                }

            }
        }
    }

    // ========== 链路数据监听 ==========
    Connections {
        target: linkManager
        function onFrameReceived(frame) {
            var target = takeResponseTarget(frame)
            if (target.length <= 0)
                return

            var response = normalizedResponseFrame(frame)
            if (target === "editor") {
                stopResponseTimeout(target)
                root.editorResponseFrame = response
                root.editorResponseStatus = response.crcValid === false ? "已收到回包，CRC异常" : "已收到回包"
            } else if (target === "quick") {
                stopResponseTimeout(target)
                root.quickResponseFrame = response
                root.quickResponseStatus = response.crcValid === false ? "已收到快捷回包，CRC异常" : "已收到快捷回包"
            }
        }
    }

    // ========== 工具函数 ==========
    function parseHexField(text) {
        var cleaned = text.replace(/^0x/i, "").replace(/\s+/g, "")
        return parseInt(cleaned, 16) || 0
    }

    function hexText(value, width) {
        return "0x" + value.toString(16).toUpperCase().padStart(width, "0")
    }

    function normalizedEditorCommand() {
        var src = parseHexField(srcField.text)
        var dst = parseHexField(dstField.text)
        var seq = parseInt(seqField.text) || 0
        var cmdset = parseHexField(cmdsetField.text)
        var cmdid = parseHexField(cmdidField.text)
        return {
            src: hexText(src, 2),
            dst: hexText(dst, 2),
            seq: seq.toString().padStart(4, "0"),
            cmdset: hexText(cmdset, 2),
            cmdid: hexText(cmdid, 2),
            data: dataField.text.trim().toUpperCase()
        }
    }

    function quickCommandDefaultName() {
        var cmd = normalizedEditorCommand()
        return "Cmd " + cmd.cmdset + "/" + cmd.cmdid
    }

    function openAddQuickCommandPopup() {
        quickNameField.text = quickCommandDefaultName()
        addQuickCmdPopup.open()
        quickNameField.forceActiveFocus()
        quickNameField.selectAll()
    }

    function addCurrentEditorToQuickCommands() {
        var cmd = normalizedEditorCommand()
        var ok = quickCmdManager.addCommand(quickNameField.text, cmd.src, cmd.dst,
                                            cmd.seq, cmd.cmdset, cmd.cmdid, cmd.data)
        if (ok) {
            addQuickCmdPopup.close()
            root.currentTab = 2
            lastSendInfo.text = "快捷: 已添加 " + (quickNameField.text.trim().length > 0 ? quickNameField.text.trim() : quickCommandDefaultName())
        } else {
            lastSendInfo.text = "快捷: 添加失败"
        }
    }

    function validateProtocolInput() {
        if (!protocolCheckInput)
            return
        root.protocolCheckHasInput = protocolCheckInput.text.trim().length > 0
        if (!root.protocolCheckHasInput) {
            root.protocolCheckResult = ({})
            root.protocolCheckOk = false
            root.protocolCheckError = ""
            return
        }

        var result = protocolEngine.parseHexFrame(protocolCheckInput.text)
        root.protocolCheckResult = result
        root.protocolCheckOk = result.valid === true
        root.protocolCheckError = result.error || ""
    }

    function checkValue(key) {
        if (!root.protocolCheckResult || root.protocolCheckResult[key] === undefined || root.protocolCheckResult[key] === null)
            return "-"
        var text = root.protocolCheckResult[key].toString()
        return text.length > 0 ? text : "-"
    }

    function checkPairColoredValue(leftKey, rightKey) {
        var left = checkValue(leftKey)
        var right = checkValue(rightKey)
        if (left === "-" && right === "-")
            return "-"
        if (left === "-" || right === "-")
            return left + " / " + right
        var ok = left === right
        var leftColor = ok ? "#77E6C7" : "#FF8C9B"
        var rightColor = "#77E6C7"
        return "<span style=\"color:" + leftColor + "\">" + left + "</span>"
                + "<span style=\"color:#8E99A6\"> / </span>"
                + "<span style=\"color:" + rightColor + "\">" + right + "</span>"
    }

    function formatUpgradeSuccessCount(value) {
        var count = Math.max(0, Math.min(9999, parseInt(value) || 0)).toString()
        while (count.length < 4)
            count = "0" + count
        return count
    }

    function buildAttr() {
        var attr = 0
        if (typeCombo.currentIndex === 1) attr |= 0x01
        return attr
    }

    function dialogKey(sender, receiver, seq, cmdSet, cmdId) {
        return sender.toString().trim().toUpperCase() + "|"
                + receiver.toString().trim().toUpperCase() + "|"
                + seq.toString().trim().toUpperCase() + "|"
                + cmdSet.toString().trim().toUpperCase() + "|"
                + cmdId.toString().trim().toUpperCase()
    }

    function normalizedResponseFrame(frame) {
        return {
            timestamp: frame.timestamp || "",
            sender: displayHexText(frame.sender || ""),
            receiver: displayHexText(frame.receiver || ""),
            seq: frame.seq || "",
            type: frame.type || "",
            cmd: displayHexText(frame.cmd || ""),
            cmdSet: displayHexText(frame.cmdSet || ""),
            cmdId: displayHexText(frame.cmdId || ""),
            len: frame.len || "",
            payloadLen: frame.payloadLen || "",
            data: displayHexText(frame.data || ""),
            cmdType: displayHexText(frame.cmdType || ""),
            headCrc: displayHexText(frame.headCrc || ""),
            crc: displayHexText(frame.crc || ""),
            crcValid: frame.crcValid,
            rawHex: displayHexText(frame.rawHex || "")
        }
    }

    function startResponseTimeout(target) {
        if (target === "editor")
            editorResponseTimeout.restart()
        else if (target === "quick")
            quickResponseTimeout.restart()
    }

    function stopResponseTimeout(target) {
        if (target === "editor")
            editorResponseTimeout.stop()
        else if (target === "quick")
            quickResponseTimeout.stop()
    }

    function clearPendingTarget(target) {
        if (!target || target.length <= 0)
            return

        var pending = root.responsePending
        for (var key in pending) {
            var queue = pending[key] || []
            var kept = []
            for (var i = 0; i < queue.length; i++) {
                if (queue[i] !== target)
                    kept.push(queue[i])
            }
            if (kept.length > 0)
                pending[key] = kept
            else
                delete pending[key]
        }
        root.responsePending = pending
    }

    function handleResponseTimeout(target) {
        clearPendingTarget(target)
        if (target === "editor") {
            root.editorResponseFrame = ({})
            root.editorResponseStatus = "回包超时（3s）"
        } else if (target === "quick") {
            root.quickResponseFrame = ({})
            root.quickResponseStatus = "快捷指令回包超时（3s）"
        }
    }

    function setResponseWaiting(target, waitingText) {
        if (target === "editor") {
            root.editorResponseFrame = ({})
            root.editorResponseStatus = waitingText
        } else if (target === "quick") {
            root.quickResponseFrame = ({})
            root.quickResponseStatus = waitingText
        }
    }

    function rememberResponseRequest(target, srcHex, dstHex, seqStr, cmdSetHex, cmdIdHex) {
        if (!target || target.length <= 0)
            return
        if (typeCombo.currentIndex !== 0)
            return
        var pending = root.responsePending
        var key = dialogKey(dstHex, srcHex, seqStr, cmdSetHex, cmdIdHex)
        var queue = pending[key] || []
        queue.push(target)
        pending[key] = queue
        root.responsePending = pending
        startResponseTimeout(target)
    }

    function takeResponseTarget(frame) {
        if (!frame || frame.type !== "ACK")
            return ""

        var key = dialogKey(frame.sender || "", frame.receiver || "",
                            frame.seq || "", frame.cmdSet || "", frame.cmdId || "")
        var pending = root.responsePending
        var queue = pending[key] || []
        if (queue.length <= 0)
            return ""

        var target = queue.shift()
        if (queue.length > 0)
            pending[key] = queue
        else
            delete pending[key]
        root.responsePending = pending
        return target
    }

    function fillEditor(src, dst, seq, cmdset, cmdid, data) {
        srcField.text = displayHexText(src)
        dstField.text = displayHexText(dst)
        seqField.text = seq
        cmdsetField.text = displayHexText(cmdset)
        cmdidField.text = displayHexText(cmdid)
        dataField.text = displayHexText(data)
    }

    function updatePreview() {
        var src = parseHexField(srcField.text)
        var dst = parseHexField(dstField.text)
        var seq = parseInt(seqField.text) || 0
        var cmdset = parseHexField(cmdsetField.text)
        var cmdid = parseHexField(cmdidField.text)
        var attr = buildAttr()
        var dataBytes = protocolEngine.hexToBytes(dataField.text)

        var frame = protocolEngine.buildFrame(src, dst, attr, seq, cmdset, cmdid, dataBytes)
        hexPreview.text = protocolEngine.bytesToHex(frame)

        bccDisplay.text = "0x" + ("0" + protocolEngine.getByte(frame, 3).toString(16).toUpperCase()).slice(-2)
        var crc16Lo = protocolEngine.getByte(frame, protocolEngine.frameLength(frame) - 2)
        var crc16Hi = protocolEngine.getByte(frame, protocolEngine.frameLength(frame) - 1)
        crc16Display.text = "0x" + ("000" + ((crc16Hi << 8) | crc16Lo).toString(16).toUpperCase()).slice(-4)
    }

    function sendFrame(recordDialog, responseTarget) {
        var target = responseTarget || ""
        if (target.length <= 0) {
            var trackEditorResponse = (recordDialog === undefined || recordDialog === null) ? (root.currentTab === 1) : recordDialog
            if (trackEditorResponse)
                target = "editor"
        }
        var src = parseHexField(srcField.text)
        var dst = parseHexField(dstField.text)
        var seq = parseInt(seqField.text) || 0
        var cmdset = parseHexField(cmdsetField.text)
        var cmdid = parseHexField(cmdidField.text)
        var attr = buildAttr()
        var dataBytes = protocolEngine.hexToBytes(dataField.text)

        var frame = protocolEngine.buildFrame(src, dst, attr, seq, cmdset, cmdid, dataBytes)

        var srcHex = "0x" + src.toString(16).toUpperCase().padStart(2, '0')
        var dstHex = "0x" + dst.toString(16).toUpperCase().padStart(2, '0')
        var cmdSetHex = "0x" + cmdset.toString(16).toUpperCase().padStart(2, '0')
        var cmdIdHex = "0x" + cmdid.toString(16).toUpperCase().padStart(2, '0')
        var seqStr = seq.toString().padStart(4, '0')
        var ts = Qt.formatTime(new Date(), "hh:mm:ss")
        if (target.length > 0) {
            if (typeCombo.currentIndex === 0) {
                setResponseWaiting(target, target === "quick" ? "等待快捷指令回包..." : "等待回包...")
                rememberResponseRequest(target, srcHex, dstHex, seqStr, cmdSetHex, cmdIdHex)
            } else {
                stopResponseTimeout(target)
                clearPendingTarget(target)
                setResponseWaiting(target, "ACK 帧不等待回包")
            }
        }

        linkManager.sendData(frame)

        bccDisplay.text = "0x" + ("0" + protocolEngine.getByte(frame, 3).toString(16).toUpperCase()).slice(-2)
        var crc16Lo = protocolEngine.getByte(frame, protocolEngine.frameLength(frame) - 2)
        var crc16Hi = protocolEngine.getByte(frame, protocolEngine.frameLength(frame) - 1)
        crc16Display.text = "0x" + ("000" + ((crc16Hi << 8) | crc16Lo).toString(16).toUpperCase()).slice(-4)
        hexPreview.text = protocolEngine.bytesToHex(frame)

        historyModel.append({
            timestamp: ts, src: srcField.text, dst: dstField.text,
            seq: seqField.text, cmdset: cmdsetField.text, cmdid: cmdidField.text,
            data: dataField.text
        })
        var maxHist = quickCmdManager.maxHistory || 20
        if (historyModel.count > maxHist) historyModel.remove(0, historyModel.count - maxHist)

        lastSendInfo.text = "最近: " + cmdSetHex + cmdIdHex + " · " + ts
    }

    function sendQuickCommand(cmd) {
        fillEditor(cmd.src, cmd.dst, cmd.seq, cmd.cmdset, cmd.cmdid, cmd.data)
        typeCombo.currentIndex = 0
        sendFrame(false, "quick")
    }



    component ToolTab: Rectangle {
        id: toolTab
        property string label: ""
        property int tabIndex: 0
        property color accentColor: root.accent
        readonly property bool selected: root.currentTab === tabIndex

        Layout.preferredWidth: Math.max(78, tabText.implicitWidth + 28)
        Layout.fillHeight: true
        radius: 7
        color: selected ? (themeManager.isDark ? "#1E2B39" : "#DBEAFE") : (tabMouse.containsMouse ? (themeManager.isDark ? "#18212A" : "#F3F4F6") : "transparent")
        border.color: selected ? accentColor : (tabMouse.containsMouse ? root.outline : "transparent")
        border.width: 1

        Text {
            id: tabText
            anchors.centerIn: parent
            text: toolTab.label
            color: toolTab.selected ? root.textPrimary : root.textSecondary
            font.pixelSize: 12
            font.weight: toolTab.selected ? Font.DemiBold : Font.Medium
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            height: 2
            radius: 1
            color: toolTab.accentColor
            visible: toolTab.selected
        }

        MouseArea {
            id: tabMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.currentTab = toolTab.tabIndex
        }
    }

    component CheckCard: Rectangle {
        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 0
        implicitHeight: Math.max(26, checkCardValue.implicitHeight + 10)
        color: themeManager.isDark ? "#171D24" : "#F9FAFB"
        border.color: root.outline
        border.width: 1
        radius: 5
        property string label: ""
        property string value: "-"
        property bool wrapValue: false
        property bool richValue: false

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

            Text {
                text: label
                Layout.preferredWidth: 48
                Layout.alignment: Qt.AlignVCenter
                color: root.textMuted
                font.pixelSize: 10
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                id: checkCardValue
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: value
                color: root.textPrimary
                font.pixelSize: 11
                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                textFormat: richValue ? Text.RichText : Text.PlainText
                wrapMode: wrapValue ? Text.WrapAnywhere : Text.NoWrap
                maximumLineCount: wrapValue ? 5 : 1
                elide: wrapValue ? Text.ElideNone : Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component AddQuickLabel: Text {
        Layout.preferredWidth: 28
        Layout.alignment: Qt.AlignVCenter
        color: root.textMuted
        font.pixelSize: 10
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }

    component AddQuickValue: Text {
        Layout.fillWidth: true
        Layout.minimumWidth: 56
        Layout.alignment: Qt.AlignVCenter
        color: root.textPrimary
        font.pixelSize: 10
        font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    component ProtoLabel: Text {
        Layout.preferredWidth: 30
        Layout.alignment: Qt.AlignVCenter
        color: root.textSecondary
        font.pixelSize: 11
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }

    component ProtoTextField: TextField {
        Layout.fillWidth: true
        Layout.minimumWidth: 68
        Layout.preferredWidth: 92
        implicitHeight: root.controlHeight
        color: root.textPrimary
        selectedTextColor: "#08121B"
        selectionColor: root.accentHover
        selectByMouse: true
        leftPadding: 8
        rightPadding: 8
        font.pixelSize: 11
        font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
        background: Rectangle {
            color: root.surfaceSoft
            border.color: parent.activeFocus ? root.accentHover : root.outlineStrong
            border.width: parent.activeFocus ? 1 : 1
            radius: 6
        }
    }

    component ProtoCombo: ComboBox {
        id: protoCombo
        Layout.fillWidth: true
        Layout.minimumWidth: 68
        Layout.preferredWidth: 92
        implicitHeight: root.controlHeight
        font.pixelSize: 11
        background: Rectangle {
            color: root.surfaceSoft
            border.color: protoCombo.activeFocus ? root.accentHover : root.outlineStrong
            radius: 6
        }
        contentItem: Text {
            text: protoCombo.currentText
            color: root.textPrimary
            font.pixelSize: 11
            font.weight: Font.Medium
            leftPadding: 8
            rightPadding: 22
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        indicator: Text {
            x: protoCombo.width - width - 8
            y: (protoCombo.height - height) / 2
            text: "▾"
            color: root.textMuted
            font.pixelSize: 10
        }
    }

    component VersionPill: Rectangle {
        id: versionPill
        property string label: ""
        property string value: "-"
        property color accentColor: root.accent

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 0
        implicitHeight: 28
        radius: 7
        color: themeManager.isDark ? "#172435" : "#F9FAFB"
        border.color: themeManager.isDark ? Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.55) : outline

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

            Text {
                text: versionPill.label
                color: themeManager.isDark ? "#9EA9B6" : "#6B7280"
                font.pixelSize: 9
                font.weight: Font.Medium
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                Layout.fillWidth: true
                text: versionPill.value
                color: versionPill.accentColor
                font.pixelSize: 12
                font.weight: Font.DemiBold
                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                horizontalAlignment: Text.AlignRight
                maximumLineCount: 1
                elide: Text.ElideRight
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    component InfoField: Rectangle {
        id: infoField
        property string label: ""
        property string value: "-"
        property bool mono: false
        property bool emphasize: false
        property int labelWidth: 48
        property int valueFontSizeMode: Text.FixedSize
        property int valueElide: Text.ElideRight

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 0
        implicitHeight: 24
        radius: 6
        color: emphasize ? (themeManager.isDark ? "#172235" : "#EFF6FF") : (themeManager.isDark ? "#151C24" : "#F9FAFB")
        border.color: emphasize ? (themeManager.isDark ? "#365D82" : "#93C5FD") : (themeManager.isDark ? "#26323E" : "#D1D5DB")

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 7

            Text {
                text: infoField.label
                color: emphasize ? (themeManager.isDark ? "#BFD0E2" : "#1D4ED8") : (themeManager.isDark ? "#8E99A6" : "#6B7280")
                font.pixelSize: 9
                font.weight: emphasize ? Font.Medium : Font.Normal
                Layout.preferredWidth: infoField.labelWidth
                Layout.alignment: Qt.AlignVCenter
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: infoField.value
                color: emphasize ? (themeManager.isDark ? "#F4F8FC" : "#111827") : (themeManager.isDark ? "#E2E8EF" : "#374151")
                font.pixelSize: 10
                fontSizeMode: infoField.valueFontSizeMode
                minimumPixelSize: 8
                font.weight: emphasize ? Font.Medium : Font.Normal
                font.family: mono ? (Qt.platform.os === "windows" ? "Consolas" : "monospace") : ""
                horizontalAlignment: Text.AlignRight
                maximumLineCount: 1
                elide: infoField.valueElide
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    component DigitalSuccessCounter: Rectangle {
        id: digitalSuccessCounter
        property int count: 0
        readonly property string digits: formatUpgradeSuccessCount(count)
        implicitWidth: 86
        implicitHeight: 60
        radius: 10
        color: themeManager.isDark ? "#182229" : "#FFFFFF"
        border.color: count > 0 ? (themeManager.isDark ? "#2B7A68" : "#059669") : (themeManager.isDark ? "#30404A" : "#D1D5DB")

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            anchors.topMargin: 6
            anchors.bottomMargin: 6
            spacing: 4

            Text {
                Layout.fillWidth: true
                text: "成功次数"
                color: count > 0 ? (themeManager.isDark ? "#8CE8D1" : "#059669") : (themeManager.isDark ? "#9BAAB3" : "#6B7280")
                font.pixelSize: 9
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 8
                color: themeManager.isDark ? "#0F1A24" : "#F8FAFC"
                border.color: count > 0 ? (themeManager.isDark ? "#1E3A30" : "#D1FAE5") : (themeManager.isDark ? "#1E2A38" : "#E2E8F0")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 2

                    // 数字翻牌效果
                    Repeater {
                        model: digitalSuccessCounter.digits.split("")
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 4
                            color: themeManager.isDark ? "#162030" : "#F1F5F9"
                            border.color: count > 0 ? (themeManager.isDark ? "#2B7A68" : "#059669") : (themeManager.isDark ? "#253040" : "#E2E8F0")

                            Text {
                                anchors.centerIn: parent
                                text: modelData
                                color: count > 0 ? (themeManager.isDark ? "#34D399" : "#059669") : (themeManager.isDark ? "#607080" : "#94A3B8")
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                font.family: "Consolas"
                            }
                        }
                    }

                    Text {
                        text: "次"
                        color: themeManager.isDark ? "#8E99A6" : "#6B7280"
                        font.pixelSize: 9
                        font.weight: Font.Medium
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }
        }
    }

    component PopHdr: Text { color: "#AAAAAA"; font.pixelSize: 9; verticalAlignment: Text.AlignVCenter; leftPadding: 3; height: parent ? parent.height : 18 }
    component PopCell: Text { font.pixelSize: 9; font.family: "Menlo"; verticalAlignment: Text.AlignVCenter; leftPadding: 3; elide: Text.ElideRight; height: parent ? parent.height : 20 }
}
