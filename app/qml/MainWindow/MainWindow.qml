import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import "../Panes" as P
import "../Widgets" as W

Rectangle {
    id: root
    color: "#151719"

    // 左侧为主工作区，右侧保持工具栏宽度，避免全屏时两侧同比放大。
    property real savedLeftColWidth: 960
    property real savedMonitorHeight: 500
    property real savedScriptHeight: 170
    property real savedLinkConfigHeight: 180
    property real savedProtoEditorHeight: 420
    property real leftMinimumWidth: 560
    property real rightMinimumWidth: 340
    property real rightMaximumWidth: 760
    property real rightWidthRatio: 0.34
    property real rightMinRatio: 0.22
    property real rightMaxRatio: 0.42
    property bool persistLayoutState: true
    property real requiredMinimumWidth: Math.max(1040, leftMinimumWidth + rightMinimumWidth + 4)
    property real requiredMinimumHeight: Math.ceil(linkConfigPanel.contentHeight + protoEditorPanel.requiredMinimumHeight + 26)
    readonly property bool luaScriptPanelActive: monitorPanel.luaScriptPanelVisible

    function clampRightRatio(value) {
        return Math.max(root.rightMinRatio, Math.min(root.rightMaxRatio, value))
    }

    function clampRightWidth(value, available) {
        var maxByLeft = Math.max(root.rightMinimumWidth, available - root.leftMinimumWidth)
        return Math.max(root.rightMinimumWidth,
                        Math.min(root.rightMaximumWidth, value, maxByLeft))
    }

    function effectiveRightWidth() {
        var available = Math.max(1, mainSplit.width - horizontalHandle.width)
        return clampRightWidth(available * root.rightWidthRatio, available)
    }

    Component.onCompleted: {
        savedLeftColWidth = settingsManager.loadValue("layout.leftColWidth", 960)
        savedMonitorHeight = settingsManager.loadValue("layout.monitorHeight", 500)
        savedScriptHeight = settingsManager.loadValue("layout.scriptHeight", 170)
        savedLinkConfigHeight = settingsManager.loadValue("layout.linkConfigHeight", 180)
        savedProtoEditorHeight = settingsManager.loadValue("layout.protoEditorHeight", 420)
        var savedRatio = Number(settingsManager.loadValue("layout.rightWidthRatio", -1))
        if (savedRatio > 0) {
            rightWidthRatio = clampRightRatio(savedRatio)
        } else {
            var baseWidth = Math.max(root.requiredMinimumWidth, root.width > 0 ? root.width : 1280)
            var legacyRightWidth = Number(settingsManager.loadValue("layout.rightPreferredWidth", 430))
            rightWidthRatio = clampRightRatio(legacyRightWidth / Math.max(1, baseWidth - 4))
        }
        // 恢复最后使用的标签页
        monitorPanel.monitorTabIndex = settingsManager.loadValue("layout.monitorTabIndex", 0)
    }

    Component.onDestruction: {
        // 保存标签页状态
        if (persistLayoutState)
            settingsManager.saveValue("layout.monitorTabIndex", monitorPanel.monitorTabIndex)
    }

    function switchToTerminal() {
        monitorPanel.monitorTabIndex = 1
    }

    function switchMonitorTab(index) {
        monitorPanel.monitorTabIndex = index
    }

    function switchToolTab(index) {
        protoEditorPanel.currentTab = index
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            id: mainSplit
            Layout.fillWidth: true
            Layout.fillHeight: true

            SplitView {
                id: leftCol
                orientation: Qt.Vertical
                anchors.left: parent.left
                anchors.right: horizontalHandle.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                handle: Rectangle {
                    implicitHeight: 4
                    color: SplitHandle.pressed ? "#3D8BFF" : (SplitHandle.hovered ? "#3D8BFF" : "#2A3037")
                }

                P.Dashboard {
                    id: monitorPanel
                    SplitView.fillHeight: true
                    SplitView.minimumHeight: monitorPanel.terminalActive ? 150 : 150
                    SplitView.preferredHeight: monitorPanel.terminalActive || !root.luaScriptPanelActive
                        ? leftCol.height - 4  // 终端模式：占满左列
                        : root.savedMonitorHeight
                }

                P.ScriptEditor {
                    id: scriptPanel
                    visible: root.luaScriptPanelActive
                    consoleVisible: true
                    SplitView.fillHeight: false
                    SplitView.minimumHeight: visible ? 142 : 0
                    SplitView.preferredHeight: visible ? Math.max(180, Math.min(root.savedScriptHeight, 300)) : 0
                }

                // 拖拽结束时保存
                onResizingChanged: {
                    if (!leftCol.resizing) {
                        settingsManager.saveValue("layout.leftColWidth", leftCol.width)
                        if (mainSplit.width > 0) {
                            var available = Math.max(1, mainSplit.width - horizontalHandle.width)
                            var rightWidth = root.clampRightWidth(available - leftCol.width, available)
                            root.rightWidthRatio = root.clampRightRatio(rightWidth / available)
                            settingsManager.saveValue("layout.rightWidthRatio", root.rightWidthRatio)
                            settingsManager.saveValue("layout.rightPreferredWidth", rightWidth)
                        }
                        settingsManager.saveValue("layout.monitorHeight", monitorPanel.height)
                        if (scriptPanel.visible)
                            settingsManager.saveValue("layout.scriptHeight", scriptPanel.height)
                    }
                }
            }

            Rectangle {
                id: horizontalHandle
                x: Math.max(root.leftMinimumWidth,
                            mainSplit.width - root.effectiveRightWidth() - width)
                width: 4
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                color: handleMouse.pressed ? "#3D8BFF" : (handleMouse.containsMouse ? "#3D8BFF" : "#2A3037")

                MouseArea {
                    id: handleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.SplitHCursor
                    onPositionChanged: {
                        if (!pressed)
                            return
                        var available = Math.max(1, mainSplit.width - horizontalHandle.width)
                        var newLeft = Math.max(root.leftMinimumWidth,
                                               Math.min(available - root.rightMinimumWidth, horizontalHandle.x + mouse.x))
                        var rightWidth = root.clampRightWidth(available - newLeft, available)
                        root.rightWidthRatio = root.clampRightRatio(rightWidth / available)
                        settingsManager.saveValue("layout.rightWidthRatio", root.rightWidthRatio)
                        settingsManager.saveValue("layout.rightPreferredWidth", rightWidth)
                    }
                }
            }

            SplitView {
                id: rightCol
                orientation: Qt.Vertical
                x: horizontalHandle.x + horizontalHandle.width
                width: Math.max(root.rightMinimumWidth, mainSplit.width - x)
                anchors.top: parent.top
                anchors.bottom: parent.bottom

                handle: Rectangle {
                    implicitHeight: 4
                    color: SplitHandle.pressed ? "#3D8BFF" : (SplitHandle.hovered ? "#3D8BFF" : "#2A3037")
                }

                P.Setup {
                    id: linkConfigPanel
                    // 高度自适应驱动配置内容，不低于用户拖拽保存值
                    SplitView.minimumHeight: linkConfigPanel.contentHeight
                    SplitView.preferredHeight: linkConfigPanel.contentHeight
                }

                P.ToolPanel {
                    id: protoEditorPanel
                    SplitView.minimumHeight: protoEditorPanel.requiredMinimumHeight
                    SplitView.preferredHeight: Math.max(protoEditorPanel.requiredMinimumHeight,
                                                        Math.min(root.savedProtoEditorHeight,
                                                                 protoEditorPanel.requiredMinimumHeight + 20))
                }

                onResizingChanged: {
                    if (!rightCol.resizing) {
                        settingsManager.saveValue("layout.linkConfigHeight", linkConfigPanel.height)
                        settingsManager.saveValue("layout.protoEditorHeight", protoEditorPanel.height)
                    }
                }
            }
        }

        W.StatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 22
        }
    }
}
