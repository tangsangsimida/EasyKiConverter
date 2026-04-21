#include "ComponentCacheService.h"

#include "CacheHealthManager.h"
#include "CachePruner.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"
#include "services/ComponentService.h"
#include "utils/logging/LogMacros.h"

#include <QAtomicInt>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

std::unique_ptr<ComponentCacheService> ComponentCacheService::s_instance;

ComponentCacheService::ComponentCacheService(QObject* parent)
    : QObject(parent), m_memoryCacheLimitMB(50), m_memoryCacheSize(0) {
    // 默认缓存目录：{用户数据目录}/easykiconverter/cache
    QString defaultCacheDir =
        QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/cache");
    setCacheDir(defaultCacheDir);

    // 初始化L1内存缓存，设置大小限制（50MB = 50 * 1024 * 1024 bytes）
    // QCache 的 cost 是存储的字节数
    m_memoryCache.setMaxCost(50 * 1024 * 1024);
}

ComponentCacheService::~ComponentCacheService() = default;

ComponentCacheService* ComponentCacheService::instance() {
    if (!s_instance) {
        s_instance = std::unique_ptr<ComponentCacheService>(new ComponentCacheService());
    }
    return s_instance.get();
}

void ComponentCacheService::setCacheDir(const QString& cacheDir) {
    {
        QMutexLocker locker(&m_mutex);
        m_cacheDir = cacheDir;

        // 确保缓存目录存在
        QDir dir;
        if (!dir.exists(m_cacheDir)) {
            dir.mkpath(m_cacheDir);
        }

        // 确保3D模型缓存目录存在
        QString model3dDir = m_cacheDir + "/model3d";
        if (!dir.exists(model3dDir)) {
            dir.mkpath(model3dDir);
        }
    }

    selfHealCache();
    LOG_DEBUG(LogModule::Core, "Cache directory set to: {}", m_cacheDir);
}

QString ComponentCacheService::cacheDir() const {
    QMutexLocker locker(&m_mutex);
    return m_cacheDir;
}

QString ComponentCacheService::componentCacheDir(const QString& lcscId) const {
    // 注意：这里不再加锁，因为只是构建路径字符串
    // m_cacheDir 在初始化时设置，之后只读，不需要互斥保护
    return QDir::cleanPath(m_cacheDir + "/" + lcscId);
}

QString ComponentCacheService::ensureComponentDir(const QString& lcscId) const {
    // 注意：不再获取锁，因为：
    // 1. componentCacheDir 只是构建路径字符串（只读操作）
    // 2. dir.mkpath 是线程安全的
    // 3. savePreviewImage 已经在持有 m_mutex 的情况下调用此函数
    QString dirPath = componentCacheDir(lcscId);
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(dirPath);
    }
    return dirPath;
}

QString ComponentCacheService::ensureModel3DCacheDir() const {
    // 注意：不再获取锁，因为：
    // 1. dir.mkpath 是线程安全的
    // 2. saveModel3D 已经在持有 m_mutex 的情况下调用此函数
    QString dirPath = m_cacheDir + "/model3d";
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(dirPath);
    }
    return dirPath;
}

QString ComponentCacheService::makeMemoryKey(const QString& lcscId, const QString& type) const {
    return lcscId + ":" + type;
}

bool ComponentCacheService::hasCache(const QString& lcscId) const {
    // 先检查文件是否存在（不使用锁，避免阻塞主线程的磁盘I/O）
    QString metaPath = metadataPath(lcscId);
    if (!QFileInfo::exists(metaPath)) {
        return false;
    }
    // 文件存在时，验证缓存完整性
    return isCacheValid(lcscId);
}

bool ComponentCacheService::isCacheValid(const QString& lcscId) const {
    const QJsonObject metadata = loadMetadata(lcscId);
    if (metadata.isEmpty()) {
        return false;
    }

    const bool hasCadJson = QFileInfo::exists(componentCacheDir(lcscId) + "/cad_data.json");
    const bool hasBasicIdentity =
        !metadata.value("lcscId").toString().isEmpty() || !metadata.value("name").toString().isEmpty();

    return hasCadJson || hasBasicIdentity;
}

bool ComponentCacheService::hasInMemoryCache(const QString& lcscId) const {
    QMutexLocker locker(&m_mutex);
    QString key = makeMemoryKey(lcscId, "metadata");
    return m_memoryCache.contains(key);
}

// ==================== L1 内存缓存操作 ====================

