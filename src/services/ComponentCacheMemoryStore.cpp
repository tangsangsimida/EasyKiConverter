#include "ComponentCacheMemoryStore.h"

#include "CacheDataValidator.h"

#include <QJsonDocument>

namespace EasyKiConverter {

/** @brief 初始化元器件一级内存缓存。 */
ComponentCacheMemoryStore::ComponentCacheMemoryStore(int maxCost) : m_store(maxCost) {}

/** @brief 判断指定元器件的元数据是否已经缓存。 */
bool ComponentCacheMemoryStore::containsMetadata(const QString& componentId) const {
    return m_store.contains(makeKey(componentId, QStringLiteral("metadata")));
}

/** @brief 解码并读取指定元器件的元数据。 */
QJsonObject ComponentCacheMemoryStore::loadMetadata(const QString& componentId) const {
    const QByteArray data = m_store.value(makeKey(componentId, QStringLiteral("metadata")));
    if (data.isEmpty())
        return {};

    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError)
        return {};
    return document.object();
}

/** @brief 将元数据编码后写入一级缓存。 */
qint64 ComponentCacheMemoryStore::saveMetadata(const QString& componentId, const QJsonObject& metadata) {
    const QJsonDocument document(metadata);
    return m_store.insert(makeKey(componentId, QStringLiteral("metadata")), document.toJson(QJsonDocument::Compact));
}

/** @brief 读取指定元器件的符号 CAD 数据。 */
QByteArray ComponentCacheMemoryStore::loadSymbol(const QString& componentId) const {
    return m_store.value(makeKey(componentId, QStringLiteral("symbol")));
}

/** @brief 校验并写入指定元器件的符号 CAD 数据。 */
std::optional<qint64> ComponentCacheMemoryStore::saveSymbol(const QString& componentId, const QByteArray& data) {
    if (!CacheDataValidator::isValidCadData(data))
        return std::nullopt;
    return m_store.insert(makeKey(componentId, QStringLiteral("symbol")), data);
}

/** @brief 读取指定元器件的封装 CAD 数据。 */
QByteArray ComponentCacheMemoryStore::loadFootprint(const QString& componentId) const {
    return m_store.value(makeKey(componentId, QStringLiteral("footprint")));
}

/** @brief 校验并写入指定元器件的封装 CAD 数据。 */
std::optional<qint64> ComponentCacheMemoryStore::saveFootprint(const QString& componentId, const QByteArray& data) {
    if (!CacheDataValidator::isValidCadData(data))
        return std::nullopt;
    return m_store.insert(makeKey(componentId, QStringLiteral("footprint")), data);
}

/** @brief 删除指定元器件的元数据、符号和封装缓存。 */
qint64 ComponentCacheMemoryStore::removeComponent(const QString& componentId) {
    return m_store.remove({makeKey(componentId, QStringLiteral("metadata")),
                           makeKey(componentId, QStringLiteral("symbol")),
                           makeKey(componentId, QStringLiteral("footprint"))});
}

/** @brief 清空所有一级缓存数据。 */
void ComponentCacheMemoryStore::clear() {
    m_store.clear();
}

/** @brief 获取一级缓存当前占用成本。 */
qint64 ComponentCacheMemoryStore::totalCost() const {
    return m_store.totalCost();
}

/** @brief 更新一级缓存容量上限。 */
void ComponentCacheMemoryStore::setMaxCost(int maxCost) {
    m_store.setMaxCost(maxCost);
}

/** @brief 获取一级缓存容量上限。 */
int ComponentCacheMemoryStore::maxCost() const {
    return m_store.maxCost();
}

/** @brief 按统一大小写规则生成一级缓存复合键。 */
QString ComponentCacheMemoryStore::makeKey(const QString& componentId, const QString& type) {
    return componentId.toUpper() + ":" + type;
}

}  // namespace EasyKiConverter
