import QtQuick
import QtQuick.Layouts
import QtQuick.Controls

Rectangle {
    color: "#171A1F"

    implicitHeight: 28

    RowLayout {
        anchors.fill: parent
        spacing: 0

        MenuBar {
            Layout.fillWidth: true

            Menu {
                title: "文件"
                Action { text: "新建项目"; shortcut: "Ctrl+N" }
                Action { text: "打开项目"; shortcut: "Ctrl+O" }
                Action { text: "保存项目" }
                MenuSeparator {}
                Action { text: "导入协议定义..." }
                Action { text: "导出监控数据..." }
                MenuSeparator {}
                Action { text: "退出"; shortcut: "Ctrl+Q" }
            }
            Menu {
                title: "编辑"
                Action { text: "撤销"; shortcut: "Ctrl+Z" }
                Action { text: "重做"; shortcut: "Ctrl+Y" }
                MenuSeparator {}
                Action { text: "清空监控"; shortcut: "Ctrl+L" }
                Action { text: "清空控制台" }
                MenuSeparator {}
                Action { text: "查找"; shortcut: "Ctrl+F" }
            }
            Menu {
                title: "工具"
                Action { text: "Hex 计算器" }
                Action { text: "CRC 计算器" }
                Action { text: "协议分析器" }
                MenuSeparator {}
                Action { text: "链路诊断" }
                Action { text: "数据回放" }
                MenuSeparator {}
                Action { text: "设置"; shortcut: "Ctrl+," }
            }
            Menu {
                title: "视图"
                Action { text: "全屏"; shortcut: "F11" }
                MenuSeparator {}
                Action { text: "切换左侧面板" }
                Action { text: "切换右侧面板" }
            }
            Menu {
                title: "帮助"
                Action { text: "使用文档"; shortcut: "F1" }
                Action { text: "快捷键速查" }
                MenuSeparator {}
                Action { text: "关于 KPtools" }
            }
        }

        Item { Layout.fillWidth: true }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#2A3037"
    }
}
