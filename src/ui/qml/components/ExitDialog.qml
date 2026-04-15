import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
SliderDialogBase {
    id: root
    // ===== 属性 =====
    property bool rememberChoice: false
    property string focusArea: "button"  // "button" or "checkbox"
    // ===== 信号 =====
    signal minimizeToTray(bool remember)
    signal exitApp(bool remember)
    signal canceled
    // ===== 按钮规格 =====
    buttonSpecs: [
        {
            text: qsTr("最小化到托盘"),
            color: AppStyle.colors.primary,
            action: function () {
                root.minimizeToTray(rememberChoice);
            }
        },
        {
            isSeparator: true
        },
        {
            text: qsTr("退出程序"),
            color: AppStyle.colors.danger,
            action: function () {
                root.exitApp(rememberChoice);
            }
        },
        {
            isSeparator: true
        },
        {
            text: qsTr("取消"),
            color: AppStyle.colors.textSecondary,
            action: function () {
                root.closeWithAnimation();
                Qt.callLater(root.canceled);
            }
        }
    ]
    // 覆写 title 和 message
    title: qsTr("关闭程序")
    message: qsTr("您可以选择最小化到系统托盘以保持后台运行，或者完全退出程序。")
    // ===== 额外内容：记住选择复选框 ======
    extraContentSource: RowLayout {
        id: rememberLayout
        spacing: AppStyle.spacing.sm
        CheckBox {
            id: rememberCheckBox
            checked: root.rememberChoice
            onClicked: {
                root.rememberChoice = checked;
            }

            indicator: Rectangle {
                implicitWidth: 22
                implicitHeight: 22
                x: rememberCheckBox.leftPadding
                y: parent.height / 2 - height / 2
                radius: 4
                color: rememberCheckBox.checked ? AppStyle.colors.primary : "transparent"
                border.color: rememberCheckBox.checked ? AppStyle.colors.primary : (root.focusArea === "checkbox" ? AppStyle.colors.primary : AppStyle.colors.textSecondary)
                border.width: root.focusArea === "checkbox" ? 2.5 : 1.5
                Behavior on color {
                    ColorAnimation {
                        duration: 150
                    }
                }
                Behavior on border.color {
                    ColorAnimation {
                        duration: 150
                    }
                }
                Behavior on border.width {
                    NumberAnimation {
                        duration: 150
                    }
                }

                Text {
                    anchors.centerIn: parent
                    text: "✓"
                    font.pixelSize: 16
                    font.bold: true
                    color: "#ffffff"
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

    // ===== 键盘事件 ======
    // 注意：这个对话框没有 Escape 键处理，让 Main.qml 的 Shortcut 处理
    function getButtonByIndex(index) {
        var btnIndex = 0;
        for (var i = 0; i < buttonSpecs.length; i++) {
            if (!buttonSpecs[i].isSeparator) {
                if (btnIndex === index) {
                    return buttonColLayout.children[i].actualButton;
                }
                btnIndex++;
            }
        }
        return null;
    }

    function getButtonSpecByIndex(index) {
        var btnIndex = 0;
        for (var i = 0; i < buttonSpecs.length; i++) {
            if (!buttonSpecs[i].isSeparator) {
                if (btnIndex === index) {
                    return buttonSpecs[i];
                }
                btnIndex++;
            }
        }
        return null;
    }

    // 获取按钮在 buttonSpecs 中的实际索引
    function getSpecIndexForButton(buttonIndex) {
        var btnIndex = 0;
        for (var i = 0; i < buttonSpecs.length; i++) {
            if (!buttonSpecs[i].isSeparator) {
                if (btnIndex === buttonIndex) {
                    return i;
                }
                btnIndex++;
            }
        }
        return -1;
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Left) {
            if (focusArea === "button") {
                // 向上/左导航
                var currentIndex = 0;
                var targetIndex = 0;
                var btnCount = 0;
                for (var i = 0; i < buttonSpecs.length; i++) {
                    if (!buttonSpecs[i].isSeparator) {
                        if (buttonColLayout.children[i].actualButton === selectedButton) {
                            currentIndex = i;
                        }
                        btnCount++;
                    }
                }
                // 找到上一个按钮
                var prevBtnIndex = -1;
                for (var j = currentIndex - 1; j >= 0; j--) {
                    if (!buttonSpecs[j].isSeparator) {
                        prevBtnIndex = j;
                        break;
                    }
                }
                if (prevBtnIndex >= 0) {
                    selectedButton = buttonColLayout.children[prevBtnIndex].actualButton;
                    updateSlider(selectedButton, buttonSpecs[prevBtnIndex].color);
                }
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Down || event.key === Qt.Key_Right) {
            if (focusArea === "button") {
                // 向下/右导航
                var currentIdx = 0;
                var nextIdx = -1;
                for (var k = 0; k < buttonSpecs.length; k++) {
                    if (!buttonSpecs[k].isSeparator) {
                        if (buttonColLayout.children[k].actualButton === selectedButton) {
                            currentIdx = k;
                        }
                    }
                }
                for (var l = currentIdx + 1; l < buttonSpecs.length; l++) {
                    if (!buttonSpecs[l].isSeparator) {
                        nextIdx = l;
                        break;
                    }
                }
                if (nextIdx >= 0) {
                    selectedButton = buttonColLayout.children[nextIdx].actualButton;
                    updateSlider(selectedButton, buttonSpecs[nextIdx].color);
                }
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Tab) {
            // Tab 键：在按钮和复选框之间切换
            if (focusArea === "button") {
                focusArea = "checkbox";
                hideSlider();
            } else {
                focusArea = "button";
                if (selectedButton) {
                    for (var m = 0; m < buttonSpecs.length; m++) {
                        if (buttonColLayout.children[m].actualButton === selectedButton) {
                            updateSlider(selectedButton, buttonSpecs[m].color);
                            break;
                        }
                    }
                }
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Space) {
            if (focusArea === "checkbox") {
                root.rememberChoice = !root.rememberChoice;
                event.accepted = true;
            }
        } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            if (focusArea === "checkbox") {
                root.rememberChoice = !root.rememberChoice;
                event.accepted = true;
            } else if (selectedButton) {
                // 执行选中按钮的动作
                for (var n = 0; n < buttonSpecs.length; n++) {
                    if (buttonColLayout.children[n].actualButton === selectedButton) {
                        if (buttonSpecs[n].action) {
                            buttonSpecs[n].action();
                        }
                        break;
                    }
                }
            }
            event.accepted = true;
        } else if (event.key === Qt.Key_Escape) {
            // ESC 键不处理，让 Main.qml 的 Shortcut 处理
            event.accepted = false;
        } else {
            event.accepted = true;
        }
    }

    // ===== 覆写 open 函数 =====
    function open() {
        visible = true;
        dialogBox.rotation = 0;
        dialogBox.scale = 1.0;
        dialogBox.opacity = 1.0;
        dialogBoxTranslate.y = 0;
        // 默认选中第一个按钮（最小化到托盘）
        selectedButton = buttonColLayout.children[0].actualButton;
        // 设置滑块默认聚焦在第一个选项上
        updateSlider(selectedButton, buttonSpecs[0].color);
        focusArea = "button";
        root.forceActiveFocus();
    }
}
