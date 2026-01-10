#ifndef MAINCONTROLLER_H
#define MAINCONTROLLER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariantList>
#include <QSharedPointer>
#include <QThreadPool>
#include <QThread>
#include <QMutex>
#include "src/ui/utils/ConfigManager.h"
#include "src/core/easyeda/EasyedaApi.h"
#include "src/core/easyeda/EasyedaImporter.h"
#include "src/core/kicad/ExporterSymbol.h"
#include "src/core/kicad/ExporterFootprint.h"
#include "src/core/kicad/Exporter3DModel.h"

namespace EasyKiConverter {

// 前置声明
class MainController;

/**
 * @brief 元件导出任务类
 *
 * 用于在后台线程中执行单个元件的数据收集任务
 */
class ComponentExportTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    explicit ComponentExportTask(
        const QString &componentId,
        const QString &outputPath,
        const QString &libName,
        bool exportSymbol,
        bool exportFootprint,
        bool exportModel3D,
        MainController *controller,
        QObject *parent = nullptr);

    ~ComponentExportTask() override;
    void run() override;

signals:
    void exportFinished(const QString &componentId, bool success, const QString &message);
    void dataCollected(const QString &componentId, bool success, const QString &message);

private:
    QString m_componentId;
    QString m_outputPath;
    QString m_libName;
    bool m_exportSymbol;
    bool m_exportFootprint;
    bool m_exportModel3D;
    MainController *m_controller;
};

/**
 * @brief 主控制器类
 *
 * 用于连接 QML 界面和 C++ 业务逻辑
 */
class MainController : public QObject
{
    Q_OBJECT
    // 声明 ComponentExportTask 为友元类，使其能够访问私有成员
    friend class ComponentExportTask;
    Q_PROPERTY(QStringList componentList READ componentList NOTIFY componentListChanged)
    Q_PROPERTY(int componentCount READ componentCount NOTIFY componentCountChanged)
    Q_PROPERTY(QString bomFilePath READ bomFilePath NOTIFY bomFilePathChanged)
    Q_PROPERTY(QString bomResult READ bomResult NOTIFY bomResultChanged)
    Q_PROPERTY(QString outputPath READ outputPath WRITE setOutputPath NOTIFY outputPathChanged)
    Q_PROPERTY(QString libName READ libName WRITE setLibName NOTIFY libNameChanged)
    Q_PROPERTY(bool exportSymbol READ exportSymbol WRITE setExportSymbol NOTIFY exportSymbolChanged)
    Q_PROPERTY(bool exportFootprint READ exportFootprint WRITE setExportFootprint NOTIFY exportFootprintChanged)
    Q_PROPERTY(bool exportModel3D READ exportModel3D WRITE setExportModel3D NOTIFY exportModel3DChanged)
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setDarkMode NOTIFY darkModeChanged)

public:
    explicit MainController(QObject *parent = nullptr);
    ~MainController() override;

    // Getter methods
    QStringList componentList() const { return m_componentList; }
    int componentCount() const { return m_componentList.count(); }
    QString bomFilePath() const { return m_bomFilePath; }
    QString bomResult() const { return m_bomResult; }
    QString outputPath() const { return m_outputPath; }
    QString libName() const { return m_libName; }
    bool exportSymbol() const { return m_exportSymbol; }
    bool exportFootprint() const { return m_exportFootprint; }
    bool exportModel3D() const { return m_exportModel3D; }
    int progress() const { return m_progress; }
    QString status() const { return m_status; }
    bool isExporting() const { return m_isExporting; }
    bool isDarkMode() const { return m_isDarkMode; }

    /**
     * @brief 解压 gzip 数据
     * @param compressedData 压缩的数据
     * @return QByteArray 解压后的数据
     */
    QByteArray decompressGzip(const QByteArray &compressedData);

    // Setter methods
    void setOutputPath(const QString &path);
    void setLibName(const QString &name);
    void setExportSymbol(bool enabled);
    void setExportFootprint(bool enabled);
    void setExportModel3D(bool enabled);

    /**
     * @brief 设置深色模式
     * @param darkMode 是否为深色模式
     */
    Q_INVOKABLE void setDarkMode(bool darkMode);

    /**
     * @brief 保存配置
     */
    Q_INVOKABLE void saveConfig();

    /**
     * @brief 重置配置
     */
    Q_INVOKABLE void resetConfig();

public slots:
    /**
     * @brief 添加元件到列表
     * @param componentId 元件ID
     */
    Q_INVOKABLE void addComponent(const QString &componentId);

    /**
     * @brief 从列表中移除元件
     * @param index 元件索引
     */
    Q_INVOKABLE void removeComponent(int index);

    /**
     * @brief 清空元件列表
     */
    Q_INVOKABLE void clearComponentList();

    /**
     * @brief 选择BOM文件
     * @param filePath 文件路径
     */
    Q_INVOKABLE void selectBomFile(const QString &filePath);

    /**
     * @brief 选择输出路径
     * @param path 路径
     */
    Q_INVOKABLE void selectOutputPath(const QString &path);

    /**
     * @brief 开始导出
     */
    Q_INVOKABLE void startExport();

    /**
     * @brief 取消导出
     */
    Q_INVOKABLE void cancelExport();

