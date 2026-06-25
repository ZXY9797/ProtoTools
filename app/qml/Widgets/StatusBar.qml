import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    color: "#161A1F"

    // 错误消息（ConnectionManager 错误信号）
    property string errorText: ""
    property bool showError: false
    property bool speedUnitKb: settingsManager.loadValue("status.speedUnitKb", false)

    function formatBytes(bytes) {
        if (bytes < 1024) return bytes + " B"
        if (bytes < 1048576) return (bytes / 1024).toFixed(1) + " KB"
        return (bytes / 1048576).toFixed(1) + " MB"
    }

    function formatSpeed(bps) {
        if (speedUnitKb)
            return (bps / 1024).toFixed(1) + " KB/s"
        return Math.round(bps) + " B/s"
    }

    Connections {
        target: linkManager
        function onErrorOccurred(msg) {
            errorText = msg
            showError = true
            errorTimer.restart()
        }
    }

    // 错误自动消失
    Timer {
        id: errorTimer
        interval: 8000
        onTriggered: showError = false
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 14

        // 左侧：当前连接的链路名称（左对齐）
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: 6
            Rectangle {
                width: 7
                height: 7
                radius: 4
                color: linkManager.connected ? "#32D583" : "#667085"
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                text: linkManager.connectedLinkName
                color: "#D7DDE5"
                font.pixelSize: 11
                font.family: "Menlo"
                Layout.alignment: Qt.AlignVCenter
            }
        }

        Item { Layout.fillWidth: true }

        // 错误信息（红色闪烁）
        Text {
            visible: showError
            text: "ERROR: " + errorText
            color: "#F44747"
            font.pixelSize: 10
            Layout.alignment: Qt.AlignVCenter
            // 点击关闭
            MouseArea {
                anchors.fill: parent
                onClicked: showError = false
            }
        }

        // 右侧：RX/TX 统计 + 链路速率
        RowLayout {
            spacing: 8
            TrafficChip {
                label: "RX"
                speed: formatSpeed(linkManager.rxSpeed)
                total: formatBytes(linkManager.rxBytes)
                accent: "#4EA7FF"
            }
            TrafficChip {
                label: "TX"
                speed: formatSpeed(linkManager.txSpeed)
                total: formatBytes(linkManager.txBytes)
                accent: "#48D1B5"
            }
            Button {
                id: unitSwitch
                text: root.speedUnitKb ? "KB/s" : "B/s"
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 54
                Layout.preferredHeight: 18
                padding: 0
                focusPolicy: Qt.TabFocus
                ToolTip.visible: hovered
                ToolTip.text: "切换速度单位"
                onClicked: {
                    root.speedUnitKb = !root.speedUnitKb
                    settingsManager.saveValue("status.speedUnitKb", root.speedUnitKb)
                }
                contentItem: Text {
                    text: unitSwitch.text
                    color: unitSwitch.hovered ? "#F7FBFF" : "#B8C1CC"
                    font.pixelSize: 10
                    font.family: "Menlo"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 9
                    color: unitSwitch.down ? "#26323D" : (unitSwitch.hovered ? "#232C35" : "#1B222A")
                    border.color: unitSwitch.activeFocus ? "#73B9FF" : "#35404A"
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: "#2A3037"
    }

    component TrafficChip: Rectangle {
        property string label: ""
        property string speed: ""
        property string total: ""
        property color accent: "#4EA7FF"
        Layout.minimumWidth: 126
        Layout.preferredWidth: 142
        Layout.preferredHeight: 18
        Layout.alignment: Qt.AlignVCenter
        radius: 9
        color: "#1B222A"
        border.color: "#2F3943"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 7
            anchors.rightMargin: 7
            spacing: 6

            Text {
                text: label
                color: accent
                font.pixelSize: 10
                font.weight: Font.DemiBold
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: speed
                color: "#F0F4F8"
                font.pixelSize: 11
                font.family: "Menlo"
                font.weight: Font.Medium
                Layout.alignment: Qt.AlignVCenter
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }

            Text {
                text: total
                color: "#7F8A96"
                font.pixelSize: 9
                font.family: "Menlo"
                Layout.alignment: Qt.AlignVCenter
                Layout.maximumWidth: 38
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
    }
}
