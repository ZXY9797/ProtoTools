import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    color: themeManager.isDark ? "#0C1017" : "#F8F9FB"

    property bool _restoring: true
    property var linkTypes: ["UART", "USB", "CAN", "BLE"]
    property string selectedLinkType: normalizeType(linkManager.linkType || "UART")
    readonly property bool pendingLinkSwitch: linkManager.connected && normalizeType(linkManager.linkType) !== selectedLinkType
    property real contentHeight: _titleBar.height + _contentCol.implicitHeight + _statusRow.implicitHeight + _connBtn.implicitHeight + 26

    // 主题颜色别名
    readonly property color surface: themeManager.isDark ? "#131920" : "#FFFFFF"
    readonly property color surfaceRaised: themeManager.isDark ? "#1A222C" : "#F3F4F6"
    readonly property color outline: themeManager.isDark ? "#253040" : "#D1D5DB"
    readonly property color outlineStrong: themeManager.isDark ? "#384858" : "#9CA3AF"
    readonly property color textPrimary: themeManager.isDark ? "#E8EDF5" : "#111827"
    readonly property color textSecondary: themeManager.isDark ? "#9CAAB8" : "#6B7280"
    readonly property color textMuted: themeManager.isDark ? "#607080" : "#9CA3AF"
    readonly property color accent: themeManager.isDark ? "#3B8AFF" : "#2563EB"
    readonly property color success: themeManager.isDark ? "#34D399" : "#059669"
    readonly property color warning: themeManager.isDark ? "#FBBF24" : "#D97706"

    function normalizeType(type) {
        if (type === "\u4e32\u53e3")
            return "UART"
        if (!type || type.length === 0)
            return "UART"
        return type
    }

    function isKnownLinkType(type) {
        var normalized = normalizeType(type)
        for (var i = 0; i < root.linkTypes.length; i++) {
            if (root.linkTypes[i] === normalized)
                return true
        }
        return false
    }

    function configSource(type) {
        switch (normalizeType(type)) {
        case "UART": return "SetupPanes/Drivers/UART.qml"
        case "USB": return "SetupPanes/Drivers/USB.qml"
        case "CAN": return "SetupPanes/Drivers/CAN.qml"
        case "BLE": return "SetupPanes/Drivers/BLE.qml"
        default: return "SetupPanes/Drivers/Stub.qml"
        }
    }

    Component.onCompleted: {
        var savedType = normalizeType(settingsManager.loadValue("setup.linkType", linkManager.linkType || "UART"))
        if (!isKnownLinkType(savedType))
            savedType = "UART"
        selectedLinkType = savedType
        if (!linkManager.connected && linkManager.linkType !== savedType)
            linkManager.linkType = savedType
        settingsManager.saveValue("setup.linkType", savedType)
        _restoring = false
    }

    Connections {
        target: linkManager
        function onLinkTypeChanged() {
            if (!_restoring && !linkManager.connected)
                selectedLinkType = normalizeType(linkManager.linkType)
        }
    }

    Rectangle {
        id: _titleBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 34
        color: root.surface

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 6

            Text {
                text: "\u94fe\u8def\u914d\u7f6e"
                color: root.textPrimary
                font.pixelSize: 13
                font.weight: Font.Medium
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: statusText.implicitWidth + 18
                implicitHeight: 20
                radius: 10
                color: linkManager.connected ? (themeManager.isDark ? "#0D2D24" : "#ECFDF5") : root.surfaceRaised
                border.color: linkManager.connected ? root.success : root.outline

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 5
                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: linkManager.connected ? root.success : root.textMuted
                    }
                    Text {
                        id: statusText
                        text: linkManager.connected ? "\u5df2\u8fde\u63a5" : "\u672a\u8fde\u63a5"
                        color: linkManager.connected ? (themeManager.isDark ? "#A7F3D0" : "#059669") : root.textSecondary
                        font.pixelSize: 10
                    }
                }
            }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: root.outline
        }
    }

    Button {
        id: _connBtn
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 6
        anchors.topMargin: 0
        implicitHeight: 30
        enabled: !linkManager.connecting
        text: linkManager.connecting ? "\u8fde\u63a5\u4e2d..."
              : (root.pendingLinkSwitch ? "\u5207\u6362\u8fde\u63a5"
                 : (linkManager.connected ? "\u5173\u95ed\u8fde\u63a5" : "\u6253\u5f00\u8fde\u63a5"))
        font.pixelSize: 12
        font.weight: Font.Medium
        onClicked: {
            if (root.pendingLinkSwitch) {
                linkManager.linkType = selectedLinkType
                linkManager.openConnection()
                return
            }
            if (linkManager.connected) {
                linkManager.closeConnection()
                return
            }
            if (linkManager.linkType !== selectedLinkType)
                linkManager.linkType = selectedLinkType
            linkManager.openConnection()
        }
        contentItem: Text {
            text: _connBtn.text
            color: _connBtn.enabled ? "#FFFFFF" : root.textMuted
            font: _connBtn.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 8
            color: root.pendingLinkSwitch
                   ? (_connBtn.down ? (themeManager.isDark ? "#78350F" : "#B45309") : (_connBtn.hovered ? (themeManager.isDark ? "#B45309" : "#D97706") : (themeManager.isDark ? "#92400E" : "#F59E0B")))
                   : (!linkManager.connected
                      ? (_connBtn.down ? (themeManager.isDark ? "#065F46" : "#047857") : (_connBtn.hovered ? (themeManager.isDark ? "#059669" : "#10B981") : (themeManager.isDark ? "#047857" : "#059669")))
                      : (_connBtn.down ? root.surfaceRaised : (_connBtn.hovered ? (themeManager.isDark ? "#253040" : "#E5E7EB") : root.surface)))
            border.width: 1
            border.color: _connBtn.activeFocus ? accent
                         : (root.pendingLinkSwitch ? warning : (!linkManager.connected ? success : outline))
        }
    }

    Item { id: _statusRow; implicitHeight: 0 }

    ScrollView {
        id: contentScroll
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: _titleBar.bottom
        anchors.bottom: _connBtn.top
        anchors.margins: 6
        anchors.bottomMargin: 6
        clip: true

        ColumnLayout {
            id: _contentCol
            width: contentScroll.availableWidth
            spacing: 6

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "\u94fe\u8def\u7c7b\u578b"
                        color: root.textSecondary
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: root.selectedLinkType
                        color: accent
                        font.pixelSize: 11
                        font.family: "Menlo"
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                RowLayout {
                    id: linkTypeRow
                    Layout.fillWidth: true
                    spacing: 5

                    Repeater {
                        model: root.linkTypes

                        Button {
                            id: linkTypeButton
                            required property string modelData
                            property bool active: root.selectedLinkType === modelData
                            text: modelData
                            Layout.fillWidth: true
                            Layout.minimumWidth: 56
                            Layout.preferredHeight: 28
                            focusPolicy: Qt.TabFocus
                            onClicked: {
                                if (root.selectedLinkType === modelData)
                                    return
                                root.selectedLinkType = modelData
                                if (!linkManager.connected && !linkManager.connecting)
                                    linkManager.linkType = modelData
                                settingsManager.saveValue("setup.linkType", modelData)
                            }
                            contentItem: Text {
                                text: linkTypeButton.text
                                color: linkTypeButton.active ? "#FFFFFF" : root.textSecondary
                                font.pixelSize: 12
                                font.weight: linkTypeButton.active ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            background: Rectangle {
                                radius: 8
                                color: linkTypeButton.active
                                       ? (linkTypeButton.down ? (themeManager.isDark ? "#1E5A94" : "#1D4ED8") : accent)
                                       : (linkTypeButton.hovered ? root.surfaceRaised : root.surface)
                                border.width: 1
                                border.color: linkTypeButton.active
                                              ? (themeManager.isDark ? "#60A5FA" : "#93C5FD")
                                              : (linkTypeButton.activeFocus ? accent : outline)
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: root.outline
            }

            Loader {
                id: driverPanel
                Layout.fillWidth: true
                Layout.preferredHeight: item ? item.implicitHeight : 60
                source: root.configSource(root.selectedLinkType)
                onLoaded: {
                    root.contentHeightChanged()
                    if (item)
                        item.implicitHeightChanged.connect(root.contentHeightChanged)
                }
            }
        }
    }
}
