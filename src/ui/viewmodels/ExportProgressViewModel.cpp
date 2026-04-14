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
#include <QtGlobal>

namespace EasyKiConverter {

namespace {

constexpr int FETCH_WEIGHT = 30;
constexpr int PROCESS_WEIGHT = 50;
constexpr int WRITE_WEIGHT = 20;

const QString TYPE_SYMBOL = QStringLiteral("Symbol");
const QString TYPE_FOOTPRINT = QStringLiteral("Footprint");
const QString TYPE_MODEL3D = QStringLiteral("Model3D");
const QString TYPE_PREVIEW = QStringLiteral("PreviewImages");
const QString TYPE_DATASHEET = QStringLiteral("Datasheet");

}  // namespace

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
    , m_writeProgress(0)
    , m_isStopping(false)
    , m_hasCompletedExport(false)
    , m_exportSymbolEnabled(true)
    , m_exportFootprintEnabled(true)
    , m_exportModel3DEnabled(false)
    , m_exportPreviewEnabled(false)
    , m_exportDatasheetEnabled(false) {
    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setInterval(150);
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
    m_exportSymbolEnabled = exportSymbol;
    m_exportFootprintEnabled = exportFootprint;
    m_exportModel3DEnabled = exportModel3D;
    m_exportPreviewEnabled = exportPreviewImages;
    m_exportDatasheetEnabled = exportDatasheet;

    m_componentIds = componentIds;
    m_totalCount = componentIds.size();
    m_successCount = 0;
    m_failureCount = 0;
    m_isStopping = false;
    m_pendingUpdate = false;
    if (m_throttleTimer) {
        m_throttleTimer->stop();
    }
    setHasCompletedExport(false);
    m_resultsList.clear();
    m_idToIndexMap.clear();
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;
    setProgress(0);

    for (int i = 0; i < componentIds.size(); ++i) {
        m_idToIndexMap[componentIds[i]] = i;
        QVariantMap item;
        item["componentId"] = componentIds[i];
        item["status"] = "pending";
        item["symbolSuccess"] = !m_exportSymbolEnabled;
        item["footprintSuccess"] = !m_exportFootprintEnabled;
        item["model3DSuccess"] = !m_exportModel3DEnabled;
        item["previewSuccess"] = !m_exportPreviewEnabled;
        item["datasheetSuccess"] = !m_exportDatasheetEnabled;
        item["symbolStatus"] = m_exportSymbolEnabled ? "pending" : "disabled";
        item["footprintStatus"] = m_exportFootprintEnabled ? "pending" : "disabled";
        item["model3DStatus"] = m_exportModel3DEnabled ? "pending" : "disabled";
        item["previewStatus"] = m_exportPreviewEnabled ? "pending" : "disabled";
        item["datasheetStatus"] = m_exportDatasheetEnabled ? "pending" : "disabled";
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
    emit filteredResultsListChanged();
    emit totalCountChanged();
    emit successCountChanged();
    emit failureCountChanged();
    emit stageProgressChanged();

    // Start preload - actual export will begin after preload completes
    m_exportService->startPreload(componentIds);
}

void ExportProgressViewModel::cancelExport() {
    qDebug() << "ExportProgressViewModel: Cancelling export";

    if (!m_exportService) {
        return;
    }

    if (!m_isStopping) {
        m_isStopping = true;
        emit isStoppingChanged();
    }
    m_exportService->cancelExport();
}

void ExportProgressViewModel::updateComponentExportStatus(const QString& componentId,
                                                          int previewImageExported,
                                                          int datasheetExported) {
    if (!m_idToIndexMap.contains(componentId)) {
        return;
    }

    const int index = m_idToIndexMap.value(componentId);
    QVariantMap result = m_resultsList[index].toMap();

    if (previewImageExported >= 0) {
        result["previewSuccess"] = (previewImageExported > 0);
        result["previewStatus"] = (previewImageExported > 0) ? "success" : "failed";
    }
    if (datasheetExported >= 0) {
        result["datasheetSuccess"] = (datasheetExported > 0);
        result["datasheetStatus"] = (datasheetExported > 0) ? "success" : "failed";
    }

    updateOverallItemStatus(result);
    m_resultsList[index] = result;
    markResultsDirty();
}

void ExportProgressViewModel::handleCloseRequest() {
    if (m_isExporting) {
        cancelExport();
    }
}

void ExportProgressViewModel::handlePreloadProgressChanged(const PreloadProgress& progress) {
    QString statusText = QString("Preloading... %1/%2").arg(progress.completedCount).arg(progress.totalCount);
    if (!progress.currentComponentId.isEmpty()) {
        statusText += QString(" (%1)").arg(progress.currentComponentId);
    }
    setStatus(statusText);

    m_fetchProgress = qBound(0, progress.percentage(), 100);
    m_processProgress = 0;
    m_writeProgress = 0;
    setProgress(weightedOverallProgress());
    emit stageProgressChanged();
}

void ExportProgressViewModel::handlePreloadCompleted(int successCount, int failedCount) {
    qInfo() << "ExportProgressViewModel: Preload completed. Success:" << successCount << "Failed:" << failedCount;

    if (failedCount > 0) {
        setStatus(QString("Preload completed with %1 errors").arg(failedCount));
    } else {
        setStatus("Starting export...");
    }

    // Start the actual export after preload completes
    // Use QTimer::singleShot to defer to event loop, preventing UI freeze during retry
    // This allows pending signals/events to be processed before starting heavy export work
    if (m_exportService) {
        QTimer::singleShot(0, this, [this]() {
            if (m_exportService) {
                m_exportService->startExport();
            }
        });
    }
}

void ExportProgressViewModel::handleProgressChanged(const ExportOverallProgress& progress) {
    // Update stage progress values for QML binding
    switch (progress.currentStage) {
        case ExportOverallProgress::Stage::Preloading:
            setStatus("Preloading components...");
            m_fetchProgress = progress.preloadProgress.percentage();
            m_processProgress = 0;
            m_writeProgress = 0;
            setProgress(weightedOverallProgress());
            break;
        case ExportOverallProgress::Stage::Exporting: {
            setStatus("Exporting components...");
            // 阶段定义统一为：
            // 1. 抓取：需要额外获取的数据是否准备完成（预览图/手册/3D源数据）
            // 2. 处理：符号/封装/3D转换任务是否完成
            // 3. 写入：最终文件是否已经落盘
            m_fetchProgress = stageTypeProgress(progress, {TYPE_PREVIEW, TYPE_DATASHEET, TYPE_MODEL3D});

            const int rawProcessProgress = stageTypeProgress(progress, {TYPE_SYMBOL, TYPE_FOOTPRINT, TYPE_MODEL3D});
            const int rawWriteProgress =
                stageTypeProgress(progress, {TYPE_SYMBOL, TYPE_FOOTPRINT, TYPE_MODEL3D, TYPE_PREVIEW, TYPE_DATASHEET});

            // UI 上的三段进度条必须遵守抓取 -> 处理 -> 写入的阶段依赖关系。
            // 即使底层某些类型先完成，后续阶段也不能先于前置阶段显示完成。
            m_processProgress = qMin(rawProcessProgress, m_fetchProgress);
            m_writeProgress = qMin(rawWriteProgress, m_processProgress);
            setProgress(weightedOverallProgress());
            break;
        }
        case ExportOverallProgress::Stage::Completed:
            m_fetchProgress = 100;
            m_processProgress = 100;
            m_writeProgress = 100;
            setProgress(100);
            setStatus("Export completed");
            break;
        case ExportOverallProgress::Stage::Cancelled:
            m_fetchProgress = 0;
            m_processProgress = 0;
            m_writeProgress = 0;
            setProgress(0);
            setStatus("Export cancelled");
            break;
        case ExportOverallProgress::Stage::Failed:
            m_fetchProgress = 0;
            m_processProgress = 0;
            m_writeProgress = 0;
            setStatus("Export failed");
            break;
        default:
            break;
    }

    emit stageProgressChanged();
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

        QString statusText = "pending";
        switch (status.status) {
            case ExportItemStatus::Status::Pending:
                statusText = "pending";
                break;
            case ExportItemStatus::Status::InProgress:
                statusText = "in_progress";
                break;
            case ExportItemStatus::Status::Success:
                statusText = "success";
                break;
            case ExportItemStatus::Status::Failed:
                statusText = "failed";
                break;
            case ExportItemStatus::Status::Skipped:
                statusText = "skipped";
                break;
        }
        result[typeStatusKey(typeName)] = statusText;

        updateOverallItemStatus(result);

        m_resultsList[index] = result;

        markResultsDirty();
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
    qInfo() << "Type completed:" << typeName << "success=" << successCount << "failed=" << failedCount;
}

void ExportProgressViewModel::handleCompleted(int successCount, int failedCount) {
    qInfo() << "Export completed: success=" << successCount << "failed=" << failedCount;
    Q_UNUSED(successCount);
    Q_UNUSED(failedCount);
    if (m_isStopping) {
        m_isStopping = false;
        emit isStoppingChanged();
    }
    setIsExporting(false);
    setHasCompletedExport(true);
    setProgress(100);
    updateResultsList();
    setStatus(m_failureCount > 0 ? QString("Completed with %1 errors").arg(m_failureCount)
                                 : "Export completed successfully");
    flushPendingUpdates();
}

void ExportProgressViewModel::handleCancelled() {
    qInfo() << "Export cancelled";

    // 将所有未完成的项标记为失败，并同步各类型状态，避免小球状态残留
    for (int i = 0; i < m_resultsList.size(); ++i) {
        QVariantMap result = m_resultsList[i].toMap();
        QString currentStatus = result["status"].toString();
        if (currentStatus == "pending" || currentStatus == "in_progress") {
            result["error"] = "Export cancelled";
            const QStringList statusKeys = {
                QStringLiteral("symbolStatus"),
                QStringLiteral("footprintStatus"),
                QStringLiteral("model3DStatus"),
                QStringLiteral("previewStatus"),
                QStringLiteral("datasheetStatus"),
            };
            for (const QString& key : statusKeys) {
                const QString currentTypeStatus = result.value(key).toString();
                if (currentTypeStatus == "pending" || currentTypeStatus == "in_progress") {
                    result[key] = "failed";
                }
            }
            updateOverallItemStatus(result);
            m_resultsList[i] = result;
        }
    }

    if (m_isStopping) {
        m_isStopping = false;
        emit isStoppingChanged();
    }
    setIsExporting(false);
    setHasCompletedExport(false);
    setProgress(0);
    setStatus("Export cancelled");
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;
    emit stageProgressChanged();
    flushPendingUpdates();
}

void ExportProgressViewModel::handleFailed(const QString& error) {
    qWarning() << "Export failed:" << error;
    if (m_isStopping) {
        m_isStopping = false;
        emit isStoppingChanged();
    }
    setIsExporting(false);
    setHasCompletedExport(false);
    setProgress(0);
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;
    emit stageProgressChanged();
    setStatus(QString("Export failed: %1").arg(error));
}

void ExportProgressViewModel::flushPendingUpdates() {
    if (m_pendingUpdate) {
        m_pendingUpdate = false;
        updateResultsList();
        emit resultsListChanged();
        emit filteredResultsListChanged();
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
    if (m_successCount != success) {
        m_successCount = success;
        emit successCountChanged();
    }
    if (m_failureCount != failure) {
        m_failureCount = failure;
        emit failureCountChanged();
    }
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
        } else if (m_filterMode == "exporting" &&
                   (map.value("status") == "pending" || map.value("status") == "in_progress")) {
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

int ExportProgressViewModel::symbolSuccessCount() const {
    return countItemsWithTypeStatus(QStringLiteral("symbolStatus"), QStringLiteral("success"));
}

int ExportProgressViewModel::footprintSuccessCount() const {
    return countItemsWithTypeStatus(QStringLiteral("footprintStatus"), QStringLiteral("success"));
}

int ExportProgressViewModel::model3DSuccessCount() const {
    return countItemsWithTypeStatus(QStringLiteral("model3DStatus"), QStringLiteral("success"));
}

int ExportProgressViewModel::previewSuccessCount() const {
    return countItemsWithTypeStatus(QStringLiteral("previewStatus"), QStringLiteral("success"));
}

int ExportProgressViewModel::datasheetSuccessCount() const {
    return countItemsWithTypeStatus(QStringLiteral("datasheetStatus"), QStringLiteral("success"));
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
    if (m_throttleTimer) {
        m_throttleTimer->stop();
    }
    m_pendingUpdate = false;
    setIsExporting(false);
    setProgress(0);
    setStatus("Ready");
    m_resultsList.clear();
    m_idToIndexMap.clear();
    m_filterMode = "all";
    m_totalCount = 0;
    m_successCount = 0;
    m_failureCount = 0;
    m_isStopping = false;
    setHasCompletedExport(false);
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;

    emit resultsListChanged();
    emit filterModeChanged();
    emit filteredResultsListChanged();
    emit isStoppingChanged();
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

    resetItemForRetry(result);
    m_resultsList[index] = result;

    // Update counts and notify UI
    markResultsDirty();

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
                resetItemForRetry(result);
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
    markResultsDirty();

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
        markResultsDirty();
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

void ExportProgressViewModel::setHasCompletedExport(bool completed) {
    if (m_hasCompletedExport != completed) {
        m_hasCompletedExport = completed;
        emit hasCompletedExportChanged();
    }
}

QString ExportProgressViewModel::typeStatusKey(const QString& typeName) const {
    if (typeName == "Symbol") {
        return "symbolStatus";
    }
    if (typeName == "Footprint") {
        return "footprintStatus";
    }
    if (typeName == "Model3D") {
        return "model3DStatus";
    }
    if (typeName == "PreviewImages") {
        return "previewStatus";
    }
    if (typeName == "Datasheet") {
        return "datasheetStatus";
    }
    return QString();
}

void ExportProgressViewModel::updateOverallItemStatus(QVariantMap& result) const {
    struct TypeRule {
        bool enabled;
        QString statusKey;
    };

    const QList<TypeRule> rules = {
        {m_exportSymbolEnabled, "symbolStatus"},
        {m_exportFootprintEnabled, "footprintStatus"},
        {m_exportModel3DEnabled, "model3DStatus"},
        {m_exportPreviewEnabled, "previewStatus"},
        {m_exportDatasheetEnabled, "datasheetStatus"},
    };

    bool hasEnabledType = false;
    bool anyFailed = false;
    bool anyInProgress = false;
    bool allDone = true;

    for (const TypeRule& rule : rules) {
        if (!rule.enabled) {
            continue;
        }

        hasEnabledType = true;
        const QString status = result.value(rule.statusKey, "pending").toString();
        if (status == "failed") {
            anyFailed = true;
        }
        if (status == "in_progress") {
            anyInProgress = true;
        }
        if (status != "success" && status != "failed" && status != "skipped") {
            allDone = false;
        }
    }

    if (!hasEnabledType) {
        result["status"] = "success";
    } else if (allDone) {
        result["status"] = anyFailed ? "failed" : "success";
    } else if (anyInProgress) {
        result["status"] = "in_progress";
    } else {
        result["status"] = "pending";
    }
}

void ExportProgressViewModel::resetItemForRetry(QVariantMap& result) const {
    result["status"] = "pending";
    result["symbolSuccess"] = !m_exportSymbolEnabled;
    result["footprintSuccess"] = !m_exportFootprintEnabled;
    result["model3DSuccess"] = !m_exportModel3DEnabled;
    result["previewSuccess"] = !m_exportPreviewEnabled;
    result["datasheetSuccess"] = !m_exportDatasheetEnabled;
    result["symbolStatus"] = m_exportSymbolEnabled ? "pending" : "disabled";
    result["footprintStatus"] = m_exportFootprintEnabled ? "pending" : "disabled";
    result["model3DStatus"] = m_exportModel3DEnabled ? "pending" : "disabled";
    result["previewStatus"] = m_exportPreviewEnabled ? "pending" : "disabled";
    result["datasheetStatus"] = m_exportDatasheetEnabled ? "pending" : "disabled";
    result["error"] = QString();
}

int ExportProgressViewModel::averageTypeProgress(const ExportOverallProgress& progress,
                                                 const QStringList& typeNames) const {
    int sum = 0;
    int count = 0;
    for (const QString& typeName : typeNames) {
        auto it = progress.exportTypeProgress.constFind(typeName);
        if (it == progress.exportTypeProgress.constEnd()) {
            continue;
        }
        sum += it.value().percentage();
        count++;
    }

    if (count == 0) {
        return 0;
    }
    return qBound(0, sum / count, 100);
}

int ExportProgressViewModel::stageTypeProgress(const ExportOverallProgress& progress,
                                               const QStringList& typeNames) const {
    bool hasEnabledType = false;
    for (const QString& typeName : typeNames) {
        const bool enabled = (typeName == "Symbol" && m_exportSymbolEnabled) ||
                             (typeName == "Footprint" && m_exportFootprintEnabled) ||
                             (typeName == "Model3D" && m_exportModel3DEnabled) ||
                             (typeName == "PreviewImages" && m_exportPreviewEnabled) ||
                             (typeName == "Datasheet" && m_exportDatasheetEnabled);
        if (enabled) {
            hasEnabledType = true;
            break;
        }
    }

    if (!hasEnabledType) {
        return 100;
    }

    return averageTypeProgress(progress, typeNames);
}

int ExportProgressViewModel::countItemsWithTypeStatus(const QString& key, const QString& expectedStatus) const {
    int count = 0;
    for (const QVariant& item : m_resultsList) {
        const QVariantMap map = item.toMap();
        if (map.value(key).toString() == expectedStatus) {
            count++;
        }
    }
    return count;
}

int ExportProgressViewModel::weightedOverallProgress() const {
    const int weighted =
        (m_fetchProgress * FETCH_WEIGHT) + (m_processProgress * PROCESS_WEIGHT) + (m_writeProgress * WRITE_WEIGHT);
    return qBound(0, weighted / 100, 100);
}

void ExportProgressViewModel::markResultsDirty() {
    m_pendingUpdate = true;
    m_throttleTimer->start();
}

}  // namespace EasyKiConverter
