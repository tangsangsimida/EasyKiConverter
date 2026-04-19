#include "ComponentService.h"

#include "BomParser.h"
#include "ComponentQueueManager.h"
#include "ConfigService.h"
#include "core/easyeda/EasyedaApi.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "core/network/NetworkClient.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQueue>
#include <QRegularExpression>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QUuid>
#include <QtConcurrent>

#include <cstdlib>

namespace EasyKiConverter {

namespace {

const QString EASYEDA_COMPONENT_API_ENDPOINT =
    QStringLiteral("https://easyeda.com/api/products/%1/components?version=6.5.51");

struct CadParseResult {
    QString componentId;
    QJsonObject resultData;
    ComponentData componentData;
    QString errorMessage;
    bool success = false;
};

struct CadFetchTaskResult {
    QString componentId;
    CadParseResult parsed;
    QString errorMessage;
    bool success = false;
};

QString normalizePreviewImageUrl(const QString& imageUrl) {
    QString normalizedUrl = imageUrl.trimmed();
    if (normalizedUrl.isEmpty()) {
        return QString();
    }

    if (normalizedUrl.startsWith(QStringLiteral("//"))) {
        normalizedUrl.prepend(QStringLiteral("https:"));
    } else if (normalizedUrl.startsWith(QStringLiteral("/image.lceda.cn/")) ||
               normalizedUrl.startsWith(QStringLiteral("/file.elecfans.com/")) ||
               normalizedUrl.startsWith(QStringLiteral("/www.lcsc.com/"))) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://"));
    } else if (normalizedUrl.startsWith(QStringLiteral("/web1/")) ||
               normalizedUrl.startsWith(QStringLiteral("/M00/"))) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://file.elecfans.com/"));
    } else if (normalizedUrl.startsWith('/')) {
        normalizedUrl.remove(0, 1);
        normalizedUrl.prepend(QStringLiteral("https://image.lceda.cn/"));
    }