signals:
    // Property change signals
    void componentListChanged();
    void componentCountChanged();
    void bomFilePathChanged();
    void bomResultChanged();
    void outputPathChanged();
    void libNameChanged();
    void exportSymbolChanged();
    void exportFootprintChanged();
    void exportModel3DChanged();
    void progressChanged();
    void statusChanged();
    void isExportingChanged();
    void darkModeChanged();

    // Result signals
    void exportCompleted(int totalCount, int successCount);
    void exportFailed(const QString &errorMessage);
    void componentExported(const QString &componentId, bool success, const QString &message);

private:
    /**
     * @brief 验证元件ID格式
     * @param componentId 元件ID
     * @return bool 是否有效
     */
    bool validateComponentId(const QString &componentId) const;

    /**
     * @brief 从文本中智能提取元件编号
     * @param text 输入文本
     * @return QString 提取的元件编号，如果未找到则返回空字符串
     */
    QString extractComponentIdFromText(const QString &text) const;

    /**
     * @brief 检查元件是否已存在
     * @param componentId 元件ID
     * @return bool 是否存在
     */
    bool componentExists(const QString &componentId) const;

    /**
     * @brief 解析BOM文件
     * @param filePath 文件路径
     * @return QStringList 解析出的元件ID列表
     */
    QStringList parseBomFile(const QString &filePath);

    /**
     * @brief 更新进度
     * @param current 当前进度
     * @param total 总数
     */
    void updateProgress(int current, int total);

    /**
     * @brief 更新状态
     * @param message 状态消息
     */
    void updateStatus(const QString &message);

    /**
     * @brief 从配置管理器加载配置
     */
    void loadConfigFromManager();

    /**
     * @brief 保存配置到配置管理器
     */
    void saveConfigToManager();

    // 处理 EasyedaApi 信号的槽函数
    void handleComponentInfoFetched(const QJsonObject &data);
    void handleCadDataFetched(const QJsonObject &data);
    void handleModel3DFetched(const QString &uuid, const QByteArray &data);
    void handleFetchError(const QString &errorMessage);

    // 处理并行导出任务的槽函数
    void handleComponentExportFinished(const QString &componentId, bool success, const QString &message);

    /**
     * @brief 处理单个元件的转换
     */
    void processNextComponent();

    /**
     * @brief 导出符号库
     */
    bool exportSymbolLibrary();

    /**
     * @brief 导出封装库
     */
    bool exportFootprintLibrary();

    /**
     * @brief 导出 3D 模型
     */
    bool export3DModels();

    /**
     * @brief 处理数据收集完成
     */
    void handleDataCollected(const QString &componentId, bool success, const QString &message);

    /**
     * @brief 导出所有收集到的组件数据
     */
    bool exportAllCollectedData();

private:
    ConfigManager *m_configManager;
    QStringList m_componentList;
    QString m_bomFilePath;
    QString m_bomResult;
    QString m_outputPath;
    QString m_libName;
    bool m_exportSymbol;
    bool m_exportFootprint;
    bool m_exportModel3D;
    int m_progress;
    QString m_status;
    bool m_isExporting;
    bool m_isDarkMode;

    // 核心转换引擎
    EasyedaApi *m_easyedaApi;
    EasyedaImporter *m_easyedaImporter;
    ExporterSymbol *m_exporterSymbol;
    ExporterFootprint *m_exporterFootprint;
    Exporter3DModel *m_exporter3DModel;

    // 当前转换状态
    int m_currentComponentIndex;
    int m_successCount;
    int m_failureCount;

    // 待处理的 3D 模型信息
    Model3DData *m_pending3DModel;
    QString m_pending3DModelPath;
    bool m_objDownloaded; // 标记是否已下载 OBJ 数据

    // 并行转换支持
    QThreadPool *m_threadPool;
    QMutex *m_mutex; // 用于保护共享数据的互斥锁
    bool m_useParallelExport; // 是否使用并行导出

    // 数据收集支持
    struct CollectedComponentData {
        QString componentId;
        SymbolData symbolData;
        FootprintData footprintData;
        QByteArray objData;
        QByteArray stepData;
        bool success;
        QString errorMessage;
    };
    QList<CollectedComponentData> m_collectedComponents; // 收集到的组件数据
    bool m_isCollectingData; // 是否正在收集数据
};

} // namespace EasyKiConverter

#endif // MAINCONTROLLER_H