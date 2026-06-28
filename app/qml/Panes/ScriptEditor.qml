import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import ProtoDebug

Rectangle {
    id: root
    color: "#0C1017"

    property bool consoleVisible: true

    // 标签页数据
    ListModel { id: tabModel }
    property int currentTabIndex: 0
    property int nextEditorId: 1

    // 标记编辑器文本更新中（切换标签时抑制 textChanged）
    property bool _updatingText: false
    property int pendingCloseIndex: -1
    property bool closeLastTab: false

    property string defaultScriptPath: ""

    Component.onCompleted: {
        initTimer.start()
    }

    Timer {
        id: initTimer
        interval: 200
        repeat: false
        onTriggered: {
            var configDir = luaEngine.getAppConfigDir()
            defaultScriptPath = configDir + "/default.lua"

            // 确保默认脚本文件存在
            var defaultContent = luaEngine.readTextFile(defaultScriptPath)
            if (defaultContent.length === 0 || defaultContent.indexOf(defaultScriptMarker) < 0) {
                luaEngine.writeTextFile(defaultScriptPath, defaultScriptReference)
                defaultContent = defaultScriptReference
            }

            // 恢复上次的标签页
            var savedTabs = settingsManager.loadValue("editor.tabs", "")
            var savedIndex = settingsManager.loadValue("editor.currentTab", 0)
            var restored = false

            if (savedTabs.length > 0) {
                var paths = savedTabs.split("|||" )
                for (var i = 0; i < paths.length; i++) {
                    var p = paths[i]
                    if (p.length === 0) continue
                    var c = luaEngine.readTextFile(p)
                    if (c.length > 0) {
                        var name = p.split('/').pop()
                        tabModel.append({ title: name, filePath: p, content: c, modified: false, editorId: nextEditorId++ })
                        restored = true
                    }
                }
            }

            // 没有恢复到任何标签，用默认脚本
            if (!restored) {
                addTab("default.lua", defaultScriptPath, defaultContent)
            } else {
                if (savedIndex >= 0 && savedIndex < tabModel.count)
                    currentTabIndex = savedIndex
                else
                    currentTabIndex = 0
                _updatingText = true
                codeEditor.text = tabModel.get(currentTabIndex).content
                _updatingText = false
            }
        }
    }

    // 标签变化时保存状态
    function saveTabState() {
        var paths = []
        for (var i = 0; i < tabModel.count; i++) {
            var p = tabModel.get(i).filePath
            if (p.length > 0) paths.push(p)
        }
        settingsManager.saveValue("editor.tabs", paths.join("|||"))
        settingsManager.saveValue("editor.currentTab", currentTabIndex)
    }

    // ---- 标签管理 ----
    function addTab(title, filePath, content) {
        var eid = nextEditorId++
        tabModel.append({ title: title, filePath: filePath, content: content, modified: false, editorId: eid })
        switchToTab(tabModel.count - 1)
        saveTabState()
    }

    function switchToTab(index) {
        if (index < 0 || index >= tabModel.count) return
        saveCurrentTabContent()
        currentTabIndex = index
        _updatingText = true
        codeEditor.text = tabModel.get(index).content
        _updatingText = false
        saveTabState()
    }

    function saveCurrentTabContent() {
        if (currentTabIndex >= 0 && currentTabIndex < tabModel.count) {
            tabModel.setProperty(currentTabIndex, "content", codeEditor.text)
        }
    }

    function closeTab(index) {
        if (index === currentTabIndex) saveCurrentTabContent()

        // 只剩一个标签时，关闭后自动新建空白标签
        if (tabModel.count <= 1) {
            if (!tabModel.get(0).modified) {
                tabModel.clear()
                addTab("NewScript", "", "")
                return
            }
            // 有改动的走确认流程，确认后新建
            pendingCloseIndex = index
            closeLastTab = true
            saveConfirmDialog.open()
            return
        }

        var tab = tabModel.get(index)
        if (tab.modified) {
            pendingCloseIndex = index
            closeLastTab = false
            saveConfirmDialog.open()
            return
        }
        doCloseTab(index)
    }

    function doCloseTab(index) {
        tabModel.remove(index)
        if (currentTabIndex >= tabModel.count) currentTabIndex = tabModel.count - 1
        _updatingText = true
        codeEditor.text = tabModel.get(currentTabIndex).content
        _updatingText = false
        saveTabState()
    }

    function markModified(index) {
        if (index >= 0 && index < tabModel.count)
            tabModel.setProperty(index, "modified", true)
    }

    function markSaved(index) {
        if (index >= 0 && index < tabModel.count)
            tabModel.setProperty(index, "modified", false)
    }

    // ---- 文件操作 ----
    function loadFile() {
        if (luaEngine.running) return
        var path = luaEngine.openLuaFileDialog()
        if (path.length === 0) return

        // 检查是否已经打开，是则切换到对应标签
        for (var i = 0; i < tabModel.count; i++) {
            if (tabModel.get(i).filePath === path) {
                switchToTab(i)
                return
            }
        }

        var content = luaEngine.readTextFile(path)
        var name = path.split('/').pop()
        addTab(name, path, content)
        markSaved(tabModel.count - 1)
    }

    function doSaveFile() {
        if (luaEngine.running) return
        saveCurrentTabContent()
        var idx = currentTabIndex
        if (idx < 0 || idx >= tabModel.count) return
        var tab = tabModel.get(idx)
        var path = tab.filePath

        if (path.length === 0) {
            path = luaEngine.saveLuaFileDialog(tab.title)
            if (path.length === 0) return
            tabModel.setProperty(idx, "filePath", path)
            tabModel.setProperty(idx, "title", path.split('/').pop())
        }
        luaEngine.writeTextFile(path, codeEditor.text)
        markSaved(idx)
        saveTabState()
    }

    // ---- 保存确认对话框 ----
    Dialog {
        id: saveConfirmDialog
        title: "保存更改"
        modal: true
        anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        Label { text: "脚本已修改，是否保存？"; color: themeManager.isDark ? "#E8EDF5" : "#111827" }
        onAccepted: {
            var idx = pendingCloseIndex
            if (idx >= 0 && idx < tabModel.count) {
                var tab = tabModel.get(idx)
                var path = tab.filePath
                if (path.length === 0) {
                    path = luaEngine.saveLuaFileDialog(tab.title)
                    if (path.length === 0) return
                }
                var content = (idx === currentTabIndex) ? codeEditor.text : tab.content
                luaEngine.writeTextFile(path, content)
            }
            if (closeLastTab) {
                tabModel.clear()
                addTab("NewScript", "", "")
            } else {
                doCloseTab(pendingCloseIndex)
            }
            pendingCloseIndex = -1
            closeLastTab = false
        }
        onDiscarded: {
            if (closeLastTab) {
                tabModel.clear()
                addTab("NewScript", "", "")
            } else {
                doCloseTab(pendingCloseIndex)
            }
            pendingCloseIndex = -1
            closeLastTab = false
        }
    }

    // ---- 布局 ----
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Tab Bar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: themeManager.isDark ? "#1A222C" : "#F3F4F6"

            RowLayout {
                anchors.fill: parent
                spacing: 0

                // 可滚动标签区域
                Flickable {
                    id: tabFlick
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: tabRow.implicitWidth
                    clip: true
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds

                    Row {
                        id: tabRow
                        height: parent.height
                        spacing: 0

                        Repeater {
                            model: tabModel
                            Rectangle {
                                width: tabLabel.implicitWidth + 52
                                height: 30
                                color: index === currentTabIndex ? (themeManager.isDark ? "#1E1E1E" : "#FFFFFF") : (tabMouse.containsMouse ? (themeManager.isDark ? "#2A2A2A" : "#F3F4F6") : (themeManager.isDark ? "#2D2D2D" : "#F9FAFB"))

                                // 活跃指示条
                                Rectangle {
                                    width: parent.width; height: 2
                                    color: themeManager.isDark ? "#3B8AFF" : "#2563EB"
                                    anchors.top: parent.top
                                    visible: index === currentTabIndex
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 4
                                    spacing: 4

                                    Text {
                                        id: tabLabel
                                        text: (model.modified ? "● " : "") + model.title
                                        color: index === currentTabIndex ? (themeManager.isDark ? "#FFFFFF" : "#111827") : (themeManager.isDark ? "#AAAAAA" : "#6B7280")
                                        font.pixelSize: 11
                                        font.family: "Menlo"
                                    }

                                    Item { Layout.fillWidth: true }

                                    // 关闭按钮
                                    Rectangle {
                                        Layout.preferredWidth: 18
                                        Layout.preferredHeight: 18
                                        radius: 9
                                        color: closeMouse.containsMouse ? (themeManager.isDark ? "#555555" : "#E5E7EB") : "transparent"
                                        Text {
                                            anchors.centerIn: parent
                                            text: "×"
                                            color: closeMouse.containsMouse ? (themeManager.isDark ? "#FFFFFF" : "#111827") : (themeManager.isDark ? "#888888" : "#9CA3AF")
                                            font.pixelSize: 13
                                        }
                                        MouseArea {
                                            id: closeMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: closeTab(index)
                                        }
                                    }
                                }

                                MouseArea {
                                    id: tabMouse
                                    anchors.fill: parent
                                    anchors.rightMargin: 22
                                    hoverEnabled: true
                                    onClicked: if (!luaEngine.running) switchToTab(index)
                                }
                            }
                        }

                        // + 新建标签按钮（紧跟最后一个标签）
                        Rectangle {
                            width: 28; height: 30
                            color: plusMouse.containsMouse ? (themeManager.isDark ? "#2A2A2A" : "#F3F4F6") : "transparent"
                            Text {
                                anchors.centerIn: parent
                                text: "+"
                                color: plusMouse.containsMouse ? (themeManager.isDark ? "#FFFFFF" : "#111827") : (themeManager.isDark ? "#888888" : "#9CA3AF")
                                font.pixelSize: 16
                            }
                            MouseArea {
                                id: plusMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    if (luaEngine.running) return
                                    addTab("NewScript", "", "")
                                }
                            }
                        }
                    }
                }
            }
        }

        // Code + Console
        SplitView {
            id: editorSplit
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            handle: Rectangle {
                implicitWidth: 4
                visible: root.consoleVisible
                color: SplitHandle.pressed ? (themeManager.isDark ? "#3B8AFF" : "#2563EB") : (SplitHandle.hovered ? (themeManager.isDark ? "#3B8AFF" : "#2563EB") : (themeManager.isDark ? "#333333" : "#D1D5DB"))
            }

            // QCodeEditor
            LuaCodeEditor {
                id: codeEditor
                SplitView.fillWidth: true
                SplitView.minimumWidth: 360
                SplitView.preferredWidth: 720
                SplitView.fillHeight: true

                onSaveRequested: {
                    if (!luaEngine.running) doSaveFile()
                }

                onTextChanged: {
                    if (!_updatingText && currentTabIndex >= 0 && currentTabIndex < tabModel.count) {
                        if (codeEditor.text !== tabModel.get(currentTabIndex).content) {
                            tabModel.setProperty(currentTabIndex, "content", codeEditor.text)
                            markModified(currentTabIndex)
                        }
                    }
                }
            }

            // Console Output
            Rectangle {
                SplitView.fillHeight: true
                SplitView.minimumWidth: root.consoleVisible ? 300 : 0
                SplitView.preferredWidth: root.consoleVisible ? 420 : 0
                visible: root.consoleVisible
                color: themeManager.isDark ? "#1A1A1A" : "#F9FAFB"
                border.color: themeManager.isDark ? "#333333" : "#E5E7EB"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 2
                    spacing: 0

                    // Console header
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 18
                        color: themeManager.isDark ? "#252526" : "#F3F4F6"
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 6
                            anchors.rightMargin: 6
                            Text {
                                text: "🖥️ Lua 控制台"
                                color: themeManager.isDark ? "#3B8AFF" : "#2563EB"
                                font.pixelSize: 10
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: luaEngine && luaEngine.running ? "运行中 ●" : "已停止 ○"
                                color: luaEngine && luaEngine.running ? (themeManager.isDark ? "#34D399" : "#059669") : (themeManager.isDark ? "#888888" : "#9CA3AF")
                                font.pixelSize: 9
                            }
                            Rectangle {
                                width: 30; height: 14; radius: 3
                                color: clearConsoleMouse.containsMouse ? (themeManager.isDark ? "#333333" : "#E5E7EB") : "transparent"
                                Text {
                                    anchors.centerIn: parent
                                    text: "清空"
                                    color: themeManager.isDark ? "#888888" : "#6B7280"
                                    font.pixelSize: 9
                                }
                                MouseArea {
                                    id: clearConsoleMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: consoleModel.clear()
                                }
                            }
                        }
                    }

                    ListView {
                        id: consoleList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: consoleModel
                        currentIndex: -1
                        onCountChanged: if (count > 0) positionViewAtEnd()

                        delegate: Rectangle {
                            width: consoleList.width
                            height: 18
                            color: index % 2 === 0 ? (themeManager.isDark ? "#1A1A1A" : "#F9FAFB") : (themeManager.isDark ? "#1E1E1E" : "#FFFFFF")
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.right: parent.right
                                anchors.rightMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                text: model.text || ""
                                color: model.isError ? (themeManager.isDark ? "#F44747" : "#DC2626") : (themeManager.isDark ? "#AAAAAA" : "#6B7280")
                                font.pixelSize: 10
                                font.family: "Menlo"
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        // Bottom Toolbar
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 28
            color: themeManager.isDark ? "#252526" : "#F3F4F6"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 6
                spacing: 4

                // 运行/停止
                Button {
                    text: luaEngine && luaEngine.running ? "⏹ 停止" : "▶ 运行"
                    font.pixelSize: 11; padding: 3; leftPadding: 8; rightPadding: 8
                    background: Rectangle {
                        radius: 3
                        color: luaEngine && luaEngine.running ? (themeManager.isDark ? "#C53030" : "#DC2626") : (themeManager.isDark ? "#16825D" : "#059669")
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "#FFFFFF"
                        font: parent.font
                    }
                    onClicked: {
                        if (luaEngine.running) {
                            luaEngine.stop()
                        } else {
                            saveCurrentTabContent()
                            luaEngine.loadScript(codeEditor.text)
                            luaEngine.start()
                        }
                    }
                }

                // 保存
                Button {
                    text: "💾 保存"
                    enabled: !luaEngine.running
                    font.pixelSize: 11; padding: 3; leftPadding: 8; rightPadding: 8
                    background: Rectangle {
                        radius: 3
                        color: enabled ? (themeManager.isDark ? "#333333" : "#E5E7EB") : (themeManager.isDark ? "#222222" : "#F3F4F6")
                        border.color: enabled ? (themeManager.isDark ? "#555555" : "#D1D5DB") : (themeManager.isDark ? "#333333" : "#E5E7EB")
                    }
                    contentItem: Text {
                        text: parent.text
                        color: enabled ? (themeManager.isDark ? "#CCCCCC" : "#374151") : (themeManager.isDark ? "#555555" : "#9CA3AF")
                        font: parent.font
                    }
                    onClicked: doSaveFile()
                }

                // 加载
                Button {
                    text: "📂 加载"
                    enabled: !luaEngine.running
                    font.pixelSize: 11; padding: 3; leftPadding: 8; rightPadding: 8
                    background: Rectangle {
                        radius: 3
                        color: enabled ? (themeManager.isDark ? "#333333" : "#E5E7EB") : (themeManager.isDark ? "#222222" : "#F3F4F6")
                        border.color: enabled ? (themeManager.isDark ? "#555555" : "#D1D5DB") : (themeManager.isDark ? "#333333" : "#E5E7EB")
                    }
                    contentItem: Text {
                        text: parent.text
                        color: enabled ? (themeManager.isDark ? "#CCCCCC" : "#374151") : (themeManager.isDark ? "#555555" : "#9CA3AF")
                        font: parent.font
                    }
                    onClicked: loadFile()
                }

                Item { Layout.fillWidth: true }

                // 当前文件路径
                Text {
                    visible: tabModel.count > 0 && currentTabIndex >= 0 && currentTabIndex < tabModel.count
                    text: (currentTabIndex < tabModel.count ? (tabModel.get(currentTabIndex).filePath || "(新文件)") : "")
                    color: themeManager.isDark ? "#555555" : "#9CA3AF"
                    font.pixelSize: 9
                    elide: Text.ElideLeft
                    Layout.maximumWidth: 300
                }

                // 控制台切换
                Button {
                    text: root.consoleVisible ? "🖥️ 隐藏控制台" : "🖥️ 显示控制台"
                    font.pixelSize: 11; padding: 3; leftPadding: 8; rightPadding: 8
                    background: Rectangle {
                        radius: 3
                        color: root.consoleVisible ? (themeManager.isDark ? "#094771" : "#DBEAFE") : (themeManager.isDark ? "#333333" : "#E5E7EB")
                        border.color: (themeManager.isDark ? "#555555" : "#D1D5DB")
                    }
                    contentItem: Text {
                        text: parent.text
                        color: (themeManager.isDark ? "#CCCCCC" : "#374151")
                        font: parent.font
                    }
                    onClicked: root.consoleVisible = !root.consoleVisible
                }
            }
        }
    }

    // Console model
    ListModel { id: consoleModel }

    Connections {
        target: luaEngine
        function onConsoleOutput(msg) {
            consoleModel.append({ text: msg, isError: false })
            if (consoleModel.count > 500) consoleModel.remove(0, 100)
        }
        function onErrorOccurred(err) {
            consoleModel.append({ text: "[ERROR] " + err, isError: true })
        }
    }

    property string defaultScriptMarker: "KPTOOLS_LUA_DEFAULT_V2"
    property string defaultScriptReference: [
        "-- ProtoTools Lua default - " + defaultScriptMarker,
        "-- on_frame_recv(frame) can return:",
        "--   false                         filter this frame",
        "--   true                          show this frame",
        "--   { show=false }                filter this frame",
        "--   { rowColor='#203A2B' }        change monitor row color",
        "--   { textColor='#E8FFF0' }       change monitor text color",
        "--   { replyHex='AA 55 01' }       send a protocol reply",
        "--   { curve={v1,v2,...} }         push values to curve C1..C9",
        "-- read_data(frame.data, offset, type) supports uint8/int8/uint16/int16/uint32/int32/uint64/int64/float/double.",
        "",
        "local recv_count = 0",
        "local send_count = 0",
        "local error_count = 0",
        "local cmd_stats = {}",
        "",
        "local function key_of(frame)",
        "  return tostring(frame.cmdSet or '') .. '.' .. tostring(frame.cmdId or '')",
        "end",
        "",
        "local function should_show(frame)",
        "  -- Example filter: only show all frames by default.",
        "  -- Change this to: return frame.cmdSet == '0x02'",
        "  return true",
        "end",
        "",
        "local function row_style(frame, value)",
        "  if frame.crcValid == 'false' or frame.crcValid == false then",
        "    return '#3A2028', '#FFD9DE'",
        "  end",
        "  if value and value > 100 then",
        "    return '#203A2B', '#E8FFF0'",
        "  end",
        "  return '', ''",
        "end",
        "",
        "local function auto_reply(frame, key)",
        "  -- Return hex text to reply. Empty string means no reply.",
        "  if key == '0x01.0x03' or key == '01.03' then",
        "    return 'AA 55 01 03 00'",
        "  end",
        "  return ''",
        "end",
        "",
        "function on_frame_recv(frame)",
        "  recv_count = recv_count + 1",
        "  local key = key_of(frame)",
        "  cmd_stats[key] = (cmd_stats[key] or 0) + 1",
        "",
        "  local value0 = read_data(frame.data, 0, 'uint16') or 0",
        "  local value1 = read_data(frame.data, 2, 'int16') or 0",
        "  local value2 = read_data(frame.data, 4, 'float') or 0",
        "",
        "  if not should_show(frame) then",
        "    return false",
        "  end",
        "",
        "  local rowColor, textColor = row_style(frame, value0)",
        "  local reply = auto_reply(frame, key)",
        "",
        "  log(get_timestamp() .. ' RX ' .. key .. ' data=' .. tostring(frame.data or '') .. ' value=' .. tostring(value0))",
        "",
        "  return {",
        "    show = true,",
        "    rowColor = rowColor,",
        "    textColor = textColor,",
        "    replyHex = reply,",
        "    curve = { value0, value1, value2 }",
        "  }",
        "end",
        "",
        "function on_frame_send(frame)",
        "  send_count = send_count + 1",
        "  log(get_timestamp() .. ' TX ' .. key_of(frame) .. ' data=' .. tostring(frame.data or ''))",
        "end",
        "",
        "set_timer(5000, function()",
        "  log('stats rx=' .. recv_count .. ' tx=' .. send_count .. ' err=' .. error_count)",
        "end)",
        ""
    ].join("\n")

    // Default script
    property string defaultScript:
        "-- ProtoTools Lua 脚本\n" +
        "-- ═══════════════════════════════════════════\n" +
        "-- 自动回调：\n" +
        "--   on_frame_recv(frame)  收到帧时触发\n" +
        "--   on_frame_send(frame)  发送帧时触发\n" +
        "--\n" +
        "-- frame 字段：timestamp, direction, sender,\n" +
        "--   receiver, seq, type(REQ/ACK), cmdSet,\n" +
        "--   cmdId, data, crc\n" +
        "-- ═══════════════════════════════════════════\n\n" +
        "-- 【统计变量】\n" +
        "local recv_count = 0\n" +
        "local send_count = 0\n" +
        "local error_count = 0\n" +
        "local cmd_stats   = {}\n" +
        "\n" +
        "-- 【log / print】输出到控制台\n" +
        "log(\"脚本初始化完成 - \" .. get_timestamp())\n" +
        "\n" +
        "-- 收帧回调\n" +
        "function on_frame_recv(frame)\n" +
        "  recv_count = recv_count + 1\n" +
        "  local key = frame.cmdSet .. \".\" .. frame.cmdId\n" +
        "  cmd_stats[key] = (cmd_stats[key] or 0) + 1\n" +
        "\n" +
        "  -- 【get_timestamp】获取当前时间戳\n" +
        "  local ts = get_timestamp()\n" +
        "\n" +
        "  if frame.type == \"ACK\" then\n" +
        "    log(ts .. \" [ACK] \" .. key .. \"  Data: \" .. frame.data)\n" +
        "  elseif frame.type == \"REQ\" then\n" +
        "    log(ts .. \" [REQ] \" .. key .. \" Seq=\" .. frame.seq)\n" +
        "  end\n" +
        "\n" +
        "  -- 【hex_to_bytes】十六进制转字节数组\n" +
        "  if frame.data then\n" +
        "    local bytes = hex_to_bytes(frame.data)\n" +
        "    log(\"  解析 \" .. #bytes .. \" 字节\")\n" +
        "  end\n" +
        "\n" +
        "  -- 【send】收到特定命令时自动回复\n" +
        "  if key == \"01.03\" then\n" +
        "    send(\"\\x01\\x03\\x00\\x00\\x00\\xAA\\x55\")\n" +
        "    log(\"  → 自动回复 ACK 01.03\")\n" +
        "  end\n" +
        "\n" +
        "  -- CRC 校验\n" +
        "  if frame.crc == nil or frame.crc == \"\" then\n" +
        "    error_count = error_count + 1\n" +
        "    log(\"  ⚠ CRC 校验异常!\")\n" +
        "  end\n" +
        "\n" +
        "  return true\n" +
        "end\n" +
        "\n" +
        "-- 发帧回调\n" +
        "function on_frame_send(frame)\n" +
        "  send_count = send_count + 1\n" +
        "  log(get_timestamp() .. \" [SEND] \" .. frame.cmdSet .. \".\" .. frame.cmdId)\n" +
        "\n" +
        "  -- 【bytes_to_hex】字节数组转十六进制\n" +
        "  local test_bytes = {0x48, 0x65, 0x6C, 0x6C, 0x6F}\n" +
        "  log(\"  字节转Hex: \" .. bytes_to_hex(test_bytes))\n" +
        "end\n" +
        "\n" +
        "-- 【set_timer】定时器，每 5 秒输出统计\n" +
        "set_timer(5000, function()\n" +
        "  log(\"--- 统计: 收=\" .. recv_count .. \" 发=\" .. send_count .. \" 错=\" .. error_count .. \" ---\")\n" +
        "  for cmd, cnt in pairs(cmd_stats) do\n" +
        "    log(\"  \" .. cmd .. \": \" .. cnt .. \" 次\")\n" +
        "  end\n" +
        "end)\n" +
        "\n" +
        "-- 【delay】延时（在主流程中谨慎使用）\n" +
        "-- delay(100)  -- 延时 100ms\n"
}