    normalizedUrl.replace(QStringLiteral("https://image.lceda.cn//image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));
    normalizedUrl.replace(QStringLiteral("http://image.lceda.cn//image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));
    normalizedUrl.replace(QStringLiteral("https://image.lceda.cn/image.lceda.cn/"),
                          QStringLiteral("https://image.lceda.cn/"));

    return normalizedUrl;
}

int previewImageIndexFromPath(const QString& path) {
    static const QRegularExpression re(QStringLiteral("preview_(\\d+)\\.jpg$"),
                                       QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = re.match(path);
    if (!match.hasMatch()) {
        return -1;
    }

    bool ok = false;
    const int index = match.captured(1).toInt(&ok);
    return ok ? index : -1;
}

CadParseResult parseCadPayload(const QString& componentId, const QJsonObject& rawData) {
    CadParseResult parsed;
    parsed.componentId = componentId;

    QJsonObject resultData = rawData;
    if (rawData.contains("result")) {
        resultData = rawData.value("result").toObject();
    }

    if (resultData.isEmpty()) {
        parsed.errorMessage = QStringLiteral("Empty CAD data");
        return parsed;
    }

    ComponentData componentData;
    componentData.setLcscId(componentId);
    componentData.setName(resultData.value("title").toString());
    componentData.setPackage(resultData.value("package").toString());
    componentData.setManufacturer(resultData.value("manufacturer").toString());
    componentData.setDatasheet(resultData.value("datasheet").toString());

    EasyedaSymbolImporter symbolImporter;
    QSharedPointer<SymbolData> symbolData = symbolImporter.importSymbolData(resultData);
    if (!symbolData) {
        parsed.errorMessage = QStringLiteral("Failed to parse Symbol data from EasyEDA JSON");
        return parsed;
    }
    componentData.setSymbolData(symbolData);

    if (!symbolData->info().thumb.isEmpty()) {
        const QString thumbUrl = normalizePreviewImageUrl(symbolData->info().thumb);
        componentData.setPreviewImages(QStringList() << thumbUrl);
    }

    if (!symbolData->info().datasheet.isEmpty() && componentData.datasheet().isEmpty()) {
        const QString datasheetUrl = normalizePreviewImageUrl(symbolData->info().datasheet);
        componentData.setDatasheet(datasheetUrl);
    }

    EasyedaFootprintImporter footprintImporter;
    QSharedPointer<FootprintData> footprintData = footprintImporter.importFootprintData(resultData);
    if (!footprintData) {
        parsed.errorMessage =
            QStringLiteral("Failed to parse Footprint data from EasyEDA JSON (Library might be corrupted)");
        return parsed;
    }
    componentData.setFootprintData(footprintData);

    QString modelUuid = footprintData->model3D().uuid();
    if (modelUuid.isEmpty() && resultData.contains("dataStr")) {
        const QJsonObject dataStr = resultData.value("dataStr").toObject();
        const QJsonArray shapes = dataStr.value("shape").toArray();
        for (const QJsonValue& shapeVal : shapes) {
            const QString shapeStr = shapeVal.toString();
            if (!shapeStr.startsWith(QStringLiteral("SVGNODE~"))) {
                continue;
            }

            const int tildeIndex = shapeStr.indexOf('~');
            if (tildeIndex < 0) {
                continue;
            }

            const QJsonDocument nodeDoc = QJsonDocument::fromJson(shapeStr.mid(tildeIndex + 1).toUtf8());
            const QJsonObject attrs = nodeDoc.object().value("attrs").toObject();
            if (attrs.value("c_etype").toString() == QLatin1String("outline3D")) {
                modelUuid = attrs.value("uuid").toString();
                if (!modelUuid.isEmpty()) {
                    break;
                }
            }
        }

        if (modelUuid.isEmpty()) {
            modelUuid = dataStr.value("head").toObject().value("uuid_3d").toString();
        }
    }

    if (!modelUuid.isEmpty()) {
        auto model3DData = QSharedPointer<Model3DData>::create();
        model3DData->setUuid(modelUuid);
        model3DData->setName(footprintData->model3D().name());
        model3DData->setTranslation(footprintData->model3D().translation());
        model3DData->setRotation(footprintData->model3D().rotation());
        componentData.setModel3DData(model3DData);
    }

    parsed.resultData = resultData;
    parsed.componentData = componentData;
    parsed.success = true;
    return parsed;
}

CadFetchTaskResult fetchAndParseCadData(const QString& componentId) {
    CadFetchTaskResult taskResult;
    taskResult.componentId = componentId;

    const RequestProfile profile = RequestProfiles::cadData();
    const RetryPolicy policy = RetryPolicy::fromProfile(profile, ConfigService::instance()->getWeakNetworkSupport());
    const QUrl url(EASYEDA_COMPONENT_API_ENDPOINT.arg(componentId));
    const NetworkResult networkResult = NetworkClient::instance().get(url, ResourceType::CadData, policy);
    if (!networkResult.success) {
        taskResult.errorMessage =
            networkResult.error.isEmpty() ? QStringLiteral("CAD request failed") : networkResult.error;
        return taskResult;
    }

    QJsonParseError parseError;
    const QJsonDocument jsonDoc = QJsonDocument::fromJson(networkResult.data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !jsonDoc.isObject()) {
        taskResult.errorMessage = QStringLiteral("Failed to parse CAD response JSON");
        return taskResult;
    }

    taskResult.parsed = parseCadPayload(componentId, jsonDoc.object());
    if (!taskResult.parsed.success) {
        taskResult.errorMessage = taskResult.parsed.errorMessage;
        return taskResult;
    }

    taskResult.success = true;
    return taskResult;
}

}  // namespace

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
    fetchingComponent.errorMessage.clear();
    fetchingComponent.pendingAsyncDownloads = 0;
}

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
    bool reuseExistingForParallel = false;
    bool shouldSkipAsDuplicate = false;

    // 先添加到 m_fetchingComponents（防止重复调度）
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);
        if (m_fetchingComponents.contains(normalizedId)) {
            const FetchingComponent& existing = m_fetchingComponents[normalizedId];
            const bool hasReusableData =
                !existing.data.lcscId().isEmpty() && (existing.hasCadData || existing.data.symbolData() != nullptr ||
                                                      existing.data.footprintData() != nullptr);

            if (m_parallelContext != nullptr && hasReusableData) {
                reusableData = existing.data;
                reuseExistingForParallel = true;
            } else if (m_parallelContext != nullptr && !hasReusableData) {
                qDebug() << "ComponentService: Removing stale fetching state for" << normalizedId
                         << "before parallel preload retry";
                m_fetchingComponents.remove(normalizedId);
            } else {
                shouldSkipAsDuplicate = true;
            }
        }

        if (!reuseExistingForParallel && !shouldSkipAsDuplicate && !m_fetchingComponents.contains(normalizedId)) {
            // 创建占位条目，防止重复调度
            FetchingComponent& fc = m_fetchingComponents[normalizedId];
            initializeFetchingComponent(fc, normalizedId, fetch3DModel);
        }
    }

