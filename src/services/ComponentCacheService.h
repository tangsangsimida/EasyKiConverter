#ifndef COMPONENTCACHESERVICE_H
#define COMPONENTCACHESERVICE_H

#include "core/network/AsyncNetworkRequest.h"
#include "core/network/NetworkClient.h"
#include "models/ComponentData.h"
#include "models/ComponentExportStatus.h"

#include <QCache>
#include <QDir>
#include <QJsonObject>
#include <QMutex>
#include <QObject>
#include <QSharedPointer>
#include <QString>

#include <memory>

namespace EasyKiConverter {

/**
 * @brief 元器件缓存服务类（两层缓存架构）
 *
 * L1 内存缓存（热点数据）：
 *   - UUID
 *   - Symbol CAD (JSON)
 *   - Footprint CAD (JSON)
 *   - URLs (手册URL、预览图URL)
 *   - 大小限制：50MB
 *   - LRU淘汰
 *
 * L2 磁盘缓存（所有数据）：
 *   - 预览图文件 (preview_*.jpg)
 *   - 手册文件 (datasheet.pdf/.html)
 *   - 3D模型文件 (*.step, *.wrl)
 *   - 元数据 (component.json)
 *   - 磁盘配额：5GB，超限LRU淘汰
 *
 * 缓存目录结构：
 * {cacheDir}/
 *   {lcscId}/
 *     component.json          # 元数据
 *     preview_0.jpg           # 预览图0
 *     preview_1.jpg           # 预览图1
 *     preview_2.jpg           # 预览图2
 *     datasheet.pdf           # 数据手册
 *   model3d/
 *     {uuid}.step             # 3D STEP模型
 *     {uuid}.wrl             # 3D WRL模型
 *
 * @note 锁顺序约定
 *   当同时需要获取多个锁时，必须严格按以下顺序获取：
 *   1. ComponentCacheService::m_mutex (如果是 ComponentCacheService 的方法)
 *   2. ComponentService::m_fetchingComponentsMutex (如果是 ComponentService 的方法)
 *
 *   违反此顺序可能导致死锁。
 */
class ComponentCacheService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static ComponentCacheService* instance();

    /**
     * @brief 析构函数
     */
    ~ComponentCacheService() override;

    /**
     * @brief 设置缓存根目录
     * @param cacheDir 缓存目录路径
     */
    void setCacheDir(const QString& cacheDir);

    /**
     * @brief 获取缓存根目录
     */
    QString cacheDir() const;

    /**
     * @brief 获取单个元器件的缓存目录
     * @param lcscId 元器件ID
     */
    QString componentCacheDir(const QString& lcscId) const;

    /**
     * @brief 预览图文件路径
     * @param lcscId 元器件ID
     * @param index 图片索引
     */
    QString previewImagePath(const QString& lcscId, int index) const;

    /**
     * @brief 下载预览图（同步网络请求，支持取消）
     * @param lcscId 元器件ID
     * @param imageUrl 图片URL
     * @param imageIndex 图片索引
     * @param diag 网络诊断信息输出参数
     * @param cancelled 可选的取消标志，如果不为nullptr且被设置为1则立即返回
     * @return QByteArray 下载的图片数据，下载失败或取消返回空
     */
    QByteArray downloadPreviewImage(const QString& lcscId,
                                    const QString& imageUrl,
                                    int imageIndex,
                                    ComponentExportStatus::NetworkDiagnostics* diag = nullptr,
                                    QAtomicInt* cancelled = nullptr,
                                    bool weakNetwork = false);

    /**
     * @brief 检查元器件是否有有效缓存
     * @param lcscId 元器件ID
     * @return bool 是否有有效缓存
     */
    bool hasCache(const QString& lcscId) const;

    /**
     * @brief 验证缓存完整性（JSON是否可解析）
     * @param lcscId 元器件ID
     * @return bool 缓存是否有效
     */
    bool isCacheValid(const QString& lcscId) const;

    // ==================== L1 内存缓存操作 ====================