QJsonObject ComponentCacheService::loadMetadataFromMemory(const QString& lcscId) const {
    QMutexLocker locker(&m_mutex);
    QString key = makeMemoryKey(lcscId, "metadata");
    QByteArray* data = m_memoryCache.object(key);
    if (!data) {
        return QJsonObject();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(*data, &error);
    if (error.error != QJsonParseError::NoError) {
        return QJsonObject();
    }

    return doc.object();
}

void ComponentCacheService::saveMetadataToMemory(const QString& lcscId, const QJsonObject& metadata) {
    qint64 sizeAfterUpdate = 0;
    {
        QMutexLocker locker(&m_mutex);

        QString key = makeMemoryKey(lcscId, "metadata");
        QJsonDocument doc(metadata);
        QByteArray* data = new QByteArray(doc.toJson(QJsonDocument::Compact));

        // 直接插入即可，QCache 会自动处理相同 key 的旧数据的删除
        m_memoryCache.insert(key, data, data->size());
        sizeAfterUpdate = m_memoryCache.totalCost();
    }
    // 锁外发送信号
    emit memoryCacheSizeChanged(sizeAfterUpdate);
}

QByteArray ComponentCacheService::loadSymbolDataFromMemory(const QString& lcscId) const {
    QString key = makeMemoryKey(lcscId, "symbol");
    QByteArray* data = m_memoryCache.object(key);
    if (data) {
        return *data;
    }
    return QByteArray();
}

void ComponentCacheService::saveSymbolDataToMemory(const QString& lcscId, const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }

    qint64 sizeAfterUpdate = 0;
    {
        QMutexLocker locker(&m_mutex);
        QString key = makeMemoryKey(lcscId, "symbol");

        // 直接插入即可，QCache 会自动处理相同 key 的旧数据的删除
        QByteArray* newData = new QByteArray(data);
        m_memoryCache.insert(key, newData, newData->size());
        sizeAfterUpdate = m_memoryCache.totalCost();
    }
    // 锁外发送信号
    emit memoryCacheSizeChanged(sizeAfterUpdate);
}

QByteArray ComponentCacheService::loadFootprintDataFromMemory(const QString& lcscId) const {
    QString key = makeMemoryKey(lcscId, "footprint");
    QByteArray* data = m_memoryCache.object(key);
    if (data) {
        return *data;
    }
    return QByteArray();
}

void ComponentCacheService::saveFootprintDataToMemory(const QString& lcscId, const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }

    qint64 sizeAfterUpdate = 0;
    {
        QMutexLocker locker(&m_mutex);
        QString key = makeMemoryKey(lcscId, "footprint");

        // 直接插入即可，QCache 会自动处理相同 key 的旧数据的删除
        QByteArray* newData = new QByteArray(data);
        m_memoryCache.insert(key, newData, newData->size());
        sizeAfterUpdate = m_memoryCache.totalCost();
    }
    // 锁外发送信号
    emit memoryCacheSizeChanged(sizeAfterUpdate);
}

// ==================== L2 磁盘缓存操作 ====================

QSharedPointer<ComponentData> ComponentCacheService::loadComponentData(const QString& lcscId) const {
    // 先检查缓存是否存在（不使用锁，因为只是检查文件是否存在）
    QString metaPath = metadataPath(lcscId);
    if (!QFileInfo::exists(metaPath)) {
        return nullptr;
    }

    // 再加载数据（使用锁保护）
    QMutexLocker locker(&m_mutex);

    QJsonObject metadata = loadMetadata(lcscId);
    if (metadata.isEmpty()) {
        return nullptr;
    }

    auto componentData = QSharedPointer<ComponentData>::create();

    // 基本信息
    componentData->setLcscId(lcscId);
    componentData->setName(metadata.value("name").toString());
    componentData->setPrefix(metadata.value("prefix").toString());
    componentData->setPackage(metadata.value("package").toString());
    componentData->setManufacturer(metadata.value("manufacturer").toString());
    componentData->setManufacturerPart(metadata.value("manufacturerPart").toString());
    componentData->setDatasheet(metadata.value("datasheet").toString());
    componentData->setDatasheetFormat(metadata.value("datasheetFormat").toString());

    // 预览图URL列表
    QJsonArray previewUrls = metadata.value("previewImages").toArray();
    QStringList urlList;
    for (const QJsonValue& val : previewUrls) {
        const QString normalizedUrl = UrlUtils::normalizePreviewImageUrl(val.toString());
        if (!normalizedUrl.isEmpty()) {
            urlList.append(normalizedUrl);
        }
    }
    componentData->setPreviewImages(urlList);

    // 注意：预览图数据不再加载到 ComponentData，只保留 URL
    // 预览图文件通过 loadPreviewImage 直接读取

    // 3D模型UUID
    if (metadata.contains("model3duuid")) {
        auto model3DData = QSharedPointer<Model3DData>::create();
        model3DData->setUuid(metadata.value("model3duuid").toString());
        model3DData->setName(metadata.value("model3dName").toString());
        if (metadata.contains("model3dTranslation") && metadata.value("model3dTranslation").isObject()) {
            Model3DBase translation;
            translation.fromJson(metadata.value("model3dTranslation").toObject());
            model3DData->setTranslation(translation);
        }
        if (metadata.contains("model3dRotation") && metadata.value("model3dRotation").isObject()) {
            Model3DBase rotation;
            rotation.fromJson(metadata.value("model3dRotation").toObject());
            model3DData->setRotation(rotation);
        }
        componentData->setModel3DData(model3DData);
    }

    LOG_DEBUG(LogModule::Core, "Loaded component data from disk cache: {}", lcscId);
    return componentData;
}

