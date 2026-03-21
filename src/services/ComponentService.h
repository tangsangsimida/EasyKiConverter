#ifndef COMPONENTSERVICE_H
#define COMPONENTSERVICE_H

#include "LcscImageService.h"
#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QImage>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QPair>
#include <QQueue>
#include <QString>
#include <QStringList>

class QNetworkAccessManager;

namespace EasyKiConverter {

/**
 * @brief 元件服务类
 *
 * 负责处理与元件相关的业务逻辑，不依赖任何 UI 组件
 * 包括数据获取、验证、解析和缓存管理
 */
class ComponentService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     *
     * @param parent 父对象
     */
    explicit ComponentService(QObject* parent = nullptr);

    /**
     * @brief 构造函数（支持注入 API 实例）
     *
     * @param api API 实例
     * @param parent 父对象
     */
    explicit ComponentService(class EasyedaApi* api, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ComponentService() override;

    /**
     * @brief 获取元件数据
     *
     * @param componentId 元件ID
     * @param fetch3DModel 是否获取3D模型
     *
     * 副作用：触发componentInfoReady、cadDataReady、model3DReady信号
     * 线程安全：可以同时调用多个fetchComponentData
     */
    void fetchComponentData(const QString& componentId, bool fetch3DModel = true);

    /**
     * @brief 获取 LCSC 预览图
     *
     * @param componentId 元件ID
     */
    void fetchLcscPreviewImage(const QString& componentId);

    /**
     * @brief 并行获取多个元件的数据
     *
     * @param componentIds 元件ID列表
     * @param fetch3DModel 是否获取3D模型
     */
    void fetchMultipleComponentsData(const QStringList& componentIds, bool fetch3DModel = true);

    /**
     * @brief 验证元件ID格式
     *
     * @param componentId 元件ID
     * @return bool 是否有效
     */
    bool validateComponentId(const QString& componentId) const;

    /**
     * @brief 从文本中智能提取元件编号
     *
     * @param text 输入文本
     * @return QStringList 提取的元件编号列表
     */
    QStringList extractComponentIdFromText(const QString& text) const;

    /**
     * @brief 解析BOM文件
     *
     * @param filePath 文件路径
     * @return QStringList 解析出的元件ID列表
     */
    QStringList parseBomFile(const QString& filePath);

    /**
     * @brief 获取元件数据(从缓存或内存)
     *
     * @param componentId 元件ID
     * @return ComponentData 元件数据
     */
    ComponentData getComponentData(const QString& componentId) const;

    /**
     * @brief 清除缓存
     */
    void clearCache();

    /**
     * @brief 设置输出路径
     *
     * @param path 输出路径
     */
    void setOutputPath(const QString& path);

    /**
     * @brief 获取输出路径
     *
     * @return QString 输出路径
     */
    QString getOutputPath() const;

signals:
    /**
     * @brief 元件信息获取成功信号
     *
     * @param componentId 元件ID
     * @param data 元件数据
     */
    void componentInfoReady(const QString& componentId, const ComponentData& data);

    /**
     * @brief CAD数据获取成功信号
     *
     * @param componentId 元件ID
     * @param data 元件数据
     */
    void cadDataReady(const QString& componentId, const ComponentData& data);

    /**
     * @brief 3D模型获取成功信号
     *
     * @param uuid 模型UUID
     * @param filePath 文件路径
     */
    void model3DReady(const QString& uuid, const QString& filePath);

    /**
     * @brief 预览图获取成功信号
     *
     * @param componentId 元件ID
     * @param image 预览图
     * @param imageIndex 图片索引（0-2）
     */
    void previewImageReady(const QString& componentId, const QImage& image, int imageIndex);

    /**
     * @brief 预览图数据获取成功信号（内存数据）
     *
     * @param componentId 元件ID
     * @param imageData 预览图数据
     * @param imageIndex 图片索引
     */
    void previewImageDataReady(const QString& componentId, const QByteArray& imageData, int imageIndex);

    /**
     * @brief 预览图获取失败信号
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void previewImageFailed(const QString& componentId, const QString& error);

    /**
     * @brief 所有预览图片下载完成信号
     *
     * @param componentId 元件ID
     * @param imageDataList 图片数据列表（内存）
     */
    void allImagesReady(const QString& componentId, const QList<QByteArray>& imageDataList);

    /**
     * @brief 获取错误信号
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void fetchError(const QString& componentId, const QString& error);

    /**
     * @brief 所有元件数据收集完成信号
     *
     * @param componentDataList 元件数据列表
     */
    void allComponentsDataCollected(const QList<ComponentData>& componentDataList);

    /**
     * @brief LCSC 数据更新信号（数据手册和预览图 URL）
     *
     * 当 LCSC API 返回数据手册和预览图 URL 后发送此信号
     * ComponentListViewModel 可以接收此信号并更新缓存的 ComponentData
     *
     * @param componentId 元件 ID
     * @param manufacturerPart 制造商部件号
     * @param datasheetUrl 数据手册 URL
     * @param imageUrls 预览图 URL 列表
     */
    void lcscDataUpdated(const QString& componentId,
                         const QString& manufacturerPart,
                         const QString& datasheetUrl,
                         const QStringList& imageUrls);

    /**
     * @brief 数据手册下载完成信号
     *
     * @param componentId 元件 ID
     * @param datasheetData 数据手册数据（内存）
     */
    void datasheetReady(const QString& componentId, const QByteArray& datasheetData);

private slots:
    /**
     * @brief 处理组件信息获取成功
     *
     * @param data 组件信息数据
     */
    void handleComponentInfoFetched(const QJsonObject& data);

    /**
     * @brief 处理CAD数据获取成功
     *
     * @param data CAD数据
     */
    void handleCadDataFetched(const QJsonObject& data);

    /**
     * @brief 处理3D模型数据获取成功
     *
     * @param uuid 模型UUID
     * @param data 模型数据
     */
    void handleModel3DFetched(const QString& uuid, const QByteArray& data);

    void handleFetchError(const QString& errorMessage);

    /**
     * @brief 处理预览图就绪
     */
    void handleImageReady(const QString& componentId, const QByteArray& imageData, int imageIndex);

    /**
     * @brief 处理 LCSC API 数据就绪（包含数据手册和预览图 URL）
     */
    void handleLcscDataReady(const QString& componentId,
                             const QString& manufacturerPart,
                             const QString& datasheetUrl,
                             const QStringList& imageUrls);

    /**
     * @brief 处理数据手册下载完成
     */
    void handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData);

