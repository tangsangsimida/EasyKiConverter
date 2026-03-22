#ifndef COMPONENTLISTVIEWMODEL_H
#define COMPONENTLISTVIEWMODEL_H

#include "models/ComponentListItemData.h"
#include "services/ComponentService.h"

#include <QAbstractListModel>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QTimer>

namespace EasyKiConverter {

/**
 * @brief 元件列表视图模型
 *
 * 负责管理元件列表相关的 UI 状态和操作
 * 连接 QML 界面和 ComponentService
 * 继承自 QAbstractListModel 以提供细粒度的 UI 更新和更高的性能
 */
class ComponentListViewModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int componentCount READ componentCount NOTIFY componentCountChanged)
    Q_PROPERTY(QString bomFilePath READ bomFilePath NOTIFY bomFilePathChanged)
    Q_PROPERTY(QString bomResult READ bomResult NOTIFY bomResultChanged)
    Q_PROPERTY(bool hasInvalidComponents READ hasInvalidComponents NOTIFY hasInvalidComponentsChanged)
    Q_PROPERTY(QString filterMode READ filterMode NOTIFY filterModeChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int validatingCount READ validatingCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int validCount READ validCount NOTIFY filteredCountChanged)
    Q_PROPERTY(int invalidCount READ invalidCount NOTIFY filteredCountChanged)

public:
    enum ComponentRoles { ItemDataRole = Qt::UserRole + 1 };

    /**
     * @brief 构造函数
     *
     * @param service 元件服务
     * @param parent 父对象
     */
    explicit ComponentListViewModel(ComponentService* service, QObject* parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~ComponentListViewModel() override;

    // QAbstractListModel 接口实现
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int componentCount() const {
        return m_componentList.count();
    }

    QString bomFilePath() const {
        return m_bomFilePath;
    }

    QString bomResult() const {
        return m_bomResult;
    }

    bool hasInvalidComponents() const {
        return m_hasInvalidComponents;
    }

    // 获取预加载的数据（用于导出流程）
    QSharedPointer<ComponentData> getPreloadedData(const QString& componentId) const;

public slots:
    /**
     * @brief 添加元件到列表
     *
     * @param componentId 元件ID
     */
    Q_INVOKABLE void addComponent(const QString& componentId);

    /**
     * @brief 从列表中移除元件
     *
     * @param index 元件索引
     */
    Q_INVOKABLE void removeComponent(int index);

    /**
     * @brief 根据ID移除元件
     *
     * @param componentId 元件ID
     */
    Q_INVOKABLE void removeComponentById(const QString& componentId);

    /**
     * @brief 清空元件列表
     */
    Q_INVOKABLE void clearComponentList();

    /**
     * @brief 批量添加元器件效率更高
     *
     * 将多个元器件一次性插入模型，避免 N 次 beginInsertRows/endInsertRows 导致的 N 次 UI 重建
     *
     * @param componentIds 元件ID列表
     */
    Q_INVOKABLE void addComponentsBatch(const QStringList& componentIds);

    /**
     * @brief 从剪贴板粘贴元器件编号
     */
    Q_INVOKABLE void pasteFromClipboard();

    /**
     * @brief 复制所有元器件编号到剪贴板
     */
    Q_INVOKABLE void copyAllComponentIds();

    /**
     * @brief 选择BOM文件
     *
     * @param filePath 文件路径
     */
    Q_INVOKABLE void selectBomFile(const QString& filePath);

    /**
     * @brief 获取元件数据
     *
     * @param componentId 元件ID
     * @param fetch3DModel 是否获取3D模型
     */
    void fetchComponentData(const QString& componentId, bool fetch3DModel = true);

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
    QString outputPath() const {
        return m_outputPath;
    }

    // 重新验证/获取信息
    Q_INVOKABLE void refreshComponentInfo(int index);

    /**
     * @brief 重试所有验证失败的元器件
     */
    Q_INVOKABLE void retryAllInvalidComponents();

    /**
     * @brief 重试获取组件的预览图
     *
     * @param componentId 元件ID
     */
    Q_INVOKABLE void retryPreviewImage(const QString& componentId);

    /**
     * @brief 获取所有元件ID列表
     * @return QStringList 元件ID列表
     */
    Q_INVOKABLE QStringList getAllComponentIds() const;

    /**
     * @brief 复制文本到剪贴板
     *
     * @param text 要复制的文本
     */
    Q_INVOKABLE void copyToClipboard(const QString& text);

    /**
     * @brief 设置筛选模式
     *
     * @param mode 筛选模式: all, validating, valid, invalid
     */
    Q_INVOKABLE void setFilterMode(const QString& mode);

    // Getter 方法
    QString filterMode() const {
        return m_filterMode;
    }

    int filteredCount() const;
    int validatingCount() const;
    int validCount() const;
    int invalidCount() const;

signals:
    // componentListChanged 信号不再需要，因为 QAbstractListModel 有自己的信号机制
    void componentCountChanged();
    void bomFilePathChanged();
    void bomResultChanged();
    void hasInvalidComponentsChanged();
    void filterModeChanged();
    void filteredCountChanged();
    void componentAdded(const QString& componentId, bool success, const QString& message);
    void componentRemoved(const QString& componentId);
    void listCleared();
    void pasteCompleted(int added, int skipped);
    void outputPathChanged();

private slots:
    /**
     * @brief 处理元件信息获取成功
     *
     * @param componentId 元件ID
     * @param data 元件数据
     */
    void handleComponentInfoReady(const QString& componentId, const ComponentData& data);

    /**
     * @brief 处理CAD数据获取成功
     *
     * @param componentId 元件ID
     * @param data CAD数据
     */
    void handleCadDataReady(const QString& componentId, const ComponentData& data);

    /**
     * @brief 处理3D模型数据获取成功
     *
     * @param uuid 模型UUID
     * @param filePath 文件路径
     */
    void handleModel3DReady(const QString& uuid, const QString& filePath);

    /**
     * @brief 处理数据获取失败
     *
     * @param componentId 元件ID
     * @param error 错误信息
     */
    void handleFetchError(const QString& componentId, const QString& error);

    /**
     * @brief 处理 LCSC 数据更新
     *
     * @param componentId 元件ID
     * @param manufacturerPart 制造商部件号
     * @param datasheetUrl 数据手册 URL
     * @param imageUrls 预览图 URL 列表
     */
    void handleLcscDataUpdated(const QString& componentId,
                               const QString& manufacturerPart,
                               const QString& datasheetUrl,
                               const QStringList& imageUrls);

    /**
     * @brief 处理数据手册下载完成
     *
     * @param componentId 元件ID
     * @param datasheetData 数据手册数据（内存）
     */
    void handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData);

    /**
     * @brief 处理预览图数据下载完成
     *
     * @param componentId 元件ID
     * @param imageData 预览图数据（内存）
     * @param imageIndex 图片索引
     */
    void handlePreviewImageDataReady(const QString& componentId, const QByteArray& imageData, int imageIndex);

    /**
     * @brief 处理所有预览图片下载完成
     *
     * @param componentId 元件ID
     * @param imageDataList 图片数据列表
     */
    void handlePreviewImageFailed(const QString& componentId, const QString& error);

    void handleAllImagesReady(const QString& componentId, const QList<QByteArray>& imageDataList);

