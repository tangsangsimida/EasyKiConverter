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
            // 筛选滑块

            Rectangle {
                id: filterSegmentedControl
                Layout.preferredWidth: 560
                Layout.preferredHeight: 40
                Layout.alignment: Qt.AlignLeft
                color: AppStyle.isDarkMode ? Qt.rgba(255, 255, 255, 0.05) : Qt.rgba(0, 0, 0, 0.05)
                radius: AppStyle.radius.lg
                visible: resultsLoader.exportProgressController.resultsList.length > 0
                clip: true
                // 滑块背景指示器

                Rectangle {
                    id: sliderIndicator
                    width: (filterSegmentedControl.width - 8) / 4
                    height: filterSegmentedControl.height - 8
                    anchors.verticalCenter: parent.verticalCenter
                    radius: AppStyle.radius.md
                    color: {
                        var mode = resultsLoader.exportProgressController.filterMode;
                        if (mode === "exporting")
                            return AppStyle.colors.warning;
                        if (mode === "success")
                            return AppStyle.colors.success;
                        if (mode === "failed")
                            return AppStyle.colors.danger;
                        return AppStyle.colors.primary;
                    }

                    // 计算位置：根据 filterMode 移动

                    x: {
                        var step = (filterSegmentedControl.width - 8) / 4;
                        if (resultsLoader.exportProgressController.filterMode === "exporting")
                            return 4 + step;
                        if (resultsLoader.exportProgressController.filterMode === "success")
                            return 4 + step * 2;
                        if (resultsLoader.exportProgressController.filterMode === "failed")
                            return 4 + step * 3;
                        return 4;
                    }

                    Behavior on x {
                        NumberAnimation {
                            duration: AppStyle.durations.normal
                            easing.type: AppStyle.easings.easeOut
                        }
                    }

                    Behavior on color {
                        ColorAnimation {
                            duration: AppStyle.durations.normal
                        }
                    }
                }

                Row {
                    anchors.fill: parent
                    anchors.margins: 4
                    // 全部

                    Item {
                        width: (filterSegmentedControl.width - 8) / 4
                        height: parent.height
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("全部 (%1)").arg(resultsLoader.exportProgressController.resultsList.length)
                            color: resultsLoader.exportProgressController.filterMode === "all" ? "#ffffff" : AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.bold: resultsLoader.exportProgressController.filterMode === "all"
                            Behavior on color {
                                ColorAnimation {
                                    duration: AppStyle.durations.fast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: resultsLoader.exportProgressController.setFilterMode("all")
                        }
                    }

                    // 导出中

                    Item {
                        width: (filterSegmentedControl.width - 8) / 4
                        height: parent.height
                        enabled: resultsLoader.exportProgressController.filteredPendingCount > 0
                        opacity: enabled ? 1.0 : 0.4
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("导出中 (%1)").arg(resultsLoader.exportProgressController.filteredPendingCount)
                            color: resultsLoader.exportProgressController.filterMode === "exporting" ? "#ffffff" : AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.bold: resultsLoader.exportProgressController.filterMode === "exporting"
                            Behavior on color {
                                ColorAnimation {
                                    duration: AppStyle.durations.fast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: resultsLoader.exportProgressController.setFilterMode("exporting")
                        }
                    }

                    // 成功

                    Item {
                        width: (filterSegmentedControl.width - 8) / 4
                        height: parent.height
                        enabled: resultsLoader.exportProgressController.successCount > 0
                        opacity: enabled ? 1.0 : 0.4
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("成功 (%1)").arg(resultsLoader.exportProgressController.successCount)
                            color: resultsLoader.exportProgressController.filterMode === "success" ? "#ffffff" : AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.bold: resultsLoader.exportProgressController.filterMode === "success"
                            Behavior on color {
                                ColorAnimation {
                                    duration: AppStyle.durations.fast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: resultsLoader.exportProgressController.setFilterMode("success")
                        }
                    }

                    // 失败

                    Item {
                        width: (filterSegmentedControl.width - 8) / 4
                        height: parent.height
                        enabled: resultsLoader.exportProgressController.failureCount > 0
                        opacity: enabled ? 1.0 : 0.4
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("失败 (%1)").arg(resultsLoader.exportProgressController.failureCount)
                            color: resultsLoader.exportProgressController.filterMode === "failed" ? "#ffffff" : AppStyle.colors.textSecondary
                            font.pixelSize: AppStyle.fontSizes.sm
                            font.bold: resultsLoader.exportProgressController.filterMode === "failed"
                            Behavior on color {
                                ColorAnimation {
                                    duration: AppStyle.durations.fast
                                }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: resultsLoader.exportProgressController.setFilterMode("failed")
                        }
                    }
                }
            }

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
                model: resultsLoader.exportProgressController.filteredResultsList
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
