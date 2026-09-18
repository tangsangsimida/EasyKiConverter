#include "CacheTombstoneRegistry.h"

#include <QMutexLocker>

namespace EasyKiConverter {

// 清空组件级和全局屏蔽状态。
void CacheTombstoneRegistry::reset() {
    QMutexLocker locker(&m_mutex);
    m_components.clear();
    m_allBlocked = false;
}

// 记录指定组件的删除屏蔽状态。
void CacheTombstoneRegistry::blockComponent(const QString& componentId) {
    QMutexLocker locker(&m_mutex);
    m_components.insert(componentId.toUpper());
}

// 允许指定组件接受新一轮异步写入。
void CacheTombstoneRegistry::clearComponent(const QString& componentId) {
    QMutexLocker locker(&m_mutex);
    m_components.remove(componentId.toUpper());
}

// 记录全局缓存清理屏蔽状态。
void CacheTombstoneRegistry::blockAll() {
    QMutexLocker locker(&m_mutex);
    m_allBlocked = true;
}

// 解除全局屏蔽但不影响组件级删除屏蔽。
void CacheTombstoneRegistry::clearGlobal() {
    QMutexLocker locker(&m_mutex);
    m_allBlocked = false;
}

// 在同一把锁下读取全局和组件级屏蔽状态。
bool CacheTombstoneRegistry::isBlocked(const QString& componentId) const {
    QMutexLocker locker(&m_mutex);
    return m_allBlocked || m_components.contains(componentId.toUpper());
}

}  // namespace EasyKiConverter