    if (reuseExistingForParallel) {
        qDebug() << "ComponentService: Reusing existing fetched data for parallel preload:" << normalizedId;
        updateComponentCache(normalizedId, reusableData);
        handleParallelDataCollected(normalizedId, reusableData);
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
    auto future = QtConcurrent::run([normalizedId]() { return fetchAndParseCadData(normalizedId); });
    auto* watcher = new QFutureWatcher<CadFetchTaskResult>(this);
    connect(watcher, &QFutureWatcher<CadFetchTaskResult>::finished, this, [this, watcher]() {
        const CadFetchTaskResult result = watcher->result();
        watcher->deleteLater();

        {
            QMutexLocker locker(&m_fetchingComponentsMutex);
            if (!m_fetchingComponents.contains(result.componentId)) {
                return;
            }
        }

        if (!result.success) {
            emit fetchError(result.componentId, result.errorMessage);
            return;
        }

        {
            QMutexLocker locker(&m_fetchingComponentsMutex);
            auto it = m_fetchingComponents.find(result.componentId);
            if (it == m_fetchingComponents.end()) {
                return;
            }
            it->data = result.parsed.componentData;
            it->hasCadData = true;
        }

        updateComponentCache(result.componentId, result.parsed.componentData);
        emit cadDataReady(result.componentId, result.parsed.componentData);
        ComponentCacheService::instance()->saveComponentMetadata(result.componentId, result.parsed.componentData);
        ComponentCacheService::instance()->saveCadDataJson(
            result.componentId, QJsonDocument(result.parsed.resultData).toJson(QJsonDocument::Compact));

        if (m_parallelContext != nullptr) {
            handleParallelDataCollected(result.componentId, result.parsed.componentData);
        }
    });
    watcher->setFuture(future);
}

void ComponentService::loadComponentDataFromCacheAsync(const QString& normalizedId,
                                                       bool fetch3DModel,
                                                       ComponentCacheService* cache) {
    // 在后台线程执行缓存加载（I/O 密集型）
    QFuture<CacheLoadResult> future = QtConcurrent::run([normalizedId, fetch3DModel, cache]() -> CacheLoadResult {
        CacheLoadResult result;
        result.componentId = normalizedId;
        result.success = false;

        QSharedPointer<ComponentData> cachedData = cache->loadComponentData(normalizedId);
        if (!cachedData) {
            return result;
        }

        // 从缓存加载CAD数据并重新导入符号和封装
        QByteArray cadJsonData = cache->loadCadDataJson(normalizedId);
        if (!cadJsonData.isEmpty()) {
            QJsonParseError parseError;
            QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonData, &parseError);
            if (parseError.error == QJsonParseError::NoError && cadDoc.isObject()) {
                QJsonObject cadDataObj = cadDoc.object();
                // 在后台线程创建 importer 并解析符号和封装数据
                EasyedaSymbolImporter symbolImporter;
                result.symbolData = symbolImporter.importSymbolData(cadDataObj);
                EasyedaFootprintImporter footprintImporter;
                result.footprintData = footprintImporter.importFootprintData(cadDataObj);
            }
        }

        // 从 metadata.json 恢复 model3DData
        QJsonObject metadata = cache->loadMetadata(normalizedId);
        if (metadata.contains("model3duuid")) {
            auto model3DData = QSharedPointer<Model3DData>::create();
            model3DData->setUuid(metadata.value("model3duuid").toString());
            cachedData->setModel3DData(model3DData);
        }

        result.cachedData = cachedData;
        result.success = true;

        // 加载预览图数据（返回字节数据，在主线程创建 QImage）
        for (int i = 0; i < 3; ++i) {
            QByteArray imageData = cache->loadPreviewImage(normalizedId, i);
            if (!imageData.isEmpty()) {
                result.previewImageData.append({i, imageData});
            }
        }

        // 加载数据手册
        QByteArray datasheetData = cache->loadDatasheet(normalizedId);
        if (!datasheetData.isEmpty()) {
            result.datasheetData = datasheetData;
        }

        return result;
    });

