import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: item
    property string componentId
    property string status // "fetching", "fetch_completed", "processing", "process_completed", "writing", "write_completed", "success", "failed"
    property string message
    signal retryClicked
    signal copyClicked
    height: message.length > 0 ? 72 : 48
    // 悬停效果
    color: itemMouseArea.containsMouse ? AppStyle.colors.background : AppStyle.colors.surface
    radius: AppStyle.radius.md
    border.color: AppStyle.colors.border
    border.width: 1
    clip: true
    Behavior on color {
        ColorAnimation {
            duration: AppStyle.durations.fast
            easing.type: AppStyle.easings.easeOut
        }
    }

    // 复制辅助组件
    TextEdit {
        id: copyHelper
        visible: false
        text: ""
    }

    // 复制成功提示
    ToolTip {
        id: copyFeedback
        parent: item
        x: parent.width - width - 8
        y: 4
        delay: 0
        timeout: 1500
        visible: false
        text: ""
        background: Rectangle {
            color: AppStyle.isDarkMode ? "#1e293b" : "#ffffff"
            radius: AppStyle.radius.md
            border.width: 1
            border.color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.1) : Qt.rgba(0, 0, 0, 0.1)
            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowBlur: 8
                shadowColor: AppStyle.isDarkMode ? "#00000088" : "#33000000"
                shadowVerticalOffset: 2
            }
        }
        contentItem: Row {
            spacing: 4
            Text {
                text: "✓"
                color: "#22c55e"
                font.bold: true
            }
            Text {
                text: qsTr("已复制")
                color: AppStyle.isDarkMode ? "#ffffff" : "#1e293b"
            }
        }
        enter: Transition {
            NumberAnimation {
                property: "opacity"
                from: 0
                to: 1
                duration: 150
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "opacity"
                from: 1
                to: 0
                duration: 150
            }
        }
    }

    Timer {
        id: copyFeedbackTimer
        interval: 1500
        onTriggered: copyFeedback.visible = false
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: AppStyle.spacing.lg
        anchors.rightMargin: AppStyle.spacing.lg
        spacing: AppStyle.spacing.md
        // 状态图标
        Text {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: {
                if (status === "fetching")
                    return "⏳";
                if (status === "fetch_completed")
                    return "✅";
                if (status === "processing")
                    return "⚙️";
                if (status === "process_completed")
                    return "✅";
                if (status === "writing")
                    return "💾";
                if (status === "write_completed")
                    return "✅";
                if (status === "success")
                    return "🌟";
                if (status === "failed")
                    return "❌";
                // Add explicit check for fetch/process failures if the status string is different
                if (status.indexOf("fail") !== -1)
                    return "❌";
                return "⏳";
            }
            Behavior on text {
                NumberAnimation {
                    duration: AppStyle.durations.fast
                }
            }
        }
        // 元件ID和消息
        ColumnLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.xs
            RowLayout {
                Layout.fillWidth: true
                spacing: AppStyle.spacing.sm
                Text {
                    text: componentId
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    font.pixelSize: AppStyle.fontSizes.sm
                    font.bold: true
                    font.family: "Courier New"
                    color: AppStyle.colors.textPrimary
                }
                // 分项状态指示灯 (S: Symbol, F: Footprint, 3: 3D)
                Row {
                    spacing: 4
                    visible: status !== "pending" && status !== "fetching"
                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        color: modelData.symbolSuccess ? AppStyle.colors.success : AppStyle.colors.border
                        Text {
                            anchors.centerIn: parent
                            text: "S"
                            font.pixelSize: 9
                            color: "white"
                            font.bold: true
                        }
                        ToolTip.visible: symMa.containsMouse
                        ToolTip.text: qsTr("符号: %1").arg(modelData.symbolSuccess ? qsTr("已导出") : qsTr("未完成"))
                        MouseArea {
                            id: symMa
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        color: modelData.footprintSuccess ? AppStyle.colors.success : AppStyle.colors.border
                        Text {
                            anchors.centerIn: parent
                            text: "F"
                            font.pixelSize: 9
                            color: "white"
                            font.bold: true
                        }
                        ToolTip.visible: ftMa.containsMouse
                        ToolTip.text: qsTr("封装: %1").arg(modelData.footprintSuccess ? qsTr("已导出") : qsTr("未完成"))
                        MouseArea {
                            id: ftMa
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        color: modelData.model3DSuccess ? AppStyle.colors.success : AppStyle.colors.border
                        Text {
                            anchors.centerIn: parent
                            text: "3"
                            font.pixelSize: 9
                            color: "white"
                            font.bold: true
                        }
                        ToolTip.visible: m3Ma.containsMouse
                        ToolTip.text: qsTr("3D模型: %1").arg(modelData.model3DSuccess ? qsTr("已导出") : qsTr("未完成"))
                        MouseArea {
                            id: m3Ma
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }
            }
            Text {
                Layout.fillWidth: true
                text: message
                font.pixelSize: AppStyle.fontSizes.xs
                color: AppStyle.colors.textSecondary
                visible: message.length > 0
                wrapMode: Text.WordWrap
            }
        }
        // 重试按钮
        Item {
            id: retryButton
            visible: status === "failed" || status.indexOf("fail") !== -1
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                anchors.fill: parent
                color: retryButtonHovered ? AppStyle.colors.warningLight : "transparent"
                radius: AppStyle.radius.sm
                Behavior on color {
                    ColorAnimation {
                        duration: AppStyle.durations.fast
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                text: "↻"
                font.pixelSize: AppStyle.fontSizes.xxl
                font.bold: true
                color: AppStyle.colors.warning
            }
            ToolTip.visible: retryButtonHovered
            ToolTip.text: qsTr("重试")
            ToolTip.delay: 500
        }
    }

    // 追踪重试按钮悬停状态
    property bool retryButtonHovered: false
    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        onMouseXChanged: {
            // 检查鼠标是否在重试按钮区域
            if (retryButton.visible) {
                var retryBtnX = item.width - AppStyle.spacing.lg - retryButton.width;
                var retryBtnY = (item.height - retryButton.height) / 2;
                if (mouseX >= retryBtnX && mouseX <= retryBtnX + retryButton.width && mouseY >= retryBtnY && mouseY <= retryBtnY + retryButton.height) {
                    retryButtonHovered = true;
                } else {
                    retryButtonHovered = false;
                }
            } else {
                retryButtonHovered = false;
            }
        }

        onClicked: mouse => {
            // 检查是否点击在重试按钮区域
            if (retryButton.visible) {
                var retryBtnX = item.width - AppStyle.spacing.lg - retryButton.width;
                var retryBtnY = (item.height - retryButton.height) / 2;
                if (mouse.x >= retryBtnX && mouse.x <= retryBtnX + retryButton.width && mouse.y >= retryBtnY && mouse.y <= retryBtnY + retryButton.height) {
                    // 点击在重试按钮区域，调用重试功能
                    item.retryClicked();
                    return;
                }
            }

            // 右键点击复制 ID
            if (mouse.button === Qt.RightButton) {
                if (componentId) {
                    copyHelper.text = componentId;
                    copyHelper.selectAll();
                    copyHelper.copy();
                    item.copyClicked();
                    copyFeedback.visible = true;
                    copyFeedbackTimer.start();
                }
            } else
            // Ctrl + 左键点击打开浏览器
            if (mouse.button === Qt.LeftButton && (mouse.modifiers & Qt.ControlModifier)) {
                if (componentId) {
                    var url = "https://so.szlcsc.com/global.html?k=" + componentId;
                    Qt.openUrlExternally(url);
                }
            }
        }
    }
}
