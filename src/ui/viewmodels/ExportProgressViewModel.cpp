#include "ExportProgressViewModel.h"

#include "services/export/ParallelExportService.h"
#include "utils/FileUtils.h"
#include "utils/logging/LogMacros.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QTimer>

namespace EasyKiConverter {

ExportProgressViewModel::ExportProgressViewModel(ParallelExportService* exportService,
                                                 ComponentService* componentService,
                                                 ComponentListViewModel* componentListViewModel,
                                                 QObject* parent)
    : QObject(parent)
    , m_exportService(exportService)
    , m_componentService(componentService)
    , m_componentListViewModel(componentListViewModel)
    , m_status("Ready")
    , m_progress(0)
    , m_isExporting(false)
    , m_totalCount(0)
    , m_successCount(0)
    , m_failureCount(0)
    , m_filterMode("all")
    , m_pendingUpdate(false) {
    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setInterval(100);
    m_throttleTimer->setSingleShot(true);
    connect(m_throttleTimer, &QTimer::timeout, this, &ExportProgressViewModel::flushPendingUpdates);

    if (m_exportService) {
        connect(m_exportService,
                &ParallelExportService::preloadProgressChanged,
                this,
                &ExportProgressViewModel::handlePreloadProgressChanged);
        connect(m_exportService,
                &ParallelExportService::preloadCompleted,
                this,
                &ExportProgressViewModel::handlePreloadCompleted);
        connect(m_exportService,
                &ParallelExportService::progressChanged,
                this,
                &ExportProgressViewModel::handleProgressChanged);
        connect(m_exportService,
                &ParallelExportService::itemStatusChanged,
                this,
                &ExportProgressViewModel::handleItemStatusChanged);
        connect(m_exportService,
                &ParallelExportService::typeCompleted,
                this,
                &ExportProgressViewModel::handleTypeCompleted);
        connect(m_exportService, &ParallelExportService::completed, this, &ExportProgressViewModel::handleCompleted);
        connect(m_exportService, &ParallelExportService::cancelled, this, &ExportProgressViewModel::handleCancelled);
        connect(m_exportService, &ParallelExportService::failed, this, &ExportProgressViewModel::handleFailed);
    }

    if (m_componentService) {
        connect(m_componentService,
                &ComponentService::cadDataReady,
                this,
                [this](const QString& componentId, const ComponentData& data) {
                    Q_UNUSED(componentId);
                    Q_UNUSED(data);
                });
    }
}

ExportProgressViewModel::~ExportProgressViewModel() {}

void ExportProgressViewModel::startExport(const QStringList& componentIds,
                                          const QString& outputPath,
                                          const QString& libName,
                                          bool exportSymbol,
                                          bool exportFootprint,
                                          bool exportModel3D,
                                          bool exportPreviewImages,
                                          bool exportDatasheet,
                                          bool overwriteExistingFiles,
                                          bool updateMode,
                                          bool debugMode) {
    Q_UNUSED(outputPath);
    Q_UNUSED(libName);
    Q_UNUSED(exportSymbol);
    Q_UNUSED(exportFootprint);
    Q_UNUSED(exportModel3D);
    Q_UNUSED(exportPreviewImages);
    Q_UNUSED(exportDatasheet);
    Q_UNUSED(overwriteExistingFiles);
    Q_UNUSED(updateMode);
    Q_UNUSED(debugMode);

    qDebug() << "ExportProgressViewModel: Starting export for" << componentIds.size() << "components";

    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }

    m_componentIds = componentIds;
    m_totalCount = componentIds.size();
    m_successCount = 0;
    m_failureCount = 0;
    m_resultsList.clear();
    m_idToIndexMap.clear();

    for (int i = 0; i < componentIds.size(); ++i) {
        m_idToIndexMap[componentIds[i]] = i;
        QVariantMap item;
        item["componentId"] = componentIds[i];
        item["status"] = "pending";
        item["symbolSuccess"] = false;
        item["footprintSuccess"] = false;
        item["model3DSuccess"] = false;
        item["previewSuccess"] = false;
        item["datasheetSuccess"] = false;
        m_resultsList.append(item);
    }

