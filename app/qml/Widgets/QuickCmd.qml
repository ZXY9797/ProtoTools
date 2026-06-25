import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    color: "#101419"

    property var onSend: function(cmd) {}
    property var onEdit: function(cmd) {}
    property int editingIndex: -1
    property string editError: ""
    property var responseFrame: ({})
    property string responseStatusText: "发送快捷指令后显示当前回包"
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
    property color danger: "#FF7F8D"

    function commandValue(cmd, key, fallback) {
        if (!cmd || cmd[key] === undefined || cmd[key] === null)
            return fallback
        var value = cmd[key].toString()
        return value.length > 0 ? value : fallback
    }

    function openEditCommand(cardIndex, cmd) {
        root.editingIndex = cardIndex
        root.editError = ""
        editNameField.text = commandValue(cmd, "name", "")
        editSrcField.text = commandValue(cmd, "src", "0x0001")
        editDstField.text = commandValue(cmd, "dst", "0x0500")
        editSeqField.text = commandValue(cmd, "seq", "0001")
        editSetField.text = commandValue(cmd, "cmdset", "0x00")
        editIdField.text = commandValue(cmd, "cmdid", "0x00")
        editDataField.text = commandValue(cmd, "data", "")
        editPopup.open()
        Qt.callLater(function() {
            editNameField.forceActiveFocus()
            editNameField.selectAll()
        })
    }

    function saveEditedCommand() {
        if (root.editingIndex < 0)
            return

        var ok = quickCmdManager.updateCommand(root.editingIndex,
                                               editNameField.text,
                                               editSrcField.text,
                                               editDstField.text,
                                               editSeqField.text,
                                               editSetField.text,
                                               editIdField.text,
                                               editDataField.text)
        if (ok) {
            editPopup.close()
            root.editingIndex = -1
            root.editError = ""
        } else {
            root.editError = "保存失败，请确认快捷指令仍然存在"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: "#111820"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Text {
                        text: "快捷指令"
                        color: root.textPrimary
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "从协议编辑添加，点击发送或编辑使用"
                        color: root.textMuted
                        font.pixelSize: 8
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                Rectangle {
                    implicitWidth: countText.implicitWidth + 16
                    implicitHeight: 20
                    radius: 7
                    color: "#172235"
                    border.color: "#2D4D6B"

                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: quickCmdManager.count + " 条"
                        color: "#BFD7F2"
                        font.pixelSize: 10
                        font.weight: Font.Medium
                    }
                }
            }
        }

        ListView {
            id: quickList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 4
            clip: true
            spacing: 4
            model: quickCmdManager.commands
            currentIndex: -1

            ScrollBar.vertical: ScrollBar {
                width: 10
                policy: ScrollBar.AsNeeded
                contentItem: Rectangle {
                    implicitWidth: 5
                    radius: 4
                    color: parent.pressed ? root.accent : root.textMuted
                }
                background: Rectangle { color: "transparent" }
            }

            delegate: Rectangle {
                id: cmdCard
                width: Math.max(0, quickList.width - 12)
                implicitHeight: Math.max(64, cardContent.implicitHeight + 8)
                radius: 6
                color: cmdHover.hovered ? "#1C2733" : root.surfaceRaised
                border.color: cmdHover.hovered ? root.outlineStrong : root.outline
                border.width: 1

                HoverHandler { id: cmdHover }

                ColumnLayout {
                    id: cardContent
                    anchors.fill: parent
                    anchors.leftMargin: 6
                    anchors.rightMargin: 6
                    anchors.topMargin: 4
                    anchors.bottomMargin: 4
                    spacing: 3

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 5

                        Rectangle {
                            Layout.preferredWidth: 18
                            Layout.preferredHeight: 18
                            radius: 5
                            color: "#142131"
                            border.color: "#2A4862"
                            Text {
                                anchors.centerIn: parent
                                text: (index + 1).toString()
                                color: "#9EC6EF"
                                font.pixelSize: 8
                                font.weight: Font.Medium
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name || "未命名"
                                color: root.textPrimary
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }

                        ActionButton {
                            text: "发送"
                            accentColor: "#24BFA5"
                            onClicked: root.onSend(quickCmdManager.commands[index])
                        }
                        ActionButton {
                            text: "编辑"
                            accentColor: "#D4A83B"
                            onClicked: root.openEditCommand(index, quickCmdManager.commands[index])
                        }
                        ActionButton {
                            text: "删除"
                            accentColor: "#E05E70"
                            onClicked: quickCmdManager.removeCommand(index)
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 3
                        FieldPill { label: "Src"; value: modelData.src || "-"; Layout.preferredWidth: 58 }
                        FieldPill { label: "Dst"; value: modelData.dst || "-"; Layout.preferredWidth: 58 }
                        FieldPill { label: "Seq"; value: modelData.seq || "-"; Layout.preferredWidth: 48 }
                        FieldPill { label: "Set"; value: modelData.cmdset || "-"; Layout.preferredWidth: 42 }
                        FieldPill { label: "ID"; value: modelData.cmdid || "-"; Layout.preferredWidth: 36 }
                        Item { Layout.fillWidth: true }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 21
                        radius: 5
                        color: root.surface
                        border.color: "#27323E"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 5
                            anchors.rightMargin: 5
                            anchors.topMargin: 3
                            anchors.bottomMargin: 3
                            spacing: 5

                            Text {
                                text: "Data"
                                color: root.textMuted
                                font.pixelSize: 8
                                font.weight: Font.Medium
                                Layout.preferredWidth: 26
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Text {
                                id: dataText
                                Layout.fillWidth: true
                                text: (modelData.data && modelData.data.length > 0) ? modelData.data : "无数据"
                                color: modelData.data && modelData.data.length > 0 ? root.textSecondary : root.textMuted
                                font.pixelSize: 8
                                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                                wrapMode: Text.NoWrap
                                maximumLineCount: 1
                                elide: Text.ElideRight
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }

            }

            Rectangle {
                anchors.centerIn: parent
                width: Math.min(parent.width - 32, 260)
                height: 84
                radius: 8
                color: root.surfaceRaised
                border.color: root.outline
                visible: quickCmdManager.count === 0

                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 24
                    spacing: 6
                    Text {
                        Layout.fillWidth: true
                        text: "暂无快捷指令"
                        color: root.textPrimary
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "在协议编辑页点击“加入快捷”"
                        color: root.textMuted
                        font.pixelSize: 10
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        ResponseFrameBox {
            Layout.fillWidth: true
            Layout.preferredHeight: 126
            Layout.minimumHeight: 96
            Layout.leftMargin: 4
            Layout.rightMargin: 4
            Layout.bottomMargin: 4
            title: "当前回包"
            placeholderText: root.responseStatusText
            frame: root.responseFrame
            accentColor: root.accent
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

    Popup {
        id: editPopup
        parent: root
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0
        width: Math.max(260, Math.min(420, root.width - 24))
        x: Math.max(12, (root.width - width) / 2)
        y: Math.max(12, Math.min(root.height - height - 12, (root.height - height) / 2))
        onClosed: {
            root.editingIndex = -1
            root.editError = ""
        }

        background: Rectangle {
            color: root.surfaceRaised
            border.color: root.outlineStrong
            border.width: 1
            radius: 8
        }

        contentItem: ColumnLayout {
            id: editPanel
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                color: "#111820"
                radius: 8

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    text: "编辑快捷指令"
                    color: root.textPrimary
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: root.outline
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: 12
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    EditLabel { text: "名称" }
                    EditTextField {
                        id: editNameField
                        Layout.fillWidth: true
                        placeholderText: "快捷指令名称"
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 4
                    columnSpacing: 8
                    rowSpacing: 7

                    EditLabel { text: "Src" }
                    EditTextField { id: editSrcField; Layout.fillWidth: true }
                    EditLabel { text: "Dst" }
                    EditTextField { id: editDstField; Layout.fillWidth: true }

                    EditLabel { text: "Seq" }
                    EditTextField { id: editSeqField; Layout.fillWidth: true }
                    EditLabel { text: "Set" }
                    EditTextField { id: editSetField; Layout.fillWidth: true }

                    EditLabel { text: "ID" }
                    EditTextField {
                        id: editIdField
                        Layout.fillWidth: true
                        Layout.columnSpan: 3
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    EditLabel {
                        text: "Data"
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 6
                    }
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 66
                        clip: true
                        background: Rectangle {
                            color: root.surfaceSoft
                            border.color: editDataField.activeFocus ? root.accentHover : root.outlineStrong
                            radius: 6
                        }

                        TextArea {
                            id: editDataField
                            selectByMouse: true
                            color: root.textPrimary
                            selectedTextColor: "#08121B"
                            selectionColor: root.accentHover
                            font.pixelSize: 11
                            font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                            wrapMode: TextArea.Wrap
                            background: null
                            placeholderText: "可为空，按字节输入 HEX"
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.editError.length > 0
                    text: root.editError
                    color: root.danger
                    font.pixelSize: 10
                    wrapMode: Text.Wrap
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Item { Layout.fillWidth: true }
                    EditCommandButton {
                        text: "取消"
                        normalColor: root.surfaceSoft
                        hoverColor: "#243241"
                        borderColor: root.outlineStrong
                        textColor: root.textSecondary
                        onClicked: editPopup.close()
                    }
                    EditCommandButton {
                        text: "保存"
                        normalColor: root.accent
                        hoverColor: root.accentHover
                        borderColor: root.accentHover
                        textColor: "#FFFFFF"
                        onClicked: root.saveEditedCommand()
                    }
                }
            }
        }
    }

    component FieldPill: Rectangle {
        property string label: ""
        property string value: "-"

        implicitWidth: 54
        implicitHeight: 18
        radius: 5
        color: root.surfaceSoft
        border.color: "#314050"

        RowLayout {
            id: pillLayout
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 2
            Text {
                text: label
                color: root.textMuted
                font.pixelSize: 7
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                Layout.fillWidth: true
                text: value
                color: root.textPrimary
                font.pixelSize: 8
                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component ActionButton: Button {
        id: actionButton
        property color accentColor: root.accent

        Layout.preferredWidth: 34
        implicitHeight: 20
        padding: 0
        font.pixelSize: 8
        font.weight: Font.Medium

        background: Rectangle {
            radius: 6
            color: actionButton.down ? Qt.darker(actionButton.accentColor, 1.25)
                                     : (actionButton.hovered ? actionButton.accentColor
                                                           : Qt.rgba(actionButton.accentColor.r,
                                                                     actionButton.accentColor.g,
                                                                     actionButton.accentColor.b,
                                                                     0.24))
            border.color: actionButton.accentColor
        }

        contentItem: Text {
            text: actionButton.text
            color: actionButton.hovered || actionButton.down ? "#FFFFFF" : Qt.lighter(actionButton.accentColor, 1.45)
            font: actionButton.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    component EditLabel: Text {
        Layout.preferredWidth: 34
        color: root.textSecondary
        font.pixelSize: 10
        font.weight: Font.Medium
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
    }

    component EditTextField: TextField {
        id: editTextField
        implicitHeight: 28
        selectByMouse: true
        color: root.textPrimary
        selectedTextColor: "#08121B"
        selectionColor: root.accentHover
        font.pixelSize: 11
        font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
        background: Rectangle {
            color: root.surfaceSoft
            border.color: editTextField.activeFocus ? root.accentHover : root.outlineStrong
            radius: 6
        }
    }

    component EditCommandButton: Button {
        id: editCommandButton
        property color normalColor: root.surfaceSoft
        property color hoverColor: root.accent
        property color borderColor: root.outlineStrong
        property color textColor: root.textSecondary

        implicitWidth: 64
        implicitHeight: 28
        padding: 0
        font.pixelSize: 11
        font.weight: Font.Medium

        background: Rectangle {
            radius: 6
            color: editCommandButton.down ? Qt.darker(editCommandButton.normalColor, 1.2)
                                          : (editCommandButton.hovered ? editCommandButton.hoverColor : editCommandButton.normalColor)
            border.color: editCommandButton.borderColor
        }

        contentItem: Text {
            text: editCommandButton.text
            color: editCommandButton.textColor
            font: editCommandButton.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }
}