void ComponentCacheService::saveComponentMetadata(const QString& componentId, const ComponentData& data) {
    QJsonObject metadata = buildMetadata(componentId, data);
    const QJsonObject existingMetadata = loadMetadata(componentId);
    metadata = mergeMetadata(existingMetadata, metadata);

    QJsonDocument doc(metadata);
    QString key = makeMemoryKey(componentId, "metadata");
    QByteArray* newData = new QByteArray(doc.toJson(QJsonDocument::Compact));
    qint64 sizeAfterUpdate = 0;

    {
        QMutexLocker locker(&m_mutex);
        m_memoryCache.insert(key, newData, newData->size());
        sizeAfterUpdate = m_memoryCache.totalCost();
    }

    saveMetadata(componentId, metadata);
    emit memoryCacheSizeChanged(sizeAfterUpdate);
    emit cacheSaved(componentId);
    LOG_DEBUG(LogModule::Core, "Saved component metadata to cache: {}", componentId);
}

void ComponentCacheService::saveComponentMetadataAsync(const QString& componentId, const ComponentData& data) {
    // 异步版本：在后台线程执行文件I/O，不阻塞UI
    // 复制需要的数据以供后台线程使用
    const ComponentData dataCopy = data;

    (void)QtConcurrent::run([this, componentId, dataCopy]() { saveComponentMetadata(componentId, dataCopy); });
}

void ComponentCacheService::saveSymbolData(const QString& lcscId, const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }

    QString symbolPath;
    {
        QMutexLocker locker(&m_mutex);
        ensureComponentDir(lcscId);
        symbolPath = componentCacheDir(lcscId) + "/symbol.json";
    }

    if (writeFileAtomically(symbolPath, data)) {
        LOG_DEBUG(LogModule::Core, "Saved symbol data to disk: {}", symbolPath);
    } else {
        LOG_WARN(LogModule::Core, "Failed to write symbol data: {}", symbolPath);
    }

    // 同时保存到L1内存缓存
    saveSymbolDataToMemory(lcscId, data);
}

QByteArray ComponentCacheService::loadSymbolData(const QString& lcscId) const {
    // 先查L1内存缓存
    QByteArray data = loadSymbolDataFromMemory(lcscId);
    if (!data.isEmpty()) {
        return data;
    }

    // L1未命中，查L2磁盘
    QString symbolPath = componentCacheDir(lcscId) + "/symbol.json";
    if (!QFileInfo::exists(symbolPath)) {
        return QByteArray();
    }

    QFile file(symbolPath);
    if (file.open(QIODevice::ReadOnly)) {
        data = file.readAll();
        file.close();
        return data;
    }

    return QByteArray();
}

void ComponentCacheService::saveFootprintData(const QString& lcscId, const QByteArray& data) {
    if (data.isEmpty()) {
        return;
    }

    QString footprintPath;
    {
        QMutexLocker locker(&m_mutex);
        ensureComponentDir(lcscId);
        footprintPath = componentCacheDir(lcscId) + "/footprint.json";
    }

    if (writeFileAtomically(footprintPath, data)) {
        LOG_DEBUG(LogModule::Core, "Saved footprint data to disk: {}", footprintPath);
    } else {
        LOG_WARN(LogModule::Core, "Failed to write footprint data: {}", footprintPath);
    }

    // 同时保存到L1内存缓存
    saveFootprintDataToMemory(lcscId, data);
}

QByteArray ComponentCacheService::loadFootprintData(const QString& lcscId) const {
    // 先查L1内存缓存
    QByteArray data = loadFootprintDataFromMemory(lcscId);
    if (!data.isEmpty()) {
        return data;
    }

    // L1未命中，查L2磁盘
    QString footprintPath = componentCacheDir(lcscId) + "/footprint.json";
    if (!QFileInfo::exists(footprintPath)) {
        return QByteArray();
    }

    QFile file(footprintPath);
    if (file.open(QIODevice::ReadOnly)) {
        data = file.readAll();
        file.close();
        return data;
    }

    return QByteArray();
}

