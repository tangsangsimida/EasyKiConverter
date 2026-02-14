#ifndef EXPORTPROGRESSVIEWMODEL_H
#define EXPORTPROGRESSVIEWMODEL_H

#include "services/ComponentService.h"
#include "services/ExportService.h"
#include "services/ExportService_Pipeline.h"
#include "ui/viewmodels/ComponentListViewModel.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVariantList>

namespace EasyKiConverter {

class ExportProgressViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    Q_PROPERTY(bool isStopping READ isStopping NOTIFY isStoppingChanged)
    Q_PROPERTY(int successCount READ successCount NOTIFY successCountChanged)
    Q_PROPERTY(int successSymbolCount READ successSymbolCount NOTIFY successCountChanged)
    Q_PROPERTY(int successFootprintCount READ successFootprintCount NOTIFY successCountChanged)
    Q_PROPERTY(int successModel3DCount READ successModel3DCount NOTIFY successCountChanged)
    Q_PROPERTY(int failureCount READ failureCount NOTIFY failureCountChanged)
    Q_PROPERTY(int fetchProgress READ fetchProgress NOTIFY fetchProgressChanged)
    Q_PROPERTY(int processProgress READ processProgress NOTIFY processProgressChanged)
    Q_PROPERTY(int writeProgress READ writeProgress NOTIFY writeProgressChanged)
    Q_PROPERTY(QVariantList resultsList READ resultsList NOTIFY resultsListChanged)
    Q_PROPERTY(bool hasStatistics READ hasStatistics NOTIFY statisticsChanged)
    Q_PROPERTY(QString statisticsReportPath READ statisticsReportPath NOTIFY statisticsChanged)
    Q_PROPERTY(QString statisticsSummary READ statisticsSummary NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsTotal READ statisticsTotal NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsSuccess READ statisticsSuccess NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsFailed READ statisticsFailed NOTIFY statisticsChanged)
    Q_PROPERTY(double statisticsSuccessRate READ statisticsSuccessRate NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 statisticsTotalDuration READ statisticsTotalDuration NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 statisticsAvgFetchTime READ statisticsAvgFetchTime NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 statisticsAvgProcessTime READ statisticsAvgProcessTime NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 statisticsAvgWriteTime READ statisticsAvgWriteTime NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsTotalNetworkRequests READ statisticsTotalNetworkRequests NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsTotalRetries READ statisticsTotalRetries NOTIFY statisticsChanged)
    Q_PROPERTY(qint64 statisticsAvgNetworkLatency READ statisticsAvgNetworkLatency NOTIFY statisticsChanged)
    Q_PROPERTY(int statisticsRateLimitHitCount READ statisticsRateLimitHitCount NOTIFY statisticsChanged)

public:
    explicit ExportProgressViewModel(ExportService* exportService,
                                     ComponentService* componentService,
                                     ComponentListViewModel* componentListViewModel,
                                     QObject* parent = nullptr);
    ~ExportProgressViewModel() override;

    int progress() const {
        return m_progress;
    }
    QString status() const {
        return m_status;
    }
    bool isExporting() const {
        return m_isExporting;
    }
    bool isStopping() const {
        return m_isStopping;
    }
    int successCount() const {
        return m_successCount;
    }
    int successSymbolCount() const {
        return m_successSymbolCount;
    }
    int successFootprintCount() const {
        return m_successFootprintCount;
    }
    int successModel3DCount() const {
        return m_successModel3DCount;
    }
    int failureCount() const {
        return m_failureCount;
    }
    int fetchProgress() const {
        return m_fetchProgress;
    }
    int processProgress() const {
        return m_processProgress;
    }
    int writeProgress() const {
        return m_writeProgress;
    }
    QVariantList resultsList() const {
        return m_resultsList;
    }
    bool hasStatistics() const {
        return m_hasStatistics;
    }
    QString statisticsReportPath() const {
        return m_statisticsReportPath;
    }
    QString statisticsSummary() const {
        return m_statisticsSummary;
    }

    int statisticsTotal() const {
        return m_resultsList.size();
    }
    int statisticsSuccess() const {
        return m_successCount;
    }
    int statisticsFailed() const {
        return m_failureCount;
    }
    double statisticsSuccessRate() const {
        return m_resultsList.isEmpty() ? 0.0 : (m_successCount * 100.0 / m_resultsList.size());
    }

