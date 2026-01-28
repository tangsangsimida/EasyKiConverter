import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
    contentItem: Item {
        // 关键：透传隐式尺寸，确保 Button 能正确计算大小，防止界面凌乱
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight
        
        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: AppStyle.spacing.sm
            
            // 只有当图标存在时才显示并占用空间
            Icon {
                id: iconItem
                anchors.verticalCenter: parent.verticalCenter
                iconName: root.iconName
                size: 28
                iconColor: root.textColor
                visible: root.iconName.length > 0
            }
            
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.text
                font: root.font
                color: root.textColor
                horizontalAlignment: Text.AlignLeft
                verticalAlignment: Text.AlignVCenter
                
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
        }
    }
}