    // 等待后台任务完成并在主线程处理结果
    QFutureWatcher<CacheLoadResult>* watcher = new QFutureWatcher<CacheLoadResult>(this);
    connect(watcher, &QFutureWatcher<CacheLoadResult>::finished, this, [this, watcher, normalizedId, fetch3DModel]() {
        CacheLoadResult result = watcher->result();
        watcher->deleteLater();

        if (!result.success || !result.cachedData) {
            qWarning() << "ComponentService: Failed to load cache for" << normalizedId
                       << ", falling back to network fetch";
            // 缓存加载失败时，回退到网络获取
            {
                QMutexLocker locker(&m_fetchingComponentsMutex);
                FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];
                initializeFetchingComponent(fetchingComponent, normalizedId, fetch3DModel);
            }
            auto retryFuture = QtConcurrent::run([normalizedId]() { return fetchAndParseCadData(normalizedId); });
            auto* retryWatcher = new QFutureWatcher<CadFetchTaskResult>(this);
            connect(retryWatcher, &QFutureWatcher<CadFetchTaskResult>::finished, this, [this, retryWatcher]() {
                const CadFetchTaskResult result = retryWatcher->result();
                retryWatcher->deleteLater();

                {
                    QMutexLocker locker(&m_fetchingComponentsMutex);
                    if (!m_fetchingComponents.contains(result.componentId)) {
                        return;
                    }
                }

                if (!result.success) {
                    emit fetchError(result.componentId, result.errorMessage);
                    return;
                }

                {
                    QMutexLocker locker(&m_fetchingComponentsMutex);
                    auto it = m_fetchingComponents.find(result.componentId);
                    if (it == m_fetchingComponents.end()) {
                        return;
                    }
                    it->data = result.parsed.componentData;
                    it->hasCadData = true;
                }

                updateComponentCache(result.componentId, result.parsed.componentData);
                emit cadDataReady(result.componentId, result.parsed.componentData);
                ComponentCacheService::instance()->saveComponentMetadata(result.componentId,
                                                                         result.parsed.componentData);
                ComponentCacheService::instance()->saveCadDataJson(
                    result.componentId, QJsonDocument(result.parsed.resultData).toJson(QJsonDocument::Compact));

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
        }

        // 更新 m_fetchingComponents
        {
            QMutexLocker locker(&m_fetchingComponentsMutex);
            FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];
            fetchingComponent.componentId = normalizedId;
            fetchingComponent.data = *result.cachedData;
            fetchingComponent.fetch3DModel = fetch3DModel;
            fetchingComponent.hasCadData =
                (result.cachedData->symbolData() != nullptr && result.cachedData->footprintData() != nullptr);
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
        // 预览图数据是 JPG 字节，直接编码为 base64 字符串
        if (!result.previewImageData.isEmpty()) {
            QStringList encodedImages(3);  // 预分配3个位置
            for (const auto& [imageIndex, imageData] : result.previewImageData) {
                if (imageIndex >= 0 && imageIndex < 3) {
                    encodedImages[imageIndex] = QString::fromLatin1(imageData.toBase64().data());
                }
            }
            // 使用 QTimer::singleShot(0) 将信号发射调度到下一个事件循环
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

void ComponentService::fetchLcscPreviewImage(const QString& componentId) {
    qDebug() << "ComponentService: Fetching LCSC preview image for component:" << componentId;
    m_imageService->fetchPreviewImages(componentId);
}

void ComponentService::fetchBatchPreviewImages(const QStringList& componentIds) {
    qDebug() << "ComponentService: Fetching batch preview images for" << componentIds.count() << "components";
    m_imageService->fetchBatchPreviewImages(componentIds);
}

void ComponentService::handleImageReady(const QString& componentId, const QByteArray& imageData, int imageIndex) {
    QImage image = QImage::fromData(imageData);
    if (!image.isNull()) {
        emit previewImageReady(componentId, image, imageIndex);
        emit previewImageDataReady(componentId, imageData, imageIndex);
        QMutexLocker locker(&m_fetchingComponentsMutex);
        if (m_fetchingComponents.contains(componentId)) {
            FetchingComponent& fetchingComponent = m_fetchingComponents[componentId];
            fetchingComponent.data.addPreviewImageData(imageData, imageIndex);

            // 更新缓存中的预览图数据
            {
                QMutexLocker cacheLocker(&m_componentCacheMutex);
                if (m_componentCache.contains(componentId)) {
                    m_componentCache[componentId] = fetchingComponent.data;
                }
            }
        }
    } else {
        qWarning() << "Failed to load image from data for component:" << componentId << "index:" << imageIndex
                   << "data size:" << imageData.size();
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

    // 加锁保护共享数据的访问
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);

        // 更新 m_fetchingComponents 中的数据
        if (m_fetchingComponents.contains(componentId)) {
            FetchingComponent& fetchingComponent = m_fetchingComponents[componentId];

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
            hasValidUpdate = true;
        } else {
            qWarning() << "Component" << componentId << "not found in m_fetchingComponents, cannot update LCSC data";
        }
    }  // 锁在这里释放

    // 锁外发送信号和保存缓存（避免信号槽死锁和锁顺序问题）
    if (hasValidUpdate) {
        // 发送 LCSC 数据更新信号，以便 ComponentListViewModel 可以更新缓存的 ComponentData
        emit lcscDataUpdated(componentId, manufacturerPart, datasheetUrl, imageUrls);

        // 保存到磁盘缓存
        ComponentCacheService::instance()->saveComponentMetadata(componentId, updatedData);

        // 更新内存缓存，确保 startPreload 时能获取到最新数据
        updateComponentCache(componentId, updatedData);
    }
}

void ComponentService::handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData) {
    qDebug() << "Datasheet downloaded for component:" << componentId << "size:" << datasheetData.size() << "bytes";

    // 准备数据用于锁外处理
    QString format;
    bool hasValidUpdate = false;
    bool shouldMarkCompleted = false;
    ComponentData completedData;

    // 加锁保护共享数据的访问
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);

        // 更新 m_fetchingComponents 中的数据
        if (m_fetchingComponents.contains(componentId)) {
            FetchingComponent& fetchingComponent = m_fetchingComponents[componentId];
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

    // 更新缓存中的数据手册数据
    if (hasValidUpdate) {
        // 避免嵌套锁：先从 fetchingComponents 获取数据副本，再更新缓存
        ComponentData dataCopy;
        bool hasDataCopy = false;
        {
            QMutexLocker fetchLocker(&m_fetchingComponentsMutex);
            if (m_fetchingComponents.contains(componentId)) {
                dataCopy = m_fetchingComponents[componentId].data;
                hasDataCopy = true;
            }
        }

        if (hasDataCopy) {
            QMutexLocker cacheLocker(&m_componentCacheMutex);
            if (m_componentCache.contains(componentId)) {
                m_componentCache[componentId] = dataCopy;
                qDebug() << "ComponentService: Updated cache with datasheet data for" << componentId;
            }
        }
    } else {
        // 组件不在 m_fetchingComponents 中（可能在批处理模式下已移除）
        // 直接更新内存缓存
        QMutexLocker cacheLocker(&m_componentCacheMutex);
        if (m_componentCache.contains(componentId)) {
            m_componentCache[componentId].setDatasheetData(datasheetData);
            // 检测数据手册格式
            format = m_componentCache[componentId].datasheetFormat();
            if (format.isEmpty()) {
                format = "pdf";
            }
            if (format == "pdf" && !isPDF(datasheetData)) {
                format = "html";
            }
            m_componentCache[componentId].setDatasheetFormat(format);
            qDebug() << "ComponentService: Updated cache directly for" << componentId;
            hasValidUpdate = true;
        }
    }

    // 锁外发送信号（避免信号槽死锁）
    if (hasValidUpdate) {
        // 发送数据手册就绪信号
        emit datasheetReady(componentId, datasheetData);
    }
}

void ComponentService::handlePreviewImageError(const QString& componentId, const QString& error) {
    if (error == QLatin1String("Image not found") || error == QLatin1String("Preview image URL not found") ||
        error == QLatin1String("No images downloaded") || error == QLatin1String("No preview image URLs available")) {
        qDebug() << "Preview image unavailable for component:" << componentId << "error:" << error;
    } else {
        qWarning() << "Preview image fetch error for component:" << componentId << "error:" << error;
    }

    // 发送预览图失败信号
    emit previewImageFailed(componentId, error);
}

void ComponentService::handleAllImagesReady(const QString& componentId, const QStringList& imagePaths) {
    qDebug() << "All images ready for component:" << componentId << "paths:" << imagePaths.size();

    // 注意：预览图已由 LcscImageService::handleDownloadResponse 在下载时保存到磁盘
    // 这里不需要再次保存，避免重复 I/O

    // 加锁保护共享数据的访问
    {
        QMutexLocker locker(&m_fetchingComponentsMutex);

        // 更新 m_fetchingComponents 中的数据
        if (m_fetchingComponents.contains(componentId)) {
            FetchingComponent& fetchingComponent = m_fetchingComponents[componentId];
            QList<QByteArray> imageDataList;
            imageDataList.resize(3);
            for (const QString& path : imagePaths) {
                const int imageIndex = previewImageIndexFromPath(path);
                if (imageIndex < 0 || imageIndex >= imageDataList.size()) {
                    continue;
                }
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) {
                    imageDataList[imageIndex] = file.readAll();
                    file.close();
                }
            }
            fetchingComponent.data.setPreviewImageData(imageDataList);
            qDebug() << "All image data updated in ComponentData for component:" << componentId
                     << "count:" << imageDataList.size();
        } else {
            qWarning() << "Component" << componentId
                       << "not found in m_fetchingComponents, cannot update all images data";
        }
    }  // 锁在这里释放

    QStringList encodedImages(3);
    for (const QString& path : imagePaths) {
        const int imageIndex = previewImageIndexFromPath(path);
        if (imageIndex < 0 || imageIndex >= encodedImages.size()) {
            continue;
        }

        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            encodedImages[imageIndex] = QString::fromLatin1(file.readAll().toBase64().data());
            file.close();
        }
    }

    // 锁外发送信号（避免信号槽死锁）
    emit previewImagesReady(componentId, encodedImages);
    emit allImagesReady(componentId, imagePaths);
}

void ComponentService::handleComponentInfoFetched(const QString& componentId, const QJsonObject& data) {
    // 解析组件信息
    ComponentData componentData;
    componentData.setLcscId(componentId);

    // 从响应中提取基本信息
    if (data.contains("result")) {
        QJsonObject result = data["result"].toObject();

        if (result.contains("title")) {
            componentData.setName(result["title"].toString());
        }
        if (result.contains("package")) {
            componentData.setPackage(result["package"].toString());
        }
        if (result.contains("manufacturer")) {
            componentData.setManufacturer(result["manufacturer"].toString());
        }
        if (result.contains("datasheet")) {
            componentData.setDatasheet(result["datasheet"].toString());
        }
    }

    emit componentInfoReady(componentId, componentData);
}

void ComponentService::handleCadDataFetched(const QString& componentId, const QJsonObject& data) {
    auto future = QtConcurrent::run([componentId, data]() { return parseCadPayload(componentId, data); });
    auto* watcher = new QFutureWatcher<CadParseResult>(this);
    connect(watcher, &QFutureWatcher<CadParseResult>::finished, this, [this, watcher]() {
        const CadParseResult parsed = watcher->result();
        watcher->deleteLater();

        if (!parsed.success) {
            emit fetchError(parsed.componentId, parsed.errorMessage);
            return;
        }

        {
            QMutexLocker locker(&m_fetchingComponentsMutex);
            auto it = m_fetchingComponents.find(parsed.componentId);
            if (it == m_fetchingComponents.end()) {
                return;
            }
            it->data = parsed.componentData;
            it->hasCadData = true;
        }

        updateComponentCache(parsed.componentId, parsed.componentData);
        emit cadDataReady(parsed.componentId, parsed.componentData);
        ComponentCacheService::instance()->saveComponentMetadata(parsed.componentId, parsed.componentData);
        ComponentCacheService::instance()->saveCadDataJson(
            parsed.componentId, QJsonDocument(parsed.resultData).toJson(QJsonDocument::Compact));

        if (m_parallelContext != nullptr) {
            handleParallelDataCollected(parsed.componentId, parsed.componentData);
        }
    });
    watcher->setFuture(future);
}

void ComponentService::handleFetchError(const QString& errorMessage) {
    qDebug() << "Fetch error:" << errorMessage;

    // 如果在并行模式下，处理并行错误
    if (m_parallelContext != nullptr) {
        handleParallelFetchError(m_currentComponentId, errorMessage);
    }

    // 最后发送信号，防止信号连接的槽函数删除了本对象导致后续访问成员变量崩溃
    emit fetchError(m_currentComponentId, errorMessage);
}

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