    /**
     * @brief 处理所有预览图片下载完成
     */
    void handleAllImagesReady(const QString& componentId, const QList<QByteArray>& imageDataList);

    /**
     * @brief 处理预览图获取失败
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void handlePreviewImageError(const QString& componentId, const QString& error);

    /**
     * @brief 处理获取错误（带 ID）
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void handleFetchErrorWithId(const QString& componentId, const QString& error);

private:
    /**
     * @brief 初始化API连接
     */
    void initializeApiConnections();

    /**
     * @brief 完成元件数据收集
     *
     * @param componentId 元件ID
     */
    void completeComponentData(const QString& componentId);

    /**
     * @brief 处理获取错误
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void handleFetchErrorForComponent(const QString& componentId, const QString& error);

    /**
     * @brief 处理并行数据收集完成
     *
     * @param componentId 元件ID
     * @param data 元件数据
     */
    void handleParallelDataCollected(const QString& componentId, const ComponentData& data);

    /**
     * @brief 处理并行获取错误
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void handleParallelFetchError(const QString& componentId, const QString& error);

    /**
     * @brief 动态队列管理：处理单个请求完成后的队列调度
     */
    void processQueueNext();

    /**
     * @brief 动态队列管理：启动队列处理
     */
    void startQueueProcessing();

    /**
     * @brief 异步队列管理：检查队列状态并启动下一个请求
     */
    void checkQueueAndProcessNext();

    /**
     * @brief 异步队列管理：处理队列超时
     */
    void handleQueueTimeout();

    /**
     * @brief 异步队列管理：重置队列状态
     */
    void resetQueueState();

    /**
     * @brief 检测数据是否为 PDF 格式
     *
     * @param data 数据
     * @return bool 是否为 PDF
     */
    bool isPDF(const QByteArray& data) const;

    // BOM 解析已移至独立的 BomParser 类

    // 图片抓取已移至独立的 LcscImageService 类

private:
    class EasyedaApi* m_api;
    class EasyedaImporter* m_importer;
    QNetworkAccessManager* m_networkManager;
    LcscImageService* m_imageService;

    // 添加互斥锁保护并发访问
    mutable QMutex m_fetchingComponentsMutex;
    mutable QMutex m_componentCacheMutex;
    mutable QMutex m_currentIdMutex;     // 保护 m_currentComponentId 的并发访问
    mutable QMutex m_parallelDataMutex;  // 保护并行数据收集状态的并发访问

    // 数据缓存
    QMap<QString, ComponentData> m_componentCache;

    // 当前正在获取的元件数据
    struct FetchingComponent {
        QString componentId;
        ComponentData data;
        bool hasComponentInfo;
        bool hasCadData;
        bool hasObjData;
        bool hasStepData;
        bool fetch3DModel;  // 是否需要获取 3D 模型
        QString errorMessage;
    };

    QMap<QString, FetchingComponent> m_fetchingComponents;

    // 当前处理的元件ID
    QString m_currentComponentId;

    // 待处理的组件数据（用于等待3D 模型数据）
    ComponentData m_pendingComponentData;

    // 待处理的 3D 模型 UUID
    QString m_pendingModelUuid;

    // 是否已经下载了WRL 格式
    bool m_hasDownloadedWrl;

    // 并行数据收集状态
    QMap<QString, ComponentData> m_parallelCollectedData;  // 已收集的数据
    QMap<QString, bool> m_parallelFetchingStatus;          // 元件ID -> 是否正在获取
    QStringList m_parallelPendingComponents;               // 待获取的元件列表
    int m_parallelTotalCount;                              // 总元件数
    int m_parallelCompletedCount;                          // 已完成数
    bool m_parallelFetching;                               // 是否正在并行获取

    // 动态队列管理
    QStringList m_requestQueue;   // 待处理的元件ID队列
    int m_activeRequestCount;     // 当前活跃的网络请求数
    int m_maxConcurrentRequests;  // 最大并发请求数（基于性能测试，5个请求在弱网环境下提供最佳稳定性）
    QTimer* m_queueTimer;         // 异步队列处理定时器（每500ms检查队列状态）
    QTimer* m_timeoutTimer;       // 批量处理总超时保护（防止队列永久阻塞）
    static const int QUEUE_CHECK_INTERVAL_MS = 500;  // 队列检查间隔，平衡响应性和CPU使用
    static const int TOTAL_TIMEOUT_MS = 300000;      // 总超时5分钟，防止弱网环境下无限等待

    // 内部状态处理

    /**
     * @brief 内部获取元件数据方法
     *
     * @param componentId 元件ID
     * @param fetch3DModel 是否获取3D模型
     */
    void fetchComponentDataInternal(const QString& componentId, bool fetch3DModel);

    // 输出路径
    QString m_outputPath;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTSERVICE_H
