import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import "../styles"

/**
 * @brief 退出选项对话框
 *
 * 当用户点击关闭按钮且无正在进行的任务时显示。
 * 提供"最小化到托盘"和"退出程序"选项。
 */
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 9999

    // 属性
    property bool rememberChoice: false

    // 信号
    signal minimizeToTray(bool remember)
    signal exitApp(bool remember)
    signal canceled

    // 打开/关闭动画
    function open() {
        visible = true;
        showAnim.start();
        minimizeButton.forceActiveFocus();
    }

    function close() {
        hideAnim.start();
    }

    // 遮罩层
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: AppStyle.isDarkMode ? "#000000" : "#1e293b"
        opacity: 0
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {} // 拦截点击
        }
    }

    // 对话框主体
    Rectangle {
        id: dialogBox
        width: Math.min(parent.width * 0.9, 440)
        implicitHeight: contentCol.implicitHeight + AppStyle.spacing.xxxl * 2
        anchors.centerIn: parent
        color: AppStyle.colors.surface
        radius: AppStyle.radius.xl
        scale: 0.9
        opacity: 0

        // 阴影
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

            // 按钮组
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: AppStyle.spacing.md
                spacing: AppStyle.spacing.md

                // 最小化到托盘
                ModernButton {
                    id: minimizeButton
                    Layout.fillWidth: true
                    text: qsTr("最小化到托盘")
                    backgroundColor: AppStyle.colors.primary
                    onClicked: {
                        root.close();
                        // 延迟触发信号以显示关闭动画
                        Qt.callLater(() => root.minimizeToTray(rememberChoice));
                    }
                }

                // 退出程序
                ModernButton {
                    id: exitButton
                    Layout.fillWidth: true
                    text: qsTr("退出程序")
                    backgroundColor: "transparent"
                    textColor: AppStyle.colors.danger
                    hoverColor: AppStyle.isDarkMode ? "#331e1e" : "#fee2e2"
                    onClicked: {
                        root.close();
                        Qt.callLater(() => root.exitApp(rememberChoice));
                    }
                }

                // 取消
                ModernButton {
                    id: cancelButton
                    Layout.fillWidth: true
                    text: qsTr("取消")
                    backgroundColor: "transparent"
                    textColor: AppStyle.colors.textSecondary
                    hoverColor: AppStyle.isDarkMode ? "#334155" : "#f1f5f9"
                    onClicked: {
                        root.close();
                        Qt.callLater(root.canceled);
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
                    checked: false
                    onClicked: {
                        rememberChoice = checked;
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

    // 动画
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
                to: 0
                duration: 200
            }
            NumberAnimation {
                target: dialogBox
                property: "opacity"
                to: 0
                duration: 200
            }
            NumberAnimation {
                target: dialogBox
                property: "scale"
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
