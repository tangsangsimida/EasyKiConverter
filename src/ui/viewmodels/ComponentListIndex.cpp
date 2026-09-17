#include "ComponentListIndex.h"

namespace EasyKiConverter {

// 判断元件编号是否已经映射到列表位置。
bool ComponentListIndex::contains(const QString& componentId) const {
    return m_indices.contains(componentId);
}

// 返回元件编号对应的位置，未找到时使用统一的无效索引值。
int ComponentListIndex::indexOf(const QString& componentId) const {
    return m_indices.value(componentId, -1);
}

// 写入元件编号和列表位置的映射。
void ComponentListIndex::insert(const QString& componentId, int index) {
    m_indices.insert(componentId, index);
}

// 删除指定元件编号的映射。
void ComponentListIndex::remove(const QString& componentId) {
    m_indices.remove(componentId);
}

// 清空列表索引。
void ComponentListIndex::clear() {
    m_indices.clear();
}

// 按元件列表的当前顺序重建所有编号映射。
void ComponentListIndex::rebuild(const QList<ComponentListItemData*>& items) {
    m_indices.clear();
    for (int index = 0; index < items.count(); ++index) {
        const ComponentListItemData* item = items.at(index);
        if (item) {
            m_indices.insert(item->componentId(), index);
        }
    }
}

// 返回当前索引的条目数。
int ComponentListIndex::size() const {
    return m_indices.size();
}

}  // namespace EasyKiConverter
