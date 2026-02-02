#include "ExportProgressViewModel.h"

#include "services/ExportService_Pipeline.h"

#include <QDebug>

namespace EasyKiConverter {

ExportProgressViewModel::ExportProgressViewModel(ExportService* exportService,
                                                 ComponentService* componentService,
                                                 QObject* parent)
    : QObject(parent)
    , m_exportService(exportService)
    , m_componentService(componentService)
    , m_status("Ready")
    , m_progress(0)
    , m_isExporting(false)
    , m_isStopping(false)
    , m_successCount(0)
    , m_successSymbolCount(0)
    , m_successFootprintCount(0)
    , m_successModel3DCount(0)
    , m_failureCount(0)
    , m_fetchedCount(0)
    , m_fetchProgress(0)
    , m_processProgress(0)
    , m_writeProgress(0)
    , m_usePipelineMode(false)
    , m_pendingUpdate(false)
    , m_hasStatistics(false) {
    // 初始化节流定时器
    m_throttleTimer = new QTimer(this);
    m_throttleTimer->setInterval(100);
    m_throttleTimer->setSingleShot(true);
    connect(m_throttleTimer, &QTimer::timeout, this, &ExportProgressViewModel::flushPendingUpdates);

    // 连接 ExportService 信号
    if (m_exportService) {
        connect(m_exportService, &ExportService::exportProgress, this, &ExportProgressViewModel::handleExportProgress);
        connect(m_exportService,
                &ExportService::componentExported,
                this,
                &ExportProgressViewModel::handleComponentExported);
        connect(
            m_exportService, &ExportService::exportCompleted, this, &ExportProgressViewModel::handleExportCompleted);
        connect(m_exportService, &ExportService::exportFailed, this, &ExportProgressViewModel::handleExportFailed);

        if (auto* pipelineService = qobject_cast<ExportServicePipeline*>(m_exportService)) {
            connect(pipelineService,
                    &ExportServicePipeline::pipelineProgressUpdated,
                    this,
                    &ExportProgressViewModel::handlePipelineProgressUpdated);
            connect(pipelineService,
                    &ExportServicePipeline::statisticsReportGenerated,
                    this,
                    &ExportProgressViewModel::handleStatisticsReportGenerated);
            m_usePipelineMode = true;
        }
    }

    if (m_componentService) {
        connect(m_componentService,
                &ComponentService::cadDataReady,
                this,
                &ExportProgressViewModel::handleComponentDataFetched);
        connect(m_componentService,
                &ComponentService::allComponentsDataCollected,
                this,
                &ExportProgressViewModel::handleAllComponentsDataCollected);
    }
}

void ExportProgressViewModel::setUsePipelineMode(bool usePipeline) {
    m_usePipelineMode = usePipeline;
}

ExportProgressViewModel::~ExportProgressViewModel() {
    if (m_throttleTimer) {
        m_throttleTimer->stop();
    }
}

void ExportProgressViewModel::startExport(const QStringList& componentIds,
                                          const QString& outputPath,
                                          const QString& libName,
                                          bool exportSymbol,
                                          bool exportFootprint,
                                          bool exportModel3D,
                                          bool overwriteExistingFiles,
                                          bool updateMode,
                                          bool debugMode) {
    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }

    // 保存导出选项
    m_exportOptions.outputPath = outputPath;
    m_exportOptions.libName = libName;
    m_exportOptions.exportSymbol = exportSymbol;
    m_exportOptions.exportFootprint = exportFootprint;
    m_exportOptions.exportModel3D = exportModel3D;
    m_exportOptions.overwriteExistingFiles = overwriteExistingFiles;
    m_exportOptions.updateMode = updateMode;
    m_exportOptions.debugMode = debugMode;

    startExportInternal(componentIds, false);
}

void ExportProgressViewModel::cancelExport() {
    if (!m_exportService)
        return;

    qDebug() << "ExportProgressViewModel::cancelExport() called";

    // 1. 瞬时响应：立即通知后端中断（原子操作，<1ms）
    m_exportService->cancelExport();

    // 2. 立即更新按钮状态（同步执行，<1ms）
    m_isStopping = true;
    emit isStoppingChanged();
    m_status = "Stopping export...";
    emit statusChanged();

    qDebug() << "Cancel operation initiated in <1ms";

    // 3. 异步处理 UI 列表更新（不阻塞取消操作）
    QTimer::singleShot(0, this, [this]() {
        bool updated = false;
        for (int i = 0; i < m_resultsList.size(); ++i) {
            QVariantMap item = m_resultsList[i].toMap();
            QString status = item["status"].toString();

            // 更新所有未完成的状态 (v3.0.5+ 改进)
            if (status == "pending" || status == "fetching" || status == "processing" || status == "writing") {
                item["status"] = "failed";
                item["message"] = "Export cancelled";
                m_resultsList[i] = item;
                updated = true;
            }
        }

        if (updated) {
            emit resultsListChanged();
            updateStatistics();
        }
        qDebug() << "UI update completed after cancellation";
    });
}

