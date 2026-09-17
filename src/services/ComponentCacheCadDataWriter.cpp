#include "ComponentCacheCadDataWriter.h"

#include "CacheDataValidator.h"
#include "CacheFileLayout.h"
#include "CacheMetadataStore.h"
#include "ComponentCacheService.h"
#include "ComponentCacheWritePolicy.h"
#include "utils/logging/LogMacros.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/** @brief 保存协作者所属的元器件缓存服务。 */
ComponentCacheCadDataWriter::ComponentCacheCadDataWriter(ComponentCacheService& owner) : m_owner(owner) {}

/** @brief 写入符号 CAD 数据并同步一级内存缓存。 */
void ComponentCacheCadDataWriter::writeSymbol(const QString& componentId,
                                              const QByteArray& data,
                                              uint64_t expectedGeneration) {
    writeCadFile(componentId, data, expectedGeneration, DataKind::Symbol);
}

/** @brief 写入封装 CAD 数据并同步一级内存缓存。 */
void ComponentCacheCadDataWriter::writeFootprint(const QString& componentId,
                                                 const QByteArray& data,
                                                 uint64_t expectedGeneration) {
    writeCadFile(componentId, data, expectedGeneration, DataKind::Footprint);
}

/** @brief 写入原始 CAD JSON 数据。 */
void ComponentCacheCadDataWriter::writeCadJson(const QString& componentId,
                                               const QByteArray& data,
                                               uint64_t expectedGeneration) {
    writeCadFile(componentId, data, expectedGeneration, DataKind::CadJson);
}

/**
 * @brief 执行 CAD 文件共用写入流程。
 * @details 保持磁盘写锁覆盖代次校验、目录创建和原子替换，避免目录迁移或清理并发破坏缓存。
 */
void ComponentCacheCadDataWriter::writeCadFile(const QString& componentId,
                                               const QByteArray& data,
                                               uint64_t expectedGeneration,
                                               DataKind kind) {
    if (!CacheDataValidator::isValidCadData(data))
        return;

    QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
    if (!ComponentCacheWritePolicy::isAllowed(
            m_owner.m_cacheGeneration.load(), expectedGeneration, m_owner.m_tombstones, componentId)) {
        return;
    }

    QString path;
    {
        QMutexLocker locker(&m_owner.m_mutex);
        const QString directory = m_owner.ensureComponentDir(componentId);
        if (directory.isEmpty())
            return;
        if (kind == DataKind::Symbol)
            path = CacheFileLayout::symbolFile(directory);
        else if (kind == DataKind::Footprint)
            path = CacheFileLayout::footprintFile(directory);
        else
            path = CacheFileLayout::cadDataFile(directory);
    }

    const bool written = CacheMetadataStore::writeAtomically(path, data);
    const QString kindName = kind == DataKind::Symbol      ? QStringLiteral("symbol")
                             : kind == DataKind::Footprint ? QStringLiteral("footprint")
                                                           : QStringLiteral("CAD data JSON");
    if (written) {
        LOG_DEBUG(LogModule::Core, "Saved {} to disk: {}", kindName, path);
        m_owner.enforceDiskCacheLimit();
    } else {
        LOG_WARN(LogModule::Core, "Failed to write {}: {}", kindName, path);
    }

    if (kind == DataKind::Symbol)
        m_owner.saveSymbolDataToMemory(componentId, data);
    else if (kind == DataKind::Footprint)
        m_owner.saveFootprintDataToMemory(componentId, data);
}

}  // namespace EasyKiConverter
