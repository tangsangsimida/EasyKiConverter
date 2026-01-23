#include "ExportProgressViewModel.h"
#include "services/ExportService_Pipeline.h"
#include <QDebug>

namespace EasyKiConverter
{

    ExportProgressViewModel::ExportProgressViewModel(ExportService *exportService, ComponentService *componentService, QObject *parent)
        : QObject(parent), m_exportService(exportService), m_componentService(componentService), m_status("Ready"), m_progress(0), m_isExporting(false), m_successCount(0), m_failureCount(0), m_fetchedCount(0), m_fetchProgress(0), m_processProgress(0), m_writeProgress(0), m_usePipelineMode(false), m_pendingUpdate(false)
    {
        // 初始化节流定时器�?00ms�?
        m_throttleTimer = new QTimer(this);
        m_throttleTimer->setInterval(100); // 100ms 节流间隔
        m_throttleTimer->setSingleShot(true);
        connect(m_throttleTimer, &QTimer::timeout, this, &ExportProgressViewModel::flushPendingUpdates);

        // 连接 ExportService 信号
        if (m_exportService)
        {
            connect(m_exportService, &ExportService::exportProgress, this, &ExportProgressViewModel::handleExportProgress);
            connect(m_exportService, &ExportService::componentExported, this, &ExportProgressViewModel::handleComponentExported);
            connect(m_exportService, &ExportService::exportCompleted, this, &ExportProgressViewModel::handleExportCompleted);
            connect(m_exportService, &ExportService::exportFailed, this, &ExportProgressViewModel::handleExportFailed);

            // 连接流水线信号（如果使用流水线模式）
            if (auto *pipelineService = qobject_cast<ExportServicePipeline *>(m_exportService))
            {
                connect(pipelineService, &ExportServicePipeline::pipelineProgressUpdated,
                        this, &ExportProgressViewModel::handlePipelineProgressUpdated);
                connect(pipelineService, &ExportServicePipeline::statisticsReportGenerated,
                        this, &ExportProgressViewModel::handleStatisticsReportGenerated);
                m_usePipelineMode = true;
            }
        }

        // 连接 ComponentService 信号
        if (m_componentService)
        {
            connect(m_componentService, &ComponentService::cadDataReady, this, &ExportProgressViewModel::handleComponentDataFetched);
            connect(m_componentService, &ComponentService::allComponentsDataCollected, this, &ExportProgressViewModel::handleAllComponentsDataCollected);
        }
    }

    void ExportProgressViewModel::setUsePipelineMode(bool usePipeline)
    {
        m_usePipelineMode = usePipeline;
    }

    ExportProgressViewModel::~ExportProgressViewModel()
    {
        if (m_throttleTimer)
        {
            m_throttleTimer->stop();
        }
    }

    void ExportProgressViewModel::startExport(const QStringList &componentIds, const QString &outputPath, const QString &libName, bool exportSymbol, bool exportFootprint, bool exportModel3D, bool overwriteExistingFiles, bool updateMode, bool debugMode)
    {
        qDebug() << "Starting export for" << componentIds.size() << "components";
        qDebug() << "Options - Path:" << outputPath << "Lib:" << libName << "Symbol:" << exportSymbol << "Footprint:" << exportFootprint << "3D:" << exportModel3D << "Overwrite:" << overwriteExistingFiles << "Update:" << updateMode << "Debug:" << debugMode;
        qDebug() << "Using pipeline mode:" << m_usePipelineMode;

        if (m_isExporting)
        {
            qWarning() << "Export already in progress";
            return;
        }

        if (!m_exportService || !m_componentService)
        {
            qWarning() << "Services not initialized";
            return;
        }

        m_isExporting = true;
        m_progress = 0;
        m_successCount = 0;
        m_failureCount = 0;
        m_fetchedCount = 0;
        m_fetchProgress = 0;
        m_processProgress = 0;
        m_writeProgress = 0;
        m_componentIds = componentIds;
        m_collectedData.clear();
        m_status = "Starting export...";

        // 清空结果列表和哈希表
        m_resultsList.clear();
        m_idToIndexMap.clear();

        // 预填充结果列表（性能优化 + 用户体验优化�?
        prepopulateResultsList(componentIds);

        // 保存导出选项
        m_exportOptions.outputPath = outputPath;
        m_exportOptions.libName = libName;
        m_exportOptions.exportSymbol = exportSymbol;
        m_exportOptions.exportFootprint = exportFootprint;
        m_exportOptions.exportModel3D = exportModel3D;
        m_exportOptions.overwriteExistingFiles = overwriteExistingFiles;
        m_exportOptions.updateMode = updateMode;
        m_exportOptions.debugMode = debugMode;

        emit isExportingChanged();
        emit progressChanged();
        emit statusChanged();
        emit resultsListChanged();

        // 如果使用流水线模式，直接调用流水线导�?
        if (m_usePipelineMode && qobject_cast<ExportServicePipeline *>(m_exportService))
        {
            m_status = "Running pipeline export...";
            emit statusChanged();

            auto *pipelineService = qobject_cast<ExportServicePipeline *>(m_exportService);
            pipelineService->executeExportPipelineWithStages(componentIds, m_exportOptions);
            return;
        }

        // 否则使用原有的并行数据收集模�?
        m_status = "Fetching component data in parallel...";
        emit statusChanged();

        // 设置组件服务的输出路�?
        m_componentService->setOutputPath(outputPath);

        // 并行收集数据
        m_componentService->fetchMultipleComponentsData(componentIds, exportModel3D);
    }

