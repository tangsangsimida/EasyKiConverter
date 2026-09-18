#include "ComponentCacheFileReadCoordinator.h"

#include "CacheFileLayout.h"
#include "CacheMetadataStore.h"
#include "ComponentCacheBinaryFileStore.h"
#include "ComponentCacheService.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/** @brief 先查询一级缓存，再在目录迁移锁内读取符号文件。 */
QByteArray ComponentCacheFileReadCoordinator::loadSymbolData(const ComponentCacheService& owner,
                                                             const QString& componentId) {
    const QByteArray memoryData = owner.loadSymbolDataFromMemory(componentId);
    if (!memoryData.isEmpty()) {
        return memoryData;
    }

    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    const QString path = CacheFileLayout::symbolFile(owner.componentCacheDir(componentId));
    return ComponentCacheBinaryFileStore::readCadData(path);
}

/** @brief 先查询一级缓存，再在目录迁移锁内读取封装文件。 */
QByteArray ComponentCacheFileReadCoordinator::loadFootprintData(const ComponentCacheService& owner,
                                                                const QString& componentId) {
    const QByteArray memoryData = owner.loadFootprintDataFromMemory(componentId);
    if (!memoryData.isEmpty()) {
        return memoryData;
    }

    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    const QString path = CacheFileLayout::footprintFile(owner.componentCacheDir(componentId));
    return ComponentCacheBinaryFileStore::readCadData(path);
}

/** @brief 在目录迁移锁内读取并校验原始 CAD JSON 文件。 */
QByteArray ComponentCacheFileReadCoordinator::loadCadDataJson(const ComponentCacheService& owner,
                                                              const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    const QString path = CacheFileLayout::cadDataFile(owner.componentCacheDir(componentId));
    return ComponentCacheBinaryFileStore::readCadData(path);
}

/** @brief 在目录迁移锁内读取并校验指定预览图文件。 */
QByteArray ComponentCacheFileReadCoordinator::loadPreviewImage(const ComponentCacheService& owner,
                                                               const QString& componentId,
                                                               int imageIndex) {
    if (imageIndex < 0 || imageIndex >= 3) {
        return QByteArray();
    }

    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    QString path;
    {
        QMutexLocker locker(&owner.m_mutex);
        path = owner.previewImagePath(componentId, imageIndex);
    }
    return ComponentCacheBinaryFileStore::readPreviewImage(path);
}

/** @brief 在目录迁移锁内读取并解析元数据 JSON。 */
QJsonObject ComponentCacheFileReadCoordinator::loadMetadata(const ComponentCacheService& owner,
                                                            const QString& componentId) {
    QMutexLocker diskLocker(&owner.m_diskWriteMutex);
    return CacheMetadataStore::read(owner.metadataPath(componentId));
}

}  // namespace EasyKiConverter
