#include "ComponentService.h"

#include "BomParser.h"
#include "CadDataLoader.h"
#include "ComponentCacheLoadWorker.h"
#include "ComponentInfoParser.h"
#include "ComponentQueueManager.h"
#include "ConfigService.h"
#include "PreviewImageDataEncoder.h"
#include "core/easyeda/EasyedaApi.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQueue>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>

#include <cstdlib>

namespace EasyKiConverter {

ComponentService::ComponentService(QObject* parent)
    : QObject(parent)
    , m_api(nullptr)
    , m_currentComponentId()
    , m_parallelContext(nullptr)
    , m_imageService(nullptr)
    , m_activeRequestCount(0)
    , m_maxConcurrentRequests(ConfigService::instance()->getValidationConcurrentCount())
    , m_queueManager(nullptr)
    , m_batchFetch3DModel(true) {
    try {
        m_api = new EasyedaApi();
        if (m_api) {
            m_api->setParent(this);
            m_api->setWeakNetworkSupport(ConfigService::instance()->getWeakNetworkSupport());
        }
        m_imageService = new LcscImageService(this);

        m_queueManager = new ComponentQueueManager(m_maxConcurrentRequests, this);
        connect(m_queueManager, &ComponentQueueManager::requestReady, this, [this](const QString& componentId) {
            fetchComponentDataInternal(componentId, m_batchFetch3DModel);
        });
        connect(m_queueManager, &ComponentQueueManager::queueEmpty, this, [this]() {
            qDebug() << "ComponentService: Queue empty signal received";
        });
        connect(m_queueManager, &ComponentQueueManager::timeout, this, [this]() { handleQueueTimeout(); });
    } catch (const std::bad_alloc& e) {
        qCritical() << "ComponentService: Memory allocation failed:" << e.what();
        std::terminate();
    } catch (...) {
        qCritical() << "ComponentService: Failed to initialize sub-services!";
        std::terminate();
    }

    initializeApiConnections();
    qDebug() << "ComponentService: Initialized successfully.";
}

ComponentService::ComponentService(EasyedaApi* api, QObject* parent)
    : QObject(parent)
    , m_api(api)
    , m_currentComponentId()
    , m_parallelContext(nullptr)
    , m_imageService(nullptr)
    , m_activeRequestCount(0)
    , m_maxConcurrentRequests(ConfigService::instance()->getValidationConcurrentCount())
    , m_queueManager(nullptr)
    , m_batchFetch3DModel(true) {
    if (m_api && !m_api->parent()) {
        m_api->setParent(this);
    }

    m_imageService = new LcscImageService(this);

    m_queueManager = new ComponentQueueManager(m_maxConcurrentRequests, this);
    connect(m_queueManager, &ComponentQueueManager::requestReady, this, [this](const QString& componentId) {
        fetchComponentDataInternal(componentId, m_batchFetch3DModel);
    });
    connect(m_queueManager, &ComponentQueueManager::queueEmpty, this, [this]() {
        qDebug() << "ComponentService: Queue empty signal received";
    });
    connect(m_queueManager, &ComponentQueueManager::timeout, this, [this]() { handleQueueTimeout(); });

    initializeApiConnections();
    qDebug() << "ComponentService (Injected API): Initialized successfully.";
}

/** @brief 连接网络服务与组件服务的异步信号。 */
void ComponentService::initializeApiConnections() {
    // 连接图片服务信号
    if (m_imageService) {
        connect(m_imageService, &LcscImageService::imageReady, this, &ComponentService::handleImageReady);
        connect(m_imageService, &LcscImageService::lcscDataReady, this, &ComponentService::handleLcscDataReady);
        connect(m_imageService, &LcscImageService::datasheetReady, this, &ComponentService::handleDatasheetReady);
        connect(m_imageService, &LcscImageService::allImagesReady, this, &ComponentService::handleAllImagesReady);
        connect(m_imageService, &LcscImageService::error, this, &ComponentService::handlePreviewImageError);
    }

    // 连接 API 信号
    if (m_api) {
        connect(m_api, &EasyedaApi::componentInfoFetched, this, &ComponentService::handleComponentInfoFetched);
        connect(m_api, &EasyedaApi::cadDataFetched, this, &ComponentService::handleCadDataFetched);
        connect(m_api, qOverload<const QString&>(&EasyedaApi::fetchError), this, &ComponentService::handleFetchError);
        connect(m_api,
                qOverload<const QString&, const QString&>(&EasyedaApi::fetchError),
                this,
                &ComponentService::handleFetchErrorWithId);
    }
}

ComponentService::~ComponentService() {}

/** @brief 请求单个元器件的完整数据。 */
void ComponentService::fetchComponentData(const QString& componentId, bool fetch3DModel) {
    fetchComponentDataInternal(componentId, fetch3DModel);
}

void ComponentService::initializeFetchingComponent(FetchingComponent& fetchingComponent,
                                                   const QString& componentId,
                                                   bool fetch3DModel) {
    fetchingComponent.componentId = componentId;
    fetchingComponent.fetch3DModel = fetch3DModel;
    fetchingComponent.hasComponentInfo = false;
    fetchingComponent.hasCadData = false;
    fetchingComponent.hasTriggeredLcscFetch = false;
    fetchingComponent.requestActive = true;
    fetchingComponent.errorMessage.clear();
    fetchingComponent.pendingAsyncDownloads = 0;
    fetchingComponent.cacheGeneration = ComponentCacheService::instance()->currentGeneration();
}

/** @brief 执行单个元器件数据请求的内部流程。 */
void ComponentService::fetchComponentDataInternal(const QString& componentId, bool fetch3DModel) {
    qDebug() << "Fetching component data (internal) for:" << componentId << "Fetch 3D:" << fetch3DModel
             << "at:" << QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

    // 确保 componentId 格式统一（大写）
    QString normalizedId = componentId.toUpper();

    // 使用互斥锁保护 m_currentComponentId 的并发访问
    {
        QMutexLocker locker(&m_currentIdMutex);
        m_currentComponentId = normalizedId;
    }

    ComponentData reusableData;
    uint64_t gen = 0;  // 请求创建时的缓存 generation，用于过滤旧回调
    bool reuseExistingData = false;
    bool shouldSkipAsDuplicate = false;

    // 先添加到 m_fetchingComponents（防止重复调度）
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        if (m_fetchingComponents.contains(normalizedId)) {
            const FetchingComponent& existing = m_fetchingComponents[normalizedId];
            const bool hasReusableData =
                !existing.data.lcscId().isEmpty() && (existing.hasCadData || existing.data.symbolData() != nullptr ||
                                                      existing.data.footprintData() != nullptr);

            if (hasReusableData) {
                reusableData = existing.data;
                reuseExistingData = true;
            } else if (m_parallelContext != nullptr || !existing.requestActive) {
                qDebug() << "ComponentService: Removing stale fetching state for" << normalizedId << "before retry";
                m_fetchingComponents.remove(normalizedId);
            } else {
                shouldSkipAsDuplicate = true;
            }
        }

        if (!reuseExistingData && !shouldSkipAsDuplicate && !m_fetchingComponents.contains(normalizedId)) {
            // 创建占位条目，防止重复调度
            FetchingComponent& fc = m_fetchingComponents[normalizedId];
            initializeFetchingComponent(fc, normalizedId, fetch3DModel);
            // 新请求已捕获当前 generation，仅解除当前组件的屏蔽并恢复全局写入。
            ComponentCacheService::instance()->clearTombstone(normalizedId);
            ComponentCacheService::instance()->clearGlobalTombstone();
        }
    }