    void ExportProgressViewModel::cancelExport()
    {
        qDebug() << "Canceling export";

        if (!m_exportService)
        {
            qWarning() << "Export service not initialized";
            return;
        }

        m_isExporting = false;
        m_status = "Export cancelled";

        emit isExportingChanged();
        emit statusChanged();

        // TODO: 调用 ExportService 取消导出
        // m_exportService->cancelExport();
    }

    void ExportProgressViewModel::handleExportProgress(int current, int total)
    {
        int newProgress = total > 0 ? (current * 100 / total) : 0;
        if (m_progress != newProgress)
        {
            m_progress = newProgress;
            emit progressChanged();
        }
    }

    QString ExportProgressViewModel::getStatusString(int stage, bool success) const
    {
        if (!success)
        {
            return "failed";
        }

        // 根据阶段返回不同的状态字符串
        switch (stage)
        {
        case 0: // Fetch
            return "fetch_completed";
        case 1: // Process
            return "processing";
        case 2: // Write
            return "success"; // 只有写入阶段完成才算真正成功
        default:
            return "success";
        }
    }

    void ExportProgressViewModel::handleComponentExported(const QString &componentId, bool success, const QString &message, int stage)
    {
        qDebug() << "Component exported:" << componentId << "Success:" << success << "Stage:" << stage << "Message:" << message;

        // 使用哈希表快速查找（O(1) 复杂度）
        int index = m_idToIndexMap.value(componentId, -1);

        QVariantMap result;
        QString statusStr = getStatusString(stage, success);

        if (index >= 0 && index < m_resultsList.size())
        {
            // 更新现有结果
            result = m_resultsList[index].toMap();
            result["status"] = statusStr;
            result["message"] = message;
            m_resultsList[index] = result;
        }
        else
        {
            // 如果没找到（不应该发生，因为已经预填充），添加新结果
            qWarning() << "Component not found in pre-populated list:" << componentId;
            result["componentId"] = componentId;
            result["status"] = statusStr;
            result["message"] = message;
            m_resultsList.append(result);
            m_idToIndexMap[componentId] = m_resultsList.size() - 1;
        }

        // 使用节流机制更新 UI（避免频繁刷新）
        if (!m_pendingUpdate)
        {
            m_pendingUpdate = true;
            m_throttleTimer->start();
        }

        // 更新统计（不依赖节流�?
        if (!success && statusStr == "failed")
        {
            m_failureCount++;
            emit failureCountChanged();
        }
        else if (success && stage == 2) // 只有写入阶段完成才算成功
        {
            m_successCount++;
            emit successCountChanged();
        }

        emit componentExported(componentId, success, message);
    }

    void ExportProgressViewModel::flushPendingUpdates()
    {
        if (m_pendingUpdate)
        {
            m_pendingUpdate = false;
            emit resultsListChanged();
            qDebug() << "Flushed" << m_resultsList.size() << "results to UI";
        }
    }

