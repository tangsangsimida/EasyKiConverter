#include "ParallelExportPreloadCoordinator.h"

#include "ParallelExportService.h"
#include "services/ComponentService.h"
#include "services/export/ExportWorkerHelpers.h"

#include <QDebug>
#include <QMutexLocker>
#include <QTimer>

namespace EasyKiConverter {

/** @brief 按批次读取磁盘缓存并收敛预加载进度。 */
void ParallelExportPreloadCoordinator::processNextBatch(ParallelExportService& service) {
    // 网络服务缺失时按批次读取磁盘缓存，避免一次性阻塞事件循环。
    if (service.m_cancelRequested || service.m_progress.currentStage != ExportOverallProgress::Stage::Preloading) {
        return;
    }

    constexpr int kBatchSize = 8;
    int processedInBatch = 0;
    while (processedInBatch < kBatchSize && service.m_nextPreloadIndex < service.m_componentIds.size() &&
           !service.m_cancelRequested) {
        const QString componentId = service.m_componentIds.at(service.m_nextPreloadIndex++);
        {
            QMutexLocker locker(&service.m_progressMutex);
            service.m_progress.preloadProgress.currentComponentId = componentId;
            service.m_progress.preloadProgress.inProgressCount = 1;
        }

        ComponentData data;
        if (service.m_componentService) {
            data = service.m_componentService->getComponentData(componentId);
            const QSharedPointer<ComponentData> diskCachedData =
                ExportWorkerHelpers::loadDiskCachedComponentData(componentId);
            ExportWorkerHelpers::mergeComponentData(data, diskCachedData);
        } else {
            const QSharedPointer<ComponentData> diskCachedData =
                ExportWorkerHelpers::loadDiskCachedComponentData(componentId);
            if (diskCachedData)
                data = *diskCachedData;
        }

        {
            QMutexLocker locker(&service.m_progressMutex);
            const bool hasValidData = !data.lcscId().isEmpty() && data.isValid() && data.symbolData() != nullptr &&
                                      data.footprintData() != nullptr;
            if (hasValidData) {
                service.m_cachedData[componentId] = QSharedPointer<ComponentData>::create(data);
                service.m_progress.preloadProgress.successCount++;
                qDebug() << "ParallelExportService: Loaded data for" << componentId;
            } else {
                service.m_progress.preloadProgress.failedCount++;
                service.m_progress.preloadProgress.failedComponents[componentId] =
                    QStringLiteral("Incomplete component data");
                qWarning() << "ParallelExportService: No valid data found for component:" << componentId;
            }
            service.m_progress.preloadProgress.completedCount++;
            service.m_progress.preloadProgress.inProgressCount = 0;
        }
        ++processedInBatch;
    }

    const bool finished = service.m_nextPreloadIndex >= service.m_componentIds.size();
    if (finished) {
        QMutexLocker locker(&service.m_progressMutex);
        service.m_progress.preloadProgress.currentComponentId.clear();
        service.m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        service.m_progress.endTime = QDateTime::currentDateTime();
        service.m_preloadCompleted = true;
    }

    qDebug() << "ParallelExportService: Preload batch completed. Success:"
             << service.m_progress.preloadProgress.successCount
             << "Failed:" << service.m_progress.preloadProgress.failedCount;
    emit service.preloadProgressChanged(service.m_progress.preloadProgress);
    service.updateOverallProgress();

    if (finished) {
        emit service.preloadCompleted(service.m_progress.preloadProgress.successCount,
                                      service.m_progress.preloadProgress.failedCount);
        return;
    }
    QTimer::singleShot(0, &service, [&service]() { processNextBatch(service); });
}

void ParallelExportPreloadCoordinator::handleCollectedData(ParallelExportService& service,
                                                           const QList<ComponentData>& componentDataList,
                                                           const QMap<QString, QString>& failedComponents) {
    // 回调只处理当前预加载，取消后的迟到结果必须被忽略。
    if (service.m_componentService) {
        QObject::disconnect(
            service.m_componentService, &ComponentService::allComponentsDataCollectedWithErrors, &service, nullptr);
    }
    {
        QMutexLocker locker(&service.m_progressMutex);
        if (service.m_cancelRequested) {
            qDebug() << "ParallelExportService: Ignoring late callback after cancel requested";
            return;
        }
    }

    int successCount = 0;
    int failedCount = failedComponents.size();
    for (auto it = failedComponents.cbegin(); it != failedComponents.cend(); ++it)
        service.m_progress.preloadProgress.failedComponents[it.key()] = it.value();

    for (const ComponentData& data : componentDataList) {
        const QString componentId = data.lcscId();
        if (componentId.isEmpty()) {
            ++failedCount;
            continue;
        }

        const bool hasValidData = data.isValid() && data.symbolData() != nullptr && data.footprintData() != nullptr;
        if (hasValidData) {
            service.m_cachedData[componentId] = QSharedPointer<ComponentData>::create(data);
            ++successCount;
            qDebug() << "ParallelExportService: Cached data for" << componentId;
        } else {
            ++failedCount;
            service.m_progress.preloadProgress.failedComponents[componentId] = QStringLiteral("No valid data found");
            qWarning() << "ParallelExportService: No valid data for component:" << componentId;
        }
    }

    {
        QMutexLocker locker(&service.m_progressMutex);
        service.m_progress.preloadProgress.successCount = successCount;
        service.m_progress.preloadProgress.failedCount = failedCount;
        service.m_progress.preloadProgress.completedCount = successCount + failedCount;
        service.m_progress.preloadProgress.inProgressCount = 0;
        service.m_progress.preloadProgress.currentComponentId.clear();
        service.m_progress.currentStage = ExportOverallProgress::Stage::Idle;
        service.m_progress.endTime = QDateTime::currentDateTime();
    }

    qDebug() << "ParallelExportService: Parallel preload completed. Success:" << successCount
             << "Failed:" << failedCount;
    service.logNetworkRuntimeStats(QStringLiteral("preload-completed"));
    service.writeExportDetailedReport(QStringLiteral("preload-completed"));
    service.m_preloadCompleted = true;
    emit service.preloadProgressChanged(service.m_progress.preloadProgress);
    emit service.preloadCompleted(successCount, failedCount);
}

}  // namespace EasyKiConverter
