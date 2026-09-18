#include "ComponentCacheService.h"

#include "CacheComponentDataReader.h"
#include "CacheDataValidator.h"
#include "CacheDirectoryMigrator.h"
#include "CacheFileLayout.h"
#include "CacheHealthManager.h"
#include "CacheMetadataStore.h"
#include "CachePathResolver.h"
#include "CachePruner.h"
#include "ComponentCacheBinaryFileStore.h"
#include "ComponentCacheCadDataWriter.h"
#include "ComponentCacheMaintenance.h"
#include "ComponentCacheMetadataWriter.h"
#include "ComponentCacheModel3DCoordinator.h"
#include "ComponentCachePreviewImageWriter.h"
#include "ComponentCacheQuotaEnforcer.h"
#include "ComponentCacheWritePolicy.h"
#include "ConfigService.h"
#include "DatasheetCacheFileStore.h"
#include "DatasheetDownloadService.h"
#include "PreviewImageDownloadService.h"
#include "core/kicad/Exporter3DModel.h"
#include "core/network/NetworkClient.h"
#include "core/utils/UrlUtils.h"
#include "services/BomParser.h"
#include "services/ComponentService.h"
#include "utils/logging/LogMacros.h"

#include <QAtomicInt>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

namespace {

// 从磁盘读取并校验 CAD 原始缓存文件。
bool hasValidCadDataFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();
    return CacheDataValidator::isValidCadData(data);
}

}  // namespace

std::unique_ptr<ComponentCacheService> ComponentCacheService::s_instance;

/** @brief 初始化默认容量、缓存目录和缓存生命周期计时器。 */
ComponentCacheService::ComponentCacheService(QObject* parent) /* 初始化默认容量和缓存目录。 */
    // 初始化 Qt 对象和 L1 缓存容量。
    : QObject(parent), m_diskCacheLimitMB(ConfigService::DEFAULT_DISK_CACHE_LIMIT_MB), m_memoryCache(50 * 1024 * 1024) {
    // 启动磁盘配额检查的冷却计时器，避免频繁写入时重复扫描缓存目录。
    m_lastEnforceTimer.start();
    setCacheDir(ConfigService::defaultCacheDir());
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
    {
        QMutexLocker diskLocker(&m_diskWriteMutex);
        if (cacheDirChanged) {
            // 先使切换前排队的异步写入失效，再进行目录迁移，避免旧请求污染新目录。
            m_cacheGeneration.fetch_add(1);
            m_tombstones.reset();
        }

        // 迁移期间持有磁盘写锁，避免异步写入与目录迁移交错。
        if (migrateExistingCache && !oldCacheDir.isEmpty() && cacheDirChanged) {
            CacheDirectoryMigrator::migrate(oldCacheDir, newCacheDir);
        }

        // 目录创建也在锁内，确保切换期间读写使用完整的目录结构。
        QDir dir;
        if (!dir.exists(newCacheDir)) {
            dir.mkpath(newCacheDir);
        }
        const QString model3dDir = newCacheDir + "/model3d";
        if (!dir.exists(model3dDir)) {
            dir.mkpath(model3dDir);
        }

        // 原子切换缓存目录指针。
        {
            QMutexLocker locker(&m_cacheDirMutex);
            m_cacheDir = newCacheDir;
        }
        if (cacheDirChanged) {
            // L1 数据没有目录归属信息，切换 L2 目录后必须全部失效，避免
            // 同一元件 ID 从旧目录泄漏到新目录。
            m_memoryCache.clear();
        }
    }
    if (cacheDirChanged)
        emit memoryCacheSizeChanged(0);

    selfHealCache();
    LOG_DEBUG(LogModule::Core, "Cache directory set to: {}", newCacheDir);
}

// 获取当前缓存根目录。
QString ComponentCacheService::cacheDir() const {
    QMutexLocker locker(&m_cacheDirMutex);
    return m_cacheDir;
}