    if (reuseExistingData) {
        qDebug() << "ComponentService: Reusing existing fetched data for:" << normalizedId;
        updateComponentCache(normalizedId, reusableData);
        emit cadDataReady(normalizedId, reusableData);

        if (m_parallelContext != nullptr) {
            handleParallelDataCollected(normalizedId, reusableData);
        }
        return;
    }

    if (shouldSkipAsDuplicate) {
        qDebug() << "ComponentService: Already fetching for" << normalizedId << ", skipping duplicate request";
        return;
    }

    // 检查是否有符号封装缓存，有则直接从缓存加载（元器件存在）
    ComponentCacheService* cache = ComponentCacheService::instance();
    if (m_api) {
        m_api->setWeakNetworkSupport(ConfigService::instance()->getWeakNetworkSupport());
    }
    if (cache->hasSymbolFootprintCache(normalizedId)) {
        qDebug() << "ComponentService: Symbol/Footprint cache hit for" << normalizedId
                 << ", loading from cache (component exists)";
        QTimer::singleShot(0, this, [this, normalizedId, fetch3DModel, cache]() {
            loadComponentDataFromCacheAsync(normalizedId, fetch3DModel, cache);
        });
        return;
    }

    // 符号封装缓存不存在，改为后台线程直接获取并解析 CAD 数据，避免主线程卡顿。
    // 从 FetchingComponent 读取请求创建时的 generation
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        auto it = m_fetchingComponents.find(normalizedId);
        if (it != m_fetchingComponents.end()) {
            gen = it->cacheGeneration;
        }
    }
    if (gen == 0) {
        qWarning() << "fetchComponentDataInternal: No FetchingComponent for" << normalizedId << ", aborting";
        return;
    }
    auto future = QtConcurrent::run([normalizedId]() { return CadDataLoader::fetchAndParseCadData(normalizedId); });
    auto* watcher = new QFutureWatcher<CadFetchTaskResult>(this);
    connect(watcher, &QFutureWatcher<CadFetchTaskResult>::finished, this, [this, watcher, gen]() {
        const CadFetchTaskResult result = watcher->result();
        watcher->deleteLater();

        {
            QMutexLocker locker(&m_fetchingComponentsMutex);
            const auto it = m_fetchingComponents.find(result.componentId);
            if (it == m_fetchingComponents.end() || it->cacheGeneration != gen) {
                qDebug() << "ComponentService: Discarding stale CAD result for" << result.componentId;
                return;
            }
        }

        if (!result.success) {
            emitFetchErrorAndClearState(result.componentId, result.errorMessage, gen);
            return;
        }

        {
            QMutexLocker locker(&m_fetchingComponentsMutex);
            auto it = m_fetchingComponents.find(result.componentId);
            if (it == m_fetchingComponents.end() || it->cacheGeneration != gen) {
                qDebug() << "ComponentService: Discarding stale CAD result for" << result.componentId;
                return;
            }
            it->data = result.parsed.componentData;
            it->hasCadData = true;
            it->requestActive = false;
        }

        updateComponentCache(result.componentId, result.parsed.componentData);
        emit cadDataReady(result.componentId, result.parsed.componentData);
        // 使用异步保存，不阻塞UI
        ComponentCacheService::instance()->saveComponentMetadataAsync(
            result.componentId, result.parsed.componentData, gen, /*replaceModel3DMetadata=*/true);
        ComponentCacheService::instance()->saveCadDataJson(
            result.componentId, QJsonDocument(result.parsed.resultData).toJson(QJsonDocument::Compact), gen);

        if (m_parallelContext != nullptr) {
            handleParallelDataCollected(result.componentId, result.parsed.componentData);
        }
    });
    watcher->setFuture(future);
}

