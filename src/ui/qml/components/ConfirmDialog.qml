import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"

/**
 * @brief 自定义美化的确认对话框
 *
 * 包含圆角、主题适配、模糊背景遮罩及平滑动画。
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
        root.close();
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
    // 打开/关闭动画
    function open() {
        visible = true;
        showAnim.start();
        // 自动聚焦到取消按钮，确保键盘操作安全性
        cancelButton.forceActiveFocus();
    }

    function close() {
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
                        root.close();
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
            }
            NumberAnimation {
                target: dialogBox
                property: "opacity"
                from: 1
                to: 0
                duration: 200
            }
            NumberAnimation {
                target: dialogBox
                property: "scale"
                from: 1
                to: 0.9
                duration: 200
            }
        }
        PropertyAction {
            target: root
            property: "visible"
            value: false
        }
    }
}
