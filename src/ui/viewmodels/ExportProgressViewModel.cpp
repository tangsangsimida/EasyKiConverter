#include "ExportProgressViewModel.h"

#include "services/export/ExportProgress.h"
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
    , m_pendingUpdate(false)
    , m_fetchProgress(0)
    , m_processProgress(0)
    , m_writeProgress(0) {
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
    qDebug() << "ExportProgressViewModel: Starting export for" << componentIds.size() << "components";

    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }

    if (!m_exportService) {
        qWarning() << "ParallelExportService is not available";
        setStatus("Export service not available");
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "No components to export";
        setStatus("No components to export");
        return;
    }

    // Store component IDs
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

    // Build export options
    ExportOptions options;
    options.outputPath = outputPath;
    options.libName = libName;
    options.exportSymbol = exportSymbol;
    options.exportFootprint = exportFootprint;
    options.exportModel3D = exportModel3D;
    options.exportPreviewImages = exportPreviewImages;
    options.exportDatasheet = exportDatasheet;
    options.overwriteExistingFiles = overwriteExistingFiles;
    options.updateMode = updateMode;
    options.debugMode = debugMode;

    m_exportService->setOptions(options);
    m_exportService->setOutputPath(outputPath);

    setIsExporting(true);
    setStatus("Preloading component data...");
    emit resultsListChanged();
    emit totalCountChanged();

    // Start preload - actual export will begin after preload completes
    m_exportService->startPreload(componentIds);
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
    qDebug() << "ExportProgressViewModel: Preload completed. Success:" << successCount << "Failed:" << failedCount;

    if (failedCount > 0) {
        setStatus(QString("Preload completed with %1 errors").arg(failedCount));
    } else {
        setStatus("Starting export...");
    }

    // Start the actual export after preload completes
    if (m_exportService) {
        m_exportService->startExport();
    }
}

