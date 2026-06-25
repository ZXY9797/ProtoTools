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
    property color pageBg: "#101419"
    property color surface: "#171D24"
    property color surfaceRaised: "#1B222B"
    property color surfaceSoft: "#202832"
    property color outline: "#303B47"
    property color outlineStrong: "#465A6F"
    property color textPrimary: "#E7EDF4"
    property color textSecondary: "#A4AFBC"
    property color textMuted: "#778390"
    property color accent: "#2F8DFF"
    property color accentHover: "#3C9BFF"
    property color success: "#5EE0C2"
    property color warning: "#D4A83B"
    property color danger: "#FF7F8D"

    function loadTextValue(key, defaultValue) {
        var value = settingsManager.loadValue(key, defaultValue)
        if (value === undefined || value === null)
            return defaultValue
        var text = value.toString()
        return text.length > 0 ? text : defaultValue
    }

    // 表单参数保存
    Component.onCompleted: {
        srcField.text = loadTextValue("protoeditor.src", "0x0001")
        dstField.text = loadTextValue("protoeditor.dst", "0x0500")
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
            radius: 6
        }

        contentItem: ColumnLayout {
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 24; color: "#111820"
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 6
                    Text { text: "发送历史（点击填入表单）"; color: root.accentHover; font.pixelSize: 11; font.weight: Font.Medium }
                    Item { Layout.fillWidth: true }
                    Text { text: historyModel.count + " 条"; color: "#555555"; font.pixelSize: 10 }
                    Rectangle {
                        width: 42; height: 18; radius: 5
                        color: popClearMouse.containsMouse ? "#C84654" : "#3A1E25"
                        border.color: popClearMouse.containsMouse ? "#FF8A98" : "#7D3A47"
                        Text { anchors.centerIn: parent; text: "清空"; color: "#FFD9DE"; font.pixelSize: 10; font.weight: Font.Medium }
                        MouseArea { id: popClearMouse; anchors.fill: parent; hoverEnabled: true; onClicked: { historyModel.clear(); historyPopup.close() } }
                    }
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 18; color: "#151B22"
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
                    color: popMouse.containsMouse ? "#20344B" : (index % 2 === 0 ? root.surfaceRaised : root.surfaceSoft)
                    Row { anchors.fill: parent; spacing: 0
                        PopCell { text: (index + 1).toString(); width: 24; color: "#555" }
                        PopCell { text: model.timestamp || ""; width: 56; color: "#888" }
                        PopCell { text: model.src || ""; width: 50; color: "#9CDCFE" }
                        PopCell { text: model.dst || ""; width: 50; color: "#9CDCFE" }
                        PopCell { text: model.seq || ""; width: 36; color: "#569CD6" }
                        PopCell { text: model.cmdset || ""; width: 36; color: "#C586C0" }
                        PopCell { text: model.cmdid || ""; width: 36; color: "#DCDCAA" }
                        PopCell { text: model.data || ""; width: Math.max(50, popupList.width - 294); color: "#D4D4D4"; elide: Text.ElideRight }
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
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                radius: 8
                color: "#111820"

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
                    implicitHeight: 30
                    selectByMouse: true
                    color: root.textPrimary
                    selectedTextColor: "#08121B"
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
            color: "#111820"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                anchors.topMargin: 4
                anchors.bottomMargin: 4
                spacing: 6

                ToolTab { label: "固件升级"; tabIndex: 0; accentColor: "#48D1B5" }
                ToolTab { label: "协议编辑"; tabIndex: 1; accentColor: "#7AB7FF" }
                ToolTab { label: "快捷指令"; tabIndex: 2; accentColor: "#C58AF9" }
                ToolTab { label: "协议校验"; tabIndex: 3; accentColor: "#D4A83B" }
                Item { Layout.fillWidth: true }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#25303B"
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
                                    text: "0x0001"
                                    onTextChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.src", text) }
                                }
                                ProtoLabel { text: "Dst" }
                                ProtoTextField {
                                    id: dstField
                                    text: "0x0002"
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
                                        color: "#D4D4D4"; wrapMode: TextArea.Wrap; selectByMouse: true; background: null
                                        onTextChanged: { updatePreview(); if (!root._restoring) settingsManager.saveValue("protoeditor.data", text) }
                                    }
                                }
                            }

                            // CRC
                            RowLayout {
                                Layout.fillWidth: true; spacing: 4
                                Text { text: "CRC8"; color: "#AAAAAA"; font.pixelSize: 11; Layout.preferredWidth: 36; verticalAlignment: Text.AlignVCenter }
                                Text { id: crc8Display; text: "auto"; color: "#666666"; font.pixelSize: 10; font.family: "Menlo" }
                                Item { width: 8; height: 1 }
                                Text { text: "CRC16"; color: "#AAAAAA"; font.pixelSize: 11; verticalAlignment: Text.AlignVCenter }
                                Text { id: crc16Display; text: "auto"; color: "#666666"; font.pixelSize: 10; font.family: "Menlo" }
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
                                        color: parent.hovered ? "#2F8DFF" : "#1C3147"
                                        border.color: parent.hovered ? "#5DAAFF" : "#31506D"
                                    }
                                    contentItem: Text {
                                        text: parent.text
                                        color: parent.hovered ? "#FFFFFF" : "#BFD7F2"
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
                                Text { text: "预览 Hex (可选中复制)"; color: "#666666"; font.pixelSize: 9 }
                                ScrollView {
                                    id: hexPreviewScroll
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 50
                                    Layout.minimumHeight: 46
                                    Layout.maximumHeight: 56
                                    clip: true
                                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                                    background: Rectangle { color: "#111820"; border.color: root.outline; radius: 5 }
                                    TextArea {
                                        id: hexPreview; text: ""; readOnly: true; selectByMouse: true
                                        color: "#4EC9B0"; font.pixelSize: 9; font.family: "Menlo"
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
                    Layout.fillWidth: true; Layout.preferredHeight: 30; color: "#111820"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 6
                        Button {
                            text: "📤 发送"; font.pixelSize: 11; padding: 3; leftPadding: 14; rightPadding: 14
                            background: Rectangle { radius: 3; color: "#0E639C" }
                            contentItem: Text { text: parent.text; color: "#FFFFFF"; font: parent.font }
                            onClicked: sendFrame()
                        }
                        Text { id: lastSendInfo; text: "最近: —"; color: "#666666"; font.pixelSize: 10 }
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
                            color: "#1B222A"
                            border.color: protocolCheckInput.activeFocus ? "#2F8DFF" : "#35404C"
                            radius: 6
                        }
                        TextArea {
                            id: protocolCheckInput
                            color: "#DDE5EE"
                            selectionColor: "#245A8D"
                            selectedTextColor: "#FFFFFF"
                            placeholderText: "AA 01 ..."
                            placeholderTextColor: "#66717E"
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
                            color: "#F7FBFF"
                            font.pixelSize: 12
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: protocolCheckButton.down ? "#1D5E99" : (protocolCheckButton.hovered ? "#2878C6" : "#236EA9")
                            border.color: "#55A9F8"
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
                            color: "#DDE5EE"
                            font.pixelSize: 12
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: protocolClearButton.down ? "#303944" : (protocolClearButton.hovered ? "#35404C" : "#252C34")
                            border.color: "#465463"
                        }
                    }
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: protocolCheckStatus.implicitWidth + 14
                        implicitHeight: 22
                        radius: 5
                        color: !root.protocolCheckHasInput ? "#20262B" : (root.protocolCheckOk ? "#123F36" : "#3A1F24")
                        border.color: !root.protocolCheckHasInput ? "#35404C" : (root.protocolCheckOk ? "#2C927D" : "#A04452")
                        Text {
                            id: protocolCheckStatus
                            anchors.centerIn: parent
                            text: !root.protocolCheckHasInput ? "等待输入" : (root.protocolCheckOk ? "协议正确" : "协议错误")
                            color: !root.protocolCheckHasInput ? "#9AA6B2" : (root.protocolCheckOk ? "#A8F0DE" : "#FFB8C2")
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

                    CheckCard { label: "Flags"; value: checkValue("flags") }
                    CheckCard { label: "Payload"; value: checkValue("payloadLen") }

                    CheckCard {
                        label: "CRC8"
                        value: checkPairColoredValue("headerCrc", "headerCrcCalc")
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

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            TextField {
                                id: compactFirmwarePathField
                                Layout.fillWidth: true
                                implicitHeight: 28
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
                                text: "..."
                                implicitWidth: 36
                                implicitHeight: 28
                                enabled: !firmwareUpgrade.running
                                onClicked: firmwareUpgrade.browseFirmware(compactFirmwarePathField.text)
                            }
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 6
                            columnSpacing: 6
                            rowSpacing: 5

                            Text { text: "Src/Dst"; color: root.textSecondary; font.pixelSize: 10; Layout.alignment: Qt.AlignVCenter }
                            TextField {
                                id: compactSrcField
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                implicitHeight: 26
                                font.pixelSize: 11
                                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                                text: settingsManager.loadValue("upgrade.src", "0x0101")
                                color: root.textPrimary
                                enabled: !firmwareUpgrade.running
                                background: Rectangle { color: root.surfaceSoft; border.color: compactSrcField.activeFocus ? root.accent : root.outlineStrong; radius: 6 }
                                onTextEdited: { firmwareUpgrade.srcAddr = parseHexField(text); settingsManager.saveValue("upgrade.src", text) }
                                Component.onCompleted: firmwareUpgrade.srcAddr = parseHexField(text)
                            }
                            TextField {
                                id: compactDstField
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                implicitHeight: 26
                                font.pixelSize: 11
                                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                                text: settingsManager.loadValue("upgrade.dst", "0x0500")
                                color: root.textPrimary
                                enabled: !firmwareUpgrade.running
                                background: Rectangle { color: root.surfaceSoft; border.color: compactDstField.activeFocus ? root.accent : root.outlineStrong; radius: 6 }
                                onTextEdited: { firmwareUpgrade.dstAddr = parseHexField(text); settingsManager.saveValue("upgrade.dst", text) }
                                Component.onCompleted: firmwareUpgrade.dstAddr = parseHexField(text)
                            }
                            Text { text: "压测次数"; color: root.textSecondary; font.pixelSize: 10; horizontalAlignment: Text.AlignRight; Layout.alignment: Qt.AlignVCenter }
                            TextField {
                                id: compactStressCountField
                                Layout.preferredWidth: 68
                                implicitHeight: 26
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
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        id: compactProgressPanel
                        Layout.fillWidth: true
                        implicitHeight: 54
                        radius: 8
                        color: root.surfaceRaised
                        border.color: root.outline
                        property real percent: Math.max(0, Math.min(100, firmwareUpgrade.progress))

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: "升级进度"; color: root.textPrimary; font.pixelSize: 11; font.weight: Font.DemiBold }
                                Item { Layout.fillWidth: true }
                                Text {
                                    text: Math.round(compactProgressPanel.percent) + "%"
                                    color: firmwareUpgrade.running ? "#6ED6FF" : root.textSecondary
                                    font.pixelSize: 11
                                    font.weight: Font.Medium
                                }
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                implicitHeight: 9
                                radius: 5
                                color: "#121820"
                                border.color: root.outline
                                clip: true
                                Rectangle {
                                    width: parent.width * compactProgressPanel.percent / 100
                                    height: parent.height
                                    radius: 5
                                    color: firmwareUpgrade.running ? root.accent : "#4D647C"
                                    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
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
                                    color: !compactQueryVersionButton.enabled ? "#252C34" : (compactQueryVersionButton.down ? "#1D5E99" : (compactQueryVersionButton.hovered ? "#2878C6" : "#236EA9"))
                                    border.color: !compactQueryVersionButton.enabled ? "#343D47" : "#55A9F8"
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
                                    color: !compactStartUpgradeButton.enabled ? "#252C34" : (compactStartUpgradeButton.down ? "#16856F" : (compactStartUpgradeButton.hovered ? "#23A78C" : "#1E967E"))
                                    border.color: !compactStartUpgradeButton.enabled ? "#343D47" : "#48D1B5"
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
                                    color: !compactStressUpgradeButton.enabled ? "#252C34" : (compactStressUpgradeButton.down ? "#7A5B10" : (compactStressUpgradeButton.hovered ? "#A77E19" : "#8C6815"))
                                    border.color: !compactStressUpgradeButton.enabled ? "#343D47" : "#D4A83B"
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
                                    color: !compactCancelUpgradeButton.enabled ? "#252C34" : (compactCancelUpgradeButton.down ? "#8B3232" : (compactCancelUpgradeButton.hovered ? "#B84646" : "#9D3A3A"))
                                    border.color: !compactCancelUpgradeButton.enabled ? "#343D47" : "#E06B6B"
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

                            VersionPill { label: "APP"; value: firmwareUpgrade.deviceAppVersion.length > 0 ? firmwareUpgrade.deviceAppVersion : "-"; accentColor: "#7AB7FF" }
                            VersionPill { label: "BL"; value: firmwareUpgrade.deviceBootloaderVersion.length > 0 ? firmwareUpgrade.deviceBootloaderVersion : "-"; accentColor: "#B6C8E8" }
                            InfoField { label: "模块"; value: firmwareUpgrade.deviceModule.length > 0 ? firmwareUpgrade.deviceModule : "-"; mono: true }
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
                root.editorResponseFrame = response
                root.editorResponseStatus = response.crcValid === false ? "已收到回包，CRC异常" : "已收到回包"
            } else if (target === "quick") {
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
            src: hexText(src, 4),
            dst: hexText(dst, 4),
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
            sender: frame.sender || "",
            receiver: frame.receiver || "",
            seq: frame.seq || "",
            type: frame.type || "",
            cmd: frame.cmd || "",
            cmdSet: frame.cmdSet || "",
            cmdId: frame.cmdId || "",
            len: frame.len || "",
            payloadLen: frame.payloadLen || "",
            data: frame.data || "",
            attr: frame.attr || frame.flags || "",
            flags: frame.flags || "",
            nextHeader: frame.nextHeader || "",
            crc: frame.crc || "",
            crcValid: frame.crcValid,
            rawHex: frame.rawHex || ""
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
        srcField.text = src
        dstField.text = dst
        seqField.text = seq
        cmdsetField.text = cmdset
        cmdidField.text = cmdid
        dataField.text = data
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

        crc8Display.text = "0x" + ("0" + protocolEngine.getByte(frame, 9).toString(16).toUpperCase()).slice(-2)
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

        var srcHex = "0x" + src.toString(16).toUpperCase().padStart(4, '0')
        var dstHex = "0x" + dst.toString(16).toUpperCase().padStart(4, '0')
        var cmdSetHex = "0x" + cmdset.toString(16).toUpperCase().padStart(2, '0')
        var cmdIdHex = "0x" + cmdid.toString(16).toUpperCase().padStart(2, '0')
        var seqStr = seq.toString().padStart(4, '0')
        var ts = Qt.formatTime(new Date(), "hh:mm:ss")
        if (target.length > 0) {
            if (typeCombo.currentIndex === 0) {
                setResponseWaiting(target, target === "quick" ? "等待快捷指令回包..." : "等待回包...")
                rememberResponseRequest(target, srcHex, dstHex, seqStr, cmdSetHex, cmdIdHex)
            } else {
                setResponseWaiting(target, "ACK 帧不等待回包")
            }
        }

        linkManager.sendData(frame)

        crc8Display.text = "0x" + ("0" + protocolEngine.getByte(frame, 9).toString(16).toUpperCase()).slice(-2)
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
        color: selected ? "#1E2B39" : (tabMouse.containsMouse ? "#18212A" : "transparent")
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
        color: "#171D24"
        border.color: "#2B3540"
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
                color: "#8E99A6"
                font.pixelSize: 10
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                id: checkCardValue
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: value
                color: "#E2E8EF"
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
        color: "#172435"
        border.color: Qt.rgba(accentColor.r, accentColor.g, accentColor.b, 0.55)

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 8

            Text {
                text: versionPill.label
                color: "#9EA9B6"
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

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        Layout.minimumWidth: 0
        implicitHeight: 24
        radius: 6
        color: emphasize ? "#172235" : "#151C24"
        border.color: emphasize ? "#365D82" : "#26323E"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            spacing: 7

            Text {
                text: infoField.label
                color: emphasize ? "#BFD0E2" : "#8E99A6"
                font.pixelSize: 9
                font.weight: emphasize ? Font.Medium : Font.Normal
                Layout.preferredWidth: 48
                Layout.alignment: Qt.AlignVCenter
                maximumLineCount: 1
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: infoField.value
                color: emphasize ? "#F4F8FC" : "#E2E8EF"
                font.pixelSize: 10
                font.weight: emphasize ? Font.Medium : Font.Normal
                font.family: mono ? (Qt.platform.os === "windows" ? "Consolas" : "monospace") : ""
                horizontalAlignment: Text.AlignRight
                maximumLineCount: 1
                elide: Text.ElideRight
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }

    component DigitalSuccessCounter: Rectangle {
        id: digitalSuccessCounter
        property int count: 0
        readonly property string digits: formatUpgradeSuccessCount(count)
        implicitWidth: 72
        implicitHeight: 54
        radius: 8
        color: "#162027"
        border.color: "#2B4148"

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            anchors.topMargin: 5
            anchors.bottomMargin: 4
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: "成功次数"
                color: "#9BAAB3"
                font.pixelSize: 9
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 23
                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                spacing: 2

                Item { Layout.fillWidth: true }

                Text {
                    id: successDigits
                    text: digitalSuccessCounter.digits
                    color: "#65E7C7"
                    font.pixelSize: 21
                    font.weight: Font.DemiBold
                    font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: "次"
                    color: "#8E99A6"
                    font.pixelSize: 9
                    font.weight: Font.Medium
                    Layout.alignment: Qt.AlignVCenter
                    Layout.topMargin: 4
                }

                Item { Layout.fillWidth: true }
            }
        }
    }

    component PopHdr: Text { color: "#AAAAAA"; font.pixelSize: 9; verticalAlignment: Text.AlignVCenter; leftPadding: 3; height: parent ? parent.height : 18 }
    component PopCell: Text { font.pixelSize: 9; font.family: "Menlo"; verticalAlignment: Text.AlignVCenter; leftPadding: 3; elide: Text.ElideRight; height: parent ? parent.height : 20 }
}
