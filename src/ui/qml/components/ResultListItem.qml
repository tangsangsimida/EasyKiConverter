import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: item
    property string componentId
    property string status // "success", "failed", "partial"
    property string message

    height: message.length > 0 ? 72 : 48

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

        // 状态图标
        Rectangle {
            Layout.preferredWidth: 10
            Layout.preferredHeight: 10
            radius: 5
            color: {
                if (status === "success") return AppStyle.colors.success
                if (status === "failed") return AppStyle.colors.danger
                if (status === "partial") return AppStyle.colors.warning
                return AppStyle.colors.textSecondary
            }

            Behavior on color {
                ColorAnimation {
                    duration: AppStyle.durations.fast
                }
            }
        }

        // 元件ID和消息
        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.xs

            Text {
                Layout.fillWidth: true
                text: componentId
                font.pixelSize: AppStyle.fontSizes.sm
                font.bold: true
                font.family: "Courier New"
                color: AppStyle.colors.textPrimary
            }

            Text {
                Layout.fillWidth: true
                text: message
                font.pixelSize: AppStyle.fontSizes.xs
                color: AppStyle.colors.textSecondary
                visible: message.length > 0
                wrapMode: Text.WordWrap
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