void ExportProgressViewModel::handleProgressChanged(const ExportOverallProgress& progress) {
    // Update stage progress values for QML binding
    switch (progress.currentStage) {
        case ExportOverallProgress::Stage::Preloading:
            setStatus("Preloading components...");
            // fetchProgress = preload percentage (0-100)
            m_fetchProgress = progress.preloadProgress.percentage();
            // process and write not started yet
            m_processProgress = 0;
            m_writeProgress = 0;
            setProgress(m_fetchProgress / 2);  // Preload is first half of overall progress
            break;
        case ExportOverallProgress::Stage::Exporting:
            setStatus("Exporting components...");
            // fetch completed
            m_fetchProgress = 100;
            // Calculate combined export progress for process
            m_processProgress = progress.overallPercentage();
            m_writeProgress = m_processProgress;  // Write happens alongside process
            setProgress(50 + m_processProgress / 2);  // Export is second half
            break;
        case ExportOverallProgress::Stage::Completed:
            m_fetchProgress = 100;
            m_processProgress = 100;
            m_writeProgress = 100;
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

    emit stageProgressChanged();
    m_pendingUpdate = true;
    m_throttleTimer->start();
}

void ExportProgressViewModel::handleItemStatusChanged(const QString& componentId,
                                                      const QString& typeName,
                                                      const ExportItemStatus& status) {
    if (m_idToIndexMap.contains(componentId)) {
        int index = m_idToIndexMap[componentId];
        QVariantMap result = m_resultsList[index].toMap();

        // Update individual type success flags
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

        // Compute overall status based on ALL enabled type success flags
        bool symbolDone = !result.contains("symbolSuccess") || result["symbolSuccess"].toBool();
        bool footprintDone = !result.contains("footprintSuccess") || result["footprintSuccess"].toBool();
        bool model3DDone = !result.contains("model3DSuccess") || result["model3DSuccess"].toBool();
        bool previewDone = !result.contains("previewSuccess") || result["previewSuccess"].toBool();
        bool datasheetDone = !result.contains("datasheetSuccess") || result["datasheetSuccess"].toBool();

        // Use individual type statuses for display (in_progress, failed, etc.)
        // Only update overall status for terminal states (success/failed/skipped)
        if (status.status == ExportItemStatus::Status::Success || status.status == ExportItemStatus::Status::Failed ||
            status.status == ExportItemStatus::Status::Skipped) {
            QString overallStatus;
            // Check all types that were attempted - if any failed, component failed
            bool anyFailed = (result.contains("symbolSuccess") && !result["symbolSuccess"].toBool()) ||
                             (result.contains("footprintSuccess") && !result["footprintSuccess"].toBool()) ||
                             (result.contains("model3DSuccess") && !result["model3DSuccess"].toBool()) ||
                             (result.contains("previewSuccess") && !result["previewSuccess"].toBool()) ||
                             (result.contains("datasheetSuccess") && !result["datasheetSuccess"].toBool());

            if (anyFailed) {
                overallStatus = "failed";
            } else {
                // All attempted types succeeded
                overallStatus = "success";
            }
            result["status"] = overallStatus;
        }
        // For InProgress/Pending, don't override the status - let it be set by the first type that starts

        m_resultsList[index] = result;

        // Update counts and emit immediately for real-time feedback
        updateResultsList();
        emit resultsListChanged();
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

int ExportProgressViewModel::filteredPendingCount() const {
    // Count items with "in_progress" or "pending" status
    int pending = 0;
    for (const auto& item : m_resultsList) {
        QVariantMap map = item.toMap();
        QString status = map.value("status").toString();
        if (status == "pending" || status == "in_progress") {
            pending++;
        }
    }
    return pending;
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
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;

    emit resultsListChanged();
    emit successCountChanged();
    emit failureCountChanged();
    emit totalCountChanged();
    emit stageProgressChanged();
}

void ExportProgressViewModel::retryComponent(const QString& componentId) {
    qDebug() << "Retry requested for component:" << componentId;

    if (!m_idToIndexMap.contains(componentId)) {
        qWarning() << "Component not found in results:" << componentId;
        return;
    }

    int index = m_idToIndexMap[componentId];
    QVariantMap result = m_resultsList[index].toMap();

    // Reset status to pending
    result["status"] = "pending";
    result["symbolSuccess"] = false;
    result["footprintSuccess"] = false;
    result["model3DSuccess"] = false;
    result["previewSuccess"] = false;
    result["datasheetSuccess"] = false;
    result["error"] = QString();
    m_resultsList[index] = result;

    // Update counts and notify UI
    updateResultsList();
    emit resultsListChanged();

    // Re-trigger export if not currently exporting
    if (!m_isExporting && m_exportService) {
        QStringList idsToRetry = {componentId};
        m_exportService->startPreload(idsToRetry);
    }
}

void ExportProgressViewModel::retryFailedComponents() {
    qDebug() << "Retry requested for all failed components";

    // Collect all failed component IDs
    QStringList failedIds;
    for (const auto& item : m_resultsList) {
        QVariantMap map = item.toMap();
        if (map.value("status") == "failed") {
            QString id = map.value("componentId").toString();
            failedIds.append(id);

            // Reset status to pending
            if (m_idToIndexMap.contains(id)) {
                int index = m_idToIndexMap[id];
                QVariantMap result = m_resultsList[index].toMap();
                result["status"] = "pending";
                result["symbolSuccess"] = false;
                result["footprintSuccess"] = false;
                result["model3DSuccess"] = false;
                result["previewSuccess"] = false;
                result["datasheetSuccess"] = false;
                result["error"] = QString();
                m_resultsList[index] = result;
            }
        }
    }

    if (failedIds.isEmpty()) {
        qDebug() << "No failed components to retry";
        return;
    }

    qDebug() << "Retrying" << failedIds.size() << "failed components";

    // Update counts and notify UI
    updateResultsList();
    emit resultsListChanged();

    // Re-trigger export if not currently exporting
    if (!m_isExporting && m_exportService) {
        m_exportService->startPreload(failedIds);
    }
}

void ExportProgressViewModel::removeResult(const QString& componentId) {
    if (m_idToIndexMap.contains(componentId)) {
        int index = m_idToIndexMap[componentId];
        m_resultsList.removeAt(index);
        m_idToIndexMap.remove(componentId);
        // Reindex remaining items
        for (int i = index; i < m_resultsList.size(); ++i) {
            QString id = m_resultsList[i].toMap().value("componentId").toString();
            m_idToIndexMap[id] = i;
        }
        // Update counts first, then notify UI of list change
        updateResultsList();
        emit resultsListChanged();
    }
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