    qDebug() << "Fetch error for component:" << componentId << "Error:" << error;

    // 如果在并行模式下，处理并行错误
    if (m_parallelContext != nullptr) {
        handleParallelFetchError(componentId, error);
    }

    emit fetchError(componentId, error);
}

void ComponentService::setOutputPath(const QString& path) {
    m_outputPath = path;
}

QString ComponentService::getOutputPath() const {
    return m_outputPath;
}

void ComponentService::fetchMultipleComponentsData(const QStringList& componentIds, bool fetch3DModel) {
    m_maxConcurrentRequests = ConfigService::instance()->getValidationConcurrentCount();
    if (m_queueManager != nullptr) {
        m_queueManager->setMaxConcurrentRequests(m_maxConcurrentRequests);
    }

    qDebug() << "Fetching data for" << componentIds.size()
             << "components with async queue (max concurrent:" << m_maxConcurrentRequests << ")";

    // 防止重复启动批量处理，避免队列状态混乱
    if (m_parallelContext != nullptr) {
        qWarning() << "Batch processing already in progress, ignoring new request";
        return;
    }

    // 初始化并行获取状态
    m_parallelContext = new ParallelFetchContext(this);
    connect(m_parallelContext, &ParallelFetchContext::allCompleted, this, [this](const QList<ComponentData>& data) {
        emit allComponentsDataCollected(data);
        m_activeRequestCount = 0;
        resetQueueState();
    });
    m_parallelContext->start(componentIds.size());
    m_activeRequestCount = 0;
    m_batchFetch3DModel = fetch3DModel;

    // 使用 ComponentQueueManager 管理队列
    m_queueManager->start(componentIds);
}

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

