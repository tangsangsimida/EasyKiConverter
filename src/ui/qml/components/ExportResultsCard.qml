import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Loader {
    id: resultsLoader

    // 外部依赖
    property var exportProgressController

    active: resultsLoader.exportProgressController.isExporting || resultsLoader.exportProgressController.resultsList.length > 0
    visible: active  // 确保 Loader 在没有结果时不占用空间
    sourceComponent: Card {
        title: qsTranslate("MainWindow", "转换结果")
        ColumnLayout {
            id: resultsContent
            width: parent.width
            spacing: AppStyle.spacing.md
            visible: true

            // 结果列表（使用 GridView 实现五列显示）
            GridView {
                id: resultsList
                Layout.fillWidth: true
                Layout.minimumHeight: 200
                Layout.preferredHeight: Math.min(resultsList.contentHeight + 20, 500)
                Layout.topMargin: AppStyle.spacing.md
                visible: resultsLoader.exportProgressController.resultsList.length > 0
                clip: true
                cellWidth: {
                    var w = width - AppStyle.spacing.md;
                    var c = Math.max(1, Math.floor(w / 230));
                    return w / c;
                }
                cellHeight: 80
                flow: GridView.FlowLeftToRight
                layoutDirection: Qt.LeftToRight
                model: resultsLoader.exportProgressController.resultsList
                delegate: ResultListItem {
                    width: resultsList.cellWidth - AppStyle.spacing.md
                    anchors.horizontalCenter: parent ? undefined : undefined
                    componentId: modelData.componentId || ""
                    status: modelData.status || "pending"
                    message: modelData.message || ""
                    onRetryClicked: resultsLoader.exportProgressController.retryComponent(componentId)
                }
                ScrollBar.vertical: ScrollBar {
                    policy: ScrollBar.AsNeeded
                }
                // 添加列表项进入动画
                add: Transition {
                    NumberAnimation {
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: AppStyle.durations.normal
                        easing.type: AppStyle.easings.easeOut
                    }
                    NumberAnimation {
                        property: "scale"
                        from: 0.8
                        to: 1
                        duration: AppStyle.durations.normal
                        easing.type: AppStyle.easings.easeOut
                    }
                }
                // 列表项移除动画
                remove: Transition {
                    NumberAnimation {
                        property: "opacity"
                        from: 1
                        to: 0
                        duration: AppStyle.durations.normal
                        easing.type: AppStyle.easings.easeOut
                    }
                }
            }
        }
    }
}
