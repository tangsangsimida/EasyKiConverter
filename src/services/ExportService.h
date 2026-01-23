#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QThreadPool>
#include <QMutex>
#include "models/ComponentData.h"
#include "models/SymbolData.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"

namespace EasyKiConverter
{

    /**
     * @brief 导出选项结构
     */
    struct ExportOptions
    {
        QString outputPath;
        QString libName;
        bool exportSymbol;
        bool exportFootprint;
        bool exportModel3D;
        bool overwriteExistingFiles;
        bool updateMode; // 更新模式：替换相同符号，保留不同符号
        bool debugMode;  // 调试模式：导出调试数据到 debug 文件�?

        ExportOptions()
            : exportSymbol(true), exportFootprint(true), exportModel3D(true), overwriteExistingFiles(false), updateMode(false), debugMode(false)
        {
        }
    };

    /**
     * @brief 导出服务�?
     *
     * 负责处理导出相关的业务逻辑，异步处�?
     * 不依赖任�?UI 组件
     */
    class ExportService : public QObject
    {
        Q_OBJECT

    public:
        /**
         * @brief 构造函�?
         *
         * @param parent 父对�?
         */
        explicit ExportService(QObject *parent = nullptr);

        /**
         * @brief 析构函数
         */
        ~ExportService() override;

        /**
         * @brief 导出符号
         *
         * @param symbol 符号数据
         * @param filePath 输出文件路径
         * @return bool 是否成功
         */
        bool exportSymbol(const SymbolData &symbol, const QString &filePath);

        /**
         * @brief 导出封装
         *
         * @param footprint 封装数据
         * @param filePath 输出文件路径
         * @return bool 是否成功
         */
        bool exportFootprint(const FootprintData &footprint, const QString &filePath);

        /**
         * @brief 导出3D模型
         *
         * @param model 3D模型数据
         * @param filePath 输出文件路径
         * @return bool 是否成功
         */
        bool export3DModel(const Model3DData &model, const QString &filePath);

        /**
         * @brief 执行批量导出流程
         *
         * @param componentIds 元件ID列表
         * @param options 导出选项
         */
        void executeExportPipeline(const QStringList &componentIds, const ExportOptions &options);

        /**
         * @brief 使用已收集的数据执行批量导出流程
         *
         * @param componentDataList 元件数据列表
         * @param options 导出选项
         */
        void executeExportPipelineWithData(const QList<ComponentData> &componentDataList, const ExportOptions &options);

        /**
         * @brief 使用已收集的数据执行并行批量导出流程
         *
         * @param componentDataList 元件数据列表
         * @param options 导出选项
         */
        void executeExportPipelineWithDataParallel(const QList<ComponentData> &componentDataList, const ExportOptions &options);

        /**
         * @brief 取消导出
         */
        void cancelExport();

        /**
         * @brief 设置导出选项
         *
         * @param options 导出选项
         */
        void setExportOptions(const ExportOptions &options);

        /**
         * @brief 获取导出选项
         *
         * @return ExportOptions 导出选项
         */
        ExportOptions getExportOptions() const;

        /**
         * @brief 是否正在导出
         *
         * @return bool 是否正在导出
         */
        bool isExporting() const;

    signals:
        /**
         * @brief 导出进度信号
         *
         * @param current 当前进度
         * @param total 总数
         */
        void exportProgress(int current, int total);

        /**
         * @brief 元件导出完成信号
         *
         * @param componentId 元件ID
         * @param success 是否成功
         * @param message 消息
         * @param stage 阶段（可选，用于流水线模式：0=Fetch, 1=Process, 2=Write, -1=未知�?
         */
        void componentExported(const QString &componentId, bool success, const QString &message, int stage = -1);

        /**
         * @brief 导出完成信号
         *
         * @param totalCount 总数
         * @param successCount 成功�?
         */
        void exportCompleted(int totalCount, int successCount);

        /**
         * @brief 导出失败信号
         *
         * @param error 错误信息
         */
        void exportFailed(const QString &error);

    private slots:
        /**
         * @brief 处理导出任务完成
         *
         * @param componentId 元件ID
         * @param success 是否成功
         * @param message 消息
         */
        void handleExportTaskFinished(const QString &componentId, bool success, const QString &message);

        /**
         * @brief 处理并行导出任务完成
         *
         * @param componentId 元件ID
         * @param success 是否成功
         * @param message 消息
         */
        void handleParallelExportTaskFinished(const QString &componentId, bool success, const QString &message);

    private:
        /**
         * @brief 导出符号�?
         *
         * @param symbols 符号列表
         * @param libName 库名�?
         * @param filePath 输出文件路径
         * @return bool 是否成功
         */
        bool exportSymbolLibrary(const QList<SymbolData> &symbols, const QString &libName, const QString &filePath);

        /**
         * @brief 导出封装�?
         *
         * @param footprints 封装列表
         * @param libName 库名�?
         * @param filePath 输出文件路径
         * @return bool 是否成功
         */
        bool exportFootprintLibrary(const QList<FootprintData> &footprints, const QString &libName, const QString &filePath);

        /**
         * @brief 导出3D模型
         *
         * @param models 3D模型列表
         * @param outputPath 输出路径
         * @return bool 是否成功
         */
        bool export3DModels(const QList<Model3DData> &models, const QString &outputPath);

        /**
         * @brief 更新进度
         *
         * @param current 当前进度
         * @param total 总数
         */
        void updateProgress(int current, int total);

        /**
         * @brief 检查文件是否存�?
         *
         * @param filePath 文件路径
         * @return bool 是否存在
         */
        bool fileExists(const QString &filePath) const;

        /**
         * @brief 创建输出目录
         *
         * @param path 路径
         * @return bool 是否成功
         */
        bool createOutputDirectory(const QString &path) const;

        /**
         * @brief 处理下一个导出任�?
         */
        void processNextExport();

    private:
        // 导出�?
        class ExporterSymbol *m_symbolExporter;
        class ExporterFootprint *m_footprintExporter;
        class Exporter3DModel *m_modelExporter;

        // 线程池和互斥�?
        QThreadPool *m_threadPool;
        QMutex *m_mutex;

        // 导出状�?
        bool m_isExporting;
        ExportOptions m_options;

        // 导出进度
        int m_currentProgress;
        int m_totalProgress;
        int m_successCount;
        int m_failureCount;

        // 收集的数�?
        struct ExportData
        {
            QString componentId;
            SymbolData symbolData;
            FootprintData footprintData;
            Model3DData model3DData;
            bool success;
            QString errorMessage;
        };
        QList<ExportData> m_exportDataList;

        // 并行导出状�?
        bool m_parallelExporting;
        int m_parallelCompletedCount;
        int m_parallelTotalCount;
        QMap<QString, bool> m_parallelExportStatus;
    };

} // namespace EasyKiConverter

#endif // EXPORTSERVICE_H