    void ExportProgressViewModel::prepopulateResultsList(const QStringList &componentIds)
    {
        qDebug() << "Pre-populating results list with" << componentIds.size() << "components";

        m_resultsList.clear();
        m_idToIndexMap.clear();

        for (int i = 0; i < componentIds.size(); ++i)
        {
            QVariantMap result;
            result["componentId"] = componentIds[i];
            result["status"] = "pending"; // 初始状态：等待�?
            result["message"] = "Waiting to start...";

            m_resultsList.append(result);
            m_idToIndexMap[componentIds[i]] = i;
        }

        qDebug() << "Pre-populated" << m_resultsList.size() << "results with hash map";
    }

    void ExportProgressViewModel::handleComponentDataFetched(const QString &componentId, const ComponentData &data)
    {
        qDebug() << "Component data fetched for:" << componentId
                 << "Symbol:" << (data.symbolData() && !data.symbolData()->info().name.isEmpty())
                 << "Footprint:" << (data.footprintData() && !data.footprintData()->info().name.isEmpty())
                 << "3D Model:" << (data.model3DData() && !data.model3DData()->uuid().isEmpty());

        // 这个方法现在不应该被调用，因为我们使用并行数据收�?
        // 如果被调用，说明有错�?
        qWarning() << "handleComponentDataFetched called in parallel mode, this should not happen";
    }

    void ExportProgressViewModel::handleAllComponentsDataCollected(const QList<ComponentData> &componentDataList)
    {
        qDebug() << "All components data collected in parallel:" << componentDataList.size() << "components";

        m_status = "Exporting components in parallel...";
        emit statusChanged();

        // 使用并行导出流程
        m_exportService->executeExportPipelineWithDataParallel(componentDataList, m_exportOptions);
    }

    void ExportProgressViewModel::handleExportCompleted(int totalCount, int successCount)
    {
        qDebug() << "Export completed:" << successCount << "/" << totalCount << "success";

        // 确保所有待处理的更新都已刷�?
        if (m_pendingUpdate)
        {
            m_throttleTimer->stop();
            flushPendingUpdates();
        }

        m_isExporting = false;
        m_status = "Export completed";
        emit isExportingChanged();
        emit statusChanged();
        emit exportCompleted(totalCount, successCount);
    }

    void ExportProgressViewModel::handleExportFailed(const QString &error)
    {
        qWarning() << "Export failed:" << error;

        // 确保所有待处理的更新都已刷�?
        if (m_pendingUpdate)
        {
            m_throttleTimer->stop();
            flushPendingUpdates();
        }

        m_status = "Export failed: " + error;
        emit statusChanged();
    }

    void ExportProgressViewModel::handlePipelineProgressUpdated(const PipelineProgress &progress)
    {
        qDebug() << "Pipeline progress updated - Fetch:" << progress.fetchProgress()
                 << "Process:" << progress.processProgress()
                 << "Write:" << progress.writeProgress()
                 << "Overall:" << progress.overallProgress();

        if (m_fetchProgress != progress.fetchProgress())
        {
            m_fetchProgress = progress.fetchProgress();
            emit fetchProgressChanged();
            qDebug() << "Fetch progress changed to:" << m_fetchProgress;
        }

        if (m_processProgress != progress.processProgress())
        {
            m_processProgress = progress.processProgress();
            emit processProgressChanged();
            qDebug() << "Process progress changed to:" << m_processProgress;
        }

        if (m_writeProgress != progress.writeProgress())
        {
            m_writeProgress = progress.writeProgress();
            emit writeProgressChanged();
            qDebug() << "Write progress changed to:" << m_writeProgress;
        }

        // 更新总进�?
        int newProgress = progress.overallProgress();
        if (m_progress != newProgress)
        {
            m_progress = newProgress;
            emit progressChanged();
            qDebug() << "Overall progress changed to:" << m_progress;
        }
    }

    void ExportProgressViewModel::handleStatisticsReportGenerated(const QString &reportPath, const ExportStatistics &statistics)
    {
        qDebug() << "Statistics report generated:" << reportPath;
        qDebug() << "Statistics summary:" << statistics.getSummary();

        // 保存统计数据
        m_hasStatistics = true;
        m_statisticsReportPath = reportPath;
        m_statistics = statistics;

        // 生成统计摘要
        m_statisticsSummary = statistics.getSummary();

        // 发送信号通知 UI
        emit statisticsChanged();

        qDebug() << "Statistics updated in ViewModel";
    }

} // namespace EasyKiConverter