    /**
     * @brief 检查L1内存缓存中是否有数据
     * @param lcscId 元器件ID
     */
    bool hasInMemoryCache(const QString& lcscId) const;

    /**
     * @brief 从L1内存缓存加载元器件元数据（UUID、URLs等小数据）
     * @param lcscId 元器件ID
     * @return QJsonObject 元数据对象，如果不存在返回空
     */
    QJsonObject loadMetadataFromMemory(const QString& lcscId) const;

    /**
     * @brief 保存元器件元数据到L1内存缓存
     * @param lcscId 元器件ID
     * @param metadata 元数据
     */
    void saveMetadataToMemory(const QString& lcscId, const QJsonObject& metadata);

    /**
     * @brief 从L1内存缓存加载符号CAD JSON
     * @param lcscId 元器件ID
     * @return QByteArray 符号CAD JSON数据
     */
    QByteArray loadSymbolDataFromMemory(const QString& lcscId) const;

    /**
     * @brief 保存符号CAD JSON到L1内存缓存
     * @param lcscId 元器件ID
     * @param data 符号CAD JSON数据
     */
    void saveSymbolDataToMemory(const QString& lcscId, const QByteArray& data);

    /**
     * @brief 从L1内存缓存加载封装CAD JSON
     * @param lcscId 元器件ID
     * @return QByteArray 封装CAD JSON数据
     */
    QByteArray loadFootprintDataFromMemory(const QString& lcscId) const;

    /**
     * @brief 保存封装CAD JSON到L1内存缓存
     * @param lcscId 元器件ID
     * @param data 封装CAD JSON数据
     */
    void saveFootprintDataToMemory(const QString& lcscId, const QByteArray& data);

    // ==================== L2 磁盘缓存操作 ====================

    /**
     * @brief 从L2磁盘缓存加载元器件数据（完整ComponentData）
     * @param lcscId 元器件ID
     * @return QSharedPointer<ComponentData> 元器件数据，如果缓存不存在或无效返回nullptr
     */
    QSharedPointer<ComponentData> loadComponentData(const QString& lcscId) const;

    /**
     * @brief 异步保存元器件元数据到L2磁盘缓存（不阻塞UI）
     * @param componentId 元器件ID
     * @param data 元器件数据
     * @note 适用于批量操作，避免频繁的同步文件I/O阻塞UI
     */
    void saveComponentMetadataAsync(const QString& componentId, const ComponentData& data);

    /**
     * @brief 保存符号CAD数据到L2磁盘缓存
     * @param lcscId 元器件ID
     * @param data 符号CAD JSON数据
     */
    void saveSymbolData(const QString& lcscId, const QByteArray& data);

    /**
     * @brief 保存封装CAD数据到L2磁盘缓存
     * @param lcscId 元器件ID
     * @param data 封装CAD JSON数据
     */
    void saveFootprintData(const QString& lcscId, const QByteArray& data);

    /**
     * @brief 从L2磁盘缓存加载符号CAD JSON
     * @param lcscId 元器件ID
     * @return QByteArray 符号CAD JSON数据
     */
    QByteArray loadSymbolData(const QString& lcscId) const;

    /**
     * @brief 从L2磁盘缓存加载封装CAD JSON
     * @param lcscId 元器件ID
     * @return QByteArray 封装CAD JSON数据
     */
    QByteArray loadFootprintData(const QString& lcscId) const;

    /**
     * @brief 保存完整的CAD数据JSON到缓存（包含符号和封装）
     * @param lcscId 元器件ID
     * @param cadData CAD数据JSON
     */
    void saveCadDataJson(const QString& lcscId, const QByteArray& cadData);

    /**
     * @brief 从缓存加载完整的CAD数据JSON
     * @param lcscId 元器件ID
     * @return QByteArray CAD数据JSON，如果不存在返回空
     */
    QByteArray loadCadDataJson(const QString& lcscId) const;

    /**
     * @brief 加载预览图
     * @param lcscId 元器件ID
     * @param imageIndex 图片索引（0-2）
     * @return QByteArray 图片数据，如果不存在返回空
     */
    QByteArray loadPreviewImage(const QString& lcscId, int imageIndex) const;