void ComponentService::loadComponentDataFromCacheAsync(const QString& normalizedId,
                                                       bool fetch3DModel,
                                                       ComponentCacheService* cache) {
    // 从 FetchingComponent 读取请求创建时的 generation
    uint64_t gen = 0;
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        auto it = m_fetchingComponents.find(normalizedId);
        if (it != m_fetchingComponents.end()) {
            gen = it->cacheGeneration;
        }
    }
    if (gen == 0) {
        qWarning() << "loadComponentDataFromCacheAsync: No FetchingComponent for" << normalizedId << ", aborting";
        return;
    }
    // 在后台线程执行缓存加载（I/O 密集型）
    QFuture<ComponentCacheLoadResult> future = QtConcurrent::run([normalizedId, fetch3DModel, cache]() {
        return ComponentCacheLoadWorker::load(normalizedId, fetch3DModel, cache);
    });

    // 等待后台任务完成并在主线程处理结果
    QFutureWatcher<ComponentCacheLoadResult>* watcher = new QFutureWatcher<ComponentCacheLoadResult>(this);
    connect(
        watcher,
        &QFutureWatcher<ComponentCacheLoadResult>::finished,
        this,
        [this, watcher, normalizedId, fetch3DModel, gen]() {
            ComponentCacheLoadResult result = watcher->result();
            watcher->deleteLater();

            // 缓存读取可能跨越取消和重试，先校验请求代次，避免旧结果进入新请求。
            {
                QMutexLocker locker(&m_fetchingComponentsMutex);
                const auto it = m_fetchingComponents.find(normalizedId);
                if (it == m_fetchingComponents.end() || it->cacheGeneration != gen) {
                    qDebug() << "ComponentService: Discarding stale cache result for" << normalizedId;
                    return;
                }
            }

            if (!result.success || !result.cachedData) {
                qWarning() << "ComponentService: Failed to load cache for" << normalizedId
                           << ", falling back to network fetch";
                // 缓存加载失败时，回退到网络获取
                uint64_t retryGen = 0;
                {
                    QMutexLocker locker(&m_fetchingComponentsMutex);
                    auto it = m_fetchingComponents.find(normalizedId);
                    if (it == m_fetchingComponents.end() || it->cacheGeneration != gen) {
                        qDebug() << "ComponentService: Discarding stale cache retry for" << normalizedId;
                        return;
                    }
                    FetchingComponent& fetchingComponent = *it;
                    initializeFetchingComponent(fetchingComponent, normalizedId, fetch3DModel);
                    // retry 视为新请求，使用新的 generation
                    retryGen = fetchingComponent.cacheGeneration;
                }
                if (retryGen == 0) {
                    qWarning() << "Cache retry: No FetchingComponent for" << normalizedId << ", aborting";
                    return;
                }
                auto retryFuture =
                    QtConcurrent::run([normalizedId]() { return CadDataLoader::fetchAndParseCadData(normalizedId); });
                auto* retryWatcher = new QFutureWatcher<CadFetchTaskResult>(this);
                connect(
                    retryWatcher,
                    &QFutureWatcher<CadFetchTaskResult>::finished,
                    this,
                    [this, retryWatcher, retryGen]() {
                        const CadFetchTaskResult result = retryWatcher->result();
                        retryWatcher->deleteLater();

                        {
                            QMutexLocker locker(&m_fetchingComponentsMutex);
                            const auto it = m_fetchingComponents.find(result.componentId);
                            if (it == m_fetchingComponents.end() || it->cacheGeneration != retryGen) {
                                qDebug() << "ComponentService: Discarding stale retry result for" << result.componentId;
                                return;
                            }
                        }

                        if (!result.success) {
                            emitFetchErrorAndClearState(result.componentId, result.errorMessage, retryGen);
                            return;
                        }

                        {
                            QMutexLocker locker(&m_fetchingComponentsMutex);
                            auto it = m_fetchingComponents.find(result.componentId);
                            if (it == m_fetchingComponents.end() || it->cacheGeneration != retryGen) {
                                qDebug() << "ComponentService: Discarding stale retry result for" << result.componentId;
                                return;
                            }
                            it->data = result.parsed.componentData;
                            it->hasCadData = true;
                            it->requestActive = false;
                        }

                        updateComponentCache(result.componentId, result.parsed.componentData);
                        emit cadDataReady(result.componentId, result.parsed.componentData);
                        // 使用异步保存，不阻塞UI
                        ComponentCacheService::instance()->saveComponentMetadataAsync(
                            result.componentId, result.parsed.componentData, retryGen, /*replaceModel3DMetadata=*/true);
                        ComponentCacheService::instance()->saveCadDataJson(
                            result.componentId,
                            QJsonDocument(result.parsed.resultData).toJson(QJsonDocument::Compact),
                            retryGen);

                        if (m_parallelContext != nullptr) {
                            handleParallelDataCollected(result.componentId, result.parsed.componentData);
                        }
                    });
                retryWatcher->setFuture(retryFuture);
                return;
            }

            // 使用后台线程预解析的符号和封装数据
            if (result.symbolData) {
                result.cachedData->setSymbolData(result.symbolData);
            }
            if (result.footprintData) {
                result.cachedData->setFootprintData(result.footprintData);
                ComponentCacheLoadWorker::restoreModel3DFromFootprint(*result.cachedData, result.footprintData);
            }

            // 更新 m_fetchingComponents
            {
                QMutexLocker locker(&m_fetchingComponentsMutex);
                auto it = m_fetchingComponents.find(normalizedId);
                if (it == m_fetchingComponents.end() || it->cacheGeneration != gen) {
                    qDebug() << "ComponentService: Discarding stale cached component for" << normalizedId;
                    return;
                }
                FetchingComponent& fetchingComponent = *it;
                fetchingComponent.componentId = normalizedId;
                fetchingComponent.data = *result.cachedData;
                fetchingComponent.fetch3DModel = fetch3DModel;
                fetchingComponent.hasCadData =
                    (result.cachedData->symbolData() != nullptr && result.cachedData->footprintData() != nullptr);
                fetchingComponent.requestActive = false;
            }

            // 发送缓存加载的信号
            qDebug() << "ComponentService: Emitting cadDataReady for" << normalizedId
                     << "with symbolData:" << (result.cachedData->symbolData() != nullptr)
                     << "footprintData:" << (result.cachedData->footprintData() != nullptr);

            // 更新缓存
            updateComponentCache(normalizedId, *result.cachedData);

            emit cadDataReady(normalizedId, *result.cachedData);

            // 如果在并行模式，处理并行数据收集
            if (m_parallelContext != nullptr) {
                handleParallelDataCollected(normalizedId, *result.cachedData);
            }

            // 从缓存加载预览图数据（批量发送，避免频繁 UI 更新）
            if (!result.encodedPreviewImages.isEmpty()) {
                const QStringList encodedImages = result.encodedPreviewImages;
                QTimer::singleShot(0, this, [this, normalizedId, encodedImages]() {
                    emit previewImagesReady(normalizedId, encodedImages);
                });
            }

            // 加载数据手册
            if (!result.datasheetData.isEmpty()) {
                emit datasheetReady(normalizedId, result.datasheetData);
            }

            qDebug() << "ComponentService: Cache loaded successfully for" << normalizedId;
        });
    watcher->setFuture(future);
}

