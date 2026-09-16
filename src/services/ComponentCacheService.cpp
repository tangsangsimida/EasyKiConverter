#include "ComponentCacheService.h"

#include "CacheHealthManager.h"
#include "CachePruner.h"
#include "ConfigService.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"
#include "services/BomParser.h"
#include "services/ComponentService.h"
#include "utils/logging/LogMacros.h"

#include <QAtomicInt>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

namespace {

/**
 * @brief 获取元器件可用于缓存的三维模型信息。
 * @param data 元器件数据。
 * @return 优先使用独立模型字段，缺失 UUID 时回退到封装模型字段。
 */
Model3DData effectiveModel3D(const ComponentData& data) {
    Model3DData model;
    if (data.model3DData())
        model = *data.model3DData();
    if (model.uuid().isEmpty() && data.footprintData()) {
        const Model3DData footprintModel = data.footprintData()->model3D();
        if (!footprintModel.uuid().isEmpty())
            model = footprintModel;
    }
    return model;
}

// 校验缓存元数据中的三维模型字段是否完整有效。
bool hasValidModel3DMetadata(const QJsonObject& metadata) {
    if (!metadata.contains(QStringLiteral("model3duuid")))
        return true;

    const QJsonValue uuid = metadata.value(QStringLiteral("model3duuid"));
    if (!uuid.isString() || uuid.toString().isEmpty())
        return false;

    if (metadata.contains(QStringLiteral("model3dName")) && !metadata.value(QStringLiteral("model3dName")).isString())
        return false;

    const auto validateVector = [&metadata](const QString& name) {
        if (!metadata.contains(name))
            return true;
        const QJsonValue value = metadata.value(name);
        if (!value.isObject())
            return false;
        Model3DBase vector;
        return vector.fromJson(value.toObject());
    };

    return validateVector(QStringLiteral("model3dTranslation")) && validateVector(QStringLiteral("model3dRotation"));
}

/**
 * @brief 从指定路径读取元器件元数据。
 * @param metadataPath 元数据文件路径。
 * @return 解析成功的 JSON 对象，读取或解析失败时返回空对象。
 */
QJsonObject readMetadataFile(const QString& metadataPath) {
    if (!QFileInfo::exists(metadataPath))
        return QJsonObject();

    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly))
        return QJsonObject();

    const QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        LOG_WARN(LogModule::Core, "JSON parse error: {}", error.errorString());
        return QJsonObject();
    }

    return doc.object();
}

}  // namespace

std::unique_ptr<ComponentCacheService> ComponentCacheService::s_instance;

ComponentCacheService::ComponentCacheService(QObject* parent)
    : QObject(parent)
    , m_memoryCacheLimitMB(50)
    , m_diskCacheLimitMB(ConfigService::DEFAULT_DISK_CACHE_LIMIT_MB)
    , m_memoryCacheSize(0) {
    m_lastEnforceTimer.start();
    setCacheDir(ConfigService::defaultCacheDir());

    // 初始化L1内存缓存，设置大小限制（50MB = 50 * 1024 * 1024 bytes）
    // QCache 的 cost 是存储的字节数
    m_memoryCache.setMaxCost(50 * 1024 * 1024);
}

ComponentCacheService::~ComponentCacheService() = default;

// 返回进程内唯一的缓存服务实例。
ComponentCacheService* ComponentCacheService::instance() {
    if (!s_instance) {
        s_instance = std::unique_ptr<ComponentCacheService>(new ComponentCacheService());
    }
    return s_instance.get();
}

// 设置缓存目录，并按需迁移旧目录中的缓存内容。
void ComponentCacheService::setCacheDir(const QString& cacheDir, bool migrateExistingCache) {
    const QString newCacheDir = QDir::cleanPath(cacheDir);
    QString oldCacheDir;
    {
        QMutexLocker locker(&m_cacheDirMutex);
        oldCacheDir = m_cacheDir;
    }

    const bool cacheDirChanged = oldCacheDir != newCacheDir;
    QMutexLocker diskLocker(&m_diskWriteMutex);
    if (cacheDirChanged) {
        // 先使切换前排队的异步写入失效，再进行目录迁移，避免旧请求污染新目录。
        m_cacheGeneration.fetch_add(1);
        QMutexLocker tombstoneLocker(&m_tombstoneMutex);
        m_allTombstoned = false;
        m_tombstones.clear();
    }

    // 迁移期间持有磁盘写锁，避免异步写入与目录迁移交错。
    if (migrateExistingCache && !oldCacheDir.isEmpty() && cacheDirChanged) {
        migrateCacheDirectory(oldCacheDir, newCacheDir);
    }

    // 目录创建也在锁外（mkpath 是线程安全的）
    QDir dir;
    if (!dir.exists(newCacheDir)) {
        dir.mkpath(newCacheDir);
    }
    const QString model3dDir = newCacheDir + "/model3d";
    if (!dir.exists(model3dDir)) {
        dir.mkpath(model3dDir);
    }

    // 原子切换缓存目录指针
    {
        QMutexLocker locker(&m_cacheDirMutex);
        m_cacheDir = newCacheDir;
    }
    if (cacheDirChanged) {
        // L1 数据没有目录归属信息，切换 L2 目录后必须全部失效，避免
        // 同一元件 ID 从旧目录泄漏到新目录。
        QMutexLocker locker(&m_mutex);
        m_memoryCache.clear();
    }
    if (cacheDirChanged)
        emit memoryCacheSizeChanged(0);

    selfHealCache();
    LOG_DEBUG(LogModule::Core, "Cache directory set to: {}", newCacheDir);
}