void ExportProgressViewModel::handleExportProgress(int current, int total) {
    int newProgress = total > 0 ? (current * 100 / total) : 0;
    if (m_progress != newProgress) {
        m_progress = newProgress;
        emit progressChanged();
    }
}

QString ExportProgressViewModel::getStatusString(int stage,
                                                 bool success,
                                                 bool symbolSuccess,
                                                 bool footprintSuccess,
                                                 bool model3DSuccess) const {
    if (!success) {
        return "failed";
    }

    switch (stage) {
        case 0:
            return "fetch_completed";
        case 1:
            return "processing";
        case 2: {
            // 严格成功判定：检查用户请求的所有项是否都已成功写入
            bool allRequestedItemsDone = true;
            if (m_exportOptions.exportSymbol && !symbolSuccess)
                allRequestedItemsDone = false;
            if (m_exportOptions.exportFootprint && !footprintSuccess)
                allRequestedItemsDone = false;
            if (m_exportOptions.exportModel3D && !model3DSuccess)
                allRequestedItemsDone = false;

            return allRequestedItemsDone ? "success" : "failed";
        }
        default:
            return "success";
    }
}

void ExportProgressViewModel::handleComponentExported(const QString& componentId,
                                                      bool success,
                                                      const QString& message,
                                                      int stage,
                                                      bool symbolSuccess,
                                                      bool footprintSuccess,
                                                      bool model3DSuccess) {
    int index = m_idToIndexMap.value(componentId, -1);
    // 使用增强后的判定逻辑
    QString statusStr = getStatusString(stage, success, symbolSuccess, footprintSuccess, model3DSuccess);

    if (index >= 0 && index < m_resultsList.size()) {
        QVariantMap result = m_resultsList[index].toMap();
        result["status"] = statusStr;
        result["message"] = statusStr == "failed" && success ? "Partial export failed (missing parts)" : message;
        // 保存分项状态
        result["symbolSuccess"] = symbolSuccess;
        result["footprintSuccess"] = footprintSuccess;
        result["model3DSuccess"] = model3DSuccess;
        m_resultsList[index] = result;
    } else {
        QVariantMap result;
        result["componentId"] = componentId;
        result["status"] = statusStr;
        result["message"] = message;
        result["symbolSuccess"] = symbolSuccess;
        result["footprintSuccess"] = footprintSuccess;
        result["model3DSuccess"] = model3DSuccess;
        m_resultsList.append(result);
        m_idToIndexMap[componentId] = m_resultsList.size() - 1;
    }

    if (!m_pendingUpdate) {
        m_pendingUpdate = true;
        m_throttleTimer->start();
    }

    updateStatistics();
    emit componentExported(componentId, success, message);
}

void ExportProgressViewModel::updateStatistics() {
    int globalSuccess = 0;
    int globalFailed = 0;
    int globalSymbol = 0;
    int globalFootprint = 0;
    int globalModel3D = 0;

    for (const auto& var : m_resultsList) {
        QVariantMap item = var.toMap();
        QString status = item["status"].toString();

        if (status == "success") {
            globalSuccess++;
        } else if (status == "failed") {
            globalFailed++;
        }

        // 累加分项成功数（只有当该类型被启用时才计数）
        if (m_exportOptions.exportSymbol && item["symbolSuccess"].toBool())
            globalSymbol++;
        if (m_exportOptions.exportFootprint && item["footprintSuccess"].toBool())
            globalFootprint++;
        if (m_exportOptions.exportModel3D && item["model3DSuccess"].toBool())
            globalModel3D++;
    }

    if (m_successCount != globalSuccess) {
        m_successCount = globalSuccess;
        emit successCountChanged();
    }

    if (m_failureCount != globalFailed) {
        m_failureCount = globalFailed;
        emit failureCountChanged();
    }

    // 更新分项计数
    if (m_successSymbolCount != globalSymbol || m_successFootprintCount != globalFootprint ||
        m_successModel3DCount != globalModel3D) {
        m_successSymbolCount = globalSymbol;
        m_successFootprintCount = globalFootprint;
        m_successModel3DCount = globalModel3D;
        emit successCountChanged();  // 触发 UI 刷新
    }
}

void ExportProgressViewModel::flushPendingUpdates() {
    if (m_pendingUpdate) {
        m_pendingUpdate = false;
        emit resultsListChanged();
    }
}

void ExportProgressViewModel::prepopulateResultsList(const QStringList& componentIds) {
    m_resultsList.clear();
    m_idToIndexMap.clear();

    for (int i = 0; i < componentIds.size(); ++i) {
        QVariantMap result;
        result["componentId"] = componentIds[i];
        result["status"] = "pending";
        result["message"] = "Waiting to start...";
        m_resultsList.append(result);
        m_idToIndexMap[componentIds[i]] = i;
    }
}

void ExportProgressViewModel::handleComponentDataFetched(const QString& componentId, const ComponentData& data) {
    Q_UNUSED(componentId);
    Q_UNUSED(data);
}