/** @brief 请求元器件预览图。 */
void ComponentService::fetchLcscPreviewImage(const QString& componentId) {
    qDebug() << "ComponentService: Fetching LCSC preview image for component:" << componentId;
    m_imageService->fetchPreviewImages(componentId);
}

/** @brief 批量请求元器件预览图。 */
void ComponentService::fetchBatchPreviewImages(const QStringList& componentIds) {
    qDebug() << "ComponentService: Fetching batch preview images for" << componentIds.count() << "components";
    m_imageService->fetchBatchPreviewImages(componentIds);
}

/** @brief 处理单张预览图下载完成事件。 */
void ComponentService::handleImageReady(const QString& componentId, const QByteArray& imageData, int imageIndex) {
    QImage image = QImage::fromData(imageData);
    if (image.isNull()) {
        qWarning() << "Failed to load image from data for component:" << componentId << "index:" << imageIndex
                   << "data size:" << imageData.size();
        return;
    }

    const QString normalizedId = componentId.toUpper();
    ComponentData updatedData;
    bool hasValidUpdate = false;
    uint64_t gen = 0;
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        const auto it = m_fetchingComponents.find(normalizedId);
        if (it == m_fetchingComponents.end() ||
            it->cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
            qDebug() << "ComponentService: Discarding stale image callback for" << componentId;
            return;
        }
        it->data.addPreviewImageData(imageData, imageIndex);
        updatedData = it->data;
        gen = it->cacheGeneration;
        hasValidUpdate = true;
    }

    if (hasValidUpdate) {
        emit previewImageReady(normalizedId, image, imageIndex);
        emit previewImageDataReady(normalizedId, imageData, imageIndex);
        if (ComponentCacheService::instance()->currentGeneration() != gen) {
            return;
        }
        m_componentCache.replaceIfPresent(normalizedId, updatedData);
    }
}

void ComponentService::handleLcscDataReady(const QString& componentId,
                                           const QString& manufacturerPart,
                                           const QString& datasheetUrl,
                                           const QStringList& imageUrls) {
    qDebug() << "LCSC data ready for component:" << componentId
             << "Manufacturer Part:" << (manufacturerPart.isEmpty() ? "none" : manufacturerPart)
             << "Datasheet:" << (datasheetUrl.isEmpty() ? "none" : datasheetUrl) << "Images:" << imageUrls.size();

    // 准备要更新的数据（在锁外构建，避免长时间持锁）
    ComponentData updatedData;
    bool hasValidUpdate = false;
    uint64_t gen = 0;

    // 加锁保护共享数据的访问
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        const QString normalizedId = componentId.toUpper();

        // 更新 m_fetchingComponents 中的数据
        if (m_fetchingComponents.contains(normalizedId)) {
            FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];
            if (fetchingComponent.cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
                qDebug() << "ComponentService: Discarding stale LCSC data callback for" << componentId;
                return;
            }

            // 保存制造商部件号
            if (!manufacturerPart.isEmpty()) {
                fetchingComponent.data.setManufacturerPart(manufacturerPart);
                qDebug() << "Manufacturer part saved to ComponentData:" << manufacturerPart;
            }

            // 保存数据手册 URL
            if (!datasheetUrl.isEmpty()) {
                fetchingComponent.data.setDatasheet(datasheetUrl);

                // 检测数据手册格式
                QString format = "pdf";
                if (datasheetUrl.toLower().contains(".html")) {
                    format = "html";
                }
                fetchingComponent.data.setDatasheetFormat(format);

                qDebug() << "Datasheet saved to ComponentData:" << datasheetUrl << "format:" << format;
            }

            // 保存预览图 URL 列表
            if (!imageUrls.isEmpty()) {
                fetchingComponent.data.setPreviewImages(imageUrls);
                qDebug() << "Preview images saved to ComponentData:" << imageUrls.size() << "images";

                // 预先创建指定数量的空元素，确保索引能够正确对应
                // 这样当图片按乱序下载时，能够填充到正确的索引位置
                QList<QByteArray> emptyImageDataList;
                emptyImageDataList.resize(imageUrls.size());
                fetchingComponent.data.setPreviewImageData(emptyImageDataList);
                qDebug() << "Pre-allocated" << imageUrls.size() << "empty image data slots";
            }

            // 复制数据用于锁外处理
            updatedData = fetchingComponent.data;
            gen = fetchingComponent.cacheGeneration;
            hasValidUpdate = true;
        } else {
            qWarning() << "Component" << componentId << "not found in m_fetchingComponents, cannot update LCSC data";
        }
    }  // 锁在这里释放

    // 锁外发送信号和保存缓存（避免信号槽死锁和锁顺序问题）
    if (hasValidUpdate) {
        if (ComponentCacheService::instance()->currentGeneration() != gen) {
            qDebug() << "ComponentService: Discarding stale LCSC data update for" << componentId;
            return;
        }
        // 发送 LCSC 数据更新信号，以便 ComponentListViewModel 可以更新缓存的 ComponentData
        emit lcscDataUpdated(componentId.toUpper(), manufacturerPart, datasheetUrl, imageUrls);

        // 保存到磁盘缓存（异步，不阻塞UI）
        ComponentCacheService::instance()->saveComponentMetadataAsync(componentId.toUpper(), updatedData, gen);

        // 更新内存缓存，确保 startPreload 时能获取到最新数据
        updateComponentCache(componentId.toUpper(), updatedData);
    }
}

