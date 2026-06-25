import QtQuick
import QtQuick.Layouts

/**
 * 未实现驱动占位面板
 */
ColumnLayout {
    spacing: 8
    Text {
        text: "🚧 该链路类型尚未实现"
        color: "#888888"; font.pixelSize: 12
        Layout.leftMargin: 4; Layout.topMargin: 8
    }
    Text {
        text: "驱动代码将在后续版本中添加"
        color: "#555555"; font.pixelSize: 10
        Layout.leftMargin: 4
    }
}