    setIsExporting(true);
    setStatus("Initializing export...");
    emit resultsListChanged();
    emit totalCountChanged();
}

void ExportProgressViewModel::cancelExport() {
    qDebug() << "ExportProgressViewModel: Cancelling export";

    if (!m_exportService) {
        return;
    }

    m_exportService->cancelExport();
}

void ExportProgressViewModel::handleCloseRequest() {
    if (m_isExporting) {
        cancelExport();
    }
}

void ExportProgressViewModel::handlePreloadProgressChanged(const PreloadProgress& progress) {
    setStatus(QString("Preloading... %1/%2").arg(progress.completedCount).arg(progress.totalCount));

    // Update progress based on preload stage
    if (progress.totalCount > 0) {
        setProgress((progress.completedCount * 100) / progress.totalCount / 2);  // Preload is first 50%
    }
}

void ExportProgressViewModel::handlePreloadCompleted(int successCount, int failedCount) {
    Q_UNUSED(successCount);
    Q_UNUSED(failedCount);
    setStatus("Starting export...");
}

void ExportProgressViewModel::handleProgressChanged(const ExportOverallProgress& progress) {
    // Update progress based on overall stage
    switch (progress.currentStage) {
        case ExportOverallProgress::Stage::Preloading:
            setStatus("Preloading components...");
            if (progress.totalComponents > 0) {
                setProgress((progress.preloadProgress.completedCount * 100) / progress.totalComponents / 2);
            }
            break;
        case ExportOverallProgress::Stage::Exporting:
            setStatus("Exporting components...");
            if (progress.totalComponents > 0) {
                int exportProgress = (progress.preloadProgress.completedCount * 100) / progress.totalComponents;
                setProgress(50 + exportProgress / 2);
            }
            break;
        case ExportOverallProgress::Stage::Completed:
            setProgress(100);
            setStatus("Export completed");
            break;
        case ExportOverallProgress::Stage::Cancelled:
            setStatus("Export cancelled");
            break;
        case ExportOverallProgress::Stage::Failed:
            setStatus("Export failed");
            break;
        default:
            break;
    }

    m_pendingUpdate = true;
    m_throttleTimer->start();
}

void ExportProgressViewModel::handleItemStatusChanged(const QString& componentId,
                                                      const QString& typeName,
                                                      const ExportItemStatus& status) {
    if (m_idToIndexMap.contains(componentId)) {
        int index = m_idToIndexMap[componentId];
        QVariantMap result = m_resultsList[index].toMap();

        // Convert enum status to string
        QString statusStr;
        switch (status.status) {
            case ExportItemStatus::Status::Pending:
                statusStr = "pending";
                break;
            case ExportItemStatus::Status::InProgress:
                statusStr = "in_progress";
                break;
            case ExportItemStatus::Status::Success:
                statusStr = "success";
                break;
            case ExportItemStatus::Status::Failed:
                statusStr = "failed";
                break;
            case ExportItemStatus::Status::Skipped:
                statusStr = "skipped";
                break;
            default:
                statusStr = "unknown";
        }

        result["status"] = statusStr;

        if (typeName == "Symbol") {
            result["symbolSuccess"] = (status.status == ExportItemStatus::Status::Success);
        } else if (typeName == "Footprint") {
            result["footprintSuccess"] = (status.status == ExportItemStatus::Status::Success);
        } else if (typeName == "Model3D") {
            result["model3DSuccess"] = (status.status == ExportItemStatus::Status::Success);
        } else if (typeName == "PreviewImages") {
            result["previewSuccess"] = (status.status == ExportItemStatus::Status::Success);
        } else if (typeName == "Datasheet") {
            result["datasheetSuccess"] = (status.status == ExportItemStatus::Status::Success);
        }

        if (status.status == ExportItemStatus::Status::Failed) {
            result["error"] = status.errorMessage;
        }

        m_resultsList[index] = result;
        m_pendingUpdate = true;
        m_throttleTimer->start();
    }
}