// 将旧缓存目录中的内容迁移到新的缓存目录。
bool ComponentCacheService::migrateCacheDirectory(const QString& oldCacheDir, const QString& newCacheDir) const {
    if (oldCacheDir.isEmpty() || newCacheDir.isEmpty() || oldCacheDir == newCacheDir) {
        return true;
    }

    QDir source(oldCacheDir);
    if (!source.exists()) {
        return true;
    }

    QDir target;
    if (!target.exists(newCacheDir) && !target.mkpath(newCacheDir)) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration target directory: {}", newCacheDir);
        return false;
    }

    const bool moved = moveDirectoryContents(oldCacheDir, newCacheDir);
    if (moved) {
        source.rmdir(oldCacheDir);
        LOG_DEBUG(LogModule::Core, "Migrated cache directory from {} to {}", oldCacheDir, newCacheDir);
    } else {
        LOG_WARN(LogModule::Core,
                 "Cache directory migration completed with skipped or failed entries: {} -> {}",
                 oldCacheDir,
                 newCacheDir);
    }
    return moved;
}

// 迁移目录中的全部缓存条目，并返回整体迁移结果。
bool ComponentCacheService::moveDirectoryContents(const QString& sourceDir, const QString& targetDir) const {
    QDir source(sourceDir);
    if (!source.exists()) {
        return true;
    }

    QDir target;
    if (!target.exists(targetDir) && !target.mkpath(targetDir)) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration directory: {}", targetDir);
        return false;
    }

    bool allMoved = true;
    const QFileInfoList entries = source.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    for (const QFileInfo& entryInfo : entries) {
        const QString sourcePath = entryInfo.absoluteFilePath();
        const QString targetPath = QDir(targetDir).filePath(entryInfo.fileName());
        if (!moveCacheEntry(sourcePath, targetPath)) {
            allMoved = false;
        }
    }

    return allMoved;
}

// 迁移单个缓存文件或目录条目。
bool ComponentCacheService::moveCacheEntry(const QString& sourcePath, const QString& targetPath) const {
    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
        return true;
    }

    QFileInfo targetInfo(targetPath);
    if (targetInfo.exists()) {
        if (sourceInfo.isDir() && targetInfo.isDir()) {
            const bool moved = moveDirectoryContents(sourcePath, targetPath);
            QDir sourceDir(sourcePath);
            if (sourceDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
                sourceDir.rmdir(sourcePath);
            }
            return moved;
        }

        LOG_WARN(LogModule::Core, "Skipping cache migration entry because target already exists: {}", targetPath);
        return false;
    }

    QDir targetParent(targetInfo.absolutePath());
    if (!targetParent.exists() && !targetParent.mkpath(QStringLiteral("."))) {
        LOG_WARN(LogModule::Core, "Failed to create cache migration parent directory: {}", targetInfo.absolutePath());
        return false;
    }

    if (sourceInfo.isDir()) {
        QDir dir;
        if (dir.rename(sourcePath, targetPath)) {
            return true;
        }

        if (!moveDirectoryContents(sourcePath, targetPath)) {
            return false;
        }

        QDir sourceDir(sourcePath);
        return sourceDir.removeRecursively();
    }

    if (QFile::rename(sourcePath, targetPath)) {
        return true;
    }

    if (QFile::copy(sourcePath, targetPath)) {
        return QFile::remove(sourcePath);
    }

    LOG_WARN(LogModule::Core, "Failed to migrate cache file: {} -> {}", sourcePath, targetPath);
    return false;
}

// 获取当前缓存根目录。
QString ComponentCacheService::cacheDir() const {
    QMutexLocker locker(&m_cacheDirMutex);
    return m_cacheDir;
}

// 获取指定元器件的缓存目录。
QString ComponentCacheService::componentCacheDir(const QString& lcscId) const {
    if (!BomParser::validateId(lcscId)) {
        qWarning() << "componentCacheDir: invalid lcscId, rejecting:" << lcscId;
        return QString();
    }
    QMutexLocker locker(&m_cacheDirMutex);
    return QDir::cleanPath(m_cacheDir + "/" + lcscId);
}

