#pragma once

#include "ExportProgress.h"

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>
#include <QStringList>

namespace EasyKiConverter {

class ComponentData;

/**
 * @brief 元器件数据缓存
 *
 * 提供L1内存缓存和L2磁盘缓存的两级缓存机制。
 * - L1: 内存缓存，存储预加载的ComponentData
 * - L2: 磁盘缓存，存储符号/封装/3D模型等文件
 *
 * 缓存键: 元器件ID (componentId)
 *
 * 使用示例:
 * @code
 * ComponentDataCache cache;
 * cache.setCacheDir("/path/to/cache");
 *
 * // 检查缓存
 * if (cache.has(componentId)) {
 *     auto data = cache.get(componentId);
 * }
 *
 * // 添加到缓存
 * cache.put(componentId, data);
 * @endcode
 */
class ComponentDataCache : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit ComponentDataCache(QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ComponentDataCache() override;

    /**
     * @brief 设置缓存目录
     * @param path 缓存根目录路径
     *
     * 缓存目录结构:
     * - path/symbols/     - 符号文件
     * - path/footprints/  - 封装文件
     * - path/3dmodels/    - 3D模型文件
     * - path/datasheets/  - 数据手册
     * - path/previews/    - 预览图
     */
    void setCacheDir(const QString& path);

    /**
     * @brief 获取缓存目录
     * @return 当前设置的缓存目录路径
     */
    QString cacheDir() const {
        return m_cacheDir;
    }

    /**
     * @brief 检查元器件数据是否在缓存中
     * @param componentId 元器件ID
     * @return true 表示缓存中存在
     */
    bool has(const QString& componentId) const;

    /**
     * @brief 获取元器件数据
     * @param componentId 元器件ID
     * @return 缓存的数据指针，不存在时返回nullptr
     */
    QSharedPointer<ComponentData> get(const QString& componentId) const;

    /**
     * @brief 添加元器件数据到缓存
     * @param componentId 元器件ID
     * @param data 元器件数据
     *
     * 数据会被存储到L1内存缓存。
     * 如果需要持久化，调用flushToDisk()。
     */
    void put(const QString& componentId, const QSharedPointer<ComponentData>& data);

    /**
     * @brief 移除元器件数据
     * @param componentId 元器件ID
     *
     * 从L1内存缓存中移除指定元器件的数据。
     */
    void remove(const QString& componentId);

    /**
     * @brief 清空所有缓存
     *
     * 清空L1内存缓存。
     * L2磁盘缓存可通过clearDiskCache()清除。
     */
    void clear();

    /**
     * @brief 获取缓存的元器件ID列表
     * @return 当前缓存中所有元器件ID
     */
    QStringList cachedComponentIds() const;

    /**
     * @brief 获取缓存的元器件数量
     * @return 缓存的元器件数量
     */
    int size() const;

    /**
     * @brief 将内存缓存中的数据同步到磁盘
     * @param componentId 元器件ID（为空则同步所有）
     *
     * 将L1缓存中的元器件数据写入L2磁盘缓存。
     */
    void flushToDisk(const QString& componentId = QString());

    /**
     * @brief 从磁盘缓存加载数据到内存
     * @param componentId 元器件ID
     * @return true 加载成功
     */
    bool loadFromDisk(const QString& componentId);

    /**
     * @brief 清空磁盘缓存
     * @param componentId 元器件ID（为空则清除所有）
     */
    void clearDiskCache(const QString& componentId = QString());

signals:
    /**
     * @brief 缓存更新信号
     * @param componentId 更新的元器件ID
     */
    void cacheUpdated(const QString& componentId);

    /**
     * @brief 缓存清除信号
     */
    void cacheCleared();

private:
    QString m_cacheDir;  ///< 缓存根目录
    mutable QMutex m_cacheMutex;  ///< 保护m_cache的互斥量
    QMap<QString, QSharedPointer<ComponentData>> m_cache;  ///< L1内存缓存
};

}  // namespace EasyKiConverter
