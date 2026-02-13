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
     */
    void previewImageReady(const QString& componentId, const QImage& image);

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
    void handleImageReady(const QString& componentId, const QString& imagePath);

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

    // BOM 解析已移至独立的 BomParser 类

    // 图片抓取已移至独立的 LcscImageService 类

private:
    class EasyedaApi* m_api;
    class EasyedaImporter* m_importer;
    QNetworkAccessManager* m_networkManager;
    LcscImageService* m_imageService;

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