/** @brief 处理数据手册下载完成事件。 */
void ComponentService::handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData) {
    qDebug() << "Datasheet downloaded for component:" << componentId << "size:" << datasheetData.size() << "bytes";

    // 准备数据用于锁外处理
    QString format;
    bool hasValidUpdate = false;
    bool shouldMarkCompleted = false;
    ComponentData completedData;
    uint64_t gen = 0;
    const QString normalizedId = componentId.toUpper();

    // 加锁保护共享数据的访问
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);

        // 更新 m_fetchingComponents 中的数据
        if (m_fetchingComponents.contains(normalizedId)) {
            FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];
            gen = fetchingComponent.cacheGeneration;
            if (gen != ComponentCacheService::instance()->currentGeneration()) {
                qDebug() << "ComponentService: Discarding stale datasheet callback for" << componentId;
                return;
            }
            fetchingComponent.data.setDatasheetData(datasheetData);

            // 检测数据手册格式（基于内容）
            format = fetchingComponent.data.datasheetFormat();
            if (format == "pdf" && !isPDF(datasheetData)) {
                format = "html";
                fetchingComponent.data.setDatasheetFormat(format);
            }

            qDebug() << "Datasheet data saved to ComponentData, size:" << datasheetData.size() << "bytes"
                     << "format:" << format;

            // 递减待处理的异步下载计数
            if (fetchingComponent.pendingAsyncDownloads > 0) {
                fetchingComponent.pendingAsyncDownloads--;
                qDebug() << "Datasheet download complete, pending async downloads:"
                         << fetchingComponent.pendingAsyncDownloads;
                if (fetchingComponent.pendingAsyncDownloads == 0) {
                    shouldMarkCompleted = true;
                    completedData = fetchingComponent.data;
                }
            }

            hasValidUpdate = true;
        } else {
            qWarning() << "Component" << componentId
                       << "not found in m_fetchingComponents, datasheet data will be saved directly to cache";
        }
    }  // 锁在这里释放

    // 如果所有异步下载都完成了，标记组件完成
    if (shouldMarkCompleted) {
        if (ComponentCacheService::instance()->currentGeneration() != gen) {
            qDebug() << "ComponentService: Discarding stale datasheet completion for" << componentId;
            return;
        }
        ParallelFetchContext* parallelContext = nullptr;
        {
            QMutexLocker locker(&m_parallelContextMutex);
            parallelContext = m_parallelContext;
        }
        if (parallelContext != nullptr) {
            parallelContext->markCompleted(normalizedId, completedData);
        }

        if (m_queueManager != nullptr) {
            m_queueManager->requestCompleted(normalizedId);
        }
    }

    // 更新缓存中的数据手册数据
    if (hasValidUpdate) {
        // 避免嵌套锁：先从 fetchingComponents 获取数据副本，再更新缓存
        ComponentData dataCopy;
        bool hasDataCopy = false;
        {
            QMutexLocker fetchLocker(&m_fetchingComponentsMutex);
            if (m_fetchingComponents.contains(normalizedId) &&
                m_fetchingComponents[normalizedId].cacheGeneration == gen) {
                dataCopy = m_fetchingComponents[normalizedId].data;
                hasDataCopy = true;
            }
        }

        if (hasDataCopy) {
            if (m_componentCache.replaceIfPresent(normalizedId, dataCopy)) {
                qDebug() << "ComponentService: Updated cache with datasheet data for" << normalizedId;
            }
        }
    } else {
        // 没有活动请求时无法确认回调归属，禁止将其写入内存缓存。
        qDebug() << "ComponentService: Discarding datasheet callback without active request for" << componentId;
    }

    // 锁外发送信号（避免信号槽死锁）
    if (hasValidUpdate) {
        // 发送数据手册就绪信号
        emit datasheetReady(componentId, datasheetData);
    }
}