private:
    /**
     * @brief 检查元件是否已存在
     *
     * @param componentId 元件ID
     * @return bool 是否存在
     */
    bool componentExists(const QString& componentId) const;

    /**
     * @brief 验证元件ID格式
     *
     * @param componentId 元件ID
     * @return bool 是否有效
     */
    bool validateComponentId(const QString& componentId) const;

    /**
     * @brief 从文本中提取元件编号
     *
     * @param text 文本内容
     * @return QStringList 提取的元件编号列表
     */
    QStringList extractComponentIdFromText(const QString& text) const;

    // 查找列表项数据
    ComponentListItemData* findItemData(const QString& componentId) const;

    // 更新 hasInvalidComponents 状态
    void updateHasInvalidComponents();

    // 批量获取所有验证通过元器件的预览图
    void fetchAllPreviewImages();

private:
    ComponentService* m_service;
    QList<ComponentListItemData*> m_componentList;
    QSet<QString> m_componentIdIndex;  // 快速查重集合，O(1) 查找
    QString m_outputPath;
    QString m_bomFilePath;
    QString m_bomResult;
    bool m_hasInvalidComponents = false;

    // 防抖定时器，用于减少频繁的 UI 更新
    QTimer* m_debounceTimer;
    QSet<QString> m_pendingUpdateIndices;  // 待更新的索引集合

    // 两阶段控制：先验证，再获取预览图
    int m_pendingValidationCount = 0;  // 待验证的元器件数量
    QStringList m_validatedComponentIds;  // 验证通过的元器件ID列表
    bool m_previewFetchEnabled = false;  // 是否允许获取预览图

    // 筛选功能
    QString m_filterMode = "all";  // 筛选模式: all, validating, valid, invalid
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTVIEWMODEL_H