void ComponentService::handleParallelFetchError(const QString& componentId, const QString& error) {
    qDebug() << "Parallel fetch error for:" << componentId << error;

    Q_UNUSED(error);
    ParallelFetchContext* parallelContext = nullptr;
    {
        QMutexLocker locker(&m_parallelContextMutex);
        parallelContext = m_parallelContext;
    }
    if (parallelContext != nullptr) {
        parallelContext->markFailed(componentId);
    }

    if (m_queueManager != nullptr) {
        m_queueManager->requestCompleted(componentId);
    }
}

bool ComponentService::validateComponentId(const QString& componentId) const {
    return BomParser::validateId(componentId);
}

QStringList ComponentService::extractComponentIdFromText(const QString& text) const {
    // 依然保留简单的提取逻辑，或者可以进一步整合进 BomParser
    QStringList extractedIds;
    QRegularExpression re("[Cc]\\d{4,}");
    QRegularExpressionMatchIterator it = re.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString id = match.captured().toUpper();
        if (!BomParser::getExcludedIds().contains(id) && !extractedIds.contains(id)) {
            extractedIds.append(id);
        }
    }
    return extractedIds;
}

QStringList ComponentService::parseBomFile(const QString& filePath) {
    BomParser parser;
    return parser.parse(filePath);
}