void ComponentCacheService::saveCadDataJson(const QString& lcscId, const QByteArray& cadData) {
    if (cadData.isEmpty()) {
        return;
    }

    QString cadDataPath;
    {
        QMutexLocker locker(&m_mutex);
        ensureComponentDir(lcscId);
        cadDataPath = componentCacheDir(lcscId) + "/cad_data.json";
    }

    if (writeFileAtomically(cadDataPath, cadData)) {
        LOG_DEBUG(LogModule::Core, "Saved CAD data JSON to disk: {}", cadDataPath);
    } else {
        LOG_WARN(LogModule::Core, "Failed to write CAD data JSON: {}", cadDataPath);
    }
}

QByteArray ComponentCacheService::loadCadDataJson(const QString& lcscId) const {
    QString cadDataPath = componentCacheDir(lcscId) + "/cad_data.json";
    if (!QFileInfo::exists(cadDataPath)) {
        return QByteArray();
    }

    QFile file(cadDataPath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        return data;
    }

    return QByteArray();
}

bool ComponentCacheService::hasSymbolFootprintCache(const QString& lcscId) const {
    // Fast path: if metadata is in memory cache, cad_data.json exists and was previously valid
    // This avoids disk I/O for components that were cached in this session
    QString metadataKey = makeMemoryKey(lcscId, "metadata");
    {
        QMutexLocker locker(&m_mutex);
        if (m_memoryCache.contains(metadataKey)) {
            // Metadata in memory means component was cached before
            // Just need to verify cad_data.json still exists (fast stat call)
            QString cadDataPath = componentCacheDir(lcscId) + "/cad_data.json";
            bool exists = QFileInfo::exists(cadDataPath);
            LOG_DEBUG(LogModule::Core,
                      "hasSymbolFootprintCache: memory hit for {}, cad_data.json exists: {}",
                      lcscId,
                      exists);
            return exists;
        }
    }

    // Slow path: memory cache miss - do fast disk check without full JSON parse
    // The actual JSON validity will be verified when loading the data
    QString cadDataPath = componentCacheDir(lcscId) + "/cad_data.json";
    QFileInfo fileInfo(cadDataPath);
    if (!fileInfo.exists()) {
        LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: no cad_data.json for {}", lcscId);
        return false;
    }

    // Quick integrity check: verify file has minimum size and ends with valid JSON terminator
    // This guards against truncated files from crashes during write, without full JSON parse
    const qint64 fileSize = fileInfo.size();
    if (fileSize < 2) {  // Minimum valid JSON: "{}"
        LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: cad_data.json too small for {}", lcscId);
        return false;
    }

    // Check last non-whitespace character is '}' (JSON object terminator)
    QFile file(cadDataPath);
    if (file.open(QIODevice::ReadOnly)) {
        const qint64 readSize = qMin(fileSize, qint64(16));
        file.seek(fileSize - readSize);  // Read last 16 bytes
        QByteArray tail = file.read(readSize);
        file.close();
        // Find last non-whitespace character
        for (int i = tail.size() - 1; i >= 0; --i) {
            char c = tail[i];
            if (c == ' ' || c == '\n' || c == '\r' || c == '\t') {
                continue;
            }
            bool valid = (c == '}');
            LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: disk check for {}, valid: {}", lcscId, valid);
            return valid;
        }
    }
    LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: failed to open cad_data.json for {}", lcscId);
    return false;
}

QByteArray ComponentCacheService::loadPreviewImage(const QString& lcscId, int imageIndex) const {
    if (imageIndex < 0 || imageIndex >= 3) {
        return QByteArray();
    }

    // 获取路径在锁外进行
    QString previewPath;
    {
        QMutexLocker locker(&m_mutex);
        previewPath = previewImagePath(lcscId, imageIndex);
    }

    // I/O 操作在锁外进行，避免长时间持锁导致其他线程阻塞
    if (!QFileInfo::exists(previewPath)) {
        return QByteArray();
    }

    QFile file(previewPath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        return data;
    }

    return QByteArray();
}

void ComponentCacheService::savePreviewImage(const QString& lcscId, const QByteArray& imageData, int imageIndex) {
    if (imageIndex < 0 || imageIndex >= 3 || imageData.isEmpty()) {
        return;
    }

    QString previewPath;
    {
        QMutexLocker locker(&m_mutex);
        ensureComponentDir(lcscId);
        previewPath = previewImagePath(lcscId, imageIndex);
    }

    if (writeFileAtomically(previewPath, imageData)) {
        LOG_DEBUG(LogModule::Core, "Saved preview image to disk: {}", previewPath);
    } else {
        LOG_WARN(LogModule::Core, "Failed to write preview image: {}", previewPath);
    }
    // 注意：预览图不存入L1内存缓存，因为数据量大
}

