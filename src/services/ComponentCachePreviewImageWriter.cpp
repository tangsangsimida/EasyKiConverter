#include "ComponentCachePreviewImageWriter.h"

#include "CacheDataValidator.h"
#include "CacheFileLayout.h"
#include "CacheMetadataStore.h"
#include "ComponentCacheService.h"
#include "ComponentCacheWritePolicy.h"
#include "utils/logging/LogMacros.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/** @brief 保存协作者所属的元器件缓存服务。 */
ComponentCachePreviewImageWriter::ComponentCachePreviewImageWriter(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 在统一磁盘写锁内完成预览图校验、路径准备和原子替换。
 * @details 预览图数据量较大，只写入二级缓存，不同步到一级内存缓存。
 */
void ComponentCachePreviewImageWriter::write(const QString& componentId,
                                             const QByteArray& imageData,
                                             int imageIndex,
                                             uint64_t expectedGeneration) {
    if (imageIndex < 0 || imageIndex >= 3 || !CacheDataValidator::isValidPreviewImage(imageData)) {
        return;
    }

    QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
    if (!ComponentCacheWritePolicy::isAllowed(
            m_owner.m_cacheGeneration.load(), expectedGeneration, m_owner.m_tombstones, componentId)) {
        return;
    }

    QString path;
    {
        QMutexLocker locker(&m_owner.m_mutex);
        if (m_owner.ensureComponentDir(componentId).isEmpty()) {
            return;
        }
        path = m_owner.previewImagePath(componentId, imageIndex);
    }

    if (CacheMetadataStore::writeAtomically(path, imageData)) {
        LOG_DEBUG(LogModule::Core, "Saved preview image to disk: {}", path);
        m_owner.enforceDiskCacheLimit();
    } else {
        LOG_WARN(LogModule::Core, "Failed to write preview image: {}", path);
    }
}

}  // namespace EasyKiConverter
