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
    // 打开/关闭
    function open() {
        visible = true;
        // 重置对话框位置和 transform
        dialogBox.y = 0;
        dialogBoxTranslate.y = 0;
        showAnim.start();
        // 自动聚焦到取消按钮，确保键盘操作安全性
        cancelButton.forceActiveFocus();
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

            // 底部按钮
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.lg
                spacing: AppStyle.spacing.md
                ModernButton {
                    id: cancelButton
                    Layout.fillWidth: true
                    text: root.cancelText
                    backgroundColor: "transparent"
                    textColor: AppStyle.colors.textSecondary
                    hoverColor: AppStyle.isDarkMode ? "#334155" : "#f1f5f9"
                    onClicked: {
                        root.rejected();
                        root.closeWithAnimation();
                    }
                }

                ModernButton {
                    id: confirmButton
                    Layout.fillWidth: true
                    text: root.confirmText
                    backgroundColor: root.confirmColor
                    onClicked: {
                        root.accepted();
                        root.close();
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
