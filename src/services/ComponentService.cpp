#include "ComponentService.h"

#include "BomParser.h"
#include "ComponentQueueManager.h"
#include "core/easyeda/EasyedaApi.h"
#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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

ComponentService::ComponentService(QObject* parent)
    : QObject(parent)
    , m_api(nullptr)
    , m_importer(nullptr)
    , m_networkManager(nullptr)
    , m_currentComponentId()
    , m_hasDownloadedWrl(false)
    , m_parallelContext(nullptr)
    , m_imageService(nullptr)
    , m_activeRequestCount(0)
    , m_maxConcurrentRequests(10)
    , m_queueManager(nullptr) {
    try {
        m_api = new EasyedaApi();
        if (m_api) {
            m_api->setParent(this);
        }
        m_importer = new EasyedaImporter(this);
        m_networkManager = new QNetworkAccessManager(this);
        m_imageService = new LcscImageService(this);

        m_queueManager = new ComponentQueueManager(m_maxConcurrentRequests, this);
        connect(m_queueManager, &ComponentQueueManager::requestReady, this, [this](const QString& componentId) {
            fetchComponentDataInternal(componentId, true);
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
    , m_importer(nullptr)
    , m_networkManager(nullptr)
    , m_currentComponentId()
    , m_hasDownloadedWrl(false)
    , m_parallelContext(nullptr)
    , m_imageService(nullptr)
    , m_activeRequestCount(0)
    , m_maxConcurrentRequests(10)
    , m_queueManager(nullptr) {
    if (m_api && !m_api->parent()) {
        m_api->setParent(this);
    }

    m_importer = new EasyedaImporter(this);
    m_networkManager = new QNetworkAccessManager(this);
    m_imageService = new LcscImageService(this);

    m_queueManager = new ComponentQueueManager(m_maxConcurrentRequests, this);
    connect(m_queueManager, &ComponentQueueManager::requestReady, this, [this](const QString& componentId) {
        fetchComponentDataInternal(componentId, true);
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
        connect(m_api, &EasyedaApi::model3DFetched, this, &ComponentService::handleModel3DFetched);
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

    // 先检查缓存是否存在（快速检查，不阻塞）
    ComponentCacheService* cache = ComponentCacheService::instance();
    if (cache->hasCache(normalizedId)) {
        // 先添加到 m_fetchingComponents（防止重复调度）
        {
            QMutexLocker locker(&m_fetchingComponentsMutex);
            if (m_fetchingComponents.contains(normalizedId)) {
                qDebug() << "ComponentService: Cache hit but already fetching for" << normalizedId << ", skipping";
                return;
            }
            // 创建占位条目，防止重复调度
            FetchingComponent& fc = m_fetchingComponents[normalizedId];
            fc.componentId = normalizedId;
            fc.fetch3DModel = fetch3DModel;
            fc.hasComponentInfo = false;
            fc.hasCadData = false;
            fc.hasObjData = false;
            fc.hasStepData = false;
        }
        qDebug() << "ComponentService: Cache hit for" << normalizedId << ", scheduling async load from disk cache";
        // 缓存加载是 I/O 密集型操作，移到后台线程执行避免阻塞 UI
        QTimer::singleShot(0, this, [this, normalizedId, fetch3DModel, cache]() {
            loadComponentDataFromCacheAsync(normalizedId, fetch3DModel, cache);
        });
        return;
    }

    // 缓存不存在，从网络获取
    qDebug() << "ComponentService: Cache miss for" << normalizedId << ", fetching from network";

    // 加锁保护共享数据的访问
    QMutexLocker locker(&m_fetchingComponentsMutex);

    // 获取或创建 FetchingComponent 条目
    FetchingComponent& fetchingComponent = m_fetchingComponents[normalizedId];
    fetchingComponent.componentId = normalizedId;
    fetchingComponent.fetch3DModel = fetch3DModel;

    // 初始化/重置状态标志
    fetchingComponent.hasComponentInfo = false;
    fetchingComponent.hasCadData = false;
    fetchingComponent.hasObjData = false;
    fetchingComponent.hasStepData = false;
    fetchingComponent.errorMessage.clear();

    // 在锁保护外执行网络请求，避免长时间持锁
    locker.unlock();

    // 首先获取 CAD 数据（包含符号和封装信息）
    // CAD 数据是后续流程的基础
    m_api->fetchCadData(normalizedId);
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
                fetchingComponent.componentId = normalizedId;
                fetchingComponent.fetch3DModel = fetch3DModel;
                fetchingComponent.hasComponentInfo = false;
                fetchingComponent.hasCadData = false;
                fetchingComponent.hasObjData = false;
                fetchingComponent.hasStepData = false;
                fetchingComponent.errorMessage.clear();
            }
            m_api->fetchCadData(normalizedId);
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
        emit cadDataReady(normalizedId, *result.cachedData);

        // 如果在并行模式，处理并行数据收集
        if (m_parallelContext != nullptr) {
            handleParallelDataCollected(normalizedId, *result.cachedData);
        }

        // 从缓存加载预览图数据（批量发送，避免频繁 UI 更新）
        // 预览图数据已经是 PNG 编码的字节，直接编码为 base64 字符串
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
    qDebug() << "[CompService] handleImageReady START - component:" << componentId << "index:" << imageIndex
             << "data size:" << imageData.size() << "bytes";

    // 从内存数据加载图片
    qDebug() << "[CompService] Creating QImage from data...";
    QImage image = QImage::fromData(imageData);
    if (!image.isNull()) {
        qDebug() << "[CompService] QImage created, emitting previewImageReady...";
        // 发送预览图就绪信号（用于 UI 显示）
        emit previewImageReady(componentId, image, imageIndex);
        qDebug() << "[CompService] previewImageReady emitted";

        qDebug() << "[CompService] Emitting previewImageDataReady...";
        // 发送预览图数据就绪信号（用于导出）
        emit previewImageDataReady(componentId, imageData, imageIndex);
        qDebug() << "[CompService] previewImageDataReady emitted";

        // 注意：预览图已由 LcscImageService::handleDownloadResponse 在发射此信号前保存到磁盘
        // 这里不需要再次保存，避免死锁（因为 savePreviewImage 会在持有 ComponentCacheService 锁的情况下被调用）

        // 加锁保护共享数据的访问
        qDebug() << "[CompService] Acquiring mutex...";
        QMutexLocker locker(&m_fetchingComponentsMutex);
        qDebug() << "[CompService] Mutex acquired, updating ComponentData...";

        // 保存图片数据到 ComponentData（内存），使用索引避免重复
        if (m_fetchingComponents.contains(componentId)) {
            FetchingComponent& fetchingComponent = m_fetchingComponents[componentId];
            int currentCount = fetchingComponent.data.previewImageData().size();
            fetchingComponent.data.addPreviewImageData(imageData, imageIndex);
            int newCount = fetchingComponent.data.previewImageData().size();

            qDebug() << "Preview image data saved to ComponentData";
            qDebug() << "  Component ID:" << componentId;
            qDebug() << "  Image index:" << imageIndex;
            qDebug() << "  Image size:" << imageData.size() << "bytes";
            qDebug() << "  Data count before:" << currentCount << "after:" << newCount;

            // 打印当前所有图片数据的状态
            auto allImageData = fetchingComponent.data.previewImageData();
            qDebug() << "  Current image data list size:" << allImageData.size();
            for (int i = 0; i < allImageData.size(); ++i) {
                qDebug() << "    Image" << i << "size:" << allImageData[i].size() << "bytes"
                         << (allImageData[i].isEmpty() ? "(EMPTY)" : "(VALID)");
            }
        }
        qDebug() << "[CompService] handleImageReady END";
    } else {
        qDebug() << "Failed to load image from data for component:" << componentId << "index:" << imageIndex
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
    }
}

void ComponentService::handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData) {
    qDebug() << "Datasheet downloaded for component:" << componentId << "size:" << datasheetData.size() << "bytes";

    // 准备数据用于锁外处理
    QString format;
    bool hasValidUpdate = false;

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

            hasValidUpdate = true;
        } else {
            qWarning() << "Component" << componentId
                       << "not found in m_fetchingComponents, cannot update datasheet data";
        }
    }  // 锁在这里释放

    // 锁外发送信号（避免信号槽死锁）
    if (hasValidUpdate) {
        // 发送数据手册就绪信号
        emit datasheetReady(componentId, datasheetData);
    }
}

void ComponentService::handlePreviewImageError(const QString& componentId, const QString& error) {
    qWarning() << "Preview image fetch error for component:" << componentId << "error:" << error;

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
            // 从路径加载图片数据
            QList<QByteArray> imageDataList;
            for (const QString& path : imagePaths) {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) {
                    imageDataList.append(file.readAll());
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

    // 锁外发送信号（避免信号槽死锁）
    emit allImagesReady(componentId, imagePaths);
}

void ComponentService::handleComponentInfoFetched(const QJsonObject& data) {
    qDebug() << "Component info fetched:" << data.keys();

    // 解析组件信息
    ComponentData componentData;
    componentData.setLcscId(m_currentComponentId);

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

    emit componentInfoReady(m_currentComponentId, componentData);
}

void ComponentService::handleCadDataFetched(const QJsonObject& data) {
    // 从数据中提取 LCSC ID
    QString lcscId;
    if (data.contains("lcscId")) {
        lcscId = data["lcscId"].toString();
    } else {
        // 如果没有 lcscId 字段，尝试从 lcsc.szlcsc.number 中提取
        if (data.contains("lcsc")) {
            QJsonObject lcsc = data["lcsc"].toObject();
            if (lcsc.contains("number")) {
                lcscId = lcsc["number"].toString();
            }
        }
    }

    if (lcscId.isEmpty()) {
        qWarning() << "Cannot extract LCSC ID from CAD data";
        return;
    }

    qDebug() << "CAD data fetched for:" << lcscId;

    // 临时保存当前的组件ID，使用互斥锁保护
    QString savedComponentId;
    {
        QMutexLocker locker(&m_currentIdMutex);
        savedComponentId = m_currentComponentId;
        m_currentComponentId = lcscId;
    }

    // 加锁保护共享数据的访问
    QMutexLocker locker(&m_fetchingComponentsMutex);

    // 获取 fetch3DModel 配置
    bool need3DModel = true;  // 默认值
    if (m_fetchingComponents.contains(lcscId)) {
        need3DModel = m_fetchingComponents[lcscId].fetch3DModel;
    } else {
        // 如果找不到配置（可能是旧的串行逻辑遗留，或者 m_fetchingComponents 被清除），
        // 尝试回退到 m_fetch3DModel，但这在并行下不可靠。
        // 不过由于我们在 fetchComponentDataInternal 中强制设置了 map，这里应该能找到。
        qWarning() << "Fetching component info not found in map for:" << lcscId << "Using default fetch3DModel=true";
    }

    // 提取 result 数据
    QJsonObject resultData;
    if (data.contains("result")) {
        resultData = data["result"].toObject();
    } else {
        // 直接使用 data
        resultData = data;
    }

    if (resultData.isEmpty()) {
        emit fetchError(lcscId, "Empty CAD data");
        m_currentComponentId = savedComponentId;
        return;
    }

    // 调试：打印resultData的结构
    qDebug() << "=== CAD Data Structure ===";
    qDebug() << "Top-level keys:" << resultData.keys();
    if (resultData.contains("dataStr")) {
        QJsonObject dataStr = resultData["dataStr"].toObject();
        qDebug() << "dataStr keys:" << dataStr.keys();
        if (dataStr.contains("shape")) {
            QJsonArray shapes = dataStr["shape"].toArray();
            qDebug() << "dataStr.shape size:" << shapes.size();
            if (!shapes.isEmpty()) {
                qDebug() << "First shape:" << shapes[0].toString().left(100);
            }
        } else {
            qDebug() << "WARNING: dataStr does NOT contain 'shape' field!";
        }
    } else {
        qDebug() << "WARNING: resultData does NOT contain 'dataStr' field!";
    }
    qDebug() << "===========================";

    // 创建 ComponentData 对象
    ComponentData componentData;
    componentData.setLcscId(lcscId);

    // 提取基本信息
    if (resultData.contains("title")) {
        componentData.setName(resultData["title"].toString());
    }
    if (resultData.contains("package")) {
        componentData.setPackage(resultData["package"].toString());
    }
    if (resultData.contains("manufacturer")) {
        componentData.setManufacturer(resultData["manufacturer"].toString());
    }
    if (resultData.contains("datasheet")) {
        componentData.setDatasheet(resultData["datasheet"].toString());
    }

    // 导入符号数据
    QSharedPointer<SymbolData> symbolData = m_importer->importSymbolData(resultData);
    if (symbolData) {
        componentData.setSymbolData(symbolData);
        qDebug() << "Symbol imported successfully - Name:" << symbolData->info().name;

        // 从符号数据中提取预览图（作为备用，稍后会被 LCSC API 的数据覆盖）
        if (!symbolData->info().thumb.isEmpty()) {
            QString thumbUrl = symbolData->info().thumb;
            // 检查是否是相对路径，如果是则拼接完整的 URL
            if (thumbUrl.startsWith("/")) {
                thumbUrl = "https://image.lceda.cn" + thumbUrl;
                qDebug() << "Preview image is relative path, constructed full URL:" << thumbUrl;
            }
            componentData.setPreviewImages(QStringList() << thumbUrl);
            qDebug() << "Preview image extracted from symbolData (EasyEDA, will be overridden by LCSC if available):"
                     << thumbUrl;
        } else {
            qDebug() << "No preview image found in symbolData";
        }

        // 从符号数据中提取手册（作为备用，稍后会被 LCSC API 的数据覆盖）
        if (!symbolData->info().datasheet.isEmpty() && componentData.datasheet().isEmpty()) {
            QString datasheetUrl = symbolData->info().datasheet;
            // 检查是否是相对路径，如果是则拼接完整的 URL
            if (datasheetUrl.startsWith("/")) {
                datasheetUrl = "https://image.lceda.cn" + datasheetUrl;
                qDebug() << "Datasheet is relative path, constructed full URL:" << datasheetUrl;
            }
            componentData.setDatasheet(datasheetUrl);
            qDebug() << "Datasheet extracted from symbolData (EasyEDA, will be overridden by LCSC if available):"
                     << datasheetUrl;
        }
    } else {
        // 使用互斥锁保护读取 m_currentComponentId
        QString currentId;
        {
            QMutexLocker locker(&m_currentIdMutex);
            currentId = m_currentComponentId;
        }
        qWarning() << "Failed to import symbol data for:" << currentId;
        emit fetchError(lcscId, "Failed to parse Symbol data from EasyEDA JSON");

        // 恢复组件 ID
        {
            QMutexLocker locker(&m_currentIdMutex);
            m_currentComponentId = savedComponentId;
        }
        return;
    }

    // 导入封装数据
    QSharedPointer<FootprintData> footprintData = m_importer->importFootprintData(resultData);
    if (footprintData) {
        componentData.setFootprintData(footprintData);
        qDebug() << "Footprint imported successfully - Name:" << footprintData->info().name;
    } else {
        qWarning() << "Failed to import footprint data for:" << m_currentComponentId;
        emit fetchError(lcscId, "Failed to parse Footprint data from EasyEDA JSON (Library might be corrupted)");
        m_currentComponentId = savedComponentId;
        return;
    }

    // 检查是否需要获取3D 模型
    // 使用本地变量 need3DModel 而不是成员变量 m_fetch3DModel
    if (need3DModel && footprintData) {
        // 检查封装数据中是否包含 3D 模型 UUID
        QString modelUuid = footprintData->model3D().uuid();

        // 如果封装数据中没有UUID，尝试从 head.uuid_3d 字段中提取
        if (modelUuid.isEmpty() && resultData.contains("head")) {
            QJsonObject head = resultData["head"].toObject();
            if (head.contains("uuid_3d")) {
                modelUuid = head["uuid_3d"].toString();
            }
        }

        if (!modelUuid.isEmpty()) {
            qDebug() << "Fetching 3D model with UUID:" << modelUuid;

            // 创建 Model3DData 对象
            QSharedPointer<Model3DData> model3DData(new Model3DData());
            model3DData->setUuid(modelUuid);

            // 如果封装数据中有 3D 模型信息，复制平移和旋转
            if (!footprintData->model3D().uuid().isEmpty()) {
                model3DData->setName(footprintData->model3D().name());
                model3DData->setTranslation(footprintData->model3D().translation());
                model3DData->setRotation(footprintData->model3D().rotation());
            }

            componentData.setModel3DData(model3DData);

            // 在并行模式下，使用 m_fetchingComponents 存储待处理的组件数据
            if (m_parallelContext != nullptr) {
                // 更新已有的 entry
                FetchingComponent& fetchingComponent = m_fetchingComponents[m_currentComponentId];
                fetchingComponent.data = componentData;
                fetchingComponent.hasComponentInfo = true;
                fetchingComponent.hasCadData = true;
                fetchingComponent.hasObjData = false;
                fetchingComponent.hasStepData = false;
                // fetch3DModel 已经在 internal 调用时设置了
            } else {
                // 串行模式下的处理
                m_pendingComponentData = componentData;
                m_pendingModelUuid = modelUuid;
                m_hasDownloadedWrl = false;
            }

            // 获取 WRL 格式的3D 模型
            m_api->fetch3DModelObj(modelUuid);

            // 保存 CAD 数据到缓存（即使需要 3D 模型也要保存，因为 symbol/footprint 数据已经完整）
            // 注意：此时 componentData 已经包含了 symbolData 和 footprintData
            QJsonDocument cadDoc(resultData);
            ComponentCacheService::instance()->saveCadDataJson(m_currentComponentId,
                                                               cadDoc.toJson(QJsonDocument::Compact));
            ComponentCacheService::instance()->saveComponentMetadata(m_currentComponentId, componentData);

            // 立即发送 cadDataReady 信号，让 ComponentListViewModel 可以立即使用 symbol/footprint 数据
            // 3D 模型下载完成后会在 handleModel3DFetched 中再次发送信号更新
            qDebug() << "ComponentService: Emitting cadDataReady (from network) for" << m_currentComponentId;
            emit cadDataReady(m_currentComponentId, componentData);

            return;  // 等待 3D 模型数据
        } else {
            qDebug() << "No 3D model UUID found for:" << m_currentComponentId;
        }
    } else {
        // 即使不需要3D模型，也要提取UUID保存到缓存，以便后续导出时使用
        // 这样可以避免导出时重新请求CAD数据
        QString modelUuid;
        if (footprintData) {
            modelUuid = footprintData->model3D().uuid();
        }
        if (modelUuid.isEmpty() && resultData.contains("head")) {
            QJsonObject head = resultData["head"].toObject();
            if (head.contains("uuid_3d")) {
                modelUuid = head["uuid_3d"].toString();
            }
        }

        if (!modelUuid.isEmpty()) {
            qDebug() << "Extracted 3D model UUID for caching:" << modelUuid;
            QSharedPointer<Model3DData> model3DData(new Model3DData());
            model3DData->setUuid(modelUuid);
            // 如果封装数据中有 3D 模型信息，复制平移和旋转
            if (footprintData && !footprintData->model3D().uuid().isEmpty()) {
                model3DData->setName(footprintData->model3D().name());
                model3DData->setTranslation(footprintData->model3D().translation());
                model3DData->setRotation(footprintData->model3D().rotation());
            }
            componentData.setModel3DData(model3DData);
        }
    }

    // 调用 LCSC API 获取数据手册和预览图 URL（异步）
    // 注意：这个调用是异步的，handleLcscDataReady 会更新 ComponentData
    // 我们需要将数据保存到 m_fetchingComponents 中，以便 handleLcscDataReady 可以更新
    m_fetchingComponents[m_currentComponentId].data = componentData;

    // 先发送完成信号，确保 ComponentListViewModel 获取到完整的 ComponentData
    // 预览图获取现在由 ComponentListViewModel 两阶段控制：验证完成后统一获取
    QString currentId;
    {
        QMutexLocker locker(&m_currentIdMutex);
        currentId = m_currentComponentId;
    }
    emit cadDataReady(currentId, componentData);

    // 保存到磁盘缓存
    ComponentCacheService::instance()->saveComponentMetadata(currentId, componentData);

    // 保存完整的CAD数据JSON（包含符号和封装数据，用于后续导出）
    QJsonDocument cadDoc(resultData);
    ComponentCacheService::instance()->saveCadDataJson(currentId, cadDoc.toJson(QJsonDocument::Compact));

    // 如果在并行模式下，处理并行数据收集
    if (m_parallelContext != nullptr) {
        handleParallelDataCollected(currentId, componentData);
    }

    // 恢复组件 ID，使用互斥锁保护
    {
        QMutexLocker locker(&m_currentIdMutex);
        m_currentComponentId = savedComponentId;
    }
}

void ComponentService::handleModel3DFetched(const QString& uuid, const QByteArray& data) {
    qDebug() << "3D model data fetched for UUID:" << uuid << "Size:" << data.size();

    // 在并行模式下，查找对应的组件
    if (m_parallelContext != nullptr) {
        QString componentId;
        bool hasObjData = false;
        bool fetch3DModel = false;
        ComponentData componentDataCopy;

        // 加锁保护共享数据的访问，迭代器遍历必须加锁
        {
            QMutexLocker locker(&m_fetchingComponentsMutex);

            // 在并行模式下，查找对应UUID的组件
            for (auto it = m_fetchingComponents.begin(); it != m_fetchingComponents.end(); ++it) {
                if (it.value().data.model3DData() && it.value().data.model3DData()->uuid() == uuid) {
                    componentId = it.key();
                    FetchingComponent& fetchingComponent = it.value();

                    if (!fetchingComponent.hasObjData) {
                        // 这是 WRL 格式的3D 模型
                        fetchingComponent.data.model3DData()->setRawObj(QString::fromUtf8(data));
                        fetchingComponent.hasObjData = true;
                        qDebug() << "WRL data saved for:" << uuid << "Size:" << data.size();

                        hasObjData = false;
                    } else {
                        // 这是 STEP 格式的3D 模型
                        fetchingComponent.data.model3DData()->setStep(data);
                        fetchingComponent.hasStepData = true;
                        qDebug() << "STEP data saved for:" << uuid << "Size:" << data.size();

                        hasObjData = true;
                        fetch3DModel = fetchingComponent.fetch3DModel;
                        componentDataCopy = fetchingComponent.data;
                    }
                }
            }
        }
        // 锁在这里释放

        // 在锁外执行网络请求，避免死锁
        if (!hasObjData && !componentId.isEmpty()) {
            // 继续下载 STEP 格式的3D 模型
            qDebug() << "Fetching STEP model with UUID:" << uuid;
            m_api->fetch3DModelStep(uuid);
        } else if (hasObjData && !componentId.isEmpty()) {
            // 调用 LCSC API 获取数据手册和预览图 URL（异步）
            // 注意：这个调用是异步的，handleLcscDataReady 会更新 ComponentData
            {
                QMutexLocker locker(&m_fetchingComponentsMutex);
                m_fetchingComponents[componentId].data = componentDataCopy;
            }

            // 先发送完成信号，确保 ComponentListViewModel 获取到完整的 ComponentData
            // 图片下载是异步的，会在后台继续进行
            emit cadDataReady(componentId, componentDataCopy);

            // 处理并行数据收集
            handleParallelDataCollected(componentId, componentDataCopy);

            // 预览图获取现在由 ComponentListViewModel 两阶段控制：验证完成后统一获取

            // 从待处理列表中移除
            {
                QMutexLocker locker(&m_fetchingComponentsMutex);
                m_fetchingComponents.remove(componentId);
            }
        }
    } else {
        // 串行模式下的处理
        if (m_pendingComponentData.model3DData() && m_pendingComponentData.model3DData()->uuid() == uuid) {
            if (!m_hasDownloadedWrl) {
                // 这是 WRL 格式的3D 模型
                m_pendingComponentData.model3DData()->setRawObj(QString::fromUtf8(data));
                qDebug() << "WRL data saved for:" << uuid << "Size:" << data.size();

                // 标记已经下载了WRL 格式
                m_hasDownloadedWrl = true;

                // 继续下载 STEP 格式的3D 模型
                qDebug() << "Fetching STEP model with UUID:" << uuid;
                m_api->fetch3DModelStep(uuid);
            } else {
                // 这是 STEP 格式的3D 模型
                m_pendingComponentData.model3DData()->setStep(data);
                qDebug() << "STEP data saved for:" << uuid << "Size:" << data.size();

                // 调用 LCSC API 获取数据手册和预览图 URL（异步）
                // 预览图获取现在由 ComponentListViewModel 两阶段控制：验证完成后统一获取
                m_fetchingComponents[m_currentComponentId].data = m_pendingComponentData;

                // 发送完成信号
                qDebug() << "ComponentService: Emitting cadDataReady (pending complete) for" << m_currentComponentId;
                emit cadDataReady(m_currentComponentId, m_pendingComponentData);

                // 清空待处理数据
                m_pendingComponentData = ComponentData();
                m_pendingModelUuid.clear();
                m_hasDownloadedWrl = false;
            }
        } else {
            qWarning() << "Received 3D model data for unexpected UUID:" << uuid;
        }
    }
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
    });
    m_parallelContext->start(componentIds.size());
    m_activeRequestCount = 0;

    // 使用 ComponentQueueManager 管理队列
    m_queueManager->start(componentIds);
}

void ComponentService::handleParallelDataCollected(const QString& componentId, const ComponentData& data) {
    qDebug() << "Parallel data collected for:" << componentId;

    QMutexLocker locker(&m_parallelContextMutex);
    if (m_parallelContext != nullptr) {
        m_parallelContext->markCompleted(componentId, data);
    }
    locker.unlock();

    if (m_queueManager != nullptr) {
        m_queueManager->requestCompleted(componentId);
    }
}

void ComponentService::handleParallelFetchError(const QString& componentId, const QString& error) {
    qDebug() << "Parallel fetch error for:" << componentId << error;

    Q_UNUSED(error);
    QMutexLocker locker(&m_parallelContextMutex);
    if (m_parallelContext != nullptr) {
        m_parallelContext->markFailed(componentId);
    }
    locker.unlock();

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
    return m_componentCache.value(componentId, ComponentData());
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

void ComponentService::completeComponentData(const QString& componentId) {
    QMutexLocker locker(&m_fetchingComponentsMutex);
    if (m_fetchingComponents.contains(componentId)) {
        FetchingComponent& fc = m_fetchingComponents[componentId];
        // 判定标准：CAD 数据必须有，如果需要 3D 则 OBJ 和 STEP 也必须有
        bool isComplete = fc.hasCadData && (!fc.fetch3DModel || (fc.hasObjData && fc.hasStepData));

        if (isComplete) {
            emit cadDataReady(componentId, fc.data);
            if (m_parallelContext != nullptr) {
                handleParallelDataCollected(componentId, fc.data);
            }
            m_fetchingComponents.remove(componentId);
        }
    }
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
