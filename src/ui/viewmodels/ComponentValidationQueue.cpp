#include "ComponentValidationQueue.h"

namespace EasyKiConverter {

/** @brief 将尚未出现的元件编号加入待验证队列。 */
bool ComponentValidationQueue::enqueue(const QString& componentId) {
    if (componentId.isEmpty() || contains(componentId)) {
        return false;
    }
    m_pendingIds.append(componentId);
    return true;
}

/** @brief 判断元件是否已在待验证或飞行中集合。 */
bool ComponentValidationQueue::contains(const QString& componentId) const {
    return m_pendingIds.contains(componentId) || m_inFlightIds.contains(componentId);
}

/** @brief 取出队首请求并转移到飞行中集合。 */
QString ComponentValidationQueue::takeNext() {
    if (m_pendingIds.isEmpty()) {
        return QString();
    }
    const QString componentId = m_pendingIds.takeFirst();
    m_inFlightIds.insert(componentId);
    return componentId;
}

/** @brief 同时移除待处理和飞行中的指定请求。 */
void ComponentValidationQueue::remove(const QString& componentId) {
    m_pendingIds.removeAll(componentId);
    m_inFlightIds.remove(componentId);
}

/** @brief 从飞行中集合移除已完成的请求。 */
void ComponentValidationQueue::complete(const QString& componentId) {
    m_inFlightIds.remove(componentId);
}

/** @brief 判断待取出的请求队列是否为空。 */
bool ComponentValidationQueue::isEmpty() const {
    return m_pendingIds.isEmpty();
}

/** @brief 返回待取出的请求数量。 */
int ComponentValidationQueue::size() const {
    return m_pendingIds.size();
}

/** @brief 判断是否存在已发出的验证请求。 */
bool ComponentValidationQueue::hasInFlight() const {
    return !m_inFlightIds.isEmpty();
}

/** @brief 判断指定元件是否已发出验证请求。 */
bool ComponentValidationQueue::isInFlight(const QString& componentId) const {
    return m_inFlightIds.contains(componentId);
}

}  // namespace EasyKiConverter