// 确保指定元器件的缓存目录存在。
QString ComponentCacheService::ensureComponentDir(const QString& lcscId) const {
    // 注意：不再获取锁，因为：
    // 1. componentCacheDir 只是构建路径字符串（只读操作）
    // 2. dir.mkpath 是线程安全的
    // 3. savePreviewImage 已经在持有 m_mutex 的情况下调用此函数
    QString dirPath = componentCacheDir(lcscId);
    if (dirPath.isEmpty()) {
        return QString();
    }
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(dirPath);
    }
    return dirPath;
}

// 确保公共三维模型缓存目录存在。
QString ComponentCacheService::ensureModel3DCacheDir() const {
    const QString dirPath = cacheDir() + "/model3d";
    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(dirPath);
    }
    return dirPath;
}

// 构造内存缓存使用的复合键。
QString ComponentCacheService::makeMemoryKey(const QString& lcscId, const QString& type) const {
    return lcscId + ":" + type;
}

// 判断元器件是否具有可用的完整磁盘缓存。
bool ComponentCacheService::hasCache(const QString& lcscId) const {
    // 统一由完整性检查负责路径读取，避免目录迁移期间出现先检查后失效。
    return isCacheValid(lcscId);
}

// 校验元数据、身份字段和 CAD 文件是否组成有效缓存。
bool ComponentCacheService::isCacheValid(const QString& lcscId) const {
    // 元数据和 CAD 文件必须在同一次目录迁移保护下完成检查。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    const QJsonObject metadata = readMetadataFile(metadataPath(lcscId));
    if (metadata.isEmpty()) {
        return false;
    }

    if (metadata.contains(QStringLiteral("lcscId")) &&
        (!metadata.value(QStringLiteral("lcscId")).isString() ||
         metadata.value(QStringLiteral("lcscId")).toString().compare(lcscId, Qt::CaseInsensitive) != 0)) {
        return false;
    }

    if (!hasValidModel3DMetadata(metadata)) {
        return false;
    }

    const bool hasCadJson = QFileInfo::exists(componentCacheDir(lcscId) + "/cad_data.json");
    const bool hasBasicIdentity =
        !metadata.value("lcscId").toString().isEmpty() || !metadata.value("name").toString().isEmpty();

    return hasCadJson || hasBasicIdentity;
}

// 判断元器件元数据是否存在于一级内存缓存。
bool ComponentCacheService::hasInMemoryCache(const QString& lcscId) const {
    QMutexLocker locker(&m_mutex);
    QString key = makeMemoryKey(lcscId, "metadata");
    return m_memoryCache.contains(key);
}

// ==================== L1 内存缓存操作 ====================

// 从一级内存缓存读取元器件元数据。
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

// 将元器件元数据写入一级内存缓存。
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

// 从一级内存缓存读取符号数据。
QByteArray ComponentCacheService::loadSymbolDataFromMemory(const QString& lcscId) const {
    QMutexLocker locker(&m_mutex);
    QString key = makeMemoryKey(lcscId, "symbol");
    QByteArray* data = m_memoryCache.object(key);
    if (data) {
        return *data;
    }
    return QByteArray();
}

// 将符号数据写入一级内存缓存。
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

// 从一级内存缓存读取封装数据。
QByteArray ComponentCacheService::loadFootprintDataFromMemory(const QString& lcscId) const {
    QMutexLocker locker(&m_mutex);
    QString key = makeMemoryKey(lcscId, "footprint");
    QByteArray* data = m_memoryCache.object(key);
    if (data) {
        return *data;
    }
    return QByteArray();
}

// 将封装数据写入一级内存缓存。
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
    // 锁住整个元数据读取过程，避免目录迁移在检查和读取之间切换路径。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    // 先检查缓存是否存在（不使用锁，因为只是检查文件是否存在）
    QString metaPath = metadataPath(lcscId);
    if (!QFileInfo::exists(metaPath)) {
        return nullptr;
    }

    // 再加载数据（使用锁保护）
    QMutexLocker locker(&m_mutex);

    QJsonObject metadata = readMetadataFile(metaPath);
    if (metadata.isEmpty()) {
        return nullptr;
    }

    if (!hasValidModel3DMetadata(metadata)) {
        LOG_WARN(LogModule::Core, "Rejected component cache with invalid 3D metadata: {}", lcscId);
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
            if (!translation.fromJson(metadata.value("model3dTranslation").toObject()))
                return nullptr;
            model3DData->setTranslation(translation);
        }
        if (metadata.contains("model3dRotation") && metadata.value("model3dRotation").isObject()) {
            Model3DBase rotation;
            if (!rotation.fromJson(metadata.value("model3dRotation").toObject()))
                return nullptr;
            model3DData->setRotation(rotation);
        }
        componentData->setModel3DData(model3DData);
    }

    LOG_DEBUG(LogModule::Core, "Loaded component data from disk cache: {}", lcscId);
    return componentData;
}

