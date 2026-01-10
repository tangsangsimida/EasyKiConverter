import QtQuick
import QtQuick.Controls
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Button {
    id: root

    property string iconName: ""
    property color backgroundColor: AppStyle.colors.primary
    property color hoverColor: AppStyle.colors.primaryHover
    property color pressedColor: AppStyle.colors.primaryPressed
    property color disabledColor: AppStyle.colors.textDisabled
    property color textColor: "#ffffff"

    font.pixelSize: AppStyle.fontSizes.md
    font.bold: true

    implicitHeight: 44
    implicitWidth: contentItem.implicitWidth + AppStyle.spacing.xl * 2

    // 动画效果
    scale: root.pressed ? 0.95 : (root.hovered ? 1.02 : 1.0)

    Behavior on scale {
        NumberAnimation {
            duration: AppStyle.durations.fast
            easing.type: AppStyle.easings.easeOut
        }
    }

    background: Rectangle {
        color: !root.enabled ? disabledColor : 
               root.pressed ? pressedColor : 
               root.hovered ? hoverColor : backgroundColor
        
        radius: AppStyle.radius.md
        
        Behavior on color {
            ColorAnimation {
                duration: AppStyle.durations.fast
                easing.type: AppStyle.easings.easeOut
            }
        }
    }

    contentItem: Row {
        spacing: AppStyle.spacing.sm
        anchors.centerIn: parent

        Icon {
            id: iconItem
            iconName: root.iconName
            size: AppStyle.fontSizes.md
            iconColor: root.textColor
            visible: root.iconName.length > 0
        }

        Text {
            text: root.text
            font: root.font
            color: root.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            
            Behavior on color {
                ColorAnimation {
                    duration: AppStyle.durations.fast
                }
            }
        }
    }
}