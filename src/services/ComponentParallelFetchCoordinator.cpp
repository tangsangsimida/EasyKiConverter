#include "ComponentParallelFetchCoordinator.h"

#include "ComponentQueueManager.h"
#include "ComponentService.h"

#include <QDebug>
#include <QMutexLocker>

namespace EasyKiConverter {

/** @brief 将成功结果写入并行上下文并通知队列释放一个请求槽位。 */
void ComponentParallelFetchCoordinator::handleDataCollected(ComponentService& service,
                                                            const QString& componentId,
                                                            const ComponentData& data) {
    Q_UNUSED(data);
    qDebug() << "Parallel data collected for:" << componentId;

    ComponentData completedData;
    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        if (service.m_fetchingComponents.contains(componentId)) {
            completedData = service.m_fetchingComponents[componentId].data;
        }
    }

    ParallelFetchContext* parallelContext = nullptr;
    {
        QMutexLocker locker(&service.m_parallelContextMutex);
        parallelContext = service.m_parallelContext;
    }
    if (parallelContext != nullptr) {
        parallelContext->markCompleted(componentId, completedData);
    }

    if (service.m_queueManager != nullptr) {
        service.m_queueManager->requestCompleted(componentId);
    }
}

/** @brief 将失败结果写入并行上下文并通知队列释放一个请求槽位。 */
void ComponentParallelFetchCoordinator::handleFetchError(ComponentService& service,
                                                         const QString& componentId,
                                                         const QString& error) {
    qDebug() << "Parallel fetch error for:" << componentId << error;

    ParallelFetchContext* parallelContext = nullptr;
    {
        QMutexLocker locker(&service.m_parallelContextMutex);
        parallelContext = service.m_parallelContext;
    }
    if (parallelContext != nullptr) {
        parallelContext->markFailed(componentId, error);
    }

    if (service.m_queueManager != nullptr) {
        service.m_queueManager->requestCompleted(componentId);
    }
}

/** @brief 处理超时批量请求并发布已经收集的数据和错误。 */
void ComponentParallelFetchCoordinator::handleQueueTimeout(ComponentService& service) {
    qWarning() << "Queue timeout reached";

    if (service.m_parallelContext != nullptr) {
        const int completedCount = service.m_parallelContext->completedCount();
        const int totalCount = service.m_parallelContext->totalCount();
        qWarning() << "Queue timeout - Completed:" << completedCount << "Total:" << totalCount;

        if (completedCount > 0) {
            const QList<ComponentData> allData = service.m_parallelContext->collectedData();
            const QMap<QString, QString> failedComponents = service.m_parallelContext->failedComponents();
            emit service.allComponentsDataCollected(allData);
            emit service.allComponentsDataCollectedWithErrors(allData, failedComponents);
        }

        reset(service);
    }
}

/** @brief 停止队列、清零活动请求并延迟销毁并行上下文。 */
void ComponentParallelFetchCoordinator::reset(ComponentService& service) {
    service.m_queueManager->stop();
    service.m_activeRequestCount = 0;

    {
        QMutexLocker locker(&service.m_parallelContextMutex);
        if (service.m_parallelContext != nullptr) {
            service.m_parallelContext->deleteLater();
            service.m_parallelContext = nullptr;
        }
    }

    qDebug() << "Queue state reset completed";
}

}  // namespace EasyKiConverter
