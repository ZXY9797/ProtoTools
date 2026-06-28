import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import ProtoDebug

Rectangle {
    id: root
    color: themeManager.isDark ? "#0C1017" : "#F8F9FB"

    // 错误消息（ConnectionManager 错误信号）
    property string errorText: ""
    property bool showError: false
    property bool speedUnitKb: settingsManager.loadValue("status.speedUnitKb", false)

    // 主题颜色别名
    readonly property color surface: themeManager.isDark ? "#131920" : "#FFFFFF"
    readonly property color outline: themeManager.isDark ? "#1E2A38" : "#E5E7EB"
    readonly property color textPrimary: themeManager.isDark ? "#E8EDF5" : "#111827"
    readonly property color textSecondary: themeManager.isDark ? "#9CAAB8" : "#6B7280"
    readonly property color accent: themeManager.isDark ? "#3B8AFF" : "#2563EB"
    readonly property color success: themeManager.isDark ? "#34D399" : "#059669"

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
                color: linkManager.connected ? success : (themeManager.isDark ? "#607080" : "#9CA3AF")
                Layout.alignment: Qt.AlignVCenter
            }
            Text {
                text: linkManager.connectedLinkName
                color: textPrimary
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
            color: themeManager.isDark ? "#F87171" : "#DC2626"
            font.pixelSize: 10
            Layout.alignment: Qt.AlignVCenter
            // 点击关闭
            MouseArea {
                anchors.fill: parent
                onClicked: showError = false
            }
        }

        // 右侧：RX/TX 统计 + 链路速率 + 主题切换
        RowLayout {
            spacing: 8
            TrafficChip {
                label: "RX"
                speed: formatSpeed(linkManager.rxSpeed)
                total: formatBytes(linkManager.rxBytes)
                accent: themeManager.isDark ? "#60A5FA" : "#3B82F6"
            }
            TrafficChip {
                label: "TX"
                speed: formatSpeed(linkManager.txSpeed)
                total: formatBytes(linkManager.txBytes)
                accent: themeManager.isDark ? "#34D399" : "#10B981"
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
                    color: unitSwitch.hovered ? textPrimary : textSecondary
                    font.pixelSize: 10
                    font.family: "Menlo"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 9
                    color: unitSwitch.down ? root.surface : (unitSwitch.hovered ? root.surface : "transparent")
                    border.color: unitSwitch.activeFocus ? accent : outline
                }
            }

            // 主题切换按钮
            Button {
                id: themeSwitch
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredWidth: 28
                Layout.preferredHeight: 18
                padding: 0
                focusPolicy: Qt.TabFocus
                ToolTip.visible: hovered
                ToolTip.text: themeManager.isDark ? "切换到浅色模式" : "切换到深色模式"
                onClicked: themeManager.toggleTheme()
                contentItem: Text {
                    text: themeManager.isDark ? "☀" : "☾"
                    color: themeManager.isDark ? "#FBBF24" : "#6366F1"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: 9
                    color: themeSwitch.down ? root.surface : (themeSwitch.hovered ? root.surface : "transparent")
                    border.color: themeSwitch.activeFocus ? accent : outline
                }
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: root.outline
    }

    component TrafficChip: Rectangle {
        property string label: ""
        property string speed: ""
        property string total: ""
        property color accent: "#60A5FA"
        Layout.minimumWidth: 126
        Layout.preferredWidth: 142
        Layout.preferredHeight: 18
        Layout.alignment: Qt.AlignVCenter
        radius: 9
        color: root.surface
        border.color: root.outline

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
                color: textPrimary
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
                color: textSecondary
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
