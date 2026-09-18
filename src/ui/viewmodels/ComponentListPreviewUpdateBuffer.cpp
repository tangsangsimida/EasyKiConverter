#include "ComponentListPreviewUpdateBuffer.h"

namespace EasyKiConverter {

/** @brief 在锁内保存一个组件的完整预览图列表。 */
void ComponentListPreviewUpdateBuffer::addComplete(const QString& componentId, const QStringList& encodedImages) {
    QMutexLocker locker(&m_mutex);
    m_completeImages.insert(componentId, encodedImages);
}

/** @brief 在锁内保存一个组件的单张预览图并覆盖同索引旧值。 */
void ComponentListPreviewUpdateBuffer::addIncremental(const QString& componentId,
                                                      int imageIndex,
                                                      const QString& encodedImage) {
    QMutexLocker locker(&m_mutex);
    m_incrementalImages[componentId][imageIndex] = encodedImage;
}

/** @brief 在同一锁区间内转移两个映射，避免 UI 更新期间丢失后台回调。 */
ComponentListPreviewUpdateBuffer::Snapshot ComponentListPreviewUpdateBuffer::take() {
    QMutexLocker locker(&m_mutex);
    Snapshot snapshot;
    snapshot.completeImages = std::move(m_completeImages);
    snapshot.incrementalImages = std::move(m_incrementalImages);
    return snapshot;
}

/** @brief 在锁内清空所有完整和增量预览图更新。 */
void ComponentListPreviewUpdateBuffer::clear() {
    QMutexLocker locker(&m_mutex);
    m_completeImages.clear();
    m_incrementalImages.clear();
}

}  // namespace EasyKiConverter