QByteArray ComponentCacheService::downloadPreviewImage(const QString& lcscId,
                                                       const QString& imageUrl,
                                                       int imageIndex,
                                                       ComponentExportStatus::NetworkDiagnostics* diag,
                                                       QAtomicInt* cancelled,
                                                       bool weakNetwork) {
    if (imageUrl.isEmpty() || imageIndex < 0) {
        return QByteArray();
    }

    QElapsedTimer timer;
    timer.start();

    // 检查磁盘缓存 - 只在锁内获取路径，文件读取在锁外执行以避免阻塞
    QString previewFilePath;
    {
        QMutexLocker locker(&m_mutex);
        previewFilePath = previewImagePath(lcscId, imageIndex);
    }

    if (QFileInfo::exists(previewFilePath)) {
        QFile file(previewFilePath);
        if (file.open(QIODevice::ReadOnly)) {
            LOG_DEBUG(LogModule::Core, "Preview image loaded from disk cache: {}", previewFilePath);
            if (diag) {
                diag->url = imageUrl;
                diag->statusCode = 200;
                diag->errorString = "";
                diag->retryCount = 0;
                diag->latencyMs = timer.elapsed();
                diag->wasRateLimited = false;
            }
            return file.readAll();
        }
    }

    // 检查取消标志
    if (cancelled && cancelled->loadRelaxed()) {
        LOG_DEBUG(LogModule::Core, "Preview image download cancelled for {} before start", lcscId);
        return QByteArray();
    }

    const RetryPolicy policy = RetryPolicy::fromProfile(RequestProfiles::previewImage(), weakNetwork);
    const NetworkResult result = NetworkClient::instance().get(QUrl(imageUrl), ResourceType::PreviewImage, policy);

    QByteArray data;
    int statusCode = result.statusCode;
    QString errorString;
    int retryCount = result.retryCount;
    bool wasRateLimited = result.diagnostic.wasRateLimited;

    if (cancelled && cancelled->loadRelaxed()) {
        errorString = "Cancelled";
    } else if (result.wasCancelled) {
        errorString = "Cancelled";
    } else if (result.success) {
        data = result.data;
    } else {
        errorString = result.error;
    }

    // 更新诊断信息
    if (diag) {
        diag->url = imageUrl;
        diag->statusCode = statusCode;
        diag->errorString = errorString;
        diag->retryCount = retryCount;
        diag->latencyMs = timer.elapsed();
        diag->wasRateLimited = wasRateLimited;
    }

    if (errorString.isEmpty() && !data.isEmpty()) {
        // 保存到磁盘缓存
        savePreviewImage(lcscId, data, imageIndex);
    } else if (!errorString.isEmpty() && errorString != "Cancelled") {
        LOG_WARN(LogModule::Core, "Preview image download failed for {}: {}", lcscId, errorString);
    }

    return data;
}

QByteArray ComponentCacheService::loadDatasheet(const QString& lcscId) const {
    const QString preferredFormat = loadMetadata(lcscId).value("datasheetFormat").toString();
    const QString datasheetFilePath = resolveDatasheetPath(lcscId, preferredFormat, false);
    if (!QFileInfo::exists(datasheetFilePath)) {
        return QByteArray();
    }

    QFile file(datasheetFilePath);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        return data;
    }

    return QByteArray();
}

void ComponentCacheService::saveDatasheet(const QString& lcscId,
                                          const QByteArray& datasheetData,
                                          const QString& format) {
    if (datasheetData.isEmpty()) {
        return;
    }

    QString actualPath;
    {
        QMutexLocker locker(&m_mutex);
        ensureComponentDir(lcscId);
        actualPath = resolveDatasheetPath(lcscId, format, true);
        const QString alternatePath = actualPath.endsWith(".pdf")
                                          ? resolveDatasheetPath(lcscId, QStringLiteral("html"), true)
                                          : resolveDatasheetPath(lcscId, QStringLiteral("pdf"), true);
        if (alternatePath != actualPath && QFile::exists(alternatePath)) {
            QFile::remove(alternatePath);
        }
    }

    if (writeFileAtomically(actualPath, datasheetData)) {
        LOG_DEBUG(LogModule::Core, "Saved datasheet to disk: {}", actualPath);
    } else {
        LOG_WARN(LogModule::Core, "Failed to write datasheet: {}", actualPath);
    }
    // 注意：数据手册不存入L1内存缓存，因为数据量大
}