void ExportProgressViewModel::handleTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    Q_UNUSED(typeName);
    Q_UNUSED(successCount);
    Q_UNUSED(failedCount);
    Q_UNUSED(skippedCount);
    qDebug() << "Type completed:" << typeName << "success=" << successCount << "failed=" << failedCount;
}

void ExportProgressViewModel::handleCompleted(int successCount, int failedCount) {
    qDebug() << "Export completed: success=" << successCount << "failed=" << failedCount;

    m_successCount = successCount;
    m_failureCount = failedCount;
    setIsExporting(false);
    setProgress(100);
    setStatus(failedCount > 0 ? QString("Completed with %1 errors").arg(failedCount) : "Export completed successfully");

    emit successCountChanged();
    emit failureCountChanged();
    flushPendingUpdates();
}

void ExportProgressViewModel::handleCancelled() {
    qDebug() << "Export cancelled";
    setIsExporting(false);
    setStatus("Export cancelled");
}

void ExportProgressViewModel::handleFailed(const QString& error) {
    qWarning() << "Export failed:" << error;
    setIsExporting(false);
    setStatus(QString("Export failed: %1").arg(error));
}

void ExportProgressViewModel::flushPendingUpdates() {
    if (m_pendingUpdate) {
        m_pendingUpdate = false;
        updateResultsList();
        emit resultsListChanged();
    }
}

void ExportProgressViewModel::updateResultsList() {
    int success = 0;
    int failure = 0;
    for (const auto& item : m_resultsList) {
        QVariantMap map = item.toMap();
        if (map.value("status") == "success") {
            success++;
        } else if (map.value("status") == "failed") {
            failure++;
        }
    }
    m_successCount = success;
    m_failureCount = failure;
    emit successCountChanged();
    emit failureCountChanged();
}

void ExportProgressViewModel::updateFilteredResults() {
    emit filteredResultsListChanged();
}

QVariantList ExportProgressViewModel::filteredResultsList() const {
    if (m_filterMode == "all") {
        return m_resultsList;
    }

    QVariantList filtered;
    for (const auto& item : m_resultsList) {
        QVariantMap map = item.toMap();
        if (m_filterMode == "success" && map.value("status") == "success") {
            filtered.append(item);
        } else if (m_filterMode == "failed" && map.value("status") == "failed") {
            filtered.append(item);
        }
    }
    return filtered;
}

int ExportProgressViewModel::filteredSuccessCount() const {
    if (m_filterMode == "all" || m_filterMode == "success") {
        return m_successCount;
    }
    return 0;
}

int ExportProgressViewModel::filteredFailedCount() const {
    if (m_filterMode == "all" || m_filterMode == "failed") {
        return m_failureCount;
    }
    return 0;
}

void ExportProgressViewModel::setFilterMode(const QString& mode) {
    if (m_filterMode != mode) {
        m_filterMode = mode;
        emit filterModeChanged();
        updateFilteredResults();
    }
}

QString ExportProgressViewModel::getLastExportedPath() const {
    if (m_exportService) {
        return m_exportService->outputPath();
    }
    return QString();
}

bool ExportProgressViewModel::openLastExportedFolder() {
    QString path = getLastExportedPath();
    if (path.isEmpty()) {
        return false;
    }

    if (!QDir(path).exists()) {
        QDir().mkpath(path);
    }

    FileUtils utils;
    return utils.openFolder(path);
}

void ExportProgressViewModel::clearCache() {
    ComponentCacheService::instance()->clearAllCache();
}

void ExportProgressViewModel::resetExport() {
    setIsExporting(false);
    setProgress(0);
    setStatus("Ready");
    m_resultsList.clear();
    m_idToIndexMap.clear();
    m_totalCount = 0;
    m_successCount = 0;
    m_failureCount = 0;

    emit resultsListChanged();
    emit successCountChanged();
    emit failureCountChanged();
    emit totalCountChanged();
}

void ExportProgressViewModel::setStatus(const QString& status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void ExportProgressViewModel::setIsExporting(bool exporting) {
    if (m_isExporting != exporting) {
        m_isExporting = exporting;
        emit isExportingChanged();
    }
}

void ExportProgressViewModel::setProgress(int progress) {
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged();
    }
}

}  // namespace EasyKiConverter
