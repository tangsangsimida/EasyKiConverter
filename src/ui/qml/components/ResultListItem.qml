import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Rectangle {
    id: item
    property string componentId
    property string status
    property string message
    property bool exportSymbol: true
    property bool exportFootprint: true
    property bool exportModel3D: true
    property bool exportPreviewImages: false
    property bool exportDatasheet: false
    property bool symbolSuccess: false
    property bool footprintSuccess: false
    property bool model3DSuccess: false
    property bool previewSuccess: false
    property bool datasheetSuccess: false
    property string symbolStatus: "pending"
    property string footprintStatus: "pending"
    property string model3DStatus: "pending"
    property string previewStatus: "pending"
    property string datasheetStatus: "pending"
    signal retryClicked
    signal copyClicked
    signal deleteClicked
    function badgeColor(typeStatus) {
        if (typeStatus === "success")
            return AppStyle.colors.success;
        if (typeStatus === "failed")
            return AppStyle.colors.danger;
        if (typeStatus === "in_progress")
            return AppStyle.colors.warning;
        if (typeStatus === "skipped" || typeStatus === "disabled")
            return AppStyle.colors.textSecondary;
        return AppStyle.colors.border;
    }

    function statusText(typeStatus, success) {
        if (typeStatus === "success")
            return qsTr("已导出");
        if (typeStatus === "failed")
            return qsTr("失败");
        if (typeStatus === "in_progress")
            return qsTr("处理中");
        if (typeStatus === "skipped")
            return qsTr("已跳过");
        if (typeStatus === "disabled")
            return qsTr("未启用");
        if (success)
            return qsTr("已导出");
        return qsTr("等待中");
    }

    function overallIcon() {
        if (status === "success")
            return "🌟";
        if (status === "failed")
            return "❌";
        if (status === "in_progress")
            return "⚙️";
        if (status === "skipped")
            return "⏭";
        return "⏳";
    }

    height: 72
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

    TextEdit {
        id: copyHelper
        visible: false
        text: ""
    }

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
        Text {
            Layout.preferredWidth: 20
            Layout.preferredHeight: 20
            font.pixelSize: 16
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: item.overallIcon()
        }

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

                Row {
                    spacing: 4
                    visible: exportSymbol || exportFootprint || exportModel3D || exportPreviewImages || exportDatasheet
                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        visible: exportSymbol
                        color: item.badgeColor(symbolStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "S"
                            font.pixelSize: 9
                            color: "white"
                            font.bold: true
                        }
                        ToolTip.visible: symMa.containsMouse
                        ToolTip.text: qsTr("符号: %1").arg(item.statusText(symbolStatus, symbolSuccess))
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
                        visible: exportFootprint
                        color: item.badgeColor(footprintStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "F"
                            font.pixelSize: 9
                            color: "white"
                            font.bold: true
                        }
                        ToolTip.visible: ftMa.containsMouse
                        ToolTip.text: qsTr("封装: %1").arg(item.statusText(footprintStatus, footprintSuccess))
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
                        visible: exportModel3D
                        color: item.badgeColor(model3DStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "3"
                            font.pixelSize: 9
                            color: "white"
                            font.bold: true
                        }
                        ToolTip.visible: m3Ma.containsMouse
                        ToolTip.text: qsTr("3D模型: %1").arg(item.statusText(model3DStatus, model3DSuccess))
                        MouseArea {
                            id: m3Ma
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        visible: exportPreviewImages
                        color: item.badgeColor(previewStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "P"
                            font.pixelSize: 9
                            color: "white"
                            font.bold: true
                        }
                        ToolTip.visible: prevMa.containsMouse
                        ToolTip.text: qsTr("预览图: %1").arg(item.statusText(previewStatus, previewSuccess))
                        MouseArea {
                            id: prevMa
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }

                    Rectangle {
                        width: 14
                        height: 14
                        radius: 7
                        visible: exportDatasheet
                        color: item.badgeColor(datasheetStatus)
                        Text {
                            anchors.centerIn: parent
                            text: "D"
                            font.pixelSize: 9
                            color: "white"
                            font.bold: true
                        }
                        ToolTip.visible: datasheetMa.containsMouse
                        ToolTip.text: qsTr("手册: %1").arg(item.statusText(datasheetStatus, datasheetSuccess))
                        MouseArea {
                            id: datasheetMa
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

        Item {
            id: retryButton
            visible: status === "failed"
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                anchors.fill: parent
                color: retryButtonHovered ? AppStyle.colors.warningLight : "transparent"
                radius: AppStyle.radius.sm
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

        Item {
            id: deleteButton
            visible: status === "failed"
            Layout.preferredWidth: 28
            Layout.preferredHeight: 28
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                anchors.fill: parent
                color: deleteButtonHovered ? AppStyle.colors.dangerLight : "transparent"
                radius: AppStyle.radius.sm
            }
            Text {
                anchors.centerIn: parent
                text: "×"
                font.pixelSize: AppStyle.fontSizes.xxl
                font.bold: true
                color: AppStyle.colors.danger
            }
            ToolTip.visible: deleteButtonHovered
            ToolTip.text: qsTr("删除")
            ToolTip.delay: 500
        }
    }

    property bool retryButtonHovered: false
    property bool deleteButtonHovered: false
    MouseArea {
        id: itemMouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.ArrowCursor
        acceptedButtons: Qt.RightButton | Qt.LeftButton
        onMouseXChanged: {
            var inRetryArea = false;
            var inDeleteArea = false;
            if (retryButton.visible) {
                var retryBtnX = item.width - AppStyle.spacing.lg - retryButton.width - AppStyle.spacing.md - deleteButton.width;
                var retryBtnY = (item.height - retryButton.height) / 2;
                if (mouseX >= retryBtnX && mouseX <= retryBtnX + retryButton.width && mouseY >= retryBtnY && mouseY <= retryBtnY + retryButton.height)
                    inRetryArea = true;
            }
            if (deleteButton.visible) {
                var deleteBtnX = item.width - AppStyle.spacing.lg - deleteButton.width;
                var deleteBtnY = (item.height - deleteButton.height) / 2;
                if (mouseX >= deleteBtnX && mouseX <= deleteBtnX + deleteButton.width && mouseY >= deleteBtnY && mouseY <= deleteBtnY + deleteButton.height)
                    inDeleteArea = true;
            }
            retryButtonHovered = inRetryArea;
            deleteButtonHovered = inDeleteArea;
        }

        onClicked: mouse => {
            if (deleteButton.visible) {
                var deleteBtnX = item.width - AppStyle.spacing.lg - deleteButton.width;
                var deleteBtnY = (item.height - deleteButton.height) / 2;
                if (mouse.x >= deleteBtnX && mouse.x <= deleteBtnX + deleteButton.width && mouse.y >= deleteBtnY && mouse.y <= deleteBtnY + deleteButton.height) {
                    item.deleteClicked();
                    return;
                }
            }

            if (retryButton.visible) {
                var retryBtnX = item.width - AppStyle.spacing.lg - retryButton.width - AppStyle.spacing.md - deleteButton.width;
                var retryBtnY = (item.height - retryButton.height) / 2;
                if (mouse.x >= retryBtnX && mouse.x <= retryBtnX + retryButton.width && mouse.y >= retryBtnY && mouse.y <= retryBtnY + retryButton.height) {
                    item.retryClicked();
                    return;
                }
            }

            if (mouse.button === Qt.RightButton) {
                if (componentId) {
                    copyHelper.text = componentId;
                    copyHelper.selectAll();
                    copyHelper.copy();
                    item.copyClicked();
                    copyFeedback.visible = true;
                    copyFeedbackTimer.start();
                }
            } else if (mouse.button === Qt.LeftButton && (mouse.modifiers & Qt.ControlModifier)) {
                if (componentId) {
                    Qt.openUrlExternally("https://so.szlcsc.com/global.html?k=" + componentId);
                }
            }
        }
    }
}
