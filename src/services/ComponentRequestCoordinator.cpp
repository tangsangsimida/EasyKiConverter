#include "ComponentRequestCoordinator.h"

#include "CadDataLoader.h"
#include "ComponentCacheLoadCoordinator.h"
#include "ComponentCacheService.h"
#include "ComponentService.h"
#include "ConfigService.h"
#include "core/easyeda/EasyedaApi.h"

#include <QDebug>
#include <QFutureWatcher>
#include <QMutexLocker>
#include <QTimer>
#include <QtConcurrent>

namespace EasyKiConverter {

/** @brief 启动单个元件请求并处理重复请求、缓存命中和 CAD 回退。 */
void ComponentRequestCoordinator::start(ComponentService& service, const QString& componentId, bool fetch3DModel) {
    // 统一编号大小写，确保请求状态和缓存键使用同一表示。
    const QString normalizedId = componentId.toUpper();
    ComponentData reusableData;
    uint64_t generation = 0;
    bool reuseExistingData = false;
    bool shouldSkipAsDuplicate = false;

    // 先登记请求状态，避免并发入口为同一个编号创建多个后台任务。
    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        if (service.m_fetchingComponents.contains(normalizedId)) {
            const ComponentService::FetchingComponent& existing = service.m_fetchingComponents[normalizedId];
            const bool hasReusableData =
                !existing.data.lcscId().isEmpty() && (existing.hasCadData || existing.data.symbolData() != nullptr ||
                                                      existing.data.footprintData() != nullptr);

            if (hasReusableData) {
                reusableData = existing.data;
                reuseExistingData = true;
            } else if (service.m_parallelContext != nullptr || !existing.requestActive) {
                qDebug() << "ComponentService: Removing stale fetching state for" << normalizedId << "before retry";
                service.m_fetchingComponents.remove(normalizedId);
            } else {
                shouldSkipAsDuplicate = true;
            }
        }

        if (!reuseExistingData && !shouldSkipAsDuplicate && !service.m_fetchingComponents.contains(normalizedId)) {
            // 创建占位条目并捕获当前缓存代次，旧回调不能覆盖后续请求。
            ComponentService::FetchingComponent& fetching = service.m_fetchingComponents[normalizedId];
            service.initializeFetchingComponent(fetching, normalizedId, fetch3DModel);
            ComponentCacheService::instance()->clearTombstone(normalizedId);
            ComponentCacheService::instance()->clearGlobalTombstone();
        }
    }

    if (reuseExistingData) {
        qDebug() << "ComponentService: Reusing existing fetched data for:" << normalizedId;
        service.updateComponentCache(normalizedId, reusableData);
        emit service.cadDataReady(normalizedId, reusableData);
        if (service.m_parallelContext != nullptr) {
            service.handleParallelDataCollected(normalizedId, reusableData);
        }
        return;
    }

    if (shouldSkipAsDuplicate) {
        qDebug() << "ComponentService: Already fetching for" << normalizedId << ", skipping duplicate request";
        return;
    }

    ComponentCacheService* cache = ComponentCacheService::instance();
    if (service.m_api) {
        service.m_api->setWeakNetworkSupport(ConfigService::instance()->getWeakNetworkSupport());
    }
    if (cache->hasSymbolFootprintCache(normalizedId)) {
        qDebug() << "ComponentService: Symbol/Footprint cache hit for" << normalizedId
                 << ", loading from cache (component exists)";
        QTimer::singleShot(0, &service, [&service, normalizedId, fetch3DModel, cache]() {
            service.loadComponentDataFromCacheAsync(normalizedId, fetch3DModel, cache);
        });
        return;
    }

    // 缓存未命中时在后台线程解析 CAD，避免请求入口阻塞主线程。
    {
        QMutexLocker locker(&service.m_fetchingComponentsMutex);
        const auto it = service.m_fetchingComponents.find(normalizedId);
        if (it != service.m_fetchingComponents.end()) {
            generation = it->cacheGeneration;
        }
    }
    if (generation == 0) {
        qWarning() << "ComponentRequestCoordinator: No fetching state for" << normalizedId << ", aborting";
        return;
    }

    const QFuture<CadFetchTaskResult> future =
        QtConcurrent::run([normalizedId]() { return CadDataLoader::fetchAndParseCadData(normalizedId); });
    auto* watcher = new QFutureWatcher<CadFetchTaskResult>(&service);
    QObject::connect(
        watcher, &QFutureWatcher<CadFetchTaskResult>::finished, &service, [&service, watcher, generation]() {
            const CadFetchTaskResult result = watcher->result();
            watcher->deleteLater();
            service.handleCadFetchResult(result, generation);
        });
    watcher->setFuture(future);
}

}  // namespace EasyKiConverter
