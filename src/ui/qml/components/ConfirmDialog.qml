import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"

/**
 * @brief 转换进行中强制退出确认对话框
 *
 * **用途**：当用户尝试关闭窗口且转换任务正在进行时显示
 * **场景**：用户点击关闭按钮 → 检测到 exportProgressViewModel.isExporting 为 true → 显示此对话框
 * **提示信息**："转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？"
 * **按钮**：
 *   - 确定（强制退出）：停止转换任务，关闭程序
 *   - 取消（继续转换）：关闭对话框，继续执行转换任务
 * **UI 设计**：使用滑块UI样式，与 ExitDialog 保持一致
 *   - 悬停时滑块移动到对应按钮位置
 *   - 弹簧动画效果（果冻拉伸）
 *   - 动态文本颜色变化
 *   - 默认聚焦在取消按钮上
 * **动画**：取消按钮点击后向下滑动消失（fade out + slide down + scale down）
 *
 * **注意**：不要与 ExitDialog 混淆
 * - ExitDialog：无任务时显示，提供"最小化到托盘"/"退出程序"/"取消"选项
 * - ConfirmDialog：有任务时显示，提供"强制退出"/"继续转换"选项
 */
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 9999 // 确保在最顶层
    clip: false // 允许阴影向外扩散
    focus: visible // 可见时获取焦点以监听键盘
    // 滑块UI相关属性
    property Item activeButton: null  // 当前活跃的按钮
    // 监听 Escape 键
    Keys.onEscapePressed: {
        root.rejected();
        root.closeWithAnimation();
    }

    // 暴露属性和信号
    property alias title: titleText.text
    property alias message: messageText.text
    property string confirmText: qsTr("确定")
    property string cancelText: qsTr("取消")
    property color confirmColor: AppStyle.colors.danger // 默认退出是危险操作
    property int radius: AppStyle.radius.lg
    signal accepted
    signal rejected
    // 更新滑块位置
    function updateSlider(targetButton, targetColor) {
        // 计算按钮在 buttonBox 中的相对位置
        var buttonY = targetButton.mapToItem(buttonBox, 0, 0).y;
        // 果冻拉伸效果：向下移动时轻微压缩高度
        var centerY = buttonBox.height / 2;
        var stretchFactor = 1.0 + Math.abs(buttonY - centerY) / centerY * 0.02;
        sliderBackground.y = buttonY;
        sliderBackground.height = buttonBox.buttonHeight * (2.0 - stretchFactor);
        sliderBackground.color = targetColor;
        sliderBackground.opacity = buttonBox.sliderOpacity;
        // 更新活跃按钮
        activeButton = targetButton;
    }

    // 隐藏滑块
    function hideSlider() {
        sliderBackground.opacity = 0;
        // 清除活跃按钮
        activeButton = null;
    }
    // 打开/关闭
    function open() {
        visible = true;
        // 重置对话框位置和 transform
        dialogBox.y = 0;
        dialogBoxTranslate.y = 0;
        showAnim.start();
        // 自动聚焦到取消按钮，确保键盘操作安全性
        cancelButton.forceActiveFocus();
        // 设置滑块默认聚焦在取消选项上
        updateSlider(cancelButton, AppStyle.colors.textSecondary);
    }

    function close() {
        // 直接隐藏对话框，不播放动画
        visible = false;
    }

    function closeWithAnimation() {
        // 播放消失动画
        hideAnim.start();
    }

    // 遮罩层 (半透明磨砂感)
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: AppStyle.isDarkMode ? "#000000" : "#1e293b"
        opacity: 0
        radius: root.radius
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {} // 拦截点击，防止穿透
        }
    }

    // 对话框主体
    Rectangle {
        id: dialogBox
        width: Math.min(parent.width * 0.8, 400)
        implicitHeight: contentCol.implicitHeight + AppStyle.spacing.xxxl * 2
        anchors.centerIn: parent
        color: AppStyle.colors.surface
        radius: AppStyle.radius.xl
        scale: 0.9
        opacity: 0
        y: 0
        transform: Translate {
            id: dialogBoxTranslate
            y: 0
        }

        // 阴影效果
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: AppStyle.shadows.lg.color
            shadowBlur: 1.0
            shadowHorizontalOffset: AppStyle.shadows.lg.horizontalOffset
            shadowVerticalOffset: AppStyle.shadows.lg.verticalOffset
        }

        ColumnLayout {
            id: contentCol
            anchors.fill: parent
            anchors.margins: AppStyle.spacing.xxxl
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

            // 内容消息
            Text {
                id: messageText
                Layout.fillWidth: true
                font.pixelSize: AppStyle.fontSizes.md
                color: AppStyle.colors.textSecondary
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.2
            }

            // 按钮组容器
            Rectangle {
                id: buttonBox
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.md
                implicitHeight: buttonCol.implicitHeight + AppStyle.spacing.md * 2
                color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(0, 0, 0, 0.03)
                radius: AppStyle.radius.lg
                border.color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.08)
                border.width: 1
                // 常量
                readonly property int buttonHeight: 48
                readonly property real sliderOpacity: 0.25
                // 滑块背景（果冻效果）
                Rectangle {
                    id: sliderBackground
                    x: AppStyle.spacing.sm
                    y: AppStyle.spacing.sm
                    width: buttonBox.width - AppStyle.spacing.sm * 2
                    height: buttonBox.buttonHeight
                    radius: AppStyle.radius.md
                    color: root.confirmColor
                    opacity: 0
                    z: 2
                    // Q弹动画（弹簧效果 - 垂直移动）
                    Behavior on y {
                        enabled: sliderBackground.visible
                        SpringAnimation {
                            spring: 5.0
                            damping: 0.5
                            mass: 0.8
                            epsilon: 0.25
                        }
                    }

                    // Q弹动画（弹簧效果 - 高度变化，果冻拉伸）
                    Behavior on height {
                        enabled: sliderBackground.visible
                        SpringAnimation {
                            spring: 6.0
                            damping: 0.4
                            mass: 0.7
                            epsilon: 0.25
                        }
                    }

                    // 颜色渐变动画
                    Behavior on color {
                        ColorAnimation {
                            duration: 250
                            easing.type: Easing.OutCubic
                        }
                    }

                    // 淡入淡出动画
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.OutCubic
                        }
                    }

                    // 发光效果（只在可见时启用以提高性能）
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
                    id: buttonCol
                    anchors.fill: parent
                    anchors.topMargin: AppStyle.spacing.sm
                    anchors.bottomMargin: AppStyle.spacing.sm
                    spacing: 0
                    z: 1
                    // 取消按钮
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: buttonBox.buttonHeight
                        ModernButton {
                            id: cancelButton
                            anchors.fill: parent
                            text: root.cancelText
                            backgroundColor: "transparent"
                            textColor: root.activeButton === cancelButton ? Qt.lighter(AppStyle.colors.textSecondary, 1.5) : AppStyle.colors.textPrimary
                            hoverColor: "transparent"
                            Behavior on textColor {
                                ColorAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            scale: pressed ? 0.95 : (root.activeButton === cancelButton) ? 1.05 : 1.0
                            Behavior on scale {
                                NumberAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            onHoveredChanged: {
                                if (hovered) {
                                    root.updateSlider(this, AppStyle.colors.textSecondary);
                                } else if (!confirmButton.hovered && !pressed) {
                                    root.hideSlider();
                                }
                            }

                            onClicked: {
                                root.rejected();
                                root.closeWithAnimation();
                            }
                        }
                    }

                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                    }

                    // 确定按钮
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: buttonBox.buttonHeight
                        ModernButton {
                            id: confirmButton
                            anchors.fill: parent
                            text: root.confirmText
                            backgroundColor: "transparent"
                            textColor: root.activeButton === confirmButton ? Qt.lighter(root.confirmColor, 1.3) : AppStyle.colors.textPrimary
                            hoverColor: "transparent"
                            Behavior on textColor {
                                ColorAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            scale: pressed ? 0.95 : (root.activeButton === confirmButton) ? 1.05 : 1.0
                            Behavior on scale {
                                NumberAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            onHoveredChanged: {
                                if (hovered) {
                                    root.updateSlider(this, root.confirmColor);
                                } else if (!cancelButton.hovered && !pressed) {
                                    root.hideSlider();
                                }
                            }

                            onClicked: {
                                root.accepted();
                                root.close();
                            }
                        }
                    }
                }
            }
        }
    }

    // 动画定义
    ParallelAnimation {
        id: showAnim
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

    SequentialAnimation {
        id: hideAnim
        ParallelAnimation {
            NumberAnimation {
                target: overlay
                property: "opacity"
                from: 0.6
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
