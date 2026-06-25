import QtQuick
import QtQuick.Window

Rectangle {
    id: titleBar
    color: "#2D2D2D"

    property var window: Window.window

    Row {
        anchors.fill: parent
        leftPadding: 12
        spacing: 8

        Repeater {
            model: ListModel {
                ListElement { hexColor: "#FF5F57"; action: "close" }
                ListElement { hexColor: "#FEBC2E"; action: "minimize" }
                ListElement { hexColor: "#28C840"; action: "maximize" }
            }
            Rectangle {
                width: 12; height: 12; radius: 6
                color: model.hexColor
                anchors.verticalCenter: parent.verticalCenter
                z: 1

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -6
                    z: 2
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (model.action === "close") window.close()
                        else if (model.action === "minimize") window.showMinimized()
                        else if (model.action === "maximize") {
                            if (window.visibility === Window.Maximized) window.showNormal()
                            else window.showMaximized()
                        }
                    }
                }
            }
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "KPtools - 协议调试工具"
            color: "#888888"
            font.pixelSize: 12
            width: parent.width - 80
            horizontalAlignment: Text.AlignHCenter
        }
    }

    // 使用系统原生拖拽 (Serial Studio 的方式)
    MouseArea {
        anchors.fill: parent
        z: -1
        onPressed: function(mouse) {
            window.startSystemMove()
        }
        onDoubleClicked: {
            if (window.visibility === Window.Maximized) window.showNormal()
            else window.showMaximized()
        }
    }
}
