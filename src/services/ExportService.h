#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include "models/ComponentData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QAtomicInt>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>

namespace EasyKiConverter {

/**
 * @brief 导出选项结构
 */
struct ExportOptions {
    QString outputPath;
    QString libName;
    bool exportSymbol;
    bool exportFootprint;
    bool exportModel3D;
    bool overwriteExistingFiles;
    bool updateMode;
    bool debugMode;

    ExportOptions()
        : exportSymbol(true)
        , exportFootprint(true)
        , exportModel3D(true)
        , overwriteExistingFiles(false)
        , updateMode(false)
        , debugMode(false) {}
};

class ExportService : public QObject {
    Q_OBJECT

public:
    explicit ExportService(QObject* parent = nullptr);
    virtual ~ExportService() override;

    bool exportSymbol(const SymbolData& symbol, const QString& filePath);
    bool exportFootprint(const FootprintData& footprint, const QString& filePath);
    bool export3DModel(const Model3DData& model, const QString& filePath);

    virtual void executeExportPipeline(const QStringList& componentIds, const ExportOptions& options);
    virtual void retryExport(const QStringList& componentIds, const ExportOptions& options);
    void executeExportPipelineWithData(const QList<ComponentData>& componentDataList, const ExportOptions& options);
    void executeExportPipelineWithDataParallel(const QList<ComponentData>& componentDataList,
                                               const ExportOptions& options);

    virtual void cancelExport();
    virtual bool waitForCompletion(int timeoutMs = 5000);
    void setExportOptions(const ExportOptions& options);
    ExportOptions getExportOptions() const;
    bool isExporting() const;

signals:
    void exportProgress(int current, int total);
    /**
     * @brief 元件导出完成信号
     *
     * @param componentId 元件ID
     * @param success 是否成功
     * @param message 消息
     * @param stage 阶段
     * @param symbolSuccess 符号是否导出成功
     * @param footprintSuccess 封装是否导出成功
     * @param model3DSuccess 3D模型是否导出成功
     */
    void componentExported(const QString& componentId,
                           bool success,
                           const QString& message,
                           int stage = -1,
                           bool symbolSuccess = false,
                           bool footprintSuccess = false,
                           bool model3DSuccess = false);
    void exportCompleted(int totalCount, int successCount);
    void exportFailed(const QString& error);

private slots:
    void handleExportTaskFinished(const QString& componentId, bool success, const QString& message);
    void handleParallelExportTaskFinished(const QString& componentId, bool success, const QString& message);

private:
    bool exportSymbolLibrary(const QList<SymbolData>& symbols, const QString& libName, const QString& filePath);
    bool exportFootprintLibrary(const QList<FootprintData>& footprints,
                                const QString& libName,
                                const QString& filePath);
    bool export3DModels(const QList<Model3DData>& models, const QString& outputPath);
    void updateProgress(int current, int total);
    bool fileExists(const QString& filePath) const;
    bool createOutputDirectory(const QString& path) const;
    void processNextExport();

protected:
    QThreadPool* m_threadPool;
    QMutex* m_mutex;

    // 状态标志位（使用原子变量确保无锁访问，防止 UI 阻塞）
    QAtomicInt m_isExporting;
    QAtomicInt m_isStopping;
    ExportOptions m_options;

    int m_currentProgress;
    int m_totalProgress;
    int m_successCount;
    int m_failureCount;

private:
    class ExporterSymbol* m_symbolExporter;
    class ExporterFootprint* m_footprintExporter;
    class Exporter3DModel* m_modelExporter;

    struct ExportData {
        QString componentId;
        SymbolData symbolData;
        FootprintData footprintData;
        Model3DData model3DData;
        bool success;
        QString errorMessage;
    };

    QList<ExportData> m_exportDataList;

    bool m_parallelExporting;
    int m_parallelCompletedCount;
    int m_parallelTotalCount;
    QMap<QString, bool> m_parallelExportStatus;
};

}  // namespace EasyKiConverter

#endif  // EXPORTSERVICE_H