    qint64 statisticsTotalDuration() const {
        return m_statistics.totalDurationMs;
    }
    qint64 statisticsAvgFetchTime() const {
        return m_statistics.avgFetchTimeMs;
    }
    qint64 statisticsAvgProcessTime() const {
        return m_statistics.avgProcessTimeMs;
    }
    qint64 statisticsAvgWriteTime() const {
        return m_statistics.avgWriteTimeMs;
    }
    int statisticsTotalNetworkRequests() const {
        return m_statistics.totalNetworkRequests;
    }
    int statisticsTotalRetries() const {
        return m_statistics.totalRetries;
    }
    qint64 statisticsAvgNetworkLatency() const {
        return m_statistics.avgNetworkLatencyMs;
    }
    int statisticsRateLimitHitCount() const {
        return m_statistics.rateLimitHitCount;
    }

    void setUsePipelineMode(bool usePipeline);

public slots:
    Q_INVOKABLE void startExport(const QStringList& componentIds,
                                 const QString& outputPath,
                                 const QString& libName,
                                 bool exportSymbol,
                                 bool exportFootprint,
                                 bool exportModel3D,
                                 bool overwriteExistingFiles,
                                 bool updateMode,
                                 bool debugMode);
    Q_INVOKABLE void retryFailedComponents();
    Q_INVOKABLE void retryComponent(const QString& componentId);
    Q_INVOKABLE void cancelExport();
    Q_INVOKABLE void resetExport();

signals:
    void progressChanged();
    void statusChanged();
    void isExportingChanged();
    void isStoppingChanged();
    void successCountChanged();
    void failureCountChanged();
    void exportCompleted(int totalCount, int successCount);
    void componentExported(const QString& componentId, bool success, const QString& message);
    void fetchProgressChanged();
    void processProgressChanged();
    void writeProgressChanged();
    void resultsListChanged();
    void statisticsChanged();

private slots:
    void handleExportProgress(int current, int total);
    void handleExportCompleted(int totalCount, int successCount);
    void handleExportFailed(const QString& error);
    void handleComponentExported(const QString& componentId,
                                 bool success,
                                 const QString& message,
                                 int stage = -1,
                                 bool symbolSuccess = false,
                                 bool footprintSuccess = false,
                                 bool model3DSuccess = false);
    void handleComponentDataFetched(const QString& componentId, const ComponentData& data);
    void handleAllComponentsDataCollected(const QList<ComponentData>& componentDataList);
    void handlePipelineProgressUpdated(const PipelineProgress& progress);
    void handleStatisticsReportGenerated(const QString& reportPath, const ExportStatistics& statistics);
    void flushPendingUpdates();

private:
    QString getStatusString(int stage,
                            bool success,
                            bool symbolSuccess,
                            bool footprintSuccess,
                            bool model3DSuccess) const;
    void prepopulateResultsList(const QStringList& componentIds);
    void startExportInternal(const QStringList& componentIds, bool isRetry);
    void updateStatistics();
    void showExportCompleteNotification();
    void initializeSystemTrayIcon();

private:
    ExportService* m_exportService;
    ComponentService* m_componentService;
    ComponentListViewModel* m_componentListViewModel;
    QString m_status;
    int m_progress;
    bool m_isExporting;
    bool m_isStopping;
    QStringList m_componentIds;
    int m_fetchedCount;
    QList<ComponentData> m_collectedData;
    ExportOptions m_exportOptions;
    int m_fetchProgress;
    int m_processProgress;
    int m_writeProgress;
    bool m_usePipelineMode;
    QVariantList m_resultsList;
    QHash<QString, int> m_idToIndexMap;
    QTimer* m_throttleTimer;
    bool m_pendingUpdate;
    bool m_hasStatistics;
    QString m_statisticsReportPath;
    QString m_statisticsSummary;
    ExportStatistics m_statistics;
    int m_successCount;
    int m_successSymbolCount;
    int m_successFootprintCount;
    int m_successModel3DCount;
    int m_failureCount;
    QSystemTrayIcon* m_systemTrayIcon;
    QMenu* m_trayMenu;
};
}  // namespace EasyKiConverter

#endif  // EXPORTPROGRESSVIEWMODEL_H
