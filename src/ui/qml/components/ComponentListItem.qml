import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: item
    property string componentId
    signal deleteClicked()

    height: 48

    // 悬停效果
    color: itemMouseArea.containsMouse ? AppStyle.colors.background : AppStyle.colors.surface
    radius: AppStyle.radius.md
    border.color: AppStyle.colors.border
    border.width: 1

    Behavior on color {
        ColorAnimation {
            duration: AppStyle.durations.fast
            easing.type: AppStyle.easings.easeOut
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: AppStyle.spacing.lg
        anchors.rightMargin: AppStyle.spacing.lg
        spacing: AppStyle.spacing.md

        // 元件ID
        Text {
            Layout.fillWidth: true
            text: componentId
            font.pixelSize: AppStyle.fontSizes.sm
            font.family: "Courier New"
            color: AppStyle.colors.textPrimary
            verticalAlignment: Text.AlignVCenter
        }

        // 删除按钮
        Button {
            Layout.preferredWidth: 32
            Layout.preferredHeight: 32

            background: Rectangle {
                color: parent.pressed ? AppStyle.colors.dangerLight : 
                       parent.hovered ? AppStyle.colors.dangerLight : "transparent"
                radius: AppStyle.radius.sm

                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }

            contentItem: Text {
                text: "×"
                font.pixelSize: AppStyle.fontSizes.xxl
                font.bold: true
                color: parent.pressed ? AppStyle.colors.dangerDark : 
                       parent.hovered ? AppStyle.colors.danger : AppStyle.colors.danger
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter

                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }

            onClicked: {
                item.deleteClicked()
            }
        }
    }

    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.NoButton
    }
}