// 合并并持久化元器件元数据，同时保留有效的三维模型关联。
void ComponentCacheService::saveComponentMetadata(const QString& componentId,
                                                  const ComponentData& data,
                                                  uint64_t expectedGeneration,
                                                  bool replaceModel3DMetadata) {
    QJsonObject metadata = buildMetadata(componentId, data);
    QString key = makeMemoryKey(componentId, "metadata");
    qint64 sizeAfterUpdate = 0;

    // 旧元数据读取、合并、代次检查、L1 写入和磁盘写入必须在同一把锁内，
    // 否则并发的部分元数据更新可能基于同一个旧快照写回并互相覆盖。
    {
        QMutexLocker diskLocker(&m_diskWriteMutex);
        if (expectedGeneration != 0) {
            if (m_cacheGeneration.load() != expectedGeneration) {
                LOG_DEBUG(LogModule::Core, "Discarded stale write for {} (generation mismatch)", componentId);
                return;
            }
            if (isTombstoned(componentId)) {
                LOG_DEBUG(LogModule::Core, "Discarded write for tombstoned component {}", componentId);
                return;
            }
        }

        const QJsonObject existingMetadata = readMetadataFile(metadataPath(componentId));
        metadata = mergeMetadata(existingMetadata, metadata);
        const Model3DData model3D = effectiveModel3D(data);
        if (replaceModel3DMetadata && model3D.uuid().isEmpty()) {
            metadata.remove(QStringLiteral("model3duuid"));
            metadata.remove(QStringLiteral("model3dName"));
            metadata.remove(QStringLiteral("model3dTranslation"));
            metadata.remove(QStringLiteral("model3dRotation"));
        }

        QJsonDocument doc(metadata);
        QByteArray* newData = new QByteArray(doc.toJson(QJsonDocument::Compact));
        {
            QMutexLocker locker(&m_mutex);
            m_memoryCache.insert(key, newData, newData->size());
            sizeAfterUpdate = m_memoryCache.totalCost();
        }
        saveMetadata(componentId, metadata);
    }
    enforceDiskCacheLimit();
    emit memoryCacheSizeChanged(sizeAfterUpdate);
    emit cacheSaved(componentId);
    LOG_DEBUG(LogModule::Core, "Saved component metadata to cache: {}", componentId);
}

void ComponentCacheService::saveComponentMetadataAsync(const QString& componentId,
                                                       const ComponentData& data,
                                                       uint64_t expectedGeneration,
                                                       bool replaceModel3DMetadata) {
    // 异步版本：在后台线程执行文件I/O，不阻塞UI
    // 复制需要的数据以供后台线程使用
    const ComponentData dataCopy = data;
    // 如果调用方没有传入 generation，则在入队时捕获当前值
    const uint64_t generation = (expectedGeneration != 0) ? expectedGeneration : m_cacheGeneration.load();

    (void)QtConcurrent::run([this, componentId, dataCopy, generation, replaceModel3DMetadata]() {
        // 写入前检查代次：如果 clearAllCache 已调用，丢弃本次写入
        if (m_cacheGeneration.load() != generation) {
            return;
        }
        saveComponentMetadata(componentId, dataCopy, generation, replaceModel3DMetadata);
    });
}

// 将符号数据原子写入二级磁盘缓存。
void ComponentCacheService::saveSymbolData(const QString& lcscId, const QByteArray& data, uint64_t expectedGeneration) {
    if (data.isEmpty()) {
        return;
    }

    QMutexLocker diskLocker(&m_diskWriteMutex);
    if (expectedGeneration != 0) {
        if (m_cacheGeneration.load() != expectedGeneration) {
            return;
        }
        if (isTombstoned(lcscId)) {
            return;
        }
    }
    QString symbolPath;
    {
        QMutexLocker locker(&m_mutex);
        if (ensureComponentDir(lcscId).isEmpty()) {
            return;
        }
        symbolPath = componentCacheDir(lcscId) + "/symbol.json";
    }

    if (writeFileAtomically(symbolPath, data)) {
        LOG_DEBUG(LogModule::Core, "Saved symbol data to disk: {}", symbolPath);
        enforceDiskCacheLimit();
    } else {
        LOG_WARN(LogModule::Core, "Failed to write symbol data: {}", symbolPath);
    }

    // 同时保存到L1内存缓存
    saveSymbolDataToMemory(lcscId, data);
}

