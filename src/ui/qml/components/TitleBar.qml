import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: titleBar
    // 外部属性
    property int windowRadius: 0
    width: parent.width
    height: 38
    color: AppStyle.colors.surface
    topLeftRadius: windowRadius
    topRightRadius: windowRadius
    z: 1000
    // Bottom separator line
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: AppStyle.colors.border
    }

    // 拖动区域
    MouseArea {
        anchors.fill: parent
        property point clickPos: "0,0"
        onPressed: mouse => {
            clickPos = Qt.point(mouse.x, mouse.y);
            if (Window.window.visibility === Window.Maximized) {
                var ratio = mouse.x / width;
                Window.window.showNormal();
                Window.window.x = mouse.screenX - (Window.window.width * ratio);
                Window.window.y = mouse.screenY - (mouse.y);
            }
            Window.window.startSystemMove();
        }
        onDoubleClicked: {
            if (Window.window.visibility === Window.Maximized) {
                Window.window.showNormal();
            } else {
                Window.window.showMaximized();
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        // 图标
        Image {
            source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/app_icon.png"
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            Layout.leftMargin: 10
            fillMode: Image.PreserveAspectFit
            mipmap: true
        }

        // 标题
        Text {
            text: "EasyKiConverter"
            color: AppStyle.colors.textPrimary
            font.pixelSize: 13
            font.bold: true
            Layout.leftMargin: 12
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
        }

        // 窗口控制按钮
        Row {
            Layout.alignment: Qt.AlignRight
            // 最小化
            Button {
                width: 46
                height: 38
                flat: true
                icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/minimize.svg"
                icon.color: "transparent"
                icon.width: 10
                icon.height: 10
                background: Rectangle {
                    color: parent.hovered ? (AppStyle.isDarkMode ? "#1affffff" : "#1a000000") : "transparent"
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                }

                onClicked: minimizeAnim.start()
            }

            // 最小化动画
            NumberAnimation {
                id: minimizeAnim
                target: Window.window
                property: "opacity"
                to: 0
                duration: 250
                easing.type: Easing.OutQuart
                onFinished: {
                    Window.window.showMinimized();
                    // 恢复不透明度
                    restoreTimer.start()
                }
            }

            Timer {
                id: restoreTimer
                interval: 300
                onTriggered: Window.window.opacity = 1
            }

            // 最大化/还原
            Button {
                width: 46
                height: 38
                flat: true
                icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/maximize.svg"
                icon.color: "transparent"
                icon.width: 10
                icon.height: 10
                background: Rectangle {
                    color: parent.hovered ? (AppStyle.isDarkMode ? "#1affffff" : "#1a000000") : "transparent"
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                }

                onClicked: {
                    if (Window.window.visibility === Window.Maximized) {
                        Window.window.showNormal();
                    } else {
                        Window.window.showMaximized();
                    }
                }
            }

            // 关闭
            Button {
                width: 46
                height: 38
                flat: true
                icon.source: "qrc:/qt/qml/EasyKiconverter_Cpp_Version/resources/icons/close.svg"
                icon.color: hovered ? "white" : "transparent"
                icon.width: 10
                icon.height: 10
                background: Rectangle {
                    color: parent.hovered ? "#c42b1c" : "transparent"
                    Behavior on color {
                        ColorAnimation {
                            duration: 150
                        }
                    }
                }

                onClicked: Window.window.close()
            }
        }
    }
}
