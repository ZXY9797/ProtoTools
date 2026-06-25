import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    id: root
    color: "#15191E"

    property bool _restoring: true
    property var linkTypes: ["UART", "USB", "CAN", "BLE"]
    property string selectedLinkType: normalizeType(linkManager.linkType || "UART")
    readonly property bool pendingLinkSwitch: linkManager.connected && normalizeType(linkManager.linkType) !== selectedLinkType
    property real contentHeight: _titleBar.height + _contentCol.implicitHeight + _statusRow.implicitHeight + _connBtn.implicitHeight + 26

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
        color: "#1C2229"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 6

            Text {
                text: "\u94fe\u8def\u914d\u7f6e"
                color: "#F0F4F8"
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
                color: linkManager.connected ? "#153E35" : "#262D35"
                border.color: linkManager.connected ? "#2FB389" : "#3B4652"

                RowLayout {
                    anchors.centerIn: parent
                    spacing: 5
                    Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: linkManager.connected ? "#3DDC97" : "#7D8793"
                    }
                    Text {
                        id: statusText
                        text: linkManager.connected ? "\u5df2\u8fde\u63a5" : "\u672a\u8fde\u63a5"
                        color: linkManager.connected ? "#CFF8E8" : "#B8C1CC"
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
            color: "#2D3742"
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
            color: _connBtn.enabled ? "#FFFFFF" : "#7E8894"
            font: _connBtn.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: 8
            color: root.pendingLinkSwitch
                   ? (_connBtn.down ? "#8A650E" : (_connBtn.hovered ? "#C18B1A" : "#A97712"))
                   : (!linkManager.connected
                      ? (_connBtn.down ? "#16694F" : (_connBtn.hovered ? "#21A67B" : "#1C8C69"))
                      : (_connBtn.down ? "#29323C" : (_connBtn.hovered ? "#36424E" : "#2C3540")))
            border.width: 1
            border.color: _connBtn.activeFocus ? "#7AB7FF"
                         : (root.pendingLinkSwitch ? "#E0B44A" : (!linkManager.connected ? "#3AD6A1" : "#465360"))
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
                        color: "#AEB7C2"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Item { Layout.fillWidth: true }

                    Text {
                        text: root.selectedLinkType
                        color: "#7AB7FF"
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
                                color: linkTypeButton.active ? "#FFFFFF" : "#B8C1CC"
                                font.pixelSize: 12
                                font.weight: linkTypeButton.active ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                                elide: Text.ElideRight
                            }
                            background: Rectangle {
                                radius: 8
                                color: linkTypeButton.active
                                       ? (linkTypeButton.down ? "#1F5E91" : "#236EA9")
                                       : (linkTypeButton.hovered ? "#252D36" : "#1B222A")
                                border.width: 1
                                border.color: linkTypeButton.active
                                              ? "#62B5F8"
                                              : (linkTypeButton.activeFocus ? "#7AB7FF" : "#33404A")
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#29313A"
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
