#include "ComponentCacheReadCoordinator.h"

#include "CacheComponentDataReader.h"
#include "CacheDataValidator.h"
#include "CacheFileLayout.h"
#include "CacheMetadataStore.h"
#include "ComponentCacheBinaryFileStore.h"
#include "ComponentCacheService.h"
#include "utils/logging/LogMacros.h"

#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>

namespace EasyKiConverter {

namespace {

/** @brief 从磁盘读取并校验 CAD 原始缓存文件。 */
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

/** @brief 在目录迁移锁内校验元数据身份、三维模型和 CAD 文件。 */
bool ComponentCacheReadCoordinator::isCacheValid(const ComponentCacheService& owner, const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    const QJsonObject metadata = CacheMetadataStore::read(owner.metadataPath(componentId));
    if (metadata.isEmpty()) {
        return false;
    }

    if (metadata.contains(QStringLiteral("lcscId")) &&
        (!metadata.value(QStringLiteral("lcscId")).isString() ||
         metadata.value(QStringLiteral("lcscId")).toString().compare(componentId, Qt::CaseInsensitive) != 0)) {
        return false;
    }

    if (!CacheMetadataStore::hasValidModel3D(metadata)) {
        return false;
    }

    const bool hasCadJson = hasValidCadDataFile(CacheFileLayout::cadDataFile(owner.componentCacheDir(componentId)));
    const bool hasBasicIdentity = !metadata.value(QStringLiteral("lcscId")).toString().isEmpty() ||
                                  !metadata.value(QStringLiteral("name")).toString().isEmpty();
    return hasCadJson || hasBasicIdentity;
}

/** @brief 在磁盘读取锁和内存缓存锁内解析完整元器件数据。 */
QSharedPointer<ComponentData> ComponentCacheReadCoordinator::loadComponentData(const ComponentCacheService& owner,
                                                                               const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    const QString metaPath = owner.metadataPath(componentId);
    if (!QFileInfo::exists(metaPath)) {
        return nullptr;
    }

    QMutexLocker locker(&owner.m_mutex);
    const QJsonObject metadata = CacheMetadataStore::read(metaPath);
    if (metadata.isEmpty()) {
        return nullptr;
    }

    if (!CacheMetadataStore::hasValidModel3D(metadata)) {
        LOG_WARN(LogModule::Core, "Rejected component cache with invalid 3D metadata: {}", componentId);
        return nullptr;
    }

    const auto componentData = CacheComponentDataReader::read(componentId, metadata);
    if (!componentData) {
        return nullptr;
    }

    LOG_DEBUG(LogModule::Core, "Loaded component data from disk cache: {}", componentId);
    return componentData;
}

/** @brief 在目录迁移锁内验证 CAD 原始 JSON 是否可解析。 */
bool ComponentCacheReadCoordinator::hasSymbolFootprintCache(const ComponentCacheService& owner,
                                                            const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    const QString cadDataPath = CacheFileLayout::cadDataFile(owner.componentCacheDir(componentId));
    const bool hasMemoryMetadata = owner.m_memoryCache.containsMetadata(componentId);
    const bool valid = hasValidCadDataFile(cadDataPath);
    if (hasMemoryMetadata) {
        LOG_DEBUG(LogModule::Core,
                  "hasSymbolFootprintCache: memory hit for {}, cad_data.json exists: {}",
                  componentId,
                  valid);
    } else if (!valid) {
        LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: no cad_data.json for {}", componentId);
    } else {
        LOG_DEBUG(LogModule::Core, "hasSymbolFootprintCache: valid CAD data for {}", componentId);
    }
    return valid;
}

}  // namespace EasyKiConverter
