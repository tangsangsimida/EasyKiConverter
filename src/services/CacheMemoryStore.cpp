#include "CacheMemoryStore.h"

#include <QMutexLocker>

namespace EasyKiConverter {

// 初始化 QCache 的最大成本，成本由调用方按数据字节数传入。
CacheMemoryStore::CacheMemoryStore(int maxCost) {
    m_cache.setMaxCost(maxCost);
}

// 清空 LRU 缓存并释放其拥有的数据。
void CacheMemoryStore::clear() {
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
}

// 在线程安全地读取键是否存在。
bool CacheMemoryStore::contains(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    return m_cache.contains(key);
}

// 返回缓存数据副本，避免把 QCache 内部指针暴露给调用方。
QByteArray CacheMemoryStore::value(const QString& key) const {
    QMutexLocker locker(&m_mutex);
    const QByteArray* data = m_cache.object(key);
    return data ? *data : QByteArray();
}

// 写入数据并返回写入后的成本统计。
qint64 CacheMemoryStore::insert(const QString& key, const QByteArray& data) {
    QMutexLocker locker(&m_mutex);
    m_cache.insert(key, new QByteArray(data), data.size());
    return m_cache.totalCost();
}

// 删除键集合并返回删除后的成本统计。
qint64 CacheMemoryStore::remove(const QStringList& keys) {
    QMutexLocker locker(&m_mutex);
    for (const QString& key : keys) {
        delete m_cache.take(key);
    }
    return m_cache.totalCost();
}

// 读取当前缓存占用成本。
qint64 CacheMemoryStore::totalCost() const {
    QMutexLocker locker(&m_mutex);
    return m_cache.totalCost();
}

// 更新最大成本，QCache 会自动淘汰超出限制的旧数据。
void CacheMemoryStore::setMaxCost(int maxCost) {
    QMutexLocker locker(&m_mutex);
    m_cache.setMaxCost(maxCost);
}

// 返回当前最大成本限制。
int CacheMemoryStore::maxCost() const {
    QMutexLocker locker(&m_mutex);
    return m_cache.maxCost();
}

}  // namespace EasyKiConverter
