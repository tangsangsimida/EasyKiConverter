#pragma once

#include "services/ComponentCacheService.h"
#include "services/ComponentService.h"
#include "services/export/ParallelExportService.h"
#include "ui/viewmodels/ComponentListViewModel.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>

namespace EasyKiConverter {

struct ExportOverallProgress;
struct ExportTypeProgress;
struct ExportItemStatus;
struct ExportStatistics;

/**
 * @brief 导出进度视图模型
 *
 * 负责显示和管理导出进度的 UI 状态和操作
 */
class ExportProgressViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool isExporting READ isExporting NOTIFY isExportingChanged)
    Q_PROPERTY(int successCount READ successCount NOTIFY successCountChanged)
    Q_PROPERTY(int failureCount READ failureCount NOTIFY failureCountChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY totalCountChanged)
    Q_PROPERTY(QString filterMode READ filterMode WRITE setFilterMode NOTIFY filterModeChanged)
    Q_PROPERTY(QVariantList resultsList READ resultsList NOTIFY resultsListChanged)
    Q_PROPERTY(QVariantList filteredResultsList READ filteredResultsList NOTIFY filteredResultsListChanged)
    Q_PROPERTY(int filteredSuccessCount READ filteredSuccessCount NOTIFY filteredResultsListChanged)
    Q_PROPERTY(int filteredFailedCount READ filteredFailedCount NOTIFY filteredResultsListChanged)

public:
    explicit ExportProgressViewModel(ParallelExportService* exportService,
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

    int successCount() const {
        return m_successCount;
    }

    int failureCount() const {
        return m_failureCount;
    }

    int totalCount() const {
        return m_totalCount;
    }

    QString filterMode() const {
        return m_filterMode;
    }

    QVariantList resultsList() const {
        return m_resultsList;
    }

    QVariantList filteredResultsList() const;
    int filteredSuccessCount() const;
    int filteredFailedCount() const;

    Q_INVOKABLE QString getLastExportedPath() const;
    Q_INVOKABLE bool openLastExportedFolder();
    Q_INVOKABLE void clearCache();
    Q_INVOKABLE void resetExport();
    Q_INVOKABLE void setFilterMode(const QString& mode);

public slots:
    void startExport(const QStringList& componentIds,
                     const QString& outputPath,
                     const QString& libName,
                     bool exportSymbol,
                     bool exportFootprint,
                     bool exportModel3D,
                     bool exportPreviewImages,
                     bool exportDatasheet,
                     bool overwriteExistingFiles,
                     bool updateMode,
                     bool debugMode);
    void cancelExport();
    void handleCloseRequest();

signals:
    void progressChanged();
    void statusChanged();
    void isExportingChanged();
    void successCountChanged();
    void failureCountChanged();
    void totalCountChanged();
    void resultsListChanged();
    void filterModeChanged();
    void filteredResultsListChanged();

private slots:
    void handlePreloadProgressChanged(const PreloadProgress& progress);
    void handlePreloadCompleted(int successCount, int failedCount);
    void handleProgressChanged(const ExportOverallProgress& progress);
    void handleItemStatusChanged(const QString& componentId, const QString& typeName, const ExportItemStatus& status);
    void handleTypeCompleted(const QString& typeName, int successCount, int failedCount, int skippedCount);
    void handleCompleted(int successCount, int failedCount);
    void handleCancelled();
    void handleFailed(const QString& error);
    void flushPendingUpdates();

private:
    void updateProgress();
    void updateResultsList();
    void updateFilteredResults();
    void setStatus(const QString& status);
    void setIsExporting(bool exporting);
    void setProgress(int progress);

private:
    ParallelExportService* m_exportService;
    ComponentService* m_componentService;
    ComponentListViewModel* m_componentListViewModel;
    QString m_status;
    int m_progress;
    bool m_isExporting;
    QStringList m_componentIds;
    int m_totalCount;
    int m_successCount;
    int m_failureCount;
    QString m_filterMode;
    QVariantList m_resultsList;
    QHash<QString, int> m_idToIndexMap;
    QTimer* m_throttleTimer;
    bool m_pendingUpdate;
};

}  // namespace EasyKiConverter
