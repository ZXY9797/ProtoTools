import QtQuick
import QtQuick.Window
import QtQuick.Controls
import ProtoDebug
import "MainWindow"

Window {
    id: root
    width: 1280
    height: mainWindow.requiredMinimumHeight
    minimumWidth: mainWindow.requiredMinimumWidth
    minimumHeight: mainWindow.requiredMinimumHeight
    onMinimumWidthChanged: {
        if (root.width < root.minimumWidth)
            root.width = root.minimumWidth
    }
    onMinimumHeightChanged: {
        if (root.height < root.minimumHeight)
            root.height = root.minimumHeight
    }
    visible: true
    title: "KPtools"
    color: "#151719"

    property string screenshotPath: ""
    property bool screenshotMode: false
    property bool demoMode: false
    property bool restoringWindowGeometry: true
    property int screenshotMonitorTab: -1
    property int screenshotToolTab: -1

    function switchToTerminalTab() {
        mainWindow.switchToTerminal()
    }

    function numericValue(value, fallbackValue) {
        var n = Number(value)
        return isFinite(n) ? n : fallbackValue
    }

    function clampValue(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, value))
    }

    Component.onCompleted: {
        // 加载窗口位置和大小
        var availableW = Math.max(640, Screen.desktopAvailableWidth)
        var availableH = Math.max(480, Screen.desktopAvailableHeight)
        var savedX = numericValue(settingsManager.loadValue("window.x", 80), 80)
        var savedY = numericValue(settingsManager.loadValue("window.y", 80), 80)
        var savedW = numericValue(settingsManager.loadValue("window.width", 1280), 1280)
        var savedH = numericValue(settingsManager.loadValue("window.height", root.minimumHeight), root.minimumHeight)
        if (!settingsManager.loadValue("migration.contentFitWindowHeight", false)) {
            if (savedH <= 780)
                savedH = root.minimumHeight
            settingsManager.saveValue("migration.contentFitWindowHeight", true)
        }
        root.width = clampValue(savedW, root.minimumWidth, Math.max(root.minimumWidth, availableW))
        root.height = clampValue(savedH, root.minimumHeight, Math.max(root.minimumHeight, availableH))
        var maxVisibleX = Math.max(0, availableW - Math.min(root.width, availableW))
        var maxVisibleY = Math.max(0, availableH - Math.min(root.height, availableH))
        root.x = clampValue(savedX, 0, maxVisibleX)
        root.y = clampValue(savedY, 0, maxVisibleY)
        restoringWindowGeometry = false

        // 检查命令行参数
        var args = Qt.application.arguments
        for (var i = 0; i < args.length; i++) {
            if (args[i] === "--screenshot" && i + 1 < args.length) {
                root.screenshotPath = args[i + 1]
                root.screenshotMode = true
                screenshotTimer.start()
            }
            if (args[i] === "--demo") {
                root.demoMode = true
            }
            if (args[i] === "--monitor-tab" && i + 1 < args.length) {
                root.screenshotMonitorTab = Math.max(0, parseInt(args[i + 1]))
            }
            if (args[i] === "--tool-tab" && i + 1 < args.length) {
                root.screenshotToolTab = Math.max(0, parseInt(args[i + 1]))
            }
        }
    }

    // 窗口移动/缩放时保存（防抖 500ms）
    property real lastSaveTime: 0
    onXChanged: saveWindowDebounced()
    onYChanged: saveWindowDebounced()
    onWidthChanged: saveWindowDebounced()
    onHeightChanged: saveWindowDebounced()

    function saveWindowDebounced() {
        if (restoringWindowGeometry)
            return
        var now = Date.now()
        if (now - lastSaveTime > 500) {
            lastSaveTime = now
            settingsManager.saveValue("window.x", root.x)
            settingsManager.saveValue("window.y", root.y)
            settingsManager.saveValue("window.width", root.width)
            settingsManager.saveValue("window.height", root.height)
        }
    }

    function injectDemoData() {
        var demoFrames = [
            { ts: "17:18:01", dir: "↑", sender: "0x0001", receiver: "0x0002", len: "18", type: "REQ", seq: "0001", cmdSet: "0x01", cmdId: "0x01", data: "00", crc: "0xA3F1" },
            { ts: "17:18:01", dir: "↓", sender: "0x0002", receiver: "0x0001", len: "22", type: "ACK", seq: "0001", cmdSet: "0x01", cmdId: "0x01", data: "01 02 03 04 56 31 2E 30", crc: "0xB7C2" },
            { ts: "17:18:03", dir: "↑", sender: "0x0001", receiver: "0x0002", len: "18", type: "REQ", seq: "0002", cmdSet: "0x02", cmdId: "0xB1", data: "", crc: "0x8D4E" },
            { ts: "17:18:03", dir: "↓", sender: "0x0002", receiver: "0x0001", len: "24", type: "ACK", seq: "0002", cmdSet: "0x02", cmdId: "0xB1", data: "41 CC 80 00 41 D3 33 33", crc: "0xE19A" },
            { ts: "17:18:05", dir: "↑", sender: "0x0001", receiver: "0x0002", len: "20", type: "REQ", seq: "0003", cmdSet: "0x02", cmdId: "0xB2", data: "01 02", crc: "0x2F6B" },
            { ts: "17:18:05", dir: "↓", sender: "0x0002", receiver: "0x0001", len: "24", type: "ACK", seq: "0003", cmdSet: "0x02", cmdId: "0xB2", data: "42 48 00 00 42 70 00 00", crc: "0x9C3D" },
            { ts: "17:18:07", dir: "↑", sender: "0x0001", receiver: "0x0002", len: "18", type: "REQ", seq: "0004", cmdSet: "0x01", cmdId: "0x10", data: "FF", crc: "0x1A8C" },
            { ts: "17:18:08", dir: "↓", sender: "0x0002", receiver: "0x0001", len: "19", type: "ACK", seq: "0004", cmdSet: "0x01", cmdId: "0x10", data: "00", crc: "0xD4E7" },
            { ts: "17:18:10", dir: "↑", sender: "0x0001", receiver: "0x0002", len: "18", type: "REQ", seq: "0005", cmdSet: "0x02", cmdId: "0xB1", data: "", crc: "0x8D4E" },
            { ts: "17:18:10", dir: "↓", sender: "0x0002", receiver: "0x0001", len: "24", type: "ACK", seq: "0005", cmdSet: "0x02", cmdId: "0xB1", data: "41 D0 00 00 41 D9 99 9A", crc: "0x5B2F" },
            { ts: "17:18:12", dir: "↑", sender: "0x0001", receiver: "0x0002", len: "25", type: "REQ", seq: "0006", cmdSet: "0x03", cmdId: "0xA0", data: "DE AD BE EF 01 02 03", crc: "0x7A1C" },
            { ts: "17:18:12", dir: "↓", sender: "0x0002", receiver: "0x0001", len: "19", type: "ACK", seq: "0006", cmdSet: "0x03", cmdId: "0xA0", data: "01", crc: "0x3E8D" },
        ]
        for (var i = 0; i < demoFrames.length; i++) {
            var f = demoFrames[i]
            var crc8 = f.crc8 || ("0x" + ("0" + ((0x50 + i) & 0xFF).toString(16).toUpperCase()).slice(-2))
            monitorModel.addFrame({
                timestamp: f.ts, direction: f.dir,
                sender: f.sender, receiver: f.receiver,
                len: f.len, type: f.type, seq: f.seq,
                cmdSet: f.cmdSet, cmdId: f.cmdId,
                data: f.data, crc8: crc8, crc: f.crc, crc16: f.crc
            })
        }
        linkManager.rxBytes = 284
        linkManager.txBytes = 196
    }

    Timer {
        id: screenshotTimer
        interval: 2000
        repeat: false
        onTriggered: {
            if (root.screenshotMonitorTab >= 0)
                mainWindow.switchMonitorTab(root.screenshotMonitorTab)
            if (root.screenshotToolTab >= 0)
                mainWindow.switchToolTab(root.screenshotToolTab)
            if (root.demoMode) injectDemoData()
            grabTimer.start()
        }
    }

    Timer {
        id: grabTimer
        interval: 800
        repeat: false
        onTriggered: {
            root.contentItem.grabToImage(function(result) {
                if (result.saveToFile(root.screenshotPath)) {
                    console.log("Screenshot saved to:", root.screenshotPath)
                }
                if (root.screenshotMode) Qt.exit(0)
            })
        }
    }

    MainWindow {
        id: mainWindow
        anchors.fill: parent
    }
}
