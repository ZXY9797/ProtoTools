import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root

    property string title: "完整回包"
    property string placeholderText: "发送后显示回包"
    property var frame: ({})
    property color surface: "#171D24"
    property color surfaceRaised: "#1B222B"
    property color surfaceSoft: "#202832"
    property color outline: "#303B47"
    property color outlineStrong: "#465A6F"
    property color textPrimary: "#E7EDF4"
    property color textSecondary: "#A4AFBC"
    property color textMuted: "#778390"
    property color accentColor: "#2F8DFF"
    property color successColor: "#5EE0C2"
    property color dangerColor: "#FF7F8D"
    property bool binaryMode: false
    readonly property bool hasFrame: frameValue("rawHex", "").length > 0

    implicitHeight: 136
    radius: 8
    color: root.surfaceRaised
    border.color: root.outline
    border.width: 1
    clip: true

    function frameValue(key, fallback) {
        if (!root.frame || root.frame[key] === undefined || root.frame[key] === null)
            return fallback
        var value = root.frame[key].toString()
        return value.length > 0 ? value.replace(/0X/g, "0x") : fallback
    }

    function byteCount(hex) {
        if (!hex)
            return 0
        var cleaned = hex.toString()
        cleaned = cleaned.replace(/0x/gi, "")
        cleaned = cleaned.replace(/[\s,;:_-]+/g, "")
        return Math.floor(cleaned.length / 2)
    }

    function escapeHtml(text) {
        return text.toString()
                .replace(/&/g, "&amp;")
                .replace(/</g, "&lt;")
                .replace(/>/g, "&gt;")
                .replace(/"/g, "&quot;")
    }

    function tokenSpan(token, background, foreground, weight) {
        return "<span style=\"background-color:" + background
                + ";color:" + foreground
                + ";font-weight:" + (weight || "600")
                + ";padding:1px 2px;\">" + escapeHtml(token) + "</span>"
    }

    function protocolStyle(index, total) {
        if (total >= 18 && index >= total - 2)
            return { bg: "#6A2C37", fg: "#FFE2E7" }      // CRC16
        if (index >= 16)
            return { bg: "#1F5A4B", fg: "#E5FFF8" }      // Data
        if (index >= 10)
            return { bg: "#5A3A22", fg: "#FFE4C4" }      // KPGenericTransHeader
        if (index >= 0)
            return { bg: "#263D5F", fg: "#DCEBFF" }      // KPHeader
        return { bg: "#303B47", fg: "#E7EDF4" }
    }

    function protocolLegendColor(part) {
        if (part === "generic")
            return "#5A3A22"
        if (part === "data")
            return "#1F5A4B"
        if (part === "crc")
            return "#6A2C37"
        return "#263D5F"
    }

    function binarySeparator(index, total) {
        if (index <= 0)
            return ""
        return " "
    }

    function highlightedProtocolHex() {
        var raw = frameValue("rawHex", "")
        if (raw.length <= 0)
            return ""

        var bytes = raw.trim().split(/\s+/)
        var parts = []
        for (var i = 0; i < bytes.length; ++i) {
            var style = protocolStyle(i, bytes.length)
            parts.push(binarySeparator(i, bytes.length)
                       + tokenSpan(bytes[i], style.bg, style.fg))
        }
        return parts.join("")
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 20
            spacing: 8

            Text {
                text: root.title
                color: root.textPrimary
                font.pixelSize: 11
                font.weight: Font.DemiBold
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                visible: root.hasFrame
                implicitWidth: typeText.implicitWidth + 14
                implicitHeight: 18
                radius: 9
                color: "#142536"
                border.color: root.accentColor
                Text {
                    id: typeText
                    anchors.centerIn: parent
                    text: root.frameValue("type", "ACK")
                    color: root.accentColor
                    font.pixelSize: 9
                    font.weight: Font.Medium
                }
            }

            RowLayout {
                spacing: 3
                Layout.alignment: Qt.AlignVCenter

                ModeButton {
                    label: "解析"
                    active: !root.binaryMode
                    onClicked: root.binaryMode = false
                }
                ModeButton {
                    label: "二进制"
                    active: root.binaryMode
                    onClicked: root.binaryMode = true
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: root.hasFrame ? (root.frameValue("timestamp", "") + "  " + root.frameValue("len", "-") + "B")
                                    : root.placeholderText
                color: root.hasFrame ? root.textMuted : root.textSecondary
                font.pixelSize: 9
                elide: Text.ElideRight
                Layout.maximumWidth: Math.max(90, root.width * 0.48)
                Layout.alignment: Qt.AlignVCenter
            }
        }

        Flow {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? childrenRect.height : 0
            spacing: 4
            visible: root.hasFrame && root.binaryMode
            clip: true

            LegendChip { label: "KPHeader"; chipColor: root.protocolLegendColor("header") }
            LegendChip { label: "KPGenericTransHeader"; chipColor: root.protocolLegendColor("generic") }
            LegendChip { label: "Data"; chipColor: root.protocolLegendColor("data") }
            LegendChip { label: "CRC16"; chipColor: root.protocolLegendColor("crc") }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 6
            rowSpacing: 5
            visible: root.hasFrame && !root.binaryMode

            ResponseField {
                label: "地址"
                value: root.frameValue("sender", "-") + " -> " + root.frameValue("receiver", "-")
            }
            ResponseField {
                label: "命令"
                value: root.frameValue("cmdSet", "-") + "/" + root.frameValue("cmdId", "-")
            }
            ResponseField {
                label: "Seq"
                value: root.frameValue("seq", "-")
            }
            ResponseField {
                label: "CRC"
                value: root.frame && root.frame.crcValid === false ? "错误 " + root.frameValue("crc", "-")
                                                                   : "正确 " + root.frameValue("crc", "-")
                valueColor: root.frame && root.frame.crcValid === false ? root.dangerColor : root.successColor
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: Math.max(24, dataText.implicitHeight + 10)
            radius: 6
            color: root.surface
            border.color: "#27323E"
            visible: root.hasFrame && !root.binaryMode

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 7
                anchors.rightMargin: 7
                anchors.topMargin: 5
                anchors.bottomMargin: 5
                spacing: 8

                Text {
                    text: "Data"
                    color: root.successColor
                    font.pixelSize: 9
                    font.weight: Font.DemiBold
                    Layout.preferredWidth: 32
                    Layout.alignment: Qt.AlignTop
                }

                Text {
                    id: dataText
                    Layout.fillWidth: true
                    text: root.frameValue("data", "无数据")
                    color: root.frameValue("data", "").length > 0 ? root.textPrimary : root.textMuted
                    font.pixelSize: 10
                    font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                    wrapMode: Text.WrapAnywhere
                }
            }
        }

        ScrollView {
            id: rawScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.hasFrame || root.binaryMode
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            background: Rectangle {
                color: root.surfaceSoft
                border.color: root.outlineStrong
                radius: 6
            }

            TextEdit {
                id: rawText
                width: rawScroll.availableWidth
                height: Math.max(rawScroll.availableHeight,
                                 rawText.contentHeight + rawText.topPadding + rawText.bottomPadding)
                readOnly: true
                selectByMouse: true
                textFormat: TextEdit.RichText
                text: !root.hasFrame ? root.escapeHtml(root.placeholderText)
                                      : root.highlightedProtocolHex()
                color: root.hasFrame ? "#D6DEE8" : root.textMuted
                selectedTextColor: "#08121B"
                selectionColor: root.accentColor
                font.pixelSize: 10
                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                wrapMode: TextEdit.Wrap
                leftPadding: 8
                rightPadding: 8
                topPadding: 7
                bottomPadding: 7
            }
        }
    }

    component ResponseField: Rectangle {
        id: responseField
        property string label: ""
        property string value: "-"
        property color valueColor: root.textPrimary

        Layout.fillWidth: true
        Layout.minimumWidth: 0
        implicitHeight: 22
        radius: 5
        color: root.surfaceSoft
        border.color: "#314050"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            spacing: 6

            Text {
                text: responseField.label
                color: root.textMuted
                font.pixelSize: 8
                Layout.preferredWidth: 24
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                Layout.fillWidth: true
                text: responseField.value
                color: responseField.valueColor
                font.pixelSize: 9
                font.family: Qt.platform.os === "windows" ? "Consolas" : "monospace"
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component LegendChip: Rectangle {
        id: legendChip
        property string label: ""
        property color chipColor: "#303B47"

        implicitWidth: legendLabel.implicitWidth + 12
        implicitHeight: 16
        radius: 5
        color: chipColor
        border.color: Qt.lighter(chipColor, 1.25)

        Text {
            id: legendLabel
            anchors.centerIn: parent
            text: legendChip.label
            color: "#F3F7FB"
            font.pixelSize: 8
            font.weight: Font.Medium
        }
    }

    component ModeButton: Rectangle {
        id: modeButton
        property string label: ""
        property bool active: false
        signal clicked()

        implicitWidth: modeLabel.implicitWidth + 12
        implicitHeight: 18
        radius: 5
        color: active ? root.accentColor : root.surfaceSoft
        border.color: active ? root.accentColor : root.outlineStrong

        Text {
            id: modeLabel
            anchors.centerIn: parent
            text: modeButton.label
            color: modeButton.active ? "#FFFFFF" : root.textSecondary
            font.pixelSize: 8
            font.weight: modeButton.active ? Font.Medium : Font.Normal
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: modeButton.clicked()
        }
    }
}
