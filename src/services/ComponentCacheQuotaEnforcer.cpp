#include "ComponentCacheQuotaEnforcer.h"

#include "CachePruner.h"
#include "ComponentCacheService.h"

#include <QMutexLocker>

namespace EasyKiConverter {

/** @brief 保存协作者所属的元器件缓存服务。 */
ComponentCacheQuotaEnforcer::ComponentCacheQuotaEnforcer(ComponentCacheService& owner) : m_owner(owner) {}

/**
 * @brief 在冷却策略允许时扫描并清理二级磁盘缓存。
 * @details 读取配置和重置计时器保持在同一锁区间内，实际目录扫描在锁外执行。
 */
void ComponentCacheQuotaEnforcer::enforce(bool bypassCooldown) {
    constexpr qint64 kCooldownMs = 3000;
    QString cacheDir;
    qint64 targetSizeBytes = 0;
    {
        QMutexLocker locker(&m_owner.m_mutex);
        if (!bypassCooldown && m_owner.m_lastEnforceTimer.elapsed() < kCooldownMs) {
            return;
        }
        cacheDir = m_owner.cacheDir();
        targetSizeBytes = static_cast<qint64>(m_owner.m_diskCacheLimitMB) * 1024 * 1024;
        m_owner.m_lastEnforceTimer.restart();
    }

    if (targetSizeBytes <= 0 || cacheDir.isEmpty()) {
        return;
    }

    CachePruner pruner(cacheDir);
    const qint64 newSize = pruner.pruneTo(targetSizeBytes);
    emit m_owner.cacheSizeChanged(newSize);
}

}  // namespace EasyKiConverter