    /**
     * @brief 保存预览图（直接写磁盘）
     * @param lcscId 元器件ID
     * @param imageData 图片数据
     * @param imageIndex 图片索引（0-2）
     */
    void savePreviewImage(const QString& lcscId, const QByteArray& imageData, int imageIndex);

    /**
     * @brief 加载数据手册
     * @param lcscId 元器件ID
     * @return QByteArray 数据手册数据，如果不存在返回空
     */
    QByteArray loadDatasheet(const QString& lcscId) const;

    /**
     * @brief 检查元器件是否有符号封装缓存（CAD数据）
     * @param lcscId 元器件ID
     * @return bool 是否有有效的符号封装缓存
     */
    bool hasSymbolFootprintCache(const QString& lcscId) const;

    /**
     * @brief 从L2磁盘缓存加载元器件元数据JSON
     * @param lcscId 元器件ID
     * @return QJsonObject 元数据对象，如果不存在返回空
     */
    QJsonObject loadMetadata(const QString& lcscId) const;

    /**
     * @brief 保存数据手册（直接写磁盘）
     * @param lcscId 元器件ID
     * @param datasheetData 数据手册数据
     * @param format 数据格式（pdf/html）
     */
    void saveDatasheet(const QString& lcscId, const QByteArray& datasheetData, const QString& format);

    /**
     * @brief 下载数据手册（同步网络请求，支持取消）
     * @param lcscId 元器件ID
     * @param datasheetUrl 数据手册URL
     * @param format 数据格式输出参数（pdf/html）
     * @param diag 网络诊断信息输出参数
     * @param cancelled 可选的取消标志，如果不为nullptr且被设置为1则立即返回
     * @return QByteArray 下载的数据，下载失败或取消返回空
     */
    QByteArray downloadDatasheet(const QString& lcscId,
                                 const QString& datasheetUrl,
                                 QString* format,
                                 ComponentExportStatus::NetworkDiagnostics* diag = nullptr,
                                 QAtomicInt* cancelled = nullptr,
                                 bool weakNetwork = false);

    /**
     * @brief 加载3D模型（STEP/WRL）
     * @param uuid 3D模型UUID
     * @param extension 文件扩展名（step/wrl）
     * @return QByteArray 模型数据，如果不存在返回空
     */
    QByteArray loadModel3D(const QString& uuid, const QString& extension) const;

    /**
     * @brief 保存3D模型（直接写磁盘）
     * @param uuid 3D模型UUID
     * @param data 模型数据
     * @param extension 文件扩展名（step/wrl）
     */
    void saveModel3D(const QString& uuid, const QByteArray& data, const QString& extension);

    /**
     * @brief 检查3D模型是否有缓存
     * @param uuid 3D模型UUID
     * @param extension 文件扩展名
     */
    bool hasModel3DCached(const QString& uuid, const QString& extension) const;

    /**
     * @brief 获取3D模型文件路径
     * @param uuid 3D模型UUID
     * @param extension 文件扩展名（step/wrl）
     */
    QString model3DPath(const QString& uuid, const QString& extension) const;

    /**
     * @brief 直接拷贝3D模型文件到目标路径（不经过内存）
     * @param uuid 3D模型UUID
     * @param extension 文件扩展名（step/wrl）
     * @param destinationPath 目标文件路径
     * @return bool 是否拷贝成功
     */
    bool copyModel3DToFile(const QString& uuid, const QString& extension, const QString& destinationPath) const;

    // ==================== 缓存管理 ====================

    /**
     * @brief 删除单个元器件的缓存
     * @param lcscId 元器件ID
     */
    void removeCache(const QString& lcscId);

    /**
     * @brief 清空所有缓存（L1内存 + L2磁盘）
     */
    Q_INVOKABLE void clearAllCache();

    /**
     * @brief 清空L1内存缓存
     */
    void clearMemoryCache();

    /**
     * @brief 获取所有缓存的元器件ID列表
     * @return QStringList 元器件ID列表
     */
    QStringList getCachedComponentIds() const;

