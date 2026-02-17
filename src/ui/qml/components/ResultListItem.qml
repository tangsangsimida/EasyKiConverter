import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: item
    property string componentId
    property string status // "fetching", "fetch_completed", "processing", "process_completed", "writing", "write_completed", "success", "failed"
    property string message
    signal retryClicked
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
        Button {
            visible: status === "failed" || status.indexOf("fail") !== -1
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            flat: true
            contentItem: Text {
                text: "↺"
                font.pixelSize: 20
                color: AppStyle.colors.warning
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                color: parent.hovered ? (AppStyle.isDarkMode ? "#33f59e0b" : AppStyle.colors.warningLight) : "transparent"
                radius: 15
            }

            ToolTip.visible: hovered
            ToolTip.text: qsTr("重试")
            ToolTip.delay: 500
            onClicked: item.retryClicked()
        }
    }
    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.NoButton
    }
}