/** @brief 处理预览图下载失败事件。 */
void ComponentService::handlePreviewImageError(const QString& componentId, const QString& error) {
    const QString normalizedId = componentId.toUpper();
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        const auto it = m_fetchingComponents.find(normalizedId);
        if (it == m_fetchingComponents.end() ||
            it->cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
            qDebug() << "ComponentService: Discarding stale preview error for" << componentId;
            return;
        }
    }

    if (error == QLatin1String("Image not found") || error == QLatin1String("Preview image URL not found") ||
        error == QLatin1String("No images downloaded") || error == QLatin1String("No preview image URLs available")) {
        qDebug() << "Preview image unavailable for component:" << componentId << "error:" << error;
    } else {
        qWarning() << "Preview image fetch error for component:" << componentId << "error:" << error;
    }

    // 发送预览图失败信号
    emit previewImageFailed(normalizedId, error);
}

/** @brief 处理全部预览图下载完成事件。 */
void ComponentService::handleAllImagesReady(const QString& componentId, const QStringList& imagePaths) {
    qDebug() << "All images ready for component:" << componentId << "paths:" << imagePaths.size();

    uint64_t gen = 0;
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        const auto it = m_fetchingComponents.find(componentId.toUpper());
        if (it == m_fetchingComponents.end()) {
            qDebug() << "ComponentService: Discarding all-images callback without active request for" << componentId;
            return;
        }
        gen = it->cacheGeneration;
    }

    // 注意：预览图已由 LcscImageService::handleDownloadResponse 在下载时保存到磁盘
    // 这里不需要再次保存，避免重复 I/O

    // 使用 QtConcurrent 在后台线程执行文件读取和 Base64 编码，避免阻塞 UI。
    // 后台任务不捕获 this，避免组件服务销毁后仍访问成员变量。
    QFuture<PreviewImageDataResult> future =
        QtConcurrent::run([imagePaths]() { return PreviewImageDataEncoder::encodeFiles(imagePaths); });

    // 使用 QFutureWatcher 在主线程接收结果并发送信号
    auto* watcher = new QFutureWatcher<PreviewImageDataResult>(this);
    connect(watcher,
            &QFutureWatcher<PreviewImageDataResult>::finished,
            this,
            [this, watcher, componentId, imagePaths, gen]() {
                const PreviewImageDataResult imageResult = watcher->result();
                watcher->deleteLater();

                if (ComponentCacheService::instance()->currentGeneration() != gen) {
                    qDebug() << "ComponentService: Discarding stale all-images result for" << componentId;
                    return;
                }
                const QString normalizedId = componentId.toUpper();
                {
                    QMutexLocker locker(&m_fetchingComponentsMutex);
                    const auto it = m_fetchingComponents.find(normalizedId);
                    if (it == m_fetchingComponents.end() || it->cacheGeneration != gen) {
                        qDebug() << "ComponentService: Discarding all-images result for replaced request"
                                 << componentId;
                        return;
                    }
                    it->data.setPreviewImageData(imageResult.imageData);
                    qDebug() << "All image data updated in ComponentData for component:" << componentId
                             << "count:" << imageResult.imageData.size();
                }
                emit previewImagesReady(componentId, imageResult.encodedImages);
                emit allImagesReady(componentId, imagePaths);
            });
    watcher->setFuture(future);
}

/** @brief 处理元器件基础信息响应。 */
void ComponentService::handleComponentInfoFetched(const QString& componentId, const QJsonObject& data) {
    const QString normalizedId = componentId.toUpper();
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        const auto it = m_fetchingComponents.find(normalizedId);
        if (it == m_fetchingComponents.end() ||
            it->cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
            qDebug() << "ComponentService: Discarding stale component info callback for" << componentId;
            return;
        }
    }

    const ComponentData componentData = ComponentInfoParser::parse(normalizedId, data);
    emit componentInfoReady(normalizedId, componentData);
}

/** @brief 处理 CAD 数据响应并启动解析流程。 */
void ComponentService::handleCadDataFetched(const QString& componentId, const QJsonObject& data) {
    // 从 FetchingComponent 读取请求创建时的 generation，而非当前值
    uint64_t gen = 0;
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        auto it = m_fetchingComponents.find(componentId.toUpper());
        if (it != m_fetchingComponents.end()) {
            gen = it->cacheGeneration;
        }
    }
    if (gen == 0) {
        // 找不到 FetchingComponent，说明是已过期的孤儿回调，直接丢弃
        qDebug() << "handleCadDataFetched: No FetchingComponent for" << componentId << ", discarding stale callback";
        return;
    }
    auto future =
        QtConcurrent::run([componentId, data]() { return CadDataLoader::parseCadPayload(componentId, data); });
    auto* watcher = new QFutureWatcher<CadParseResult>(this);
    connect(watcher, &QFutureWatcher<CadParseResult>::finished, this, [this, watcher, gen]() {
        const CadParseResult parsed = watcher->result();
        watcher->deleteLater();

        if (!parsed.success) {
            emitFetchErrorAndClearState(parsed.componentId, parsed.errorMessage, gen);
            return;
        }

        {
            QMutexLocker locker(&m_fetchingComponentsMutex);
            auto it = m_fetchingComponents.find(parsed.componentId);
            if (it == m_fetchingComponents.end() || it->cacheGeneration != gen) {
                qDebug() << "ComponentService: Discarding stale parsed CAD result for" << parsed.componentId;
                return;
            }
            it->data = parsed.componentData;
            it->hasCadData = true;
            it->requestActive = false;
        }

        updateComponentCache(parsed.componentId, parsed.componentData);
        emit cadDataReady(parsed.componentId, parsed.componentData);
        // 使用异步保存，不阻塞UI
        ComponentCacheService::instance()->saveComponentMetadataAsync(
            parsed.componentId, parsed.componentData, gen, /*replaceModel3DMetadata=*/true);
        ComponentCacheService::instance()->saveCadDataJson(
            parsed.componentId, QJsonDocument(parsed.resultData).toJson(QJsonDocument::Compact), gen);

        if (m_parallelContext != nullptr) {
            handleParallelDataCollected(parsed.componentId, parsed.componentData);
        }
    });
    watcher->setFuture(future);
}

