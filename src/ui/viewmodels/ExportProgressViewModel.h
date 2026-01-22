#ifndef EXPORTPROGRESSVIEWMODEL_H
#define EXPORTPROGRESSVIEWMODEL_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QHash>
#include <QTimer>
#include "src/services/ExportService.h"
#include "src/services/ComponentService.h"
#include "src/services/ExportService_Pipeline.h"

namespace EasyKiConverter
{

    /**
     * @brief 导出进度视图模型类
     *
     * 负责管理导出进度和结果相关的 UI 状态
     */
    class ExportProgressViewModel : public QObject
    {
        Q_OBJECT
        Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
        Q_PROPERTY(QString status READ status NOTIFY statusChanged)
        Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
        Q_PROPERTY(int successCount READ successCount NOTIFY successCountChanged)
        Q_PROPERTY(int failureCount READ failureCount NOTIFY failureCountChanged)
        Q_PROPERTY(int fetchProgress READ fetchProgress NOTIFY fetchProgressChanged)
        Q_PROPERTY(int processProgress READ processProgress NOTIFY processProgressChanged)
        Q_PROPERTY(int writeProgress READ writeProgress NOTIFY writeProgressChanged)
        Q_PROPERTY(QVariantList resultsList READ resultsList NOTIFY resultsListChanged)

    public:
        explicit ExportProgressViewModel(ExportService *exportService, ComponentService *componentService, QObject *parent = nullptr);
        ~ExportProgressViewModel() override;

        // Getter 方法
        int progress() const { return m_progress; }
        QString status() const { return m_status; }
        bool isExporting() const { return m_isExporting; }
        int successCount() const { return m_successCount; }
        int failureCount() const { return m_failureCount; }
        int fetchProgress() const { return m_fetchProgress; }
        int processProgress() const { return m_processProgress; }
        int writeProgress() const { return m_writeProgress; }
        QVariantList resultsList() const { return m_resultsList; }

        // Setter 方法
        void setUsePipelineMode(bool usePipeline);

    public slots:
        Q_INVOKABLE void startExport(const QStringList &componentIds, const QString &outputPath, const QString &libName, bool exportSymbol, bool exportFootprint, bool exportModel3D, bool overwriteExistingFiles, bool updateMode, bool debugMode);
        Q_INVOKABLE void cancelExport();

    signals:
        void progressChanged();
        void statusChanged();
        void isExportingChanged();
        void successCountChanged();
        void failureCountChanged();
        void exportCompleted(int totalCount, int successCount);
        void componentExported(const QString &componentId, bool success, const QString &message);
        void fetchProgressChanged();
        void processProgressChanged();
        void writeProgressChanged();
        void resultsListChanged();

    private slots:
        void handleExportProgress(int current, int total);
        void handleExportCompleted(int totalCount, int successCount);
        void handleExportFailed(const QString &error);
        void handleComponentExported(const QString &componentId, bool success, const QString &message, int stage = -1);
        void handleComponentDataFetched(const QString &componentId, const ComponentData &data);
        void handleAllComponentsDataCollected(const QList<ComponentData> &componentDataList);
        void handlePipelineProgressUpdated(const PipelineProgress &progress);

        // 节流定时器槽函数
        void flushPendingUpdates();

    private:
        /**
         * @brief 根据阶段获取状态字符串
         * @param stage 阶段（0=Fetch, 1=Process, 2=Write, -1=未知）
         * @param success 是否成功
         * @return 状态字符串
         */
        QString getStatusString(int stage, bool success) const;

        /**
         * @brief 预填充结果列表
         * @param componentIds 元件ID列表
         */
        void prepopulateResultsList(const QStringList &componentIds);

    private:
        ExportService *m_exportService;
        ComponentService *m_componentService;
        QString m_status;
        int m_progress;
        bool m_isExporting;
        int m_successCount;
        int m_failureCount;
        QStringList m_componentIds;
        int m_fetchedCount;
        QList<ComponentData> m_collectedData;
        ExportOptions m_exportOptions;
        int m_fetchProgress;
        int m_processProgress;
        int m_writeProgress;
        bool m_usePipelineMode;
        QVariantList m_resultsList;

        // 性能优化：哈希表用于快速查找
        QHash<QString, int> m_idToIndexMap;

        // UI 节流：定时器和待更新列表
        QTimer *m_throttleTimer;
        bool m_pendingUpdate;
    };
} // namespace EasyKiConverter

#endif // EXPORTPROGRESSVIEWMODEL_H
