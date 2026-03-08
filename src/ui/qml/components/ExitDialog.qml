import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"

/**
 * @brief 退出选项对话框（无任务时显示）
 *
 * **用途**：当用户点击关闭按钮且无正在进行的任务时显示
 * **场景**：用户点击关闭按钮 → 检测到 exportProgressViewModel.isExporting 为 false → 显示此对话框
 * **选项**：
 *   - 最小化到托盘：最小化窗口到系统托盘，保持后台运行
 *   - 退出程序：完全退出应用程序
 *   - 取消：关闭对话框，继续使用程序
 * **记住选择**：用户可以选择"记住我的选择"，下次自动执行相同操作
 * **动画**：取消按钮点击后向下滑动消失（fade out + slide down + scale down）
 *
 * **注意**：不要与 ConfirmDialog 混淆
 * - ExitDialog：无任务时显示，提供"最小化到托盘"/"退出程序"/"取消"选项
 * - ConfirmDialog：有任务时显示，提供"强制退出"/"继续转换"选项
 */
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 9999

    // 常量
    readonly property int buttonHeight: 48
    readonly property real sliderOpacity: 0.25

    // 属性
    property bool rememberChoice: false
    property Item activeButton: null  // 当前活跃的按钮
    property Item selectedButton: exitButton  // 当前键盘选中的按钮，默认为退出按钮

    // 信号
    signal minimizeToTray(bool remember)
    signal exitApp(bool remember)
    signal canceled

    // 更新滑块位置
    function updateSlider(targetButton, targetColor) {
        // 计算按钮在 buttonBox 中的相对位置
        var buttonY = targetButton.mapToItem(buttonBox, 0, 0).y;

        // 果冻拉伸效果：向下移动时轻微压缩高度
        var centerY = buttonBox.height / 2;
        var stretchFactor = 1.0 + Math.abs(buttonY - centerY) / centerY * 0.02;

        sliderBackground.y = buttonY;
        sliderBackground.height = buttonHeight * (2.0 - stretchFactor);  // 使用常量
        sliderBackground.color = targetColor;
        sliderBackground.opacity = sliderOpacity;  // 使用常量

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
        // 直接设置对话框为完全显示状态，不播放打开动画
        dialogBox.rotation = 0;
        dialogBox.scale = 1.0;
        dialogBox.opacity = 1.0;
        dialogBoxTranslate.y = 0;
        // 设置默认选中为退出按钮
        selectedButton = exitButton;
        // 强制焦点到 root 以便键盘导航
        root.forceActiveFocus();
    }

    function close() {
        visible = false;
    }

    function closeWithAnimation() {
        hideAnim.start();
    }

    // 对话框主体
    Rectangle {
        id: dialogBox
        width: Math.min(parent.width * 0.9, 440)
        implicitHeight: contentCol.implicitHeight + AppStyle.spacing.xxxl * 2
        anchors.centerIn: parent
        color: AppStyle.isDarkMode ? "#1e293b" : "#ffffff"
        radius: AppStyle.radius.xl
        scale: 1.0
        opacity: 1.0
        rotation: 0  // 添加旋转属性

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

            // 标题
            Text {
                text: qsTr("关闭程序")
                Layout.fillWidth: true
                font.pixelSize: AppStyle.fontSizes.xl
                font.bold: true
                color: AppStyle.colors.textPrimary
                horizontalAlignment: Text.AlignHCenter
            }

            // 提示内容
            Text {
                text: qsTr("您可以选择最小化到系统托盘以保持后台运行，或者完全退出程序。")
                Layout.fillWidth: true
                font.pixelSize: AppStyle.fontSizes.md
                color: AppStyle.colors.textSecondary
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
                lineHeight: 1.3
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

                // 滑块背景（果冻效果）
                Rectangle {
                    id: sliderBackground
                    x: AppStyle.spacing.sm
                    y: AppStyle.spacing.sm
                    width: buttonBox.width - AppStyle.spacing.sm * 2
                    height: buttonHeight  // 使用常量
                    radius: AppStyle.radius.md
                    color: AppStyle.colors.primary
                    opacity: 0
                    z: 2

                    // Q弹动画（弹簧效果 - 垂直移动）
                    Behavior on y {
                        enabled: sliderBackground.visible
                        SpringAnimation {
                            spring: 5.0
                            damping: 0.5
                            mass: 0.8
                            epsilon: 0.25  // 防止在微小像素点上持续计算
                        }
                    }

                    // Q弹动画（弹簧效果 - 高度变化，果冻拉伸）
                    Behavior on height {
                        enabled: sliderBackground.visible
                        SpringAnimation {
                            spring: 6.0  // 稍微更强的弹簧，让拉伸更有弹性
                            damping: 0.4  // 更低的阻尼，让效果更明显
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

                    // 最小化到托盘
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: buttonHeight  // 使用常量

                        ModernButton {
                            id: minimizeButton
                            anchors.fill: parent
                            text: qsTr("最小化到托盘")
                            backgroundColor: "transparent"
                            // 动态文本颜色：当滑块在上方时变亮，键盘选中时使用不同颜色
                            textColor: root.activeButton === minimizeButton ? Qt.lighter(AppStyle.colors.primary, 1.3) : (root.selectedButton === minimizeButton ? AppStyle.colors.primary : AppStyle.colors.text)
                            hoverColor: "transparent"

                            // 文本颜色渐变动画
                            Behavior on textColor {
                                ColorAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            // 缩放效果：当滑块在上方时轻微放大，键盘选中时也放大
                            scale: (root.activeButton === minimizeButton || root.selectedButton === minimizeButton) ? 1.05 : 1.0
                            Behavior on scale {
                                NumberAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            onHoveredChanged: {
                                if (hovered) {
                                    root.updateSlider(this, AppStyle.colors.primary);
                                } else if (!exitButton.hovered && !cancelButton.hovered) {
                                    root.hideSlider();
                                }
                            }

                            onClicked: {
                                root.minimizeToTray(rememberChoice);
                            }
                        }
                    }

                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                    }

                    // 退出程序
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: buttonHeight  // 使用常量

                        ModernButton {
                            id: exitButton
                            anchors.fill: parent
                            text: qsTr("退出程序")
                            backgroundColor: "transparent"
                            // 动态文本颜色：当滑块在上方时变亮
                            textColor: root.activeButton === exitButton ? Qt.lighter(AppStyle.colors.danger, 1.3) : (root.selectedButton === exitButton ? AppStyle.colors.danger : AppStyle.colors.text)
                            hoverColor: "transparent"

                            // 文本颜色渐变动画
                            Behavior on textColor {
                                ColorAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            // 缩放效果：当滑块在上方时轻微放大，键盘选中时也放大
                            scale: (root.activeButton === exitButton || root.selectedButton === exitButton) ? 1.05 : 1.0
                            Behavior on scale {
                                NumberAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            onHoveredChanged: {
                                if (hovered) {
                                    root.updateSlider(this, AppStyle.colors.danger);
                                } else if (!minimizeButton.hovered && !cancelButton.hovered) {
                                    root.hideSlider();
                                }
                            }

                            onClicked: {
                                root.exitApp(rememberChoice);
                            }
                        }
                    }

                    // 分隔线
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 1
                        color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.08) : Qt.rgba(0, 0, 0, 0.06)
                    }

                    // 取消
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: buttonHeight  // 使用常量

                        ModernButton {
                            id: cancelButton
                            anchors.fill: parent
                            text: qsTr("取消")
                            backgroundColor: "transparent"
                            // 动态文本颜色：当滑块在上方时变亮
                            textColor: root.activeButton === cancelButton ? Qt.lighter(AppStyle.colors.textSecondary, 1.5) : (root.selectedButton === cancelButton ? AppStyle.colors.textSecondary : AppStyle.colors.text)
                            hoverColor: "transparent"

                            // 文本颜色渐变动画
                            Behavior on textColor {
                                ColorAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            // 缩放效果：当滑块在上方时轻微放大，键盘选中时也放大
                            scale: (root.activeButton === cancelButton || root.selectedButton === cancelButton) ? 1.05 : 1.0
                            Behavior on scale {
                                NumberAnimation {
                                    duration: 150
                                    easing.type: Easing.OutCubic
                                }
                            }

                            onHoveredChanged: {
                                if (hovered) {
                                    root.updateSlider(this, AppStyle.colors.textSecondary);
                                } else if (!minimizeButton.hovered && !exitButton.hovered) {
                                    root.hideSlider();
                                }
                            }

                            onClicked: {
                                root.closeWithAnimation();
                                Qt.callLater(root.canceled);
                            }
                        }
                    }
                }
            }

            // 记住选择复选框
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.md
                spacing: AppStyle.spacing.sm

                CheckBox {
                    id: rememberCheckBox
                    checked: root.rememberChoice  // 双向绑定
                    onClicked: {
                        root.rememberChoice = checked;
                    }

                    indicator: Rectangle {
                        implicitWidth: 20
                        implicitHeight: 20
                        x: rememberCheckBox.leftPadding
                        y: parent.height / 2 - height / 2
                        radius: 4
                        border.color: rememberCheckBox.checked ? AppStyle.colors.primary : AppStyle.colors.border
                        border.width: 2
                        color: rememberCheckBox.checked ? AppStyle.colors.primary : "transparent"

                        Rectangle {
                            width: 10
                            height: 10
                            anchors.centerIn: parent
                            radius: 2
                            color: AppStyle.isDarkMode ? "#ffffff" : "#ffffff"
                            visible: rememberCheckBox.checked
                        }
                    }
                }

                Text {

                    text: qsTr("记住我的选择")

                    font.pixelSize: AppStyle.fontSizes.sm

                    color: AppStyle.colors.textSecondary
                }
            }
        }
    }

    // 键盘事件处理
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
            // 向上/左导航
            if (selectedButton === exitButton) {
                selectedButton = minimizeButton;
            } else if (selectedButton === cancelButton) {
                selectedButton = exitButton;
            } else if (selectedButton === minimizeButton) {
                selectedButton = cancelButton;
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
            // 向下/右导航
            if (selectedButton === minimizeButton) {
                selectedButton = exitButton;
            } else if (selectedButton === exitButton) {
                selectedButton = cancelButton;
            } else if (selectedButton === cancelButton) {
                selectedButton = minimizeButton;
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Tab) {
            // Tab 键循环导航
            if (selectedButton === minimizeButton) {
                selectedButton = exitButton;
            } else if (selectedButton === exitButton) {
                selectedButton = cancelButton;
            } else if (selectedButton === cancelButton) {
                selectedButton = minimizeButton;
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            // 回车键确认选择
            if (selectedButton === minimizeButton) {
                root.minimizeToTray(rememberChoice);
            } else if (selectedButton === exitButton) {
                root.exitApp(rememberChoice);
            } else if (selectedButton === cancelButton) {
                root.closeWithAnimation();
                Qt.callLater(root.canceled);
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            // ESC 键取消
            root.closeWithAnimation();
            Qt.callLater(root.canceled);
            event.accepted = true;
        }
    }

    // 向下滑动消失动画

    SequentialAnimation {
        id: hideAnim

        ParallelAnimation {

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

                from: 1.0

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
