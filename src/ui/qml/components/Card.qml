import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: card

    // API
    property alias title: titleText.text
    default property alias content: contentLayout.children
    
    // 新增：折叠功能属性
    property bool collapsible: true
    property bool isCollapsed: false

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
        acceptedButtons: Qt.NoButton // 只处理 hover，不拦截点击
        onContainsMouseChanged: {
            card.state = containsMouse ? "hovered" : "normal"
        }
    }

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: AppStyle.spacing.xl
        spacing: AppStyle.spacing.lg

        // 标题栏区域
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: titleRow.implicitHeight

            RowLayout {
                id: titleRow
                anchors.fill: parent
                spacing: AppStyle.spacing.sm

                // 标题文本
                Text {
                    id: titleText
                    Layout.fillWidth: true
                    font.pixelSize: AppStyle.fontSizes.xl
                    font.bold: true
                    color: AppStyle.colors.textPrimary
                    horizontalAlignment: Text.AlignHCenter
                }

                // 折叠指示箭头 (仅在开启折叠且有标题时显示)
                Text {
                    id: collapseIcon
                    text: "▼" 
                    color: AppStyle.colors.textSecondary
                    font.pixelSize: AppStyle.fontSizes.md
                    visible: card.collapsible && titleText.text.length > 0
                    
                    // 旋转状态
                    rotation: card.isCollapsed ? -90 : 0
                    
                    Behavior on rotation {
                        NumberAnimation {
                            duration: AppStyle.durations.normal
                            easing.type: AppStyle.easings.easeOut
                        }
                    }
                }
            }

            // 标题栏点击区域（用于触发折叠）
            MouseArea {
                anchors.fill: parent
                enabled: card.collapsible
                cursorShape: card.collapsible ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: {
                    card.isCollapsed = !card.isCollapsed
                }
            }
        }

        // 分隔线
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: AppStyle.colors.border
            visible: titleText.text.length > 0
            opacity: card.isCollapsed ? 0 : 1
            
            Behavior on opacity {
                NumberAnimation { duration: AppStyle.durations.fast }
            }
        }

        // 内容区域包裹器（用于实现折叠动画）
        Item {
            id: contentWrapper
            Layout.fillWidth: true
            // 如果折叠，高度为0；否则为内容高度
            Layout.preferredHeight: card.isCollapsed ? 0 : contentLayout.implicitHeight
            
            clip: true // 裁剪溢出内容，确保折叠时内容不可见
            
            Behavior on Layout.preferredHeight {
                NumberAnimation {
                    duration: AppStyle.durations.normal
                    easing.type: AppStyle.easings.easeOut
                }
            }
            
            // 真正的内容布局
            ColumnLayout {
                id: contentLayout
                width: parent.width
                // 不设置 anchors.fill，让它根据内容自动撑开高度
                // 它的 implicitHeight 会被外部 Wrapper 读取
                
                spacing: AppStyle.spacing.md
                
                // 内容淡入淡出
                opacity: card.isCollapsed ? 0 : 1
                Behavior on opacity {
                    NumberAnimation {
                        duration: AppStyle.durations.fast
                        easing.type: AppStyle.easings.easeIn
                    }
                }
            }
        }
    }
}