// 从二级磁盘缓存读取符号数据。
QByteArray ComponentCacheService::loadSymbolData(const QString& lcscId) const {
    // 先查L1内存缓存
    QByteArray data = loadSymbolDataFromMemory(lcscId);
    if (!data.isEmpty()) {
        return data;
    }

    // L2 文件读取必须与目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);
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

// 将封装数据原子写入二级磁盘缓存。
void ComponentCacheService::saveFootprintData(const QString& lcscId,
                                              const QByteArray& data,
                                              uint64_t expectedGeneration) {
    if (data.isEmpty()) {
        return;
    }

    QMutexLocker diskLocker(&m_diskWriteMutex);
    if (expectedGeneration != 0) {
        if (m_cacheGeneration.load() != expectedGeneration) {
            return;
        }
        if (isTombstoned(lcscId)) {
            return;
        }
    }
    QString footprintPath;
    {
        QMutexLocker locker(&m_mutex);
        if (ensureComponentDir(lcscId).isEmpty()) {
            return;
        }
        footprintPath = componentCacheDir(lcscId) + "/footprint.json";
    }

    if (writeFileAtomically(footprintPath, data)) {
        LOG_DEBUG(LogModule::Core, "Saved footprint data to disk: {}", footprintPath);
        enforceDiskCacheLimit();
    } else {
        LOG_WARN(LogModule::Core, "Failed to write footprint data: {}", footprintPath);
    }

    // 同时保存到L1内存缓存
    saveFootprintDataToMemory(lcscId, data);
}

// 从二级磁盘缓存读取封装数据。
QByteArray ComponentCacheService::loadFootprintData(const QString& lcscId) const {
    // 先查L1内存缓存
    QByteArray data = loadFootprintDataFromMemory(lcscId);
    if (!data.isEmpty()) {
        return data;
    }

    // L2 文件读取必须与目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);
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

// 将 CAD 原始 JSON 原子写入二级磁盘缓存。
void ComponentCacheService::saveCadDataJson(const QString& lcscId,
                                            const QByteArray& cadData,
                                            uint64_t expectedGeneration) {
    if (cadData.isEmpty()) {
        return;
    }

    QMutexLocker diskLocker(&m_diskWriteMutex);
    if (expectedGeneration != 0) {
        if (m_cacheGeneration.load() != expectedGeneration) {
            return;
        }
        if (isTombstoned(lcscId)) {
            return;
        }
    }
    QString cadDataPath;
    {
        QMutexLocker locker(&m_mutex);
        if (ensureComponentDir(lcscId).isEmpty()) {
            return;
        }
        cadDataPath = componentCacheDir(lcscId) + "/cad_data.json";
    }

    if (writeFileAtomically(cadDataPath, cadData)) {
        LOG_DEBUG(LogModule::Core, "Saved CAD data JSON to disk: {}", cadDataPath);
        enforceDiskCacheLimit();
    } else {
        LOG_WARN(LogModule::Core, "Failed to write CAD data JSON: {}", cadDataPath);
    }
}

// 从二级磁盘缓存读取 CAD 原始 JSON。
QByteArray ComponentCacheService::loadCadDataJson(const QString& lcscId) const {
    // CAD 文件读取必须与目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);
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

// 判断符号、封装和 CAD 数据缓存是否完整。
bool ComponentCacheService::hasSymbolFootprintCache(const QString& lcscId) const {
    // 缓存存在性和完整性检查必须使用稳定的缓存目录。
    QMutexLocker diskLocker(&m_diskWriteMutex);
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

// 从二级磁盘缓存读取指定预览图。
QByteArray ComponentCacheService::loadPreviewImage(const QString& lcscId, int imageIndex) const {
    if (imageIndex < 0 || imageIndex >= 3) {
        return QByteArray();
    }

    // 预览图读取必须与缓存目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);
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

void ComponentCacheService::savePreviewImage(const QString& lcscId,
                                             const QByteArray& imageData,
                                             int imageIndex,
                                             uint64_t expectedGeneration) {
    if (imageIndex < 0 || imageIndex >= 3 || imageData.isEmpty()) {
        return;
    }

    QMutexLocker diskLocker(&m_diskWriteMutex);
    if (expectedGeneration != 0) {
        if (m_cacheGeneration.load() != expectedGeneration) {
            return;
        }
        if (isTombstoned(lcscId)) {
            return;
        }
    }
    QString previewPath;
    {
        QMutexLocker locker(&m_mutex);
        if (ensureComponentDir(lcscId).isEmpty()) {
            return;
        }
        previewPath = previewImagePath(lcscId, imageIndex);
    }

    if (writeFileAtomically(previewPath, imageData)) {
        LOG_DEBUG(LogModule::Core, "Saved preview image to disk: {}", previewPath);
        enforceDiskCacheLimit();
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
    const uint64_t gen = currentGeneration();

    QElapsedTimer timer;
    timer.start();

    // 缓存目录路径、文件检查和读取必须与目录迁移串行化。
    {
        QMutexLocker diskLocker(&m_diskWriteMutex);
        const QString previewFilePath = previewImagePath(lcscId, imageIndex);
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
        savePreviewImage(lcscId, data, imageIndex, gen);
    } else if (!errorString.isEmpty() && errorString != "Cancelled") {
        LOG_WARN(LogModule::Core, "Preview image download failed for {}: {}", lcscId, errorString);
    }

    return data;
}

// 从二级磁盘缓存读取数据手册。
QByteArray ComponentCacheService::loadDatasheet(const QString& lcscId) const {
    // 数据手册路径和文件读取必须与目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    const QString preferredFormat = readMetadataFile(metadataPath(lcscId)).value("datasheetFormat").toString();
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
                                          const QString& format,
                                          uint64_t expectedGeneration) {
    if (datasheetData.isEmpty()) {
        return;
    }

    QMutexLocker diskLocker(&m_diskWriteMutex);
    if (expectedGeneration != 0) {
        if (m_cacheGeneration.load() != expectedGeneration) {
            return;
        }
        if (isTombstoned(lcscId)) {
            return;
        }
    }
    QString actualPath;
    {
        QMutexLocker locker(&m_mutex);
        if (ensureComponentDir(lcscId).isEmpty()) {
            return;
        }
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
        enforceDiskCacheLimit();
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
    const uint64_t gen = currentGeneration();

    QElapsedTimer timer;
    timer.start();

    // 确定格式
    QString ext = datasheetUrl.toLower().contains(".html") ? "html" : "pdf";
    if (format) {
        *format = ext;
    }

    // 缓存目录路径、文件检查和读取必须与目录迁移串行化。
    {
        QMutexLocker diskLocker(&m_diskWriteMutex);
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
        saveDatasheet(lcscId, data, ext, gen);
    } else if (!errorString.isEmpty() && errorString != "Cancelled") {
        LOG_WARN(LogModule::Core, "Datasheet download failed for {}: {}", lcscId, errorString);
    }

    return data;
}

// 判断指定格式的三维模型文件是否存在且非空。
bool ComponentCacheService::hasModel3DCached(const QString& uuid, const QString& extension) const {
    // 与目录迁移和模型写入串行化，避免检查到迁移中的文件。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    QMutexLocker locker(&m_mutex);
    QString path = model3DPath(uuid, extension);
    QFileInfo fileInfo(path);
    // 空文件视为未缓存（防止损坏文件被误判）
    return fileInfo.exists() && fileInfo.size() > 0;
}

// 从公共三维模型缓存读取指定格式的数据。
QByteArray ComponentCacheService::loadModel3D(const QString& uuid, const QString& extension) const {
    // 与目录迁移和模型写入串行化，避免读取到不完整的文件。
    QMutexLocker diskLocker(&m_diskWriteMutex);
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

void ComponentCacheService::saveModel3D(const QString& uuid,
                                        const QByteArray& data,
                                        const QString& extension,
                                        uint64_t expectedGeneration) {
    if (data.isEmpty()) {
        return;
    }

    QMutexLocker diskLocker(&m_diskWriteMutex);
    if (expectedGeneration != 0 && m_cacheGeneration.load() != expectedGeneration) {
        return;
    }
    QString path;
    {
        QMutexLocker locker(&m_mutex);
        ensureModel3DCacheDir();
        path = model3DPath(uuid, extension);
        if (path.isEmpty()) {
            return;
        }
    }

    if (writeFileAtomically(path, data)) {
        LOG_DEBUG(LogModule::Core, "Saved 3D model to disk: {}", path);
        enforceDiskCacheLimit();
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

    // 保证源文件在复制期间不会被缓存目录迁移或写入操作替换。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    QString sourcePath = model3DPath(uuid, extension);
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile() || sourceInfo.size() <= 0) {
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

    if (QFile::copy(sourcePath, destinationPath) && QFileInfo(destinationPath).size() > 0) {
        LOG_DEBUG(LogModule::Core, "Copied 3D model from cache to: {}", destinationPath);
        return true;
    } else {
        QFile::remove(destinationPath);
        LOG_WARN(LogModule::Core, "copyModel3DToFile: Failed to copy {} -> {}", sourcePath, destinationPath);
        return false;
    }
}

// ==================== 缓存管理 ====================

void ComponentCacheService::removeCache(const QString& lcscId) {
    // 锁顺序：先 disk，后 tombstone（与其他方法一致）
    QMutexLocker diskLocker(&m_diskWriteMutex);
    // 标记为 tombstone，阻止旧回调写回
    {
        QMutexLocker tombLocker(&m_tombstoneMutex);
        m_tombstones.insert(lcscId.toUpper());
    }
    // 先删除L2磁盘缓存
    QString dirPath = componentCacheDir(lcscId);
    if (dirPath.isEmpty()) {
        return;
    }
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

// 清除指定元器件的旧请求屏蔽标记。
void ComponentCacheService::clearTombstone(const QString& lcscId) {
    QMutexLocker tombLocker(&m_tombstoneMutex);
    m_tombstones.remove(lcscId.toUpper());
}

// 清除全局旧请求屏蔽标记。
void ComponentCacheService::clearGlobalTombstone() {
    QMutexLocker tombLocker(&m_tombstoneMutex);
    m_allTombstoned = false;
    m_tombstones.clear();
}

// 判断元器件是否仍被旧请求屏蔽。
bool ComponentCacheService::isTombstoned(const QString& lcscId) const {
    QMutexLocker tombLocker(&m_tombstoneMutex);
    return m_allTombstoned || m_tombstones.contains(lcscId.toUpper());
}

// 清空一级和二级缓存，并使旧异步写入失效。
void ComponentCacheService::clearAllCache() {
    // 递增代次，使所有正在排队的异步写入任务失效
    m_cacheGeneration.fetch_add(1);
    // 锁顺序：先 disk，后 tombstone（与 save 方法一致，避免死锁）
    QMutexLocker diskLocker(&m_diskWriteMutex);
    // 全局 tombstone：阻止所有旧回调写入
    {
        QMutexLocker tombLocker(&m_tombstoneMutex);
        m_allTombstoned = true;
    }
    // 先清空L2磁盘缓存（不需要锁）
    {
        QDir dir(cacheDir());
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

    // 再清空L1内存缓存（不重置 tombstone）
    clearMemoryCacheInternal();

    emit memoryCacheSizeChanged(0);
    emit cacheSizeChanged(0);
}

// 清空一级内存缓存的内部实现。
void ComponentCacheService::clearMemoryCacheInternal() {
    QMutexLocker locker(&m_mutex);
    m_memoryCache.clear();
}

// 清空一级内存缓存并解除旧请求屏蔽。
void ComponentCacheService::clearMemoryCache() {
    {
        QMutexLocker tombLocker(&m_tombstoneMutex);
        m_allTombstoned = false;
        m_tombstones.clear();
    }
    clearMemoryCacheInternal();
    LOG_DEBUG(LogModule::Core, "Cleared memory cache");
    emit memoryCacheSizeChanged(0);
}

// 枚举当前缓存目录中具有有效元数据的元器件编号。
QStringList ComponentCacheService::getCachedComponentIds() const {
    // 目录枚举和元数据检查必须与目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);

    QStringList result;
    QDir dir(cacheDir());
    if (!dir.exists()) {
        return result;
    }

    for (const QString& entry : dir.entryList(QDir::Dirs)) {
        if (entry != "." && entry != ".." && entry != "model3d") {
            // 检查是否是有效的缓存目录（有component.json）。
            if (QFileInfo::exists(metadataPath(entry))) {
                result.append(entry);
            }
        }
    }

    return result;
}

// 统计当前缓存目录的磁盘占用大小。
qint64 ComponentCacheService::getCacheSize() const {
    // 目录大小统计必须与目录迁移串行化，避免返回混合目录的大小。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    return calculateDirSize(cacheDir());
}

// 返回一级内存缓存的当前占用大小。
qint64 ComponentCacheService::getMemoryCacheSize() const {
    QMutexLocker locker(&m_mutex);
    return m_memoryCache.totalCost();
}

// 递归计算指定目录及其子目录的文件大小。
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

// 将磁盘缓存裁剪到指定大小以内。
void ComponentCacheService::pruneCache(qint64 targetSizeBytes) {
    // 手动清理必须与目录迁移串行化，避免清理旧目录或新目录中的部分内容。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    CachePruner pruner(cacheDir());
    qint64 newSize = pruner.pruneTo(targetSizeBytes);
    emit cacheSizeChanged(newSize);
}

// 设置一级内存缓存的最大容量。
void ComponentCacheService::setMemoryCacheLimit(int maxSizeMB) {
    QMutexLocker locker(&m_mutex);
    m_memoryCacheLimitMB = maxSizeMB;
    m_memoryCache.setMaxCost(maxSizeMB * 1024 * 1024);
    LOG_DEBUG(LogModule::Core, "Memory cache limit set to: {} MB", maxSizeMB);
}

// 获取一级内存缓存的最大容量。
int ComponentCacheService::memoryCacheLimit() const {
    QMutexLocker locker(&m_mutex);
    return m_memoryCacheLimitMB;
}

// 设置二级磁盘缓存的最大容量并立即触发清理。
void ComponentCacheService::setDiskCacheLimit(int maxSizeMB) {
    {
        QMutexLocker locker(&m_mutex);
        m_diskCacheLimitMB = qMax(1, maxSizeMB);
    }

    // 用户主动修改限制时绕过冷却机制，立即执行清理
    enforceDiskCacheLimit(/*bypassCooldown=*/true);
    LOG_DEBUG(LogModule::Core, "Disk cache limit set to: {} MB", maxSizeMB);
}

// 获取二级磁盘缓存的最大容量。
int ComponentCacheService::diskCacheLimit() const {
    QMutexLocker locker(&m_mutex);
    return m_diskCacheLimitMB;
}

// 按冷却策略执行二级磁盘缓存容量限制。
void ComponentCacheService::enforceDiskCacheLimit(bool bypassCooldown) {
    // 冷却机制：避免批量保存时频繁扫描目录（每次扫描开销较大）
    constexpr qint64 kCooldownMs = 3000;
    QString cacheDir;
    qint64 targetSizeBytes = 0;
    {
        QMutexLocker locker(&m_mutex);
        if (!bypassCooldown && m_lastEnforceTimer.elapsed() < kCooldownMs) {
            return;
        }
        cacheDir = this->cacheDir();
        targetSizeBytes = static_cast<qint64>(m_diskCacheLimitMB) * 1024 * 1024;
        m_lastEnforceTimer.restart();
    }

    if (targetSizeBytes <= 0 || cacheDir.isEmpty()) {
        return;
    }

    CachePruner pruner(cacheDir);
    const qint64 newSize = pruner.pruneTo(targetSizeBytes);
    emit cacheSizeChanged(newSize);
}

// 从二级磁盘缓存读取元器件元数据。
QJsonObject ComponentCacheService::loadMetadata(const QString& lcscId) const {
    // 外部元数据读取必须与缓存目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    return readMetadataFile(metadataPath(lcscId));
}

// 将元器件元数据原子写入二级磁盘缓存。
void ComponentCacheService::saveMetadata(const QString& lcscId, const QJsonObject& metadata) {
    if (ensureComponentDir(lcscId).isEmpty()) {
        return;
    }
    QString metaPath = metadataPath(lcscId);
    QJsonDocument doc(metadata);
    if (!writeFileAtomically(metaPath, doc.toJson(QJsonDocument::Indented))) {
        LOG_WARN(LogModule::Core, "Failed to open metadata file for writing: {}", metaPath);
    }
}

// 从元器件及其封装数据构建可持久化的缓存元数据。
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

    const Model3DData model3D = effectiveModel3D(data);
    if (!model3D.uuid().isEmpty()) {
        metadata["model3duuid"] = model3D.uuid();
        metadata["model3dName"] = model3D.name();
        metadata["model3dTranslation"] = model3D.translation().toJson();
        metadata["model3dRotation"] = model3D.rotation().toJson();
    }

    return metadata;
}

// 合并新旧元数据并保留未被空值覆盖的字段。
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

// 使用临时文件和提交操作原子写入缓存文件。
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

// 根据格式和现有文件解析数据手册的实际路径。
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

// 执行缓存目录的完整性修复。
void ComponentCacheService::selfHealCache() {
    CacheHealthManager healer(cacheDir());
    healer.healAll();
}

// 获取指定元器件缓存目录的最后修改时间。
QDateTime ComponentCacheService::getCacheAccessTime(const QString& lcscId) const {
    QString dirPath = componentCacheDir(lcscId);
    QFileInfo info(dirPath);
    if (info.exists()) {
        return info.lastModified();
    }
    return QDateTime();
}

// 构造元器件元数据文件路径。
QString ComponentCacheService::metadataPath(const QString& lcscId) const {
    const QString dir = componentCacheDir(lcscId);
    if (dir.isEmpty()) {
        return QString();
    }
    return dir + "/component.json";
}

// 构造元器件预览图文件路径。
QString ComponentCacheService::previewImagePath(const QString& lcscId, int index) const {
    const QString dir = componentCacheDir(lcscId);
    if (dir.isEmpty()) {
        return QString();
    }
    return dir + "/preview_" + QString::number(index) + ".jpg";
}

// 构造元器件数据手册基础路径。
QString ComponentCacheService::datasheetPath(const QString& lcscId) const {
    const QString dir = componentCacheDir(lcscId);
    if (dir.isEmpty()) {
        return QString();
    }
    return dir + "/datasheet";
}

// 根据经过校验的模型标识构造三维模型缓存路径。
QString ComponentCacheService::model3DPath(const QString& uuid, const QString& extension) const {
    // 严格校验 uuid 格式：仅允许字母、数字、下划线、短横线，防止路径穿越
    static const QRegularExpression uuidRe(QStringLiteral("^[A-Za-z0-9_-]+$"));
    if (uuid.isEmpty() || !uuidRe.match(uuid).hasMatch()) {
        qWarning() << "model3DPath: invalid uuid, rejecting:" << uuid;
        return QString();
    }
    // 校验 extension：仅允许字母数字
    static const QRegularExpression extRe(QStringLiteral("^[A-Za-z0-9]+$"));
    if (extension.isEmpty() || !extRe.match(extension).hasMatch()) {
        qWarning() << "model3DPath: invalid extension, rejecting:" << extension;
        return QString();
    }
    return cacheDir() + "/model3d/" + uuid + "." + extension;
}

}  // namespace EasyKiConverter

// 我攻略了你之后，别人也可以攻略你[惊讶]？！
// 不是这啥情况啊[疑惑]？我记得旮旯game不是单机游戏吗，
// 什么时候联网的[思考]？而就算你是玩家[给心心]！
// 我是这个攻略对象，也不能这样子吧[傲娇]，
// 是不是有点不道德啊[生气]？他是黑客[doge]。
// 什么黑客[思考]哪里黑了[大哭]！？哦还是国际服，
// 我不玩了[灵魂出窍]。