// 获取指定元器件的缓存目录。
QString ComponentCacheService::componentCacheDir(const QString& lcscId) const {
    QMutexLocker locker(&m_cacheDirMutex);
    const QString path = CachePathResolver::componentDir(m_cacheDir, lcscId);
    if (path.isEmpty()) {
        qWarning() << "componentCacheDir: invalid lcscId, rejecting:" << lcscId;
    }
    return path;
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

// 判断元器件是否具有可用的完整磁盘缓存。
bool ComponentCacheService::hasCache(const QString& lcscId) const {
    // 统一由完整性检查负责路径读取，避免目录迁移期间出现先检查后失效。
    return isCacheValid(lcscId);
}

// 校验元数据、身份字段和 CAD 文件是否组成有效缓存。
bool ComponentCacheService::isCacheValid(const QString& lcscId) const {
    // 元数据和 CAD 文件必须在同一次目录迁移保护下完成检查。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    const QJsonObject metadata = CacheMetadataStore::read(metadataPath(lcscId));
    if (metadata.isEmpty()) {
        return false;
    }

    if (metadata.contains(QStringLiteral("lcscId")) &&
        (!metadata.value(QStringLiteral("lcscId")).isString() ||
         metadata.value(QStringLiteral("lcscId")).toString().compare(lcscId, Qt::CaseInsensitive) != 0)) {
        return false;
    }

    if (!CacheMetadataStore::hasValidModel3D(metadata)) {
        return false;
    }

    const bool hasCadJson = hasValidCadDataFile(CacheFileLayout::cadDataFile(componentCacheDir(lcscId)));
    const bool hasBasicIdentity =
        !metadata.value("lcscId").toString().isEmpty() || !metadata.value("name").toString().isEmpty();

    return hasCadJson || hasBasicIdentity;
}

// 判断元器件元数据是否存在于一级内存缓存。
bool ComponentCacheService::hasInMemoryCache(const QString& lcscId) const {
    return m_memoryCache.containsMetadata(lcscId);
}

// ==================== L1 内存缓存操作 ====================

// 从一级内存缓存读取元器件元数据。
QJsonObject ComponentCacheService::loadMetadataFromMemory(const QString& lcscId) const {
    return m_memoryCache.loadMetadata(lcscId);
}

// 将元器件元数据写入一级内存缓存。
void ComponentCacheService::saveMetadataToMemory(const QString& lcscId, const QJsonObject& metadata) {
    const qint64 sizeAfterUpdate = m_memoryCache.saveMetadata(lcscId, metadata);
    // 锁外发送信号
    emit memoryCacheSizeChanged(sizeAfterUpdate);
}

// 从一级内存缓存读取符号数据。
QByteArray ComponentCacheService::loadSymbolDataFromMemory(const QString& lcscId) const {
    return m_memoryCache.loadSymbol(lcscId);
}

// 将符号数据写入一级内存缓存。
void ComponentCacheService::saveSymbolDataToMemory(const QString& lcscId, const QByteArray& data) {
    const std::optional<qint64> sizeAfterUpdate = m_memoryCache.saveSymbol(lcscId, data);
    if (!sizeAfterUpdate.has_value()) {
        return;
    }
    // 锁外发送信号
    emit memoryCacheSizeChanged(*sizeAfterUpdate);
}

// 从一级内存缓存读取封装数据。
QByteArray ComponentCacheService::loadFootprintDataFromMemory(const QString& lcscId) const {
    return m_memoryCache.loadFootprint(lcscId);
}

// 将封装数据写入一级内存缓存。
void ComponentCacheService::saveFootprintDataToMemory(const QString& lcscId, const QByteArray& data) {
    const std::optional<qint64> sizeAfterUpdate = m_memoryCache.saveFootprint(lcscId, data);
    if (!sizeAfterUpdate.has_value()) {
        return;
    }
    // 锁外发送信号
    emit memoryCacheSizeChanged(*sizeAfterUpdate);
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

    QJsonObject metadata = CacheMetadataStore::read(metaPath);
    if (metadata.isEmpty()) {
        return nullptr;
    }

    if (!CacheMetadataStore::hasValidModel3D(metadata)) {
        LOG_WARN(LogModule::Core, "Rejected component cache with invalid 3D metadata: {}", lcscId);
        return nullptr;
    }

    auto componentData = CacheComponentDataReader::read(lcscId, metadata);
    if (!componentData) {
        return nullptr;
    }

    LOG_DEBUG(LogModule::Core, "Loaded component data from disk cache: {}", lcscId);
    return componentData;
}

// 合并并持久化元器件元数据，同时保留有效的三维模型关联。
void ComponentCacheService::saveComponentMetadata(const QString& componentId,
                                                  const ComponentData& data,
                                                  uint64_t expectedGeneration,
                                                  bool replaceModel3DMetadata) {
    ComponentCacheMetadataWriter metadataWriter(*this);
    const std::optional<qint64> sizeAfterUpdate =
        metadataWriter.write(componentId, data, expectedGeneration, replaceModel3DMetadata);
    if (!sizeAfterUpdate.has_value())
        return;

    enforceDiskCacheLimit();
    emit memoryCacheSizeChanged(*sizeAfterUpdate);
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
    ComponentCacheCadDataWriter writer(*this);
    writer.writeSymbol(lcscId, data, expectedGeneration);
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
    const QString symbolPath = CacheFileLayout::symbolFile(componentCacheDir(lcscId));
    return ComponentCacheBinaryFileStore::readCadData(symbolPath);
}

// 将封装数据原子写入二级磁盘缓存。
void ComponentCacheService::saveFootprintData(const QString& lcscId,
                                              const QByteArray& data,
                                              uint64_t expectedGeneration) {
    ComponentCacheCadDataWriter writer(*this);
    writer.writeFootprint(lcscId, data, expectedGeneration);
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
    const QString footprintPath = CacheFileLayout::footprintFile(componentCacheDir(lcscId));
    return ComponentCacheBinaryFileStore::readCadData(footprintPath);
}

// 将 CAD 原始 JSON 原子写入二级磁盘缓存。
void ComponentCacheService::saveCadDataJson(const QString& lcscId,
                                            const QByteArray& cadData,
                                            uint64_t expectedGeneration) {
    ComponentCacheCadDataWriter writer(*this);
    writer.writeCadJson(lcscId, cadData, expectedGeneration);
}

// 从二级磁盘缓存读取 CAD 原始 JSON。
QByteArray ComponentCacheService::loadCadDataJson(const QString& lcscId) const {
    // CAD 文件读取必须与目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    const QString cadDataPath = CacheFileLayout::cadDataFile(componentCacheDir(lcscId));
    return ComponentCacheBinaryFileStore::readCadData(cadDataPath);
}

// 判断符号、封装和 CAD 数据缓存是否完整。
bool ComponentCacheService::hasSymbolFootprintCache(const QString& lcscId) const {
    // 缓存存在性和完整性检查必须使用稳定的缓存目录。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    // 内存缓存命中时仍需验证 CAD JSON 内容，避免损坏文件仅因存在而通过检查。
    if (m_memoryCache.containsMetadata(lcscId)) {
        const QString cadDataPath = CacheFileLayout::cadDataFile(componentCacheDir(lcscId));
        const bool exists = hasValidCadDataFile(cadDataPath);
        LOG_DEBUG(
            LogModule::Core, "hasSymbolFootprintCache: memory hit for {}, cad_data.json exists: {}", lcscId, exists);
        return exists;
    }

    // 内存缓存未命中时直接校验磁盘中的 CAD JSON。
    const QString cadDataPath = CacheFileLayout::cadDataFile(componentCacheDir(lcscId));
    if (!hasValidCadDataFile(cadDataPath)) {
        LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: no cad_data.json for {}", lcscId);
        return false;
    }
    LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: valid CAD data for {}", lcscId);
    return true;
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

    // I/O 操作由文件存储器完成，避免服务层重复实现校验和损坏文件清理。
    return ComponentCacheBinaryFileStore::readPreviewImage(previewPath);
}

// 使用 Qt 图片解码器拒绝错误页、截断文件和其他非图片缓存内容。
bool ComponentCacheService::isValidPreviewImageData(const QByteArray& imageData) {
    return CacheDataValidator::isValidPreviewImage(imageData);
}

void ComponentCacheService::savePreviewImage(const QString& lcscId,
                                             const QByteArray& imageData,
                                             int imageIndex,
                                             uint64_t expectedGeneration) {
    ComponentCachePreviewImageWriter imageWriter(*this);
    imageWriter.write(lcscId, imageData, imageIndex, expectedGeneration);
}

QByteArray ComponentCacheService::downloadPreviewImage(const QString& lcscId,
                                                       const QString& imageUrl,
                                                       int imageIndex,
                                                       ComponentExportStatus::NetworkDiagnostics* diag,
                                                       QAtomicInt* cancelled,
                                                       bool weakNetwork,
                                                       uint64_t expectedGeneration) {
    PreviewImageDownloadService downloader(*this);
    return downloader.download(lcscId, imageUrl, imageIndex, diag, cancelled, weakNetwork, expectedGeneration);
}

// 从二级磁盘缓存读取数据手册。
QByteArray ComponentCacheService::loadDatasheet(const QString& lcscId) const {
    return DatasheetCacheFileStore::load(*this, lcscId);
}

// 校验 PDF 签名或 HTML 文档标记，拒绝被错误响应污染的数据手册缓存。
bool ComponentCacheService::isValidDatasheetData(const QByteArray& datasheetData, const QString& format) {
    return CacheDataValidator::isValidDatasheet(datasheetData, format);
}

void ComponentCacheService::saveDatasheet(const QString& lcscId,
                                          const QByteArray& datasheetData,
                                          const QString& format,
                                          uint64_t expectedGeneration) {
    DatasheetCacheFileStore fileStore(*this);
    fileStore.save(lcscId, datasheetData, format, expectedGeneration);
}

QByteArray ComponentCacheService::downloadDatasheet(const QString& lcscId,
                                                    const QString& datasheetUrl,
                                                    QString* format,
                                                    ComponentExportStatus::NetworkDiagnostics* diag,
                                                    QAtomicInt* cancelled,
                                                    bool weakNetwork,
                                                    uint64_t expectedGeneration) {
    DatasheetDownloadService downloader(*this);
    return downloader.download(lcscId, datasheetUrl, format, diag, cancelled, weakNetwork, expectedGeneration);
}

// 判断指定格式的三维模型文件是否存在且非空。
bool ComponentCacheService::hasModel3DCached(const QString& uuid, const QString& extension) const {
    // 协调器只读取缓存服务状态；const_cast 仅用于复用统一的私有锁边界，不会修改服务数据。
    ComponentCacheModel3DCoordinator coordinator(const_cast<ComponentCacheService&>(*this));
    return coordinator.hasCached(uuid, extension);
}

// 从公共三维模型缓存读取指定格式的数据。
QByteArray ComponentCacheService::loadModel3D(const QString& uuid, const QString& extension) const {
    // 协调器只读取缓存服务状态；const_cast 仅用于复用统一的私有锁边界，不会修改服务数据。
    ComponentCacheModel3DCoordinator coordinator(const_cast<ComponentCacheService&>(*this));
    return coordinator.load(uuid, extension);
}

void ComponentCacheService::saveModel3D(const QString& uuid,
                                        const QByteArray& data,
                                        const QString& extension,
                                        uint64_t expectedGeneration) {
    ComponentCacheModel3DCoordinator coordinator(*this);
    coordinator.save(uuid, data, extension, expectedGeneration);
}

bool ComponentCacheService::copyModel3DToFile(const QString& uuid,
                                              const QString& extension,
                                              const QString& destinationPath) const {
    // 协调器只读取缓存服务状态；const_cast 仅用于复用统一的私有锁边界，不会修改服务数据。
    ComponentCacheModel3DCoordinator coordinator(const_cast<ComponentCacheService&>(*this));
    return coordinator.copyToFile(uuid, extension, destinationPath);
}

// ==================== 缓存管理 ====================

void ComponentCacheService::removeCache(const QString& lcscId) {
    ComponentCacheMaintenance maintenance(*this);
    maintenance.remove(lcscId);
}

// 清除指定元器件的旧请求屏蔽标记。
void ComponentCacheService::clearTombstone(const QString& lcscId) {
    m_tombstones.clearComponent(lcscId);
}

// 清除全局旧请求屏蔽标记。
void ComponentCacheService::clearGlobalTombstone() {
    m_tombstones.clearGlobal();
}

// 判断元器件是否仍被旧请求屏蔽。
bool ComponentCacheService::isTombstoned(const QString& lcscId) const {
    return m_tombstones.isBlocked(lcscId);
}

// 清空一级和二级缓存，并使旧异步写入失效。
void ComponentCacheService::clearAllCache() {
    ComponentCacheMaintenance maintenance(*this);
    maintenance.clearAll();
}

// 清空一级内存缓存并使清理前创建的异步写入失效。
void ComponentCacheService::clearMemoryCache() {
    ComponentCacheMaintenance maintenance(*this);
    maintenance.clearMemory();
}

// 枚举当前缓存目录中具有有效元数据的元器件编号。
QStringList ComponentCacheService::getCachedComponentIds() const {
    return ComponentCacheMaintenance::cachedComponentIds(*this);
}

// 统计当前缓存目录的磁盘占用大小。
qint64 ComponentCacheService::getCacheSize() const {
    return ComponentCacheMaintenance::cacheSize(*this);
}

// 返回一级内存缓存的当前占用大小。
qint64 ComponentCacheService::getMemoryCacheSize() const {
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
    qint64 newSize = 0;
    {
        QMutexLocker diskLocker(&m_diskWriteMutex);
        CachePruner pruner(cacheDir());
        newSize = pruner.pruneTo(targetSizeBytes);
    }
    emit cacheSizeChanged(newSize);
}

// 设置一级内存缓存的最大容量。
void ComponentCacheService::setMemoryCacheLimit(int maxSizeMB) {
    m_memoryCache.setMaxCost(maxSizeMB * 1024 * 1024);
    LOG_DEBUG(LogModule::Core, "Memory cache limit set to: {} MB", maxSizeMB);
}

// 获取一级内存缓存的最大容量。
int ComponentCacheService::memoryCacheLimit() const {
    return m_memoryCache.maxCost() / (1024 * 1024);
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
    ComponentCacheQuotaEnforcer quotaEnforcer(*this);
    quotaEnforcer.enforce(bypassCooldown);
}

// 从二级磁盘缓存读取元器件元数据。
QJsonObject ComponentCacheService::loadMetadata(const QString& lcscId) const {
    // 外部元数据读取必须与缓存目录迁移串行化。
    QMutexLocker diskLocker(&m_diskWriteMutex);
    return CacheMetadataStore::read(metadataPath(lcscId));
}

// 将元器件元数据原子写入二级磁盘缓存。
void ComponentCacheService::saveMetadata(const QString& lcscId, const QJsonObject& metadata) {
    if (ensureComponentDir(lcscId).isEmpty()) {
        return;
    }
    QString metaPath = metadataPath(lcscId);
    QJsonDocument doc(metadata);
    if (!CacheMetadataStore::writeAtomically(metaPath, doc.toJson(QJsonDocument::Indented))) {
        LOG_WARN(LogModule::Core, "Failed to open metadata file for writing: {}", metaPath);
    }
}

// 根据格式和现有文件解析数据手册的实际路径。
QString ComponentCacheService::resolveDatasheetPath(const QString& lcscId,
                                                    const QString& preferredFormat,
                                                    bool forWrite) const {
    const QString basePath = datasheetPath(lcscId);
    const QString normalizedFormat = preferredFormat.toLower();

    if (normalizedFormat.contains("pdf")) {
        if (!forWrite && !QFileInfo::exists(basePath + ".pdf") && QFileInfo::exists(basePath + ".html")) {
            return basePath + ".html";
        }
        return basePath + ".pdf";
    }
    if (normalizedFormat.contains("html")) {
        if (!forWrite && !QFileInfo::exists(basePath + ".html") && QFileInfo::exists(basePath + ".pdf")) {
            return basePath + ".pdf";
        }
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
    return CachePathResolver::metadataPath(cacheDir(), lcscId);
}

// 构造元器件预览图文件路径。
QString ComponentCacheService::previewImagePath(const QString& lcscId, int index) const {
    return CachePathResolver::previewImagePath(cacheDir(), lcscId, index);
}

// 构造元器件数据手册基础路径。
QString ComponentCacheService::datasheetPath(const QString& lcscId) const {
    return CachePathResolver::datasheetPath(cacheDir(), lcscId);
}

// 根据经过校验的模型标识构造三维模型缓存路径。
QString ComponentCacheService::model3DPath(const QString& uuid, const QString& extension) const {
    const QString path = CachePathResolver::model3DPath(cacheDir(), uuid, extension);
    if (path.isEmpty()) {
        qWarning() << "model3DPath: invalid uuid or extension, rejecting:" << uuid << extension;
    }
    return path;
}

}  // namespace EasyKiConverter

// 我攻略了你之后，别人也可以攻略你[惊讶]？！
// 不是这啥情况啊[疑惑]？我记得旮旯game不是单机游戏吗，
// 什么时候联网的[思考]？而就算你是玩家[给心心]！
// 我是这个攻略对象，也不能这样子吧[傲娇]，
// 是不是有点不道德啊[生气]？他是黑客[doge]。
// 什么黑客[思考]哪里黑了[大哭]！？哦还是国际服，
// 我不玩了[灵魂出窍]。
