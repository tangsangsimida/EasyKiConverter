import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"

/**
 * @brief 滑块UI对话框基类
 *
 * 提供共用的滑块动画、按钮容器和动画效果。
 * 子类通过 buttonSpecs 属性定义按钮，通过 extraContent 添加额外内容。
 *
 * 按钮规格格式:
 * - { text: "按钮文本", color: 颜色, action: function() }
 * - { isSeparator: true }  // 分隔线
 *
 * 共用组件:
 * - sliderBackground: 果冻滑块效果
 * - buttonBox: 按钮容器
 * - hideAnim: 关闭动画
 * - showAnim: 打开动画（hasOverlay 控制）
 */
FocusScope {
    id: root
    anchors.fill: parent
    visible: false
    z: 9999
    focus: visible
    // ===== 子类可覆盖的属性 =====
    property bool hasOverlay: false
    /** @property Component extraContentSource - 子类可通过此属性添加额外内容组件（如复选框） */
    property Component extraContentSource: null
    // ===== 共用常量 =====
    readonly property int buttonHeight: 48
    readonly property real sliderOpacity: 0.25
    // ===== 子类可使用的属性 =====
    property Item activeButton: null  // 当前悬停的按钮
    property Item selectedButton: null  // 当前键盘选中的按钮（用于 ExitDialog 键盘导航）
    property string title: qsTr("对话框")  // 对话框标题
    property string message: qsTr("提示信息")  // 对话框消息
    // ===== 内部元素引用（供子类使用）=====
    property alias dialogBox: dialogBox  // 对话框容器
    property alias dialogBoxTranslate: dialogBoxTranslate  // 对话框位移
    property alias buttonColLayout: buttonColLayout  // 按钮列布局
    // ===== 按钮规格列表 =====
    // 格式: { text, color, action } 或 { isSeparator: true }
    property list<var> buttonSpecs
    // ===== 公共函数 =====

    function updateSlider(targetButton, targetColor) {
        if (!buttonBox)
            return;
        var buttonY = targetButton.mapToItem(buttonBox, 0, 0).y;
        var centerY = buttonBox.height / 2;
        var stretchFactor = 1.0 + Math.abs(buttonY - centerY) / centerY * 0.02;
        sliderBackground.y = buttonY;
        sliderBackground.height = buttonHeight * (2.0 - stretchFactor);
        sliderBackground.color = targetColor;
        sliderBackground.opacity = sliderOpacity;
        activeButton = targetButton;
    }

    function hideSlider() {
        sliderBackground.opacity = 0;
        activeButton = null;
    }

    function open() {
        visible = true;
        if (hasOverlay) {
            showAnim.start();
        } else {
            dialogBox.rotation = 0;
            dialogBox.scale = 1.0;
            dialogBox.opacity = 1.0;
            dialogBoxTranslate.y = 0;
        }
        root.forceActiveFocus();
    }

    function close() {
        visible = false;
    }

    function closeWithAnimation() {
        hideAnim.start();
    }

    // ===== 遮罩层 =====
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: AppStyle.isDarkMode ? "#000000" : "#1e293b"
        opacity: hasOverlay ? 0 : 0
        visible: hasOverlay
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {}
        }
    }

    // ===== 对话框主体 =====
    Rectangle {
        id: dialogBox
        width: Math.min(parent.width * (hasOverlay ? 0.8 : 0.9), hasOverlay ? 400 : 440)
        implicitHeight: contentCol.implicitHeight + AppStyle.spacing.xxxl * 2
        anchors.centerIn: parent
        color: AppStyle.isDarkMode ? "#1e293b" : "#ffffff"
        radius: AppStyle.radius.xl
        scale: hasOverlay ? 0.9 : 1.0
        opacity: hasOverlay ? 0 : 1.0
        transform: Translate {
            id: dialogBoxTranslate
            y: 0
        }

        // 边框
        Rectangle {
            anchors.fill: parent
            radius: AppStyle.radius.xl
            color: "transparent"
            border.color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.1)
            border.width: 1
            z: 1
        }

        // 阴影
        Rectangle {
            anchors.fill: parent
            radius: AppStyle.radius.xl
            color: "transparent"
            z: -1
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: AppStyle.shadows.lg.color
                shadowBlur: 1.5
                shadowHorizontalOffset: AppStyle.shadows.lg.horizontalOffset
                shadowVerticalOffset: AppStyle.shadows.lg.verticalOffset
            }
        }

        ColumnLayout {
            id: contentCol
            anchors.fill: parent
            anchors.margins: AppStyle.spacing.xxxl
            spacing: AppStyle.spacing.lg
            // 标题（子类可覆盖 ID）
            Text {
                id: titleText
                Layout.fillWidth: true
                font.pixelSize: AppStyle.fontSizes.xl
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
                text: root.title
            }

            // 内容（子类可覆盖 ID）
            Text {
                id: messageText
                Layout.fillWidth: true
                font.pixelSize: AppStyle.fontSizes.md
                color: AppStyle.colors.textSecondary
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.3
                text: root.message
            }

            // 按钮组容器
            Rectangle {
                id: buttonBox
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.md
                implicitHeight: buttonColLayout.implicitHeight + AppStyle.spacing.md * 2
                color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                radius: AppStyle.radius.lg
                border.color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.08)
                border.width: 1
                // 滑块背景
                Rectangle {
                    id: sliderBackground
                    x: AppStyle.spacing.sm
                    y: AppStyle.spacing.sm
                    width: buttonBox.width - AppStyle.spacing.sm * 2
                    height: buttonHeight
                    radius: AppStyle.radius.md
                    color: AppStyle.colors.primary
                    opacity: 0
                    z: 2
                    Behavior on y {
                        enabled: sliderBackground.visible
                        SpringAnimation {
                            spring: 5.0
                            damping: 0.5
                            mass: 0.8
                            epsilon: 0.25
                        }
                    }

                    Behavior on height {
                        enabled: sliderBackground.visible
                        SpringAnimation {
                            spring: 6.0
                            damping: 0.4
                            mass: 0.7
                            epsilon: 0.25
                        }
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: 250
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on opacity {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutCubic
                        }
                    }

                    layer.enabled: sliderBackground.opacity > 0
                    layer.effect: MultiEffect {
                        shadowEnabled: true
                        shadowColor: sliderBackground.color
                        shadowBlur: 1.0
                        shadowVerticalOffset: 0
                        shadowHorizontalOffset: 0
                    }
                }

                // 按钮列
                ColumnLayout {
                    id: buttonColLayout
                    anchors.fill: parent
                    anchors.topMargin: AppStyle.spacing.sm
                    anchors.bottomMargin: AppStyle.spacing.sm
                    spacing: 0
                    z: 1
                    // 动态生成按钮
                    Repeater {
                        model: root.buttonSpecs
                        delegate: buttonDelegate
                    }
                }
            }

            // 额外内容（由子类通过 extraContentSource 设置）
            Loader {
                id: extraContentLoader
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.md
                sourceComponent: root.extraContentSource
                visible: root.extraContentSource
            }
        }
    }

    // 按钮/分隔线代理
    Component {
        id: buttonDelegate
        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: modelData.isSeparator ? 1 : buttonHeight
            property var specData: modelData  // 传递数据
            sourceComponent: modelData.isSeparator ? separatorComponent : buttonComponent
            // 暴露内部按钮引用，以便外部（ExitDialog键盘导航）可以获取实际按钮
            property Item actualButton: item
        }
    }

    // 分隔线组件
    Component {
        id: separatorComponent
        Rectangle {
            color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.06)
        }
    }

    // 按钮组件
    Component {
        id: buttonComponent
        ModernButton {
            id: btn
            // 标记自身以便父组件识别
            property bool isSliderButton: true
            // 关联的颜色 - parent是Loader，Loader.parent是ColumnLayout
            property color sliderColor: parent.specData ? parent.specData.color : AppStyle.colors.primary
            property string btnText: parent.specData ? parent.specData.text : ""
            text: btnText
            backgroundColor: "transparent"
            textColor: {
                // 悬停状态优先
                if (root.activeButton === btn) {
                    return Qt.lighter(sliderColor, 1.3);
                }
                // 键盘选中状态
                if (root.selectedButton === btn) {
                    return sliderColor;
                }
                return AppStyle.colors.textPrimary;
            }
            hoverColor: "transparent"
            Behavior on textColor {
                ColorAnimation {
                    duration: 150
                    easing.type: Easing.OutCubic
                }
            }

            scale: pressed ? 0.95 : ((root.activeButton === btn) || (root.selectedButton === btn)) ? 1.05 : 1.0
            Behavior on scale {
                NumberAnimation {
                    duration: 150
                    easing.type: Easing.OutCubic
                }
            }

            onHoveredChanged: {
                if (hovered) {
                    root.activeButton = btn;
                    root.updateSlider(btn, sliderColor);
                } else {
                    root.hideSlider();
                }
            }

            onClicked: {
                if (parent.specData && parent.specData.action) {
                    parent.specData.action();
                }
            }
        }
    }

    // ===== 打开动画 =====
    ParallelAnimation {
        id: showAnim
        running: hasOverlay
        NumberAnimation {
            target: overlay
            property: "opacity"
            from: 0
            to: 0.6
            duration: 250
            easing.type: AppStyle.easings.easeOut
        }
        NumberAnimation {
            target: dialogBox
            property: "opacity"
            from: 0
            to: 1
            duration: 250
            easing.type: AppStyle.easings.easeOut
        }
        NumberAnimation {
            target: dialogBox
            property: "scale"
            from: 0.8
            to: 1
            duration: 300
            easing.type: Easing.OutBack
        }
    }

    // ===== 关闭动画 =====
    SequentialAnimation {
        id: hideAnim
        ParallelAnimation {
            NumberAnimation {
                target: overlay
                property: "opacity"
                from: hasOverlay ? 0.6 : 0
                to: 0
                duration: 200
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: dialogBox
                property: "opacity"
                from: 1
                to: 0
                duration: 200
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: dialogBoxTranslate
                property: "y"
                from: 0
                to: root.height
                duration: 200
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: dialogBox
                property: "scale"
                from: 1
                to: 0.9
                duration: 200
                easing.type: Easing.OutQuad
            }
        }
        PropertyAction {
            target: root
            property: "visible"
            value: false
        }
    }
}
