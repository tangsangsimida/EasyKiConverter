import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0
Rectangle {
    id: card
    property alias title: titleText.text
    default property alias content: contentLayout.children
    implicitHeight: contentColumn.implicitHeight + 48
    // 根据主题模式动态计算颜色
    color: {
        if (AppStyle.isDarkMode) {
            return Qt.rgba(0, 0, 0, 0.5)
        } else {
            return Qt.rgba(255, 255, 255, 0.5)
        }
    }
    radius: AppStyle.radius.lg
    border.color: {
        if (AppStyle.isDarkMode) {
            return Qt.rgba(255, 255, 255, 0.15)
        } else {
            return Qt.rgba(0, 0, 0, 0.08)
        }
    }
    border.width: 1
    // 颜色变化动画
    Behavior on color {
        ColorAnimation {
            duration: AppStyle.durations.themeSwitch
            easing.type: AppStyle.easings.easeOut
        }
    }
    Behavior on border.color {
        ColorAnimation {
            duration: AppStyle.durations.themeSwitch
            easing.type: AppStyle.easings.easeOut
        }
    }
    // 进入动画
    opacity: 0
    scale: 0.95
    Component.onCompleted: {
        opacity = 1
        card.state = "normal"
    }
    Behavior on opacity {
        NumberAnimation {
            duration: AppStyle.durations.normal
            easing.type: AppStyle.easings.easeOut
        }
    }
    // 使用 State 管理不同的缩放状态
    states: [
        State {
            name: "normal"
            PropertyChanges { target: card; scale: 1.0 }
        },
        State {
            name: "hovered"
            PropertyChanges { target: card; scale: 1.01 }
        }
    ]
    transitions: [
        Transition {
            from: "*"; to: "*"
            NumberAnimation {
                property: "scale"
                duration: AppStyle.durations.fast
                easing.type: AppStyle.easings.easeOut
            }
        }
    ]
    MouseArea {
        id: cardMouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        onContainsMouseChanged: {
            card.state = containsMouse ? "hovered" : "normal"
        }
    }
    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: AppStyle.spacing.xl
        spacing: AppStyle.spacing.lg
        // 标题
        Text {
            id: titleText
            Layout.fillWidth: true
            font.pixelSize: AppStyle.fontSizes.xl
            font.bold: true
            color: AppStyle.colors.textPrimary
            horizontalAlignment: Text.AlignHCenter
        }
        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: AppStyle.colors.border
            visible: titleText.text.length > 0
        }
        // 内容区域
        ColumnLayout {
            id: contentLayout
            Layout.fillWidth: true
            spacing: AppStyle.spacing.md
        }
    }
}