QByteArray ComponentCacheService::downloadDatasheet(const QString& lcscId,
                                                    const QString& datasheetUrl,
                                                    QString* format,
                                                    ComponentExportStatus::NetworkDiagnostics* diag,
                                                    QAtomicInt* cancelled,
                                                    bool weakNetwork) {
    if (datasheetUrl.isEmpty()) {
        return QByteArray();
    }

    QElapsedTimer timer;
    timer.start();

    // 确定格式
    QString ext = datasheetUrl.toLower().contains(".html") ? "html" : "pdf";
    if (format) {
        *format = ext;
    }

    // 检查磁盘缓存是否已存在
    {
        QMutexLocker locker(&m_mutex);
        QString datasheetFilePath = datasheetPath(lcscId);
        QString fullPath = datasheetFilePath;
        if (!fullPath.endsWith(".pdf") && !fullPath.endsWith(".html")) {
            fullPath += "." + ext;
        }
        if (QFileInfo::exists(fullPath)) {
            QFile file(fullPath);
            if (file.open(QIODevice::ReadOnly)) {
                LOG_DEBUG(LogModule::Core, "Datasheet loaded from disk cache: {}", fullPath);
                if (diag) {
                    diag->url = datasheetUrl;
                    diag->statusCode = 200;
                    diag->errorString = "";
                    diag->retryCount = 0;
                    diag->latencyMs = timer.elapsed();
                    diag->wasRateLimited = false;
                }
                return file.readAll();
            }
        }
    }

    // 检查取消标志
    if (cancelled && cancelled->loadRelaxed()) {
        LOG_DEBUG(LogModule::Core, "Datasheet download cancelled for {} before start", lcscId);
        return QByteArray();
    }

    const RetryPolicy policy = RetryPolicy::fromProfile(RequestProfiles::datasheet(), weakNetwork);
    const NetworkResult result = NetworkClient::instance().get(QUrl(datasheetUrl), ResourceType::Datasheet, policy);

    QByteArray data;
    int statusCode = result.statusCode;
    QString errorString;
    int retryCount = result.retryCount;
    bool wasRateLimited = result.diagnostic.wasRateLimited;

    if (cancelled && cancelled->loadRelaxed()) {
        errorString = "Cancelled";
    } else if (result.wasCancelled) {
        errorString = "Cancelled";
    } else if (result.success) {
        data = result.data;
        if (format && ext == "pdf" && data.size() >= 5 && !data.startsWith("%PDF-")) {
            ext = "html";
            *format = ext;
        }
    } else {
        errorString = result.error;
    }

    // 更新诊断信息
    if (diag) {
        diag->url = datasheetUrl;
        diag->statusCode = statusCode;
        diag->errorString = errorString;
        diag->retryCount = retryCount;
        diag->latencyMs = timer.elapsed();
        diag->wasRateLimited = wasRateLimited;
    }

    if (errorString.isEmpty() && !data.isEmpty()) {
        // 保存到磁盘缓存
        saveDatasheet(lcscId, data, ext);
    } else if (!errorString.isEmpty() && errorString != "Cancelled") {
        LOG_WARN(LogModule::Core, "Datasheet download failed for {}: {}", lcscId, errorString);
    }

    return data;
}

bool ComponentCacheService::hasModel3DCached(const QString& uuid, const QString& extension) const {
    QMutexLocker locker(&m_mutex);
    QString path = model3DPath(uuid, extension);
    return QFileInfo::exists(path);
}

QByteArray ComponentCacheService::loadModel3D(const QString& uuid, const QString& extension) const {
    QMutexLocker locker(&m_mutex);

    QString path = model3DPath(uuid, extension);
    if (!QFileInfo::exists(path)) {
        return QByteArray();
    }

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray data = file.readAll();
        file.close();
        return data;
    }

    return QByteArray();
}

void ComponentCacheService::saveModel3D(const QString& uuid, const QByteArray& data, const QString& extension) {
    if (data.isEmpty()) {
        return;
    }

    QString path;
    {
        QMutexLocker locker(&m_mutex);
        ensureModel3DCacheDir();
        path = model3DPath(uuid, extension);
    }

    if (writeFileAtomically(path, data)) {
        LOG_DEBUG(LogModule::Core, "Saved 3D model to disk: {}", path);
    } else {
        LOG_WARN(LogModule::Core, "Failed to write 3D model: {}", path);
    }
    // 注意：3D模型数据量大，不存入L1内存缓存
}

bool ComponentCacheService::copyModel3DToFile(const QString& uuid,
                                              const QString& extension,
                                              const QString& destinationPath) const {
    if (uuid.isEmpty() || extension.isEmpty() || destinationPath.isEmpty()) {
        return false;
    }

    QString sourcePath = model3DPath(uuid, extension);
    if (!QFileInfo::exists(sourcePath)) {
        LOG_WARN(LogModule::Core, "copyModel3DToFile: Source file does not exist: {}", sourcePath);
        return false;
    }

    // 确保目标目录存在
    QFileInfo destInfo(destinationPath);
    QDir destDir = destInfo.dir();
    if (!destDir.exists()) {
        if (!destDir.mkpath(destDir.path())) {
            LOG_WARN(LogModule::Core, "copyModel3DToFile: Failed to create destination directory: {}", destDir.path());
            return false;
        }
    }

    // 直接拷贝文件，不经过内存
    if (QFile::exists(destinationPath)) {
        QFile::remove(destinationPath);
    }

    if (QFile::copy(sourcePath, destinationPath)) {
        LOG_DEBUG(LogModule::Core, "Copied 3D model from cache to: {}", destinationPath);
        return true;
    } else {
        LOG_WARN(LogModule::Core, "copyModel3DToFile: Failed to copy {} -> {}", sourcePath, destinationPath);
        return false;
    }
}

