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
    property int hoverScaleDelay: AppStyle.interactions.hoverIntentDelay
    property bool delayedHoverActive: false
    property real visualScale: root.pressed ? 0.95 : (root.delayedHoverActive ? 1.02 : 1.0)
    property real visualScaleOriginX: width / 2
    property real visualScaleOriginY: height / 2
    property point lastPointerPosition: Qt.point(width / 2, height / 2)
    font.pixelSize: AppStyle.fontSizes.md
    font.bold: true
    implicitHeight: 44
    implicitWidth: contentItem.implicitWidth + AppStyle.spacing.xl * 2

    HoverHandler {
        id: pointerTracker
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onPointChanged: {
            if (!root.pressed) {
                root.lastPointerPosition = point.position;
            }
        }
    }
    onPressedChanged: {
        if (pressed) {
            visualScaleOriginX = Math.max(0, Math.min(width, lastPointerPosition.x));
            visualScaleOriginY = Math.max(0, Math.min(height, lastPointerPosition.y));
        }
    }
    onHoveredChanged: {
        if (hovered && enabled) {
            hoverScaleDelayTimer.restart();
        } else {
            hoverScaleDelayTimer.stop();
            delayedHoverActive = false;
        }
    }
    onEnabledChanged: {
        if (!enabled) {
            hoverScaleDelayTimer.stop();
            delayedHoverActive = false;
        }
    }
    Timer {
        id: hoverScaleDelayTimer
        interval: root.hoverScaleDelay
        repeat: false
        onTriggered: {
            root.delayedHoverActive = root.hovered && root.enabled;
        }
    }
    Behavior on visualScale {
        NumberAnimation {
            duration: AppStyle.durations.fast
            easing.type: AppStyle.easings.easeOut
        }
    }
    background: Item {
        Rectangle {
            anchors.fill: parent
            transform: Scale {
                origin.x: root.visualScaleOriginX
                origin.y: root.visualScaleOriginY
                xScale: root.visualScale
                yScale: root.visualScale
            }
            color: !root.enabled ? disabledColor : root.pressed ? pressedColor : root.hovered ? hoverColor : backgroundColor
            radius: AppStyle.radius.md
            Behavior on color {
                ColorAnimation {
                    duration: AppStyle.durations.fast
                    easing.type: AppStyle.easings.easeOut
                }
            }
        }
    }
    contentItem: Item {
        // 关键：透传隐式尺寸，确保 Button 能正确计算大小，防止界面凌乱
        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight
        transform: Scale {
            origin.x: root.visualScaleOriginX
            origin.y: root.visualScaleOriginY
            xScale: root.visualScale
            yScale: root.visualScale
        }
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
