#include "ComponentDataMemoryStore.h"

#include <QMutexLocker>

namespace EasyKiConverter {

// 在线程安全地判断组件数据是否已缓存。
bool ComponentDataMemoryStore::contains(const QString& normalizedId) const {
    QMutexLocker locker(&m_mutex);
    return m_data.contains(normalizedId);
}

// 返回组件数据副本，避免暴露容器内部引用。
ComponentData ComponentDataMemoryStore::value(const QString& normalizedId) const {
    QMutexLocker locker(&m_mutex);
    return m_data.value(normalizedId, ComponentData());
}

// 写入或覆盖指定组件的数据。
void ComponentDataMemoryStore::set(const QString& normalizedId, const ComponentData& data) {
    QMutexLocker locker(&m_mutex);
    m_data[normalizedId] = data;
}

// 在缓存项存在时原子替换其数据。
bool ComponentDataMemoryStore::replaceIfPresent(const QString& normalizedId, const ComponentData& data) {
    QMutexLocker locker(&m_mutex);
    auto it = m_data.find(normalizedId);
    if (it == m_data.end()) {
        return false;
    }
    it.value() = data;
    return true;
}

// 更新符号和封装中的描述字段。
bool ComponentDataMemoryStore::updateDescription(const QString& normalizedId, const QString& description) {
    QMutexLocker locker(&m_mutex);
    auto it = m_data.find(normalizedId);
    if (it == m_data.end()) {
        return false;
    }

    if (it->symbolData()) {
        SymbolInfo info = it->symbolData()->info();
        info.description = description;
        it->symbolData()->setInfo(info);
    }
    if (it->footprintData()) {
        FootprintInfo info = it->footprintData()->info();
        info.description = description;
        it->footprintData()->setInfo(info);
    }
    return true;
}

// 清空组件服务的全部内存缓存。
void ComponentDataMemoryStore::clear() {
    QMutexLocker locker(&m_mutex);
    m_data.clear();
}

// 只删除无效组件缓存，保留仍可复用的有效数据。
void ComponentDataMemoryStore::removeIfInvalid(const QString& normalizedId) {
    QMutexLocker locker(&m_mutex);
    const auto it = m_data.find(normalizedId);
    if (it != m_data.end() && !it.value().isValid()) {
        m_data.erase(it);
    }
}

}  // namespace EasyKiConverter