    /**
     * @brief 获取缓存统计信息
     * @return qint64 L2磁盘缓存大小（字节）
     */
    qint64 getCacheSize() const;

    /**
     * @brief 获取L1内存缓存大小
     * @return qint64 字节数
     */
    qint64 getMemoryCacheSize() const;

    /**
     * @brief 执行LRU清理，删除最久未访问的缓存直到低于目标大小
     * @param targetSizeBytes 目标大小（字节）
     */
    void pruneCache(qint64 targetSizeBytes);

    /**
     * @brief 设置L1内存缓存大小限制
     * @param maxSizeMB 大小限制（MB）
     */
    void setMemoryCacheLimit(int maxSizeMB);

    /**
     * @brief 获取L1内存缓存限制
     */
    int memoryCacheLimit() const;

signals:
    /**
     * @brief 缓存加载完成信号
     */
    void cacheLoaded(const QString& lcscId);

    /**
     * @brief 缓存保存完成信号
     */
    void cacheSaved(const QString& lcscId);

    /**
     * @brief 缓存大小变化信号
     */
    void cacheSizeChanged(qint64 newSize);

    /**
     * @brief L1内存缓存大小变化信号
     */
    void memoryCacheSizeChanged(qint64 newSize);

private:
    /**
     * @brief 私有构造函数（单例模式）
     */
    explicit ComponentCacheService(QObject* parent = nullptr);

    /**
     * @brief 确保元器件缓存目录存在
     */
    QString ensureComponentDir(const QString& lcscId) const;

    /**
     * @brief 确保3D模型缓存目录存在
     */
    QString ensureModel3DCacheDir() const;

    /**
     * @brief 计算目录大小
     */
    qint64 calculateDirSize(const QString& dirPath) const;

    /**
     * @brief 获取缓存目录的访问时间
     */
    QDateTime getCacheAccessTime(const QString& lcscId) const;

    /**
     * @brief 获取缓存元数据文件路径
     */
    QString metadataPath(const QString& lcscId) const;

    /**
     * @brief 数据手册文件路径
     */
    QString datasheetPath(const QString& lcscId) const;

    /**
     * @brief 保存元数据JSON到L2
     */
    void saveMetadata(const QString& lcscId, const QJsonObject& metadata);

    /**
     * @brief 从ComponentData构建元数据
     */
    QJsonObject buildMetadata(const QString& componentId, const ComponentData& data) const;

    /**
     * @brief 合并新旧元数据，避免新数据缺字段时覆盖掉已有缓存
     */
    QJsonObject mergeMetadata(const QJsonObject& existing, const QJsonObject& incoming) const;

    /**
     * @brief 原子写文件，防止半写入缓存损坏
     */
    bool writeFileAtomically(const QString& path, const QByteArray& data) const;

    /**
     * @brief 解析datasheet缓存文件实际路径
     */
    QString resolveDatasheetPath(const QString& lcscId,
                                 const QString& preferredFormat = QString(),
                                 bool forWrite = false) const;

    /**
     * @brief 启动时自修复缓存目录
     */
    void selfHealCache();

    /**
     * @brief 修复单个元器件缓存目录
     * @return true 表示目录保留，false 表示目录已删除或不可用
     */
    bool repairComponentCache(const QString& lcscId);

    /**
     * @brief 修复3D模型缓存目录中的坏文件
     */
    void repairModel3DCache();

    /**
     * @brief 生成缓存key
     */
    QString makeMemoryKey(const QString& lcscId, const QString& type) const;

    static std::unique_ptr<ComponentCacheService> s_instance;
    mutable QMutex m_mutex;
    QString m_cacheDir;
    int m_memoryCacheLimitMB;

    // L1 内存缓存：QCache 自动 LRU 淘汰
    // Key 格式: "lcscId:type" (如 "C12345:metadata", "C12345:symbol")
    // Value: QByteArray
    QCache<QString, QByteArray> m_memoryCache;

    // 跟踪L1缓存总大小（字节）
    qint64 m_memoryCacheSize;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTCACHESERVICE_H
