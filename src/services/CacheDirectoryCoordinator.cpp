#include "CacheDirectoryCoordinator.h"

#include "CacheDirectoryMigrator.h"
#include "ComponentCacheService.h"
#include "utils/logging/LogMacros.h"

#include <QDir>
#include <QMutexLocker>

namespace EasyKiConverter {

/** @brief 保存缓存服务引用。 */
CacheDirectoryCoordinator::CacheDirectoryCoordinator(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 在统一锁边界内完成缓存目录切换。
 * @details 切换目录会使旧代次写入失效，并清空没有目录归属信息的一级缓存。
 */
void CacheDirectoryCoordinator::setDirectory(const QString& cacheDir, bool migrateExistingCache) {
    const QString newCacheDir = QDir::cleanPath(cacheDir);
    QString oldCacheDir;
    {
        QMutexLocker locker(&m_owner.m_cacheDirMutex);
        oldCacheDir = m_owner.m_cacheDir;
    }

    const bool cacheDirChanged = oldCacheDir != newCacheDir;
    {
        QMutexLocker diskLocker(&m_owner.m_diskWriteMutex);
        if (cacheDirChanged) {
            // 先使切换前排队的异步写入失效，再进行目录迁移，避免旧请求污染新目录。
            m_owner.m_cacheGeneration.fetch_add(1);
            m_owner.m_tombstones.reset();
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
            QMutexLocker locker(&m_owner.m_cacheDirMutex);
            m_owner.m_cacheDir = newCacheDir;
        }
        if (cacheDirChanged) {
            // L1 数据没有目录归属信息，切换 L2 目录后必须全部失效，避免
            // 同一元件 ID 从旧目录泄漏到新目录。
            m_owner.m_memoryCache.clear();
        }
    }
    if (cacheDirChanged) {
        emit m_owner.memoryCacheSizeChanged(0);
    }

    m_owner.selfHealCache();
    LOG_DEBUG(LogModule::Core, "Cache directory set to: {}", newCacheDir);
}

}  // namespace EasyKiConverter