// ==================== 缓存管理 ====================

void ComponentCacheService::removeCache(const QString& lcscId) {
    // 先删除L2磁盘缓存（不需要锁）
    QString dirPath = componentCacheDir(lcscId);
    {
        QDir dir(dirPath);
        if (dir.exists()) {
            dir.removeRecursively();
            LOG_DEBUG(LogModule::Core, "Removed disk cache for: {}", lcscId);
        }
    }

    // 再删除L1内存缓存（需要锁）
    qint64 sizeAfterUpdate = 0;
    {
        QMutexLocker locker(&m_mutex);

        QString metadataKey = makeMemoryKey(lcscId, "metadata");
        QString symbolKey = makeMemoryKey(lcscId, "symbol");
        QString footprintKey = makeMemoryKey(lcscId, "footprint");

        // take() 会自动从 QCache 的 totalCost() 中扣除被移除项的 cost
        delete m_memoryCache.take(metadataKey);
        delete m_memoryCache.take(symbolKey);
        delete m_memoryCache.take(footprintKey);

        sizeAfterUpdate = m_memoryCache.totalCost();
    }
    // 锁外发送信号
    emit memoryCacheSizeChanged(sizeAfterUpdate);
}

void ComponentCacheService::clearAllCache() {
    // 先清空L2磁盘缓存（不需要锁）
    {
        QDir dir(m_cacheDir);
        if (dir.exists()) {
            // 删除所有元器件缓存目录
            for (const QString& subDir : dir.entryList(QDir::Dirs)) {
                if (subDir != "." && subDir != "..") {
                    // 使用 QDir::filePath 确保跨平台路径正确
                    QDir subDirToRemove(dir.filePath(subDir));
                    subDirToRemove.removeRecursively();
                }
            }
            LOG_DEBUG(LogModule::Core, "Cleared all disk cache");
        }
    }

    // 再清空L1内存缓存（需要锁）
    clearMemoryCache();

    emit cacheSizeChanged(0);
}

void ComponentCacheService::clearMemoryCache() {
    {
        QMutexLocker locker(&m_mutex);

        // QCache::clear() 会删除所有对象并重置 totalCost() 为 0
        m_memoryCache.clear();
    }
    LOG_DEBUG(LogModule::Core, "Cleared memory cache");
    emit memoryCacheSizeChanged(0);
}

QStringList ComponentCacheService::getCachedComponentIds() const {
    QMutexLocker locker(&m_mutex);

    QStringList result;
    QDir dir(m_cacheDir);
    if (!dir.exists()) {
        return result;
    }

    for (const QString& entry : dir.entryList(QDir::Dirs)) {
        if (entry != "." && entry != ".." && entry != "model3d") {
            // 检查是否是有效的缓存目录（有metadata.json）
            if (QFileInfo::exists(metadataPath(entry))) {
                result.append(entry);
            }
        }
    }

    return result;
}

qint64 ComponentCacheService::getCacheSize() const {
    QMutexLocker locker(&m_mutex);
    return calculateDirSize(m_cacheDir);
}

qint64 ComponentCacheService::getMemoryCacheSize() const {
    QMutexLocker locker(&m_mutex);
    return m_memoryCache.totalCost();
}

qint64 ComponentCacheService::calculateDirSize(const QString& dirPath) const {
    qint64 size = 0;
    QDir dir(dirPath);
    if (!dir.exists()) {
        return size;
    }

    for (const QFileInfo& info : dir.entryInfoList(QDir::Files)) {
        size += info.size();
    }

    for (const QString& subDir : dir.entryList(QDir::Dirs)) {
        if (subDir != "." && subDir != "..") {
            size += calculateDirSize(dirPath + "/" + subDir);
        }
    }

    return size;
}

void ComponentCacheService::pruneCache(qint64 targetSizeBytes) {
    CachePruner pruner(m_cacheDir);
    qint64 newSize = pruner.pruneTo(targetSizeBytes);
    emit cacheSizeChanged(newSize);
}

void ComponentCacheService::setMemoryCacheLimit(int maxSizeMB) {
    QMutexLocker locker(&m_mutex);
    m_memoryCacheLimitMB = maxSizeMB;
    m_memoryCache.setMaxCost(maxSizeMB * 1024 * 1024);
    LOG_DEBUG(LogModule::Core, "Memory cache limit set to: {} MB", maxSizeMB);
}

int ComponentCacheService::memoryCacheLimit() const {
    QMutexLocker locker(&m_mutex);
    return m_memoryCacheLimitMB;
}

