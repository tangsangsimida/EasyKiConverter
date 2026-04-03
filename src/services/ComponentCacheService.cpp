#include "ComponentCacheService.h"

#include "utils/logging/LogMacros.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace EasyKiConverter {

ComponentCacheService* ComponentCacheService::s_instance = nullptr;

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
        s_instance = new ComponentCacheService();
    }
    return s_instance;
}

void ComponentCacheService::setCacheDir(const QString& cacheDir) {
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
    // 文件存在时，再用锁保护读取操作
    QMutexLocker locker(&m_mutex);
    return QFileInfo::exists(metaPath);
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
        urlList.append(val.toString());
    }
    componentData->setPreviewImages(urlList);

    // 注意：预览图数据不再加载到 ComponentData，只保留 URL
    // 预览图文件通过 loadPreviewImage 直接读取

    // 3D模型UUID
    if (metadata.contains("model3duuid")) {
        auto model3DData = QSharedPointer<Model3DData>::create();
        model3DData->setUuid(metadata.value("model3duuid").toString());
        componentData->setModel3DData(model3DData);
    }

    LOG_DEBUG(LogModule::Core, "Loaded component data from disk cache: {}", lcscId);
    return componentData;
}

void ComponentCacheService::saveComponentMetadata(const QString& componentId, const ComponentData& data) {
    // 构建元数据JSON（在锁外构建）
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

    // 预览图URL列表
    QJsonArray previewUrls;
    for (const QString& url : data.previewImages()) {
        previewUrls.append(url);
    }
    metadata["previewImages"] = previewUrls;

    // 3D模型UUID
    if (data.model3DData() && !data.model3DData()->uuid().isEmpty()) {
        metadata["model3duuid"] = data.model3DData()->uuid();
    }

    // 先原子性地更新内存缓存（内存优先，失败则不回写磁盘）
    QJsonDocument doc(metadata);
    QString key = makeMemoryKey(componentId, "metadata");
    QByteArray* newData = new QByteArray(doc.toJson(QJsonDocument::Compact));

    qint64 sizeAfterUpdate = 0;
    {
        QMutexLocker locker(&m_mutex);

        // 直接插入即可，QCache 会自动处理相同 key 的旧数据的删除
        m_memoryCache.insert(key, newData, newData->size());
        sizeAfterUpdate = m_memoryCache.totalCost();
    }

    // 内存更新成功后保存到磁盘（磁盘失败不影响内存缓存一致性）
    saveMetadata(componentId, metadata);

    // 在锁外发送信号
    emit memoryCacheSizeChanged(sizeAfterUpdate);
    emit cacheSaved(componentId);
    LOG_DEBUG(LogModule::Core, "Saved component metadata to cache: {}", componentId);
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

    QFile file(symbolPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
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

    QFile file(footprintPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
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

    QFile file(cadDataPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(cadData);
        file.close();
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

QByteArray ComponentCacheService::loadPreviewImage(const QString& lcscId, int imageIndex) const {
    QMutexLocker locker(&m_mutex);

    if (imageIndex < 0 || imageIndex >= 3) {
        return QByteArray();
    }

    QString previewPath = previewImagePath(lcscId, imageIndex);
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
    qDebug() << "[Cache] savePreviewImage START - lcscId:" << lcscId << "index:" << imageIndex
             << "dataSize:" << imageData.size();

    if (imageIndex < 0 || imageIndex >= 3 || imageData.isEmpty()) {
        qDebug() << "[Cache] savePreviewImage: Early return due to invalid params";
        return;
    }

    qDebug() << "[Cache] Step 1: About to acquire mutex...";
    QString previewPath;
    {
        QMutexLocker locker(&m_mutex);
        qDebug() << "[Cache] Step 2: Mutex acquired, calling ensureComponentDir...";
        ensureComponentDir(lcscId);
        qDebug() << "[Cache] Step 3: ensureComponentDir done, getting previewPath...";
        previewPath = previewImagePath(lcscId, imageIndex);
        qDebug() << "[Cache] Step 4: previewPath:" << previewPath;
    }
    qDebug() << "[Cache] Step 5: Mutex released, previewPath:" << previewPath;

    qDebug() << "[Cache] Step 6: About to open file for writing...";
    QFile file(previewPath);
    if (file.open(QIODevice::WriteOnly)) {
        qDebug() << "[Cache] Step 7: File opened, writing data...";
        file.write(imageData);
        file.close();
        qDebug() << "[Cache] Step 8: File written successfully";
        LOG_DEBUG(LogModule::Core, "Saved preview image to disk: {}", previewPath);
    } else {
        qDebug() << "[Cache] Step 7 ERROR: Failed to open file for writing";
        LOG_WARN(LogModule::Core, "Failed to write preview image: {}", previewPath);
    }
    qDebug() << "[Cache] savePreviewImage END";
    // 注意：预览图不存入L1内存缓存，因为数据量大
}

QByteArray ComponentCacheService::loadDatasheet(const QString& lcscId) const {
    QMutexLocker locker(&m_mutex);

    QString datasheetFilePath = datasheetPath(lcscId);
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
        QString datasheetFilePath = datasheetPath(lcscId);

        // 根据格式设置文件扩展名
        QString ext = (format.toLower().contains("pdf")) ? ".pdf" : ".html";
        actualPath = datasheetFilePath;
        if (!actualPath.endsWith(ext)) {
            actualPath += ext;
        }
    }

    QFile file(actualPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(datasheetData);
        file.close();
        LOG_DEBUG(LogModule::Core, "Saved datasheet to disk: {}", actualPath);
    } else {
        LOG_WARN(LogModule::Core, "Failed to write datasheet: {}", actualPath);
    }
    // 注意：数据手册不存入L1内存缓存，因为数据量大
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

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
        LOG_DEBUG(LogModule::Core, "Saved 3D model to disk: {}", path);
    } else {
        LOG_WARN(LogModule::Core, "Failed to write 3D model: {}", path);
    }
    // 注意：3D模型数据量大，不存入L1内存缓存
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
                    QDir subDirToRemove(m_cacheDir + "/" + subDir);
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
    // 第一阶段：锁外收集所有缓存目录的信息
    QList<QPair<QString, QDateTime>> cacheList;
    qint64 currentSize = 0;

    {
        QDir dir(m_cacheDir);
        for (const QString& subDir : dir.entryList(QDir::Dirs)) {
            if (subDir == "." || subDir == ".." || subDir == "model3d") {
                continue;
            }
            QString dirPath = QDir::cleanPath(m_cacheDir + "/" + subDir);
            QFileInfo info(dirPath);
            if (info.exists()) {
                cacheList.append(qMakePair(subDir, info.lastModified()));
                currentSize += calculateDirSize(dirPath);
            }
        }
    }

    if (currentSize <= targetSizeBytes) {
        return;
    }

    // 按访问时间排序（最老的在前）
    std::sort(
        cacheList.begin(), cacheList.end(), [](const QPair<QString, QDateTime>& a, const QPair<QString, QDateTime>& b) {
            return a.second < b.second;
        });

    // 第二阶段：锁内执行删除操作
    {
        QMutexLocker locker(&m_mutex);

        // 删除最老的缓存直到达到目标大小
        for (const auto& pair : cacheList) {
            if (currentSize <= targetSizeBytes) {
                break;
            }

            QString dirPath = QDir::cleanPath(m_cacheDir + "/" + pair.first);
            qint64 dirSize = calculateDirSize(dirPath);

            QDir d;
            if (d.exists(dirPath)) {
                d.removeRecursively();
                currentSize -= dirSize;
                LOG_DEBUG(LogModule::Core, "Pruned cache: {} size: {}", pair.first, dirSize);
            }
        }
    }  // 锁在此处释放

    // 在锁外发送信号，避免死锁
    emit cacheSizeChanged(currentSize);
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

    QFile file(metaPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LOG_WARN(LogModule::Core, "Failed to open metadata file for writing: {}", metaPath);
        return;
    }

    QJsonDocument doc(metadata);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
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
