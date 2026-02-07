#ifndef COMPONENTLISTVIEWMODEL_H
#define COMPONENTLISTVIEWMODEL_H

#include "models/ComponentListItemData.h"
#include "services/ComponentService.h"

#include <QObject>
#include <QQmlListProperty>
#include <QStringList>
#include <QVector>

namespace EasyKiConverter {

/**
 * @brief 元件列表视图模型
 *
 * 负责管理元件列表相关的 UI 状态和操作
 * 连接 QML 界面和 ComponentService
 */
class ComponentListViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<EasyKiConverter::ComponentListItemData> componentList READ componentList NOTIFY
                   componentListChanged)
    Q_PROPERTY(int componentCount READ componentCount NOTIFY componentCountChanged)
    Q_PROPERTY(QString bomFilePath READ bomFilePath NOTIFY bomFilePathChanged)
    Q_PROPERTY(QString bomResult READ bomResult NOTIFY bomResultChanged)

public:
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

    // Getter 方法
    QQmlListProperty<ComponentListItemData> componentList();

    int componentCount() const {
        return m_componentList.count();
    }
    QString bomFilePath() const {
        return m_bomFilePath;
    }
    QString bomResult() const {
        return m_bomResult;
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
     * @brief 从剪贴板粘贴元器件编号
     */
    Q_INVOKABLE void pasteFromClipboard();

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
     * @brief 复制文本到剪贴板
     *
     * @param text 要复制的文本
     */
    Q_INVOKABLE void copyToClipboard(const QString& text);

signals:
    void componentListChanged();
    void componentCountChanged();
    void bomFilePathChanged();
    void bomResultChanged();
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

    // QQmlListProperty 辅助函数
    static void appendComponent(QQmlListProperty<ComponentListItemData>* list, ComponentListItemData* p);
    static qsizetype componentCount(QQmlListProperty<ComponentListItemData>* list);
    static ComponentListItemData* componentAt(QQmlListProperty<ComponentListItemData>* list, qsizetype i);
    static void clearComponents(QQmlListProperty<ComponentListItemData>* list);

private:
    ComponentService* m_service;
    QList<ComponentListItemData*> m_componentList;
    QString m_outputPath;
    QString m_bomFilePath;
    QString m_bomResult;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTLISTVIEWMODEL_H