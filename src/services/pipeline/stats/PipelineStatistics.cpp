#include "PipelineStatistics.h"

#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace EasyKiConverter {

ExportStatistics PipelineStatistics::generate(const QList<QSharedPointer<ComponentExportStatus>>& completedStatuses,
                                              const ExportOptions& options,
                                              int totalTasks,
                                              qint64 startTimeMs,
                                              bool isRetryMode,
                                              const ExportStatistics& originalStats) {
    ExportStatistics statistics;

    // 使用原始总数量（包括重试），如果未设置则使用当前批次数量
    statistics.total = (originalStats.total > 0) ? originalStats.total : totalTasks;

    QSet<QString> uniqueComponentIds;
    int actualSuccessCount = 0;
    int actualFailureCount = 0;

    for (const QSharedPointer<ComponentExportStatus>& status : completedStatuses) {
        if (!status)
            continue;

        QString componentId = status->componentId;
        if (uniqueComponentIds.contains(componentId))
            continue;
        uniqueComponentIds.insert(componentId);

        if (options.exportSymbol && status->symbolWritten)
            statistics.successSymbol++;
        if (options.exportFootprint && status->footprintWritten)
            statistics.successFootprint++;
        if (options.exportModel3D && status->model3DWritten)
            statistics.successModel3D++;

        if (status->isCompleteSuccess()) {
            actualSuccessCount++;
        } else {
            actualFailureCount++;
            statistics.stageFailures[status->getFailedStage()]++;
            statistics.failureReasons[status->getFailureReason()]++;
        }
    }

    statistics.success = actualSuccessCount;
    statistics.failed = actualFailureCount;

    if (!isRetryMode) {
        qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
        if (startTimeMs > 0) {
            statistics.totalDurationMs = currentTimeMs - startTimeMs;
        }

        qint64 totalFetchTime = 0, totalProcessTime = 0, totalWriteTime = 0;
        int completedCount = uniqueComponentIds.size();

        for (const QSharedPointer<ComponentExportStatus>& status : completedStatuses) {
            if (!status)
                continue;
            totalFetchTime += status->fetchDurationMs;
            totalProcessTime += status->processDurationMs;
            totalWriteTime += status->writeDurationMs;

            for (const auto& diag : status->networkDiagnostics) {
                statistics.totalNetworkRequests++;
                statistics.totalRetries += diag.retryCount;
                statistics.avgNetworkLatencyMs += diag.latencyMs;
                if (diag.wasRateLimited)
                    statistics.rateLimitHitCount++;
                statistics.statusCodeDistribution[diag.statusCode]++;
            }
        }

        if (statistics.totalNetworkRequests > 0)
            statistics.avgNetworkLatencyMs /= statistics.totalNetworkRequests;
        if (completedCount > 0) {
            statistics.avgFetchTimeMs = totalFetchTime / completedCount;
            statistics.avgProcessTimeMs = totalProcessTime / completedCount;
            statistics.avgWriteTimeMs = totalWriteTime / completedCount;
        }
    } else {
        // 重试模式：保留原始统计信息的时间数据
        if (originalStats.total > 0) {
            statistics.totalDurationMs = originalStats.totalDurationMs;
            statistics.avgFetchTimeMs = originalStats.avgFetchTimeMs;
            statistics.avgProcessTimeMs = originalStats.avgProcessTimeMs;
            statistics.avgWriteTimeMs = originalStats.avgWriteTimeMs;
            statistics.totalNetworkRequests = originalStats.totalNetworkRequests;
            statistics.totalRetries = originalStats.totalRetries;
            statistics.avgNetworkLatencyMs = originalStats.avgNetworkLatencyMs;
            statistics.rateLimitHitCount = originalStats.rateLimitHitCount;
            statistics.statusCodeDistribution = originalStats.statusCodeDistribution;
        }
    }

    // 内存峰值统计：保留旧值或使用当前观察到的峰值 (逻辑在 Service 中处理，此处仅迁移保存逻辑)
    statistics.peakMemoryUsage = originalStats.peakMemoryUsage;

    return statistics;
}

bool PipelineStatistics::saveReport(const ExportStatistics& statistics,
                                    const QString& outputPath,
                                    const QString& libName,
                                    const QString& reportPath) {
    QJsonObject reportObj, overview, timing, failures, stageFailures, failureReasons, options;

    overview["total"] = statistics.total;
    overview["success"] = statistics.success;
    overview["failed"] = statistics.failed;
    overview["successSymbol"] = statistics.successSymbol;
    overview["successFootprint"] = statistics.successFootprint;
    overview["successModel3D"] = statistics.successModel3D;
    overview["successRate"] = QString::number(statistics.getSuccessRate(), 'f', 2) + "%";
    overview["totalDurationMs"] = statistics.totalDurationMs;
    overview["peakMemoryUsageMB"] = QString::number(statistics.peakMemoryUsage / (1024.0 * 1024.0), 'f', 2) + " MB";

    reportObj["overview"] = overview;
    timing["avgFetchTimeMs"] = statistics.avgFetchTimeMs;
    timing["avgProcessTimeMs"] = statistics.avgProcessTimeMs;
    timing["avgWriteTimeMs"] = statistics.avgWriteTimeMs;
    reportObj["timing"] = timing;

    for (auto it = statistics.stageFailures.constBegin(); it != statistics.stageFailures.constEnd(); ++it)
        stageFailures[it.key()] = it.value();
    failures["stageFailures"] = stageFailures;

    for (auto it = statistics.failureReasons.constBegin(); it != statistics.failureReasons.constEnd(); ++it)
        failureReasons[it.key()] = it.value();
    failures["failureReasons"] = failureReasons;

    reportObj["failures"] = failures;
    options["outputPath"] = outputPath;
    options["libName"] = libName;
    reportObj["exportOptions"] = options;
    reportObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonDocument doc(reportObj);
    QFile file(reportPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

}  // namespace EasyKiConverter