ComponentData ComponentService::getComponentData(const QString& componentId) const {
    QMutexLocker locker(&m_componentCacheMutex);
    return m_componentCache.value(componentId, ComponentData());
}

void ComponentService::updateComponentCache(const QString& componentId, const ComponentData& data) {
    QMutexLocker locker(&m_componentCacheMutex);
    m_componentCache[componentId] = data;
    qDebug() << "ComponentService: Updated cache for" << componentId;
}

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

void ComponentService::cancelAllPreviewImageFetches() {
    qDebug() << "ComponentService: Cancelling all preview image fetches";
    if (m_imageService) {
        m_imageService->cancelAll();
    }
}

void ComponentService::cancelAllPendingRequests() {
    qDebug() << "ComponentService: Cancelling all pending component data requests";

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

void ComponentService::handleFetchErrorForComponent(const QString& componentId, const QString& error) {
    qWarning() << "Fetch error for component" << componentId << ":" << error;
    if (m_parallelContext != nullptr) {
        handleParallelFetchError(componentId, error);
    }
    emit fetchError(componentId, error);
}

bool ComponentService::isPDF(const QByteArray& data) const {
    // PDF 文件以 %PDF- 开头
    if (data.size() < 5) {
        return false;
    }
    return data.startsWith("%PDF-");
}

// 异步队列管理方法实现
// 使用 ComponentQueueManager 管理队列

void ComponentService::handleQueueTimeout() {
    qWarning() << "Queue timeout reached";

    if (m_parallelContext != nullptr) {
        int completedCount = m_parallelContext->completedCount();
        int totalCount = m_parallelContext->totalCount();
        qWarning() << "Queue timeout - Completed:" << completedCount << "Total:" << totalCount;

        if (completedCount > 0) {
            QList<ComponentData> allData = m_parallelContext->collectedData();
            emit allComponentsDataCollected(allData);
        }

        resetQueueState();
    }
}

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
