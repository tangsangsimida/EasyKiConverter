import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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
SliderDialogBase {
    id: root

    // ===== 子类覆盖的属性 =====
    hasOverlay: true
    title: qsTr("退出确认")
    message: qsTr("转换正在进行中。退出将取消当前转换，已导出的文件会保留。确定要退出吗？")

    // ===== 信号 =====
    signal accepted
    signal rejected

    // ===== 按钮规格 =====
    buttonSpecs: [
        {
            text: qsTr("取消"),
            color: AppStyle.colors.textSecondary,
            action: function() {
                root.rejected()
                root.closeWithAnimation()
            }
        },
        { isSeparator: true },
        {
            text: qsTr("确定"),
            color: AppStyle.colors.danger,
            action: function() {
                root.accepted()
                root.close()
            }
        }
    ]

    // ===== 键盘事件 ======
    Keys.onEscapePressed: {
        root.rejected()
        root.closeWithAnimation()
    }

    // ===== 覆写 open 函数 =====
    function open() {
        visible = true
        dialogBox.y = 0
        dialogBoxTranslate.y = 0
        showAnim.start()
        root.forceActiveFocus()
        // 默认聚焦在取消按钮（第一个）
        root.updateSlider(buttonColLayout.children[0].actualButton, AppStyle.colors.textSecondary)
    }
}