QJsonObject ComponentCacheService::loadMetadata(const QString& lcscId) const {
    QString metaPath = metadataPath(lcscId);
    if (!QFileInfo::exists(metaPath)) {
        return QJsonObject();
    }

    QFile file(metaPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QJsonObject();
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        LOG_WARN(LogModule::Core, "JSON parse error: {}", error.errorString());
        return QJsonObject();
    }

    return doc.object();
}

void ComponentCacheService::saveMetadata(const QString& lcscId, const QJsonObject& metadata) {
    ensureComponentDir(lcscId);
    QString metaPath = metadataPath(lcscId);
    QJsonDocument doc(metadata);
    if (!writeFileAtomically(metaPath, doc.toJson(QJsonDocument::Indented))) {
        LOG_WARN(LogModule::Core, "Failed to open metadata file for writing: {}", metaPath);
    }
}

QJsonObject ComponentCacheService::buildMetadata(const QString& componentId, const ComponentData& data) const {
    QJsonObject metadata;
    metadata["lcscId"] = componentId;
    metadata["name"] = data.name();
    metadata["prefix"] = data.prefix();
    metadata["package"] = data.package();
    metadata["manufacturer"] = data.manufacturer();
    metadata["manufacturerPart"] = data.manufacturerPart();
    metadata["datasheet"] = data.datasheet();
    metadata["datasheetFormat"] = data.datasheetFormat();
    metadata["cachedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray previewUrls;
    for (const QString& url : data.previewImages()) {
        const QString normalizedUrl = UrlUtils::normalizePreviewImageUrl(url);
        if (!normalizedUrl.isEmpty()) {
            previewUrls.append(normalizedUrl);
        }
    }
    metadata["previewImages"] = previewUrls;

    if (data.model3DData() && !data.model3DData()->uuid().isEmpty()) {
        metadata["model3duuid"] = data.model3DData()->uuid();
        metadata["model3dName"] = data.model3DData()->name();
        metadata["model3dTranslation"] = data.model3DData()->translation().toJson();
        metadata["model3dRotation"] = data.model3DData()->rotation().toJson();
    }

    return metadata;
}

QJsonObject ComponentCacheService::mergeMetadata(const QJsonObject& existing, const QJsonObject& incoming) const {
    QJsonObject merged = existing;
    for (auto it = incoming.begin(); it != incoming.end(); ++it) {
        const QJsonValue& value = it.value();
        bool shouldWrite = true;

        if (value.isString() && value.toString().isEmpty()) {
            shouldWrite = false;
        } else if (value.isArray() && value.toArray().isEmpty()) {
            shouldWrite = false;
        } else if (value.isObject() && value.toObject().isEmpty()) {
            shouldWrite = false;
        }

        if (shouldWrite) {
            merged[it.key()] = value;
        }
    }

    merged["cachedAt"] = incoming.value("cachedAt").toString(QDateTime::currentDateTime().toString(Qt::ISODate));
    return merged;
}

bool ComponentCacheService::writeFileAtomically(const QString& path, const QByteArray& data) const {
    QFileInfo info(path);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

QString ComponentCacheService::resolveDatasheetPath(const QString& lcscId,
                                                    const QString& preferredFormat,
                                                    bool forWrite) const {
    const QString basePath = datasheetPath(lcscId);
    const QString normalizedFormat = preferredFormat.toLower();

    if (normalizedFormat.contains("pdf")) {
        return basePath + ".pdf";
    }
    if (normalizedFormat.contains("html")) {
        return basePath + ".html";
    }

    const QString pdfPath = basePath + ".pdf";
    const QString htmlPath = basePath + ".html";

    if (!forWrite) {
        if (QFileInfo::exists(pdfPath)) {
            return pdfPath;
        }
        if (QFileInfo::exists(htmlPath)) {
            return htmlPath;
        }
    }

    return pdfPath;
}

void ComponentCacheService::selfHealCache() {
    CacheHealthManager healer(m_cacheDir);
    healer.healAll();
}

QDateTime ComponentCacheService::getCacheAccessTime(const QString& lcscId) const {
    QString dirPath = componentCacheDir(lcscId);
    QFileInfo info(dirPath);
    if (info.exists()) {
        return info.lastModified();
    }
    return QDateTime();
}

QString ComponentCacheService::metadataPath(const QString& lcscId) const {
    return componentCacheDir(lcscId) + "/component.json";
}

QString ComponentCacheService::previewImagePath(const QString& lcscId, int index) const {
    return componentCacheDir(lcscId) + "/preview_" + QString::number(index) + ".jpg";
}

QString ComponentCacheService::datasheetPath(const QString& lcscId) const {
    return componentCacheDir(lcscId) + "/datasheet";
}

QString ComponentCacheService::model3DPath(const QString& uuid, const QString& extension) const {
    return m_cacheDir + "/model3d/" + uuid + "." + extension;
}

}  // namespace EasyKiConverter
