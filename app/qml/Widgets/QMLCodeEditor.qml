import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import ProtoDebug

Rectangle {
    id: root
    color: "#1E1E1E"

    property bool consoleVisible: false
    property bool scriptRunning: luaEngine && luaEngine.running
    // 行号垂直偏移微调（像素），正值向下，负值向上
    property real lineNumberYOffset: 2
    // 行间距（像素），需要和 TextEdit 的 lineSpacing 匹配
    property real editorLineSpacing: 5

    // 标签页数据
    ListModel { id: tabModel }
    property int currentTabIndex: 0
    property int nextEditorId: 1

    // 高亮器工厂
    Component { id: hlFactory; QMLLuaHighlighter {} }

    Component.onCompleted: { addNewTab("script_1.lua", "", defaultScript) }

    function addNewTab(title, filePath, content) {
        var eid = nextEditorId++
        tabModel.append({ title: title, filePath: filePath, content: content, modified: false, editorId: eid })
        currentTabIndex = tabModel.count - 1
    }

    function closeTab(index) {
        if (tabModel.count <= 1) return
        tabModel.remove(index)
        if (currentTabIndex >= tabModel.count) currentTabIndex = tabModel.count - 1
    }

    function markModified(index) { if (index >= 0 && index < tabModel.count) tabModel.setProperty(index, "modified", true) }
    function markSaved(index) { if (index >= 0 && index < tabModel.count) tabModel.setProperty(index, "modified", false) }

    // 保存确认对话框
    Dialog {
        id: saveConfirmDialog
        title: "保存更改"; modal: true; anchors.centerIn: Overlay.overlay
        standardButtons: Dialog.Save | Dialog.Discard | Dialog.Cancel
        Label { text: "脚本已修改，是否保存？"; color: "#D4D4D4" }
        onAccepted: { doSaveFile(); closeTab(pendingCloseIndex) }
        onDiscarded: closeTab(pendingCloseIndex)
        property int pendingCloseIndex: -1
    }

    function requestCloseTab(index) {
        if (tabModel.get(index).modified) { saveConfirmDialog.pendingCloseIndex = index; saveConfirmDialog.open() }
        else closeTab(index)
    }

    function loadFile() {
        if (scriptRunning) return
        var path = luaEngine.openLuaFileDialog()
        if (path.length > 0) { addNewTab(path.split('/').pop(), path, luaEngine.readTextFile(path)) }
    }

    function doSaveFile() {
        if (scriptRunning) return
        var idx = currentTabIndex
        if (idx < 0 || idx >= tabModel.count) return
        var tab = tabModel.get(idx)
        var editor = getEditorForTab(idx)
        if (!editor) return
        var path = tab.filePath
        if (path.length === 0) {
            path = luaEngine.saveLuaFileDialog(tab.title)
            if (path.length === 0) return
            tabModel.setProperty(idx, "filePath", path)
            tabModel.setProperty(idx, "title", path.split('/').pop())
        }
        luaEngine.writeTextFile(path, editor.text)
        markSaved(idx)
    }

    function getEditorForTab(index) {
        if (index < 0 || index >= tabModel.count) return null
        var eid = tabModel.get(index).editorId
        for (var i = 0; i < editorRepeater.count; i++) {
            var item = editorRepeater.itemAt(i)
            if (item && item.editorId === eid) return item.editor
        }
        return null
    }

    ColumnLayout {
        anchors.fill: parent; spacing: 0

        // Tab Bar
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 28; color: "#2D2D2D"
            RowLayout {
                anchors.fill: parent; spacing: 0

                Repeater {
                    model: tabModel
                    Rectangle {
                        Layout.preferredWidth: tabLabel.implicitWidth + 50; Layout.fillHeight: true
                        color: index === currentTabIndex ? "#1E1E1E" : "#2D2D2D"
                        Rectangle { width: parent.width; height: 2; color: "#007ACC"; anchors.top: parent.top; visible: index === currentTabIndex }
                        RowLayout {
                            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 4; spacing: 4
                            Text { id: tabLabel; text: (model.modified ? "● " : "") + model.title; color: index === currentTabIndex ? "#FFF" : "#AAA"; font.pixelSize: 11 }
                            Item { Layout.fillWidth: true }
                            Rectangle {
                                Layout.preferredWidth: 16; Layout.preferredHeight: 16; radius: 8
                                color: closeMouse.containsMouse ? "#444" : "transparent"
                                Text { anchors.centerIn: parent; text: "×"; color: "#888"; font.pixelSize: 12 }
                                MouseArea { id: closeMouse; anchors.fill: parent; hoverEnabled: true; onClicked: requestCloseTab(index) }
                            }
                        }
                        MouseArea { anchors.fill: parent; anchors.rightMargin: 20; onClicked: if (!scriptRunning) currentTabIndex = index }
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 28; Layout.fillHeight: true
                    color: plusMouse.containsMouse ? "#333" : "transparent"
                    Text { anchors.centerIn: parent; text: "+"; color: "#888"; font.pixelSize: 16 }
                    MouseArea {
                        id: plusMouse; anchors.fill: parent; hoverEnabled: true
                        onClicked: { if (scriptRunning) return; addNewTab("NewScript", "", "") }
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }

        // Editor Area
        Item {
            Layout.fillWidth: true; Layout.fillHeight: true

            Repeater {
                id: editorRepeater
                model: tabModel

                Item {
                    visible: index === currentTabIndex
                    anchors.fill: parent

                    property int editorId: model.editorId
                    property var editor: codeEdit

                    // 代码编辑器（TextEdit + Flickable，完全控制坐标系）
                    Flickable {
                        id: codeFlick
                        anchors.fill: parent
                        clip: true
                        contentWidth: codeEdit.contentWidth + lineGutter.width
                        contentHeight: codeEdit.contentHeight
                        flickableDirection: Flickable.VerticalFlick
                        boundsBehavior: Flickable.StopAtBounds

                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        // 自动跟随光标滚动
                        property bool userScrolled: false
                        onContentYChanged: userScrolled = true

                        Row {
                            // 行号（精确匹配 TextEdit 文字位置）
                            Item {
                                id: lineGutter
                                width: 48; height: codeEdit.height
                                clip: true

                                Column {
                                    id: lineCol
                                    Repeater {
                                        model: codeEdit.lineCount
                                        Item {
                                            width: lineGutter.width
                                            // 行高 = 实测行高（cursorRectangle 差值）
                                            height: codeEdit.measuredLineHeight

                                            Text {
                                                anchors.right: parent.right
                                                anchors.rightMargin: 8
                                                anchors.top: parent.top
                                                // 微调行号与代码文字对齐
                                                anchors.topMargin: root.lineNumberYOffset
                                                text: (index + 1).toString()
                                                color: index === codeEdit.currentLine ? "#FFF" : "#555"
                                                font.pixelSize: codeEdit.font.pixelSize
                                                font.family: codeEdit.font.family
                                            }
                                        }
                                    }
                                }
                            }

                            // 代码编辑区（TextEdit 不自带滚动，高度=内容高度）
                            TextEdit {
                                id: codeEdit
                                width: Math.max(codeFlick.width - lineGutter.width, contentWidth + 20)
                                height: contentHeight
                                color: "#D4D4D4"
                                font.pixelSize: 12; font.family: "Menlo"
                                selectionColor: "#264F78"; selectedTextColor: "#FFFFFF"
                                wrapMode: TextEdit.NoWrap
                                selectByMouse: true
                                focus: visible
                                cursorVisible: true
                                tabStopDistance: 48
                                text: model.content

                                property int lineCount: text.split('\n').length
                                property int currentLine: {
                                    var before = text.substring(0, cursorPosition)
                                    return before.split('\n').length - 1
                                }

                                // 行高直接取 cursorRectangle.height
                                property int measuredLineHeight: Math.round(cursorRectangle.height) || 18
                                Timer {
                                    running: true; interval: 100; repeat: false
                                    onTriggered: codeEdit.measuredLineHeight = Math.round(codeEdit.cursorRectangle.height) || 18
                                }

                                // 光标变化时自动滚动到可见区域
                                onCursorPositionChanged: {
                                    if (!codeFlick.userScrolled) {
                                        var cy = cursorRectangle.y
                                        var vh = codeFlick.height
                                        if (cy < codeFlick.contentY)
                                            codeFlick.contentY = cy
                                        else if (cy + cursorRectangle.height > codeFlick.contentY + vh)
                                            codeFlick.contentY = cy + cursorRectangle.height - vh
                                    }
                                }

                                onTextChanged: {
                                    codeFlick.userScrolled = false
                                    if (text !== tabModel.get(index).content) {
                                        tabModel.setProperty(index, "content", text)
                                        markModified(index)
                                    }
                                }

                                property var _hlObj: null
                                Component.onCompleted: {
                                    if (textDocument) _hlObj = hlFactory.createObject(codeEdit, {"document": textDocument})
                                }
                                Component.onDestruction: {
                                    if (_hlObj) { _hlObj.document = null; _hlObj.destroy(); _hlObj = null }
                                }


                                Keys.onReturnPressed: (event) => {
                                    var before = text.substring(0, cursorPosition)
                                    var lastLine = before.split('\n').pop()
                                    var indent = ""
                                    for (var i = 0; i < lastLine.length; i++) {
                                        if (lastLine[i] === ' ' || lastLine[i] === '\t') indent += lastLine[i]
                                        else break
                                    }
                                    var t = lastLine.replace(/\s+$/, "")
                                    if (t.endsWith("then") || t.endsWith("do") || t.endsWith("function") || t.endsWith("else") || t.endsWith("elseif") || t.endsWith("repeat"))
                                        indent += "    "
                                    insert(cursorPosition, "\n" + indent)
                                    event.accepted = true
                                }
                                Keys.onTabPressed: (event) => { insert(cursorPosition, "    "); event.accepted = true }
                            }
                        }
                    }
                }
            }
        }

        // Console
        Rectangle {
            Layout.fillWidth: true
            Layout.minimumHeight: consoleVisible ? 50 : 0
            Layout.preferredHeight: consoleVisible ? 100 : 0
            visible: consoleVisible
            color: "#1A1A1A"; border.color: "#333"; border.width: 1
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 2; spacing: 0
                Rectangle {
                    Layout.fillWidth: true; Layout.preferredHeight: 18; color: "#252526"
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6
                        Text { text: "🖥️ Lua 控制台"; color: "#007ACC"; font.pixelSize: 10 }
                        Item { Layout.fillWidth: true }
                        Text { text: scriptRunning ? "运行中 ●" : "已停止 ○"; color: scriptRunning ? "#4EC9B0" : "#888"; font.pixelSize: 9 }
                    }
                }
                ListView {
                    Layout.fillWidth: true; Layout.fillHeight: true; clip: true; model: consoleModel; currentIndex: -1
                    onCountChanged: if (count > 0) positionViewAtEnd()
                    delegate: Rectangle {
                        width: parent.width; height: 18; color: index % 2 === 0 ? "#1A1A1A" : "#1E1E1E"
                        Text { anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter; text: model.text || ""; color: model.isError ? "#F44747" : "#AAAAAA"; font.pixelSize: 10; font.family: "Menlo" }
                    }
                }
            }
        }

        // Toolbar
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 28; color: "#252526"
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 4
                Button {
                    text: scriptRunning ? "⏹ 停止" : "▶ 运行"
                    font.pixelSize: 11; padding: 3; leftPadding: 8; rightPadding: 8
                    background: Rectangle { radius: 3; color: scriptRunning ? "#C53030" : "#16825D" }
                    contentItem: Text { text: parent.text; color: "#FFF"; font: parent.font }
                    onClicked: {
                        var editor = getEditorForTab(currentTabIndex)
                        if (!editor) return
                        if (scriptRunning) luaEngine.stop()
                        else { luaEngine.loadScript(editor.text); luaEngine.start() }
                    }
                }
                Button {
                    text: "📂 加载"; enabled: !scriptRunning
                    font.pixelSize: 11; padding: 3; leftPadding: 8; rightPadding: 8
                    background: Rectangle { radius: 3; color: enabled ? "#333" : "#222"; border.color: "#555" }
                    contentItem: Text { text: parent.text; color: enabled ? "#CCC" : "#555"; font: parent.font }
                    onClicked: loadFile()
                }
                Button {
                    text: "💾 保存"; enabled: !scriptRunning
                    font.pixelSize: 11; padding: 3; leftPadding: 8; rightPadding: 8
                    background: Rectangle { radius: 3; color: enabled ? "#333" : "#222"; border.color: "#555" }
                    contentItem: Text { text: parent.text; color: enabled ? "#CCC" : "#555"; font: parent.font }
                    onClicked: doSaveFile()
                }
                Item { Layout.fillWidth: true }
                Text {
                    text: tabModel.count > 0 && currentTabIndex < tabModel.count ? (tabModel.get(currentTabIndex).filePath || "(新文件)") : ""
                    color: "#555"; font.pixelSize: 9; elide: Text.ElideLeft; Layout.maximumWidth: 300
                }
                Button {
                    text: consoleVisible ? "🖥️ 隐藏" : "🖥️ 控制台"
                    font.pixelSize: 11; padding: 3; leftPadding: 8; rightPadding: 8
                    background: Rectangle { radius: 3; color: consoleVisible ? "#094771" : "#333"; border.color: "#555" }
                    contentItem: Text { text: parent.text; color: "#CCC"; font: parent.font }
                    onClicked: consoleVisible = !consoleVisible
                }
            }
        }
    }

    ListModel { id: consoleModel }
    Connections {
        target: luaEngine
        function onConsoleOutput(msg) { consoleModel.append({ text: msg, isError: false }); if (consoleModel.count > 500) consoleModel.remove(0, 100) }
        function onErrorOccurred(err) { consoleModel.append({ text: "[ERROR] " + err, isError: true }) }
    }

    property string defaultScript:
        '-- KPtools Lua 脚本\n' +
        '-- 收到帧数据时自动调用 on_frame_recv(frame)\n' +
        '-- 发送帧数据时自动调用 on_frame_send(frame)\n\n' +
        'local ACK_COUNT = 0\n' +
        'local REQ_COUNT = 0\n' +
        'local ERROR_COUNT = 0\n\n' +
        'local cmd_stats = {}\n\n' +
        'function on_frame_recv(frame)\n' +
        '  ACK_COUNT = ACK_COUNT + 1\n\n' +
        '  local key = frame.cmdSet .. frame.cmdId\n' +
        '  cmd_stats[key] = (cmd_stats[key] or 0) + 1\n\n' +
        '  if frame.type == "ACK" then\n' +
        '    log("收到 ACK: " .. key)\n' +
        '    log("  Data: " .. frame.data)\n' +
        '  elseif frame.type == "REQ" then\n' +
        '    REQ_COUNT = REQ_COUNT + 1\n' +
        '    log("收到 REQ: " .. key .. " Seq=" .. frame.seq)\n' +
        '  end\n\n' +
        '  if frame.crc == nil then\n' +
        '    ERROR_COUNT = ERROR_COUNT + 1\n' +
        '    log("[ERROR] CRC 校验失败!")\n' +
        '  end\n\n' +
        '  return true\n' +
        'end\n\n' +
        'function on_frame_send(frame)\n' +
        '  log("发送: " .. frame.cmdSet .. frame.cmdId)\n' +
        'end\n'
}