void ExportProgressViewModel::handleAllComponentsDataCollected(const QList<ComponentData>& componentDataList) {
    m_status = "Exporting components in parallel...";
    emit statusChanged();
    m_exportService->executeExportPipelineWithDataParallel(componentDataList, m_exportOptions);
}

void ExportProgressViewModel::handleExportCompleted(int totalCount, int successCount) {
    Q_UNUSED(totalCount);
    Q_UNUSED(successCount);

    // 扫尾：确保 pending 项标记为取消
    for (int i = 0; i < m_resultsList.size(); ++i) {
        QVariantMap item = m_resultsList[i].toMap();
        if (item["status"].toString() == "pending") {
            item["status"] = "failed";
            item["message"] = "Export cancelled";
            m_resultsList[i] = item;
        }
    }

    // 全量刷新统计
    updateStatistics();

    if (m_pendingUpdate) {
        m_throttleTimer->stop();
        flushPendingUpdates();
    }

    m_isStopping = false;
    emit isStoppingChanged();
    m_isExporting = false;
    emit isExportingChanged();
    m_status = "Export completed";
    emit statusChanged();
    emit exportCompleted(m_successCount, m_failureCount);
}

void ExportProgressViewModel::handleExportFailed(const QString& error) {
    if (m_pendingUpdate) {
        m_throttleTimer->stop();
        flushPendingUpdates();
    }

    m_isStopping = false;
    m_isExporting = false;
    emit isStoppingChanged();
    emit isExportingChanged();
    m_status = "Export failed: " + error;
    emit statusChanged();
    updateStatistics();
}

void ExportProgressViewModel::handlePipelineProgressUpdated(const PipelineProgress& progress) {
    if (m_fetchProgress != progress.fetchProgress()) {
        m_fetchProgress = progress.fetchProgress();
        emit fetchProgressChanged();
    }
    if (m_processProgress != progress.processProgress()) {
        m_processProgress = progress.processProgress();
        emit processProgressChanged();
    }
    if (m_writeProgress != progress.writeProgress()) {
        m_writeProgress = progress.writeProgress();
        emit writeProgressChanged();
    }

    int newProgress = progress.overallProgress();
    if (m_progress != newProgress) {
        m_progress = newProgress;
        emit progressChanged();
    }
}

void ExportProgressViewModel::handleStatisticsReportGenerated(const QString& reportPath,
                                                              const ExportStatistics& statistics) {
    m_hasStatistics = true;
    m_statisticsReportPath = reportPath;
    m_statistics = statistics;
    m_statisticsSummary = statistics.getSummary();
    emit statisticsChanged();
}

void ExportProgressViewModel::retryFailedComponents() {
    if (m_isExporting)
        return;

    QStringList failedIds;
    for (int i = 0; i < m_resultsList.size(); ++i) {
        if (m_resultsList[i].toMap()["status"].toString() != "success") {
            failedIds << m_resultsList[i].toMap()["componentId"].toString();
        }
    }

    if (!failedIds.isEmpty()) {
        startExportInternal(failedIds, true);
    }
}

void ExportProgressViewModel::retryComponent(const QString& componentId) {
    if (m_isExporting)
        return;
    startExportInternal(QStringList() << componentId, true);
}

void ExportProgressViewModel::startExportInternal(const QStringList& componentIds, bool isRetry) {
    if (!m_exportService || !m_componentService)
        return;

    m_isExporting = true;
    m_progress = 0;
    m_fetchedCount = 0;
    m_fetchProgress = 0;
    m_processProgress = 0;
    m_writeProgress = 0;
    m_componentIds = componentIds;
    m_collectedData.clear();
    m_status = isRetry ? "Retrying..." : "Starting export...";

    if (!isRetry) {
        m_successCount = 0;
        m_failureCount = 0;
        prepopulateResultsList(componentIds);
    } else {
        // 重试模式：仅更新被重试项的状态
        for (const QString& id : componentIds) {
            int index = m_idToIndexMap.value(id, -1);
            if (index >= 0) {
                QVariantMap item = m_resultsList[index].toMap();
                item["status"] = "pending";
                item["message"] = "Waiting to retry...";
                m_resultsList[index] = item;
            }
        }
    }

    // 无论是新开始还是重试，都先执行一次全量统计同步
    updateStatistics();
    emit resultsListChanged();
    emit isExportingChanged();
    emit progressChanged();
    emit statusChanged();

    if (isRetry) {
        m_exportService->retryExport(componentIds, m_exportOptions);
    } else {
        if (m_usePipelineMode && qobject_cast<ExportServicePipeline*>(m_exportService)) {
            auto* pipelineService = qobject_cast<ExportServicePipeline*>(m_exportService);
            pipelineService->executeExportPipelineWithStages(componentIds, m_exportOptions);
        } else {
            m_componentService->setOutputPath(m_exportOptions.outputPath);
            m_componentService->fetchMultipleComponentsData(componentIds, m_exportOptions.exportModel3D);
        }
    }
}

}  // namespace EasyKiConverter