/** @brief 处理未携带元器件编号的请求错误。 */
void ComponentService::handleFetchError(const QString& errorMessage) {
    emitFetchErrorAndClearState(m_currentComponentId, errorMessage);
}

/** @brief 处理携带元器件编号或模型 UUID 的请求错误。 */
void ComponentService::handleFetchErrorWithId(const QString& idOrUuid, const QString& error) {
    QString componentId = idOrUuid;

    // 如果在并行模式下，尝试解析 UUID 为组件 ID
    if (m_parallelContext != nullptr) {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        // 遍历查找匹配的 UUID
        for (auto it = m_fetchingComponents.begin(); it != m_fetchingComponents.end(); ++it) {
            if (it.value().data.model3DData() && it.value().data.model3DData()->uuid() == idOrUuid) {
                componentId = it.key();
                qDebug() << "Resolved UUID" << idOrUuid << "to component ID" << componentId;
                break;
            }
        }
    }

    emitFetchErrorAndClearState(componentId, error);
}

/** @brief 设置导出输出目录。 */
void ComponentService::setOutputPath(const QString& path) {
    m_outputPath = path;
}

/** @brief 返回当前导出输出目录。 */
QString ComponentService::getOutputPath() const {
    return m_outputPath;
}

/** @brief 启动多个元器件的并行数据请求。 */
void ComponentService::fetchMultipleComponentsData(const QStringList& componentIds, bool fetch3DModel) {
    m_maxConcurrentRequests = ConfigService::instance()->getValidationConcurrentCount();
    if (m_queueManager != nullptr) {
        m_queueManager->setMaxConcurrentRequests(m_maxConcurrentRequests);
    }

    QStringList normalizedComponentIds;
    QSet<QString> seenComponentIds;
    for (const QString& componentId : componentIds) {
        const QString normalizedId = componentId.toUpper();
        if (!normalizedId.isEmpty() && !seenComponentIds.contains(normalizedId)) {
            seenComponentIds.insert(normalizedId);
            normalizedComponentIds.append(normalizedId);
        }
    }

    qDebug() << "Fetching data for" << normalizedComponentIds.size()
             << "components with async queue (max concurrent:" << m_maxConcurrentRequests << ")";

    // 防止重复启动批量处理，避免队列状态混乱
    if (m_parallelContext != nullptr) {
        qWarning() << "Batch processing already in progress, ignoring new request";
        return;
    }

    // 初始化并行获取状态
    m_parallelContext = new ParallelFetchContext(this);
    connect(m_parallelContext, &ParallelFetchContext::allCompleted, this, [this](const QList<ComponentData>& data) {
        const QMap<QString, QString> failedComponents = m_parallelContext->failedComponents();
        emit allComponentsDataCollected(data);
        emit allComponentsDataCollectedWithErrors(data, failedComponents);
        m_activeRequestCount = 0;
        resetQueueState();
    });
    m_parallelContext->start(normalizedComponentIds.size());
    m_activeRequestCount = 0;
    m_batchFetch3DModel = fetch3DModel;

    // 使用 ComponentQueueManager 管理队列
    m_queueManager->start(normalizedComponentIds);
}

/** @brief 处理并行请求中的单个元器件完成事件。 */
void ComponentService::handleParallelDataCollected(const QString& componentId, const ComponentData& data) {
    Q_UNUSED(data);
    qDebug() << "Parallel data collected for:" << componentId;

    ComponentData completedData;
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        if (m_fetchingComponents.contains(componentId)) {
            FetchingComponent& fc = m_fetchingComponents[componentId];
            completedData = fc.data;
        }
    }

    ParallelFetchContext* parallelContext = nullptr;
    {
        QMutexLocker locker(&m_parallelContextMutex);
        parallelContext = m_parallelContext;
    }
    if (parallelContext != nullptr) {
        parallelContext->markCompleted(componentId, completedData);
    }

    if (m_queueManager != nullptr) {
        m_queueManager->requestCompleted(componentId);
    }
}

/** @brief 处理并行请求中的单个元器件失败事件。 */
void ComponentService::handleParallelFetchError(const QString& componentId, const QString& error) {
    qDebug() << "Parallel fetch error for:" << componentId << error;

    Q_UNUSED(error);
    ParallelFetchContext* parallelContext = nullptr;
    {
        QMutexLocker locker(&m_parallelContextMutex);
        parallelContext = m_parallelContext;
    }
    if (parallelContext != nullptr) {
        parallelContext->markFailed(componentId, error);
    }

    if (m_queueManager != nullptr) {
        m_queueManager->requestCompleted(componentId);
    }
}

/** @brief 校验元器件编号格式。 */
bool ComponentService::validateComponentId(const QString& componentId) const {
    return BomParser::validateId(componentId);
}

/** @brief 从文本中提取元器件编号。 */
QStringList ComponentService::extractComponentIdFromText(const QString& text) const {
    return BomParser::extractIdsFromText(text);
}

/** @brief 解析 BOM 文件中的元器件编号。 */
QStringList ComponentService::parseBomFile(const QString& filePath) {
    BomParser parser;
    return parser.parse(filePath);
}

/** @brief 从内存缓存中读取元器件数据。 */
ComponentData ComponentService::getComponentData(const QString& componentId) const {
    const QString normalizedId = componentId.toUpper();
    return m_componentCache.value(normalizedId);
}

/** @brief 更新内存中的元器件缓存。 */
void ComponentService::updateComponentCache(const QString& componentId, const ComponentData& data) {
    const QString normalizedId = componentId.toUpper();
    m_componentCache.set(normalizedId, data);
    qDebug() << "ComponentService: Updated cache for" << normalizedId;
}

/** @brief 更新缓存元器件的描述字段。 */
void ComponentService::updateComponentDescription(const QString& componentId, const QString& description) {
    const QString normalizedId = componentId.toUpper();
    m_componentCache.updateDescription(normalizedId, description);
}

/** @brief 清理元器件及其预览图缓存。 */
void ComponentService::clearCache() {
    m_componentCache.clear();

    // 清空 LCSC 图片服务的缓存
    if (m_imageService) {
        m_imageService->clearCache();
    }

    // 清空 ComponentCacheService 的内存缓存，防止内存累积
    ComponentCacheService::instance()->clearMemoryCache();

    qDebug() << "Component cache cleared (including LCSC image service and memory cache)";
}

/** @brief 取消全部预览图请求。 */
void ComponentService::cancelAllPreviewImageFetches() {
    qDebug() << "ComponentService: Cancelling all preview image fetches";
    if (m_imageService) {
        m_imageService->cancelAll();
    }
}

/** @brief 取消全部未完成的元器件请求。 */
void ComponentService::cancelAllPendingRequests() {
    qDebug() << "ComponentService: Cancelling all pending component data requests";

    // 通过当前 API 实例取消请求，确保注入的网络客户端也能同步清理活动请求。
    if (m_api) {
        m_api->cancelRequest();
    }
    // 同步清理全局网络客户端中的排队请求，避免取消后的队列任务阻塞后续请求。
    NetworkClient::instance().cancelAllRequests();

    // 清空正在获取的组件记录，防止响应到达时更新已清除的数据
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        m_fetchingComponents.clear();
    }

    // 取消预览图获取
    if (m_imageService) {
        m_imageService->cancelAll();
    }

    qDebug() << "ComponentService: All pending requests cancelled";
}

/** @brief 取消指定元器件的请求。 */
void ComponentService::cancelRequestForComponent(const QString& componentId) {
    QString normalizedId = componentId.toUpper();
    qDebug() << "ComponentService: Cancelling request for component" << normalizedId;

    // 从正在获取的组件记录中移除，防止响应到达时更新已清除的数据
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        m_fetchingComponents.remove(normalizedId);
    }

    // 同步取消 EasyEDA API 请求，避免单组件删除后仍占用网络请求配额。
    if (m_api) {
        m_api->cancelRequestForId(normalizedId);
    }

    // 取消预览图获取
    if (m_imageService) {
        m_imageService->cancelRequestForComponent(normalizedId);
    }

    qDebug() << "ComponentService: Request cancelled for component" << normalizedId;
}

/** @brief 清理失败请求状态并发送错误信号。 */
void ComponentService::emitFetchErrorAndClearState(const QString& componentId,
                                                   const QString& error,
                                                   uint64_t expectedGeneration) {
    qWarning() << "Fetch error for component" << componentId << ":" << error;

    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        const auto it = m_fetchingComponents.find(componentId.toUpper());
        if (it == m_fetchingComponents.end() ||
            (expectedGeneration != 0 && it->cacheGeneration != expectedGeneration)) {
            if (expectedGeneration != 0) {
                qDebug() << "ComponentService: Discarding stale error for" << componentId;
            }
            return;
        }
        m_fetchingComponents.erase(it);
    }
    m_componentCache.removeIfInvalid(componentId.toUpper());

    handleParallelFetchError(componentId, error);

    // fetchError 必须最后发送，因为连接的槽函数可能会删除本对象
    emit fetchError(componentId, error);
}

/** @brief 判断数据是否具有 PDF 文件签名。 */
bool ComponentService::isPDF(const QByteArray& data) const {
    // PDF 文件以 %PDF- 开头
    if (data.size() < 5) {
        return false;
    }
    return data.startsWith("%PDF-");
}

// 异步队列管理方法实现
// 使用 ComponentQueueManager 管理队列

/** @brief 处理批量请求队列超时。 */
void ComponentService::handleQueueTimeout() {
    qWarning() << "Queue timeout reached";

    if (m_parallelContext != nullptr) {
        int completedCount = m_parallelContext->completedCount();
        int totalCount = m_parallelContext->totalCount();
        qWarning() << "Queue timeout - Completed:" << completedCount << "Total:" << totalCount;

        if (completedCount > 0) {
            QList<ComponentData> allData = m_parallelContext->collectedData();
            const QMap<QString, QString> failedComponents = m_parallelContext->failedComponents();
            emit allComponentsDataCollected(allData);
            emit allComponentsDataCollectedWithErrors(allData, failedComponents);
        }

        resetQueueState();
    }
}

/** @brief 中止当前批量请求并清理状态。 */
void ComponentService::abortBatchFetch() {
    // 取消所有网络请求（包括预览图、数据手册、CAD数据）
    cancelAllPendingRequests();
    // 重置队列和并行上下文
    resetQueueState();
    qDebug() << "ComponentService: Batch fetch aborted, all state cleared";
}

/** @brief 重置批量请求队列及并行上下文。 */
void ComponentService::resetQueueState() {
    m_queueManager->stop();
    m_activeRequestCount = 0;

    {
        QMutexLocker locker(&m_parallelContextMutex);
        if (m_parallelContext != nullptr) {
            m_parallelContext->deleteLater();
            m_parallelContext = nullptr;
        }
    }

    qDebug() << "Queue state reset completed";
}

}  // namespace EasyKiConverter
