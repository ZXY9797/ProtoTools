import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Item {
    id: root
    implicitHeight: grid.implicitHeight + hint.implicitHeight + 8
    implicitWidth: grid.implicitWidth

    property bool _restoring: true
    property int controlHeight: 30

    function parseNumber(text, fallbackValue) {
        var s = (text || "").trim()
        if (s.length === 0) return fallbackValue
        var v = s.indexOf("0x") === 0 || s.indexOf("0X") === 0
                ? parseInt(s.substring(2), 16)
                : parseInt(s, 10)
        return isNaN(v) ? fallbackValue : v
    }

    function hexText(value, width) {
        var s = Number(value).toString(16).toUpperCase()
        while (s.length < width) s = "0" + s
        return "0x" + s
    }

    function normalizeHexPrefix(value) {
        return String(value || "").replace(/0X/g, "0x")
    }

    Component.onCompleted: {
        vidField.text = normalizeHexPrefix(settingsManager.loadValue("setup.usb.vendorId", "0x0000"))
        pidField.text = normalizeHexPrefix(settingsManager.loadValue("setup.usb.productId", "0x0000"))
        ifaceField.text = settingsManager.loadValue("setup.usb.interfaceNumber", 0).toString()
        inEpField.text = normalizeHexPrefix(settingsManager.loadValue("setup.usb.bulkInEndpoint", "0x81"))
        outEpField.text = normalizeHexPrefix(settingsManager.loadValue("setup.usb.bulkOutEndpoint", "0x01"))
        readLenField.text = settingsManager.loadValue("setup.usb.readPacketSize", 512).toString()

        usbDriver.vendorId = parseNumber(vidField.text, 0)
        usbDriver.productId = parseNumber(pidField.text, 0)
        usbDriver.interfaceNumber = parseNumber(ifaceField.text, 0)
        usbDriver.bulkInEndpoint = parseNumber(inEpField.text, 0x81)
        usbDriver.bulkOutEndpoint = parseNumber(outEpField.text, 0x01)
        usbDriver.readPacketSize = parseNumber(readLenField.text, 512)
        usbDriver.timeoutMs = 20
        settingsManager.saveValue("setup.usb.timeoutMs", 20)
        _restoring = false
    }

    GridLayout {
        id: grid
        anchors.left: parent.left
        anchors.right: parent.right
        columns: 4
        rowSpacing: 5
        columnSpacing: 6

        UsbLabel { text: "VID" }
        UsbField {
            id: vidField
            placeholderText: "0x1234"
            onEdited: function(v) {
                usbDriver.vendorId = v
                settingsManager.saveValue("setup.usb.vendorId", root.hexText(v, 4))
            }
        }
        UsbLabel { text: "PID" }
        UsbField {
            id: pidField
            placeholderText: "0x5678"
            onEdited: function(v) {
                usbDriver.productId = v
                settingsManager.saveValue("setup.usb.productId", root.hexText(v, 4))
            }
        }

        UsbLabel { text: "接口" }
        TextField {
            id: ifaceField
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 1
            implicitHeight: root.controlHeight
            font.pixelSize: 11
            color: "#E6EAF0"
            validator: IntValidator { bottom: 0; top: 32 }
            background: Rectangle {
                color: "#22272E"
                border.color: ifaceField.activeFocus ? "#7AB7FF" : "#3A424C"
                radius: 6
            }
            onTextEdited: {
                if (root._restoring) return
                var v = root.parseNumber(text, 0)
                usbDriver.interfaceNumber = v
                settingsManager.saveValue("setup.usb.interfaceNumber", v)
            }
        }
        UsbLabel { text: "读长度" }
        TextField {
            id: readLenField
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredWidth: 1
            implicitHeight: root.controlHeight
            font.pixelSize: 11
            color: "#E6EAF0"
            validator: IntValidator { bottom: 64; top: 65536 }
            background: Rectangle {
                color: "#22272E"
                border.color: readLenField.activeFocus ? "#7AB7FF" : "#3A424C"
                radius: 6
            }
            onTextEdited: {
                if (root._restoring) return
                var v = root.parseNumber(text, 512)
                usbDriver.readPacketSize = v
                settingsManager.saveValue("setup.usb.readPacketSize", v)
            }
        }

        UsbLabel { text: "EP_IN" }
        UsbField {
            id: inEpField
            placeholderText: "0x81"
            onEdited: function(v) {
                usbDriver.bulkInEndpoint = v
                settingsManager.saveValue("setup.usb.bulkInEndpoint", root.hexText(v, 2))
            }
        }
        UsbLabel { text: "EP_OUT" }
        UsbField {
            id: outEpField
            placeholderText: "0x01"
            onEdited: function(v) {
                usbDriver.bulkOutEndpoint = v
                settingsManager.saveValue("setup.usb.bulkOutEndpoint", root.hexText(v, 2))
            }
        }

    }

    Text {
        id: hint
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: grid.bottom
        anchors.topMargin: 8
        text: linkManager.connected && linkManager.linkType === "USB"
              ? "已连接 USB bulk"
              : "Windows 设备需绑定 WinUSB，Linux 使用 libusb bulk 访问"
        color: linkManager.connected && linkManager.linkType === "USB" ? "#4EC9B0" : "#667085"
        font.pixelSize: 10
        wrapMode: Text.WordWrap
    }

    component UsbLabel: Label {
        Layout.preferredWidth: 44
        Layout.alignment: Qt.AlignVCenter
        color: "#AEB7C2"
        font.pixelSize: 11
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    component UsbField: TextField {
        id: usbFieldControl
        signal edited(int value)
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.preferredWidth: 1
        implicitHeight: root.controlHeight
        font.pixelSize: 11
        font.family: "Menlo"
        color: "#E6EAF0"
        background: Rectangle {
            color: "#22272E"
            border.color: usbFieldControl.activeFocus ? "#7AB7FF" : "#3A424C"
            radius: 6
        }
        onTextEdited: {
            if (root._restoring) return
            edited(root.parseNumber(text, 0))
        }
    }
}
