#include "ComponentCacheMetadataWriter.h"

#include "CacheMetadataStore.h"
#include "ComponentCacheService.h"
#include "ComponentCacheWritePolicy.h"
#include "utils/logging/LogMacros.h"

#include <QJsonDocument>
#include <QMutexLocker>

namespace EasyKiConverter {

ComponentCacheMetadataWriter::ComponentCacheMetadataWriter(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 在统一写锁内完成元数据快照合并、内存更新和磁盘持久化
 */
std::optional<qint64> ComponentCacheMetadataWriter::write(const QString& componentId,
                                                          const ComponentData& data,
                                                          uint64_t expectedGeneration,
                                                          bool replaceModel3DMetadata) {
    QJsonObject metadata = CacheMetadataStore::build(componentId, data);
    const QString key = m_owner.makeMemoryKey(componentId, QStringLiteral("metadata"));
    qint64 sizeAfterUpdate = 0;

    // 旧快照读取、代次校验、合并和双层提交必须保持原子顺序，避免并发更新互相覆盖。
    {
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        if (!ComponentCacheWritePolicy::isAllowed(
                m_owner.m_cacheGeneration.load(), expectedGeneration, m_owner.m_tombstones, componentId)) {
            LOG_DEBUG(LogModule::Core, "Discarded stale metadata write for {}", componentId);
            return std::nullopt;
        }

        const QJsonObject existingMetadata = CacheMetadataStore::read(m_owner.metadataPath(componentId));
        metadata = CacheMetadataStore::merge(existingMetadata, metadata);
        if (replaceModel3DMetadata && !CacheMetadataStore::hasModel3D(data)) {
            metadata.remove(QStringLiteral("model3duuid"));
            metadata.remove(QStringLiteral("model3dName"));
            metadata.remove(QStringLiteral("model3dTranslation"));
            metadata.remove(QStringLiteral("model3dRotation"));
        }

        sizeAfterUpdate = m_owner.m_memoryCache.insert(key, QJsonDocument(metadata).toJson(QJsonDocument::Compact));
        m_owner.saveMetadata(componentId, metadata);
    }

    return sizeAfterUpdate;
}

}  // namespace EasyKiConverter
