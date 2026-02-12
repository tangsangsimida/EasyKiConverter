import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import EasyKiconverter_Cpp_Version.src.ui.qml.styles 1.0

Card {
    id: exportStatisticsCard

    // 外部依赖
    property var exportProgressController
    property var exportSettingsController

    title: qsTranslate("MainWindow", "导出统计")
    visible: exportStatisticsCard.exportProgressController.hasStatistics
    ColumnLayout {
        width: parent.width
        spacing: AppStyle.spacing.md
        // 基本统计信息
        Text {
            text: qsTranslate("MainWindow", "基本统计")
            font.pixelSize: AppStyle.fontSizes.md
            font.bold: true
            color: AppStyle.colors.textPrimary
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StatItem {
                label: qsTranslate("MainWindow", "总数")
                value: exportStatisticsCard.exportProgressController.statisticsTotal
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "成功")
                value: exportStatisticsCard.exportProgressController.statisticsSuccess
                valueColor: AppStyle.colors.success
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "失败")
                value: exportStatisticsCard.exportProgressController.statisticsFailed
                valueColor: AppStyle.colors.danger
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "成功率")
                value: exportStatisticsCard.exportProgressController.statisticsSuccessRate.toFixed(2) + "%"
                Layout.fillWidth: true
            }
        }
        // 时间统计信息
        Text {
            text: qsTranslate("MainWindow", "时间统计")
            font.pixelSize: AppStyle.fontSizes.md
            font.bold: true
            color: AppStyle.colors.textPrimary
            Layout.topMargin: AppStyle.spacing.sm
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StatItem {
                label: qsTranslate("MainWindow", "总耗时")
                value: (exportStatisticsCard.exportProgressController.statisticsTotalDuration / 1000).toFixed(2) + "s"
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "平均抓取")
                value: exportStatisticsCard.exportProgressController.statisticsAvgFetchTime + "ms"
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "平均处理")
                value: exportStatisticsCard.exportProgressController.statisticsAvgProcessTime + "ms"
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "平均写入")
                value: exportStatisticsCard.exportProgressController.statisticsAvgWriteTime + "ms"
                Layout.fillWidth: true
            }
        }
        // 导出成果统计
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StatItem {
                label: qsTranslate("MainWindow", "符号")
                value: exportStatisticsCard.exportProgressController.successSymbolCount
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "封装")
                value: exportStatisticsCard.exportProgressController.successFootprintCount
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "3D模型")
                value: exportStatisticsCard.exportProgressController.successModel3DCount
                Layout.fillWidth: true
            }
        }
        // 网络统计信息
        Text {
            text: qsTranslate("MainWindow", "网络统计")
            font.pixelSize: AppStyle.fontSizes.md
            font.bold: true
            color: AppStyle.colors.textPrimary
            Layout.topMargin: AppStyle.spacing.sm
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: AppStyle.spacing.lg
            StatItem {
                label: qsTranslate("MainWindow", "总请求数")
                value: exportStatisticsCard.exportProgressController.statisticsTotalNetworkRequests
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "重试次数")
                value: exportStatisticsCard.exportProgressController.statisticsTotalRetries
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "平均延迟")
                value: exportStatisticsCard.exportProgressController.statisticsAvgNetworkLatency + "ms"
                Layout.fillWidth: true
            }
            StatItem {
                label: qsTranslate("MainWindow", "速率限制")
                value: exportStatisticsCard.exportProgressController.statisticsRateLimitHitCount
                Layout.fillWidth: true
            }
        }
        // 底部按钮组（居中排列）
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: AppStyle.spacing.sm
            spacing: AppStyle.spacing.lg

            // 打开详细报告按钮（只在调试模式下显示）
            ModernButton {
                text: qsTranslate("MainWindow", "打开详细统计报告")
                iconName: "folder"
                backgroundColor: AppStyle.colors.surface
                textColor: AppStyle.colors.textPrimary
                hoverColor: AppStyle.colors.border
                pressedColor: AppStyle.colors.borderFocus
                visible: exportStatisticsCard.exportSettingsController.debugMode // 只在调试模式下显示

                onClicked: {
                    Qt.openUrlExternally("file:///" + exportStatisticsCard.exportProgressController.statisticsReportPath);
                }
            }
        }
    }
}
