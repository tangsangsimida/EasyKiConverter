#include "ExportProgressViewModel.h"
#include "src/services/ExportService_Pipeline.h"
#include <QDebug>

namespace EasyKiConverter
{

ExportProgressViewModel::ExportProgressViewModel(ExportService *exportService, ComponentService *componentService, QObject *parent)
    : QObject(parent)
    , m_exportService(exportService)
    , m_componentService(componentService)
    , m_progress(0)
    , m_isExporting(false)
    , m_successCount(0)
    , m_failureCount(0)
    , m_status("Ready")
    , m_fetchedCount(0)
    , m_fetchProgress(0)
    , m_processProgress(0)
    , m_writeProgress(0)
    , m_usePipelineMode(false)
{
    // 连接 ExportService 信号
    if (m_exportService) {
        connect(m_exportService, &ExportService::exportProgress, this, &ExportProgressViewModel::handleExportProgress);
        connect(m_exportService, &ExportService::componentExported, this, &ExportProgressViewModel::handleComponentExported);
        connect(m_exportService, &ExportService::exportCompleted, this, &ExportProgressViewModel::handleExportCompleted);
        connect(m_exportService, &ExportService::exportFailed, this, &ExportProgressViewModel::handleExportFailed);

        // 连接流水线信号（如果使用流水线模式）
        if (auto *pipelineService = qobject_cast<ExportServicePipeline*>(m_exportService)) {
            connect(pipelineService, &ExportServicePipeline::pipelineProgressUpdated,
                    this, &ExportProgressViewModel::handlePipelineProgressUpdated);
            m_usePipelineMode = true;
        }
    }
    
    // 连接 ComponentService 信号
    if (m_componentService) {
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
}

void ExportProgressViewModel::startExport(const QStringList &componentIds, const QString &outputPath, const QString &libName, bool exportSymbol, bool exportFootprint, bool exportModel3D, bool overwriteExistingFiles)
{
    qDebug() << "Starting export for" << componentIds.size() << "components";
    qDebug() << "Options - Path:" << outputPath << "Lib:" << libName << "Symbol:" << exportSymbol << "Footprint:" << exportFootprint << "3D:" << exportModel3D << "Overwrite:" << overwriteExistingFiles;
    qDebug() << "Using pipeline mode:" << m_usePipelineMode;
    
    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }
    
    if (!m_exportService || !m_componentService) {
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
    
    // 保存导出选项
    m_exportOptions.outputPath = outputPath;
    m_exportOptions.libName = libName;
    m_exportOptions.exportSymbol = exportSymbol;
    m_exportOptions.exportFootprint = exportFootprint;
    m_exportOptions.exportModel3D = exportModel3D;
    m_exportOptions.overwriteExistingFiles = overwriteExistingFiles;
    
    emit isExportingChanged();
    emit progressChanged();
    emit statusChanged();
    
    // 如果使用流水线模式，直接调用流水线导出
    if (m_usePipelineMode && qobject_cast<ExportServicePipeline*>(m_exportService)) {
        m_status = "Running pipeline export...";
        emit statusChanged();
        
        auto *pipelineService = qobject_cast<ExportServicePipeline*>(m_exportService);
        pipelineService->executeExportPipelineWithStages(componentIds, m_exportOptions);
        return;
    }
    
    // 否则使用原有的并行数据收集模式
    m_status = "Fetching component data in parallel...";
    emit statusChanged();
    
    // 设置组件服务的输出路径
    m_componentService->setOutputPath(outputPath);
    
    // 并行收集数据
    m_componentService->fetchMultipleComponentsData(componentIds, exportModel3D);
}

void ExportProgressViewModel::cancelExport()
{
    qDebug() << "Canceling export";
    
    if (!m_exportService) {
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
    if (m_progress != newProgress) {
        m_progress = newProgress;
        emit progressChanged();
    }
}

void ExportProgressViewModel::handleComponentExported(const QString &componentId, bool success, const QString &message)
{
    qDebug() << "Component exported:" << componentId << "Success:" << success << "Message:" << message;
    
    // 更新结果列表
    QVariantMap result;
    result["componentId"] = componentId;
    result["status"] = success ? "success" : "failed";
    result["message"] = message;
    
    // 查找并更新现有结果
    bool found = false;
    for (int i = 0; i < m_resultsList.size(); ++i) {
        QVariantMap existingResult = m_resultsList[i].toMap();
        if (existingResult["componentId"].toString() == componentId) {
            m_resultsList[i] = result;
            found = true;
            break;
        }
    }
    
    // 如果没找到，添加新结果
    if (!found) {
        m_resultsList.append(result);
    }
    
    emit resultsListChanged();
    
    if (success) {
        m_successCount++;
        emit successCountChanged();
    } else {
        m_failureCount++;
        emit failureCountChanged();
    }
    
    emit componentExported(componentId, success, message);
}

void ExportProgressViewModel::handleComponentDataFetched(const QString &componentId, const ComponentData &data)
{
    qDebug() << "Component data fetched for:" << componentId 
             << "Symbol:" << (data.symbolData() && !data.symbolData()->info().name.isEmpty())
             << "Footprint:" << (data.footprintData() && !data.footprintData()->info().name.isEmpty())
             << "3D Model:" << (data.model3DData() && !data.model3DData()->uuid().isEmpty());
    
    // 这个方法现在不应该被调用，因为我们使用并行数据收集
    // 如果被调用，说明有错误
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
    
    m_isExporting = false;
    m_status = "Export completed";
    emit isExportingChanged();
    emit statusChanged();
    emit exportCompleted(totalCount, successCount);
}

void ExportProgressViewModel::handleExportFailed(const QString &error)
{
    qWarning() << "Export failed:" << error;
    
    m_status = "Export failed: " + error;
    emit statusChanged();
}

void ExportProgressViewModel::handlePipelineProgressUpdated(const PipelineProgress &progress)
{
    qDebug() << "Pipeline progress updated - Fetch:" << progress.fetchProgress()
             << "Process:" << progress.processProgress()
             << "Write:" << progress.writeProgress()
             << "Overall:" << progress.overallProgress();

    if (m_fetchProgress != progress.fetchProgress()) {
        m_fetchProgress = progress.fetchProgress();
        emit fetchProgressChanged();
        qDebug() << "Fetch progress changed to:" << m_fetchProgress;
    }
    
    if (m_processProgress != progress.processProgress()) {
        m_processProgress = progress.processProgress();
        emit processProgressChanged();
        qDebug() << "Process progress changed to:" << m_processProgress;
    }
    
    if (m_writeProgress != progress.writeProgress()) {
        m_writeProgress = progress.writeProgress();
        emit writeProgressChanged();
        qDebug() << "Write progress changed to:" << m_writeProgress;
    }
    
    // 更新总进度
    int newProgress = progress.overallProgress();
    if (m_progress != newProgress) {
        m_progress = newProgress;
        emit progressChanged();
        qDebug() << "Overall progress changed to:" << m_progress;
    }
}

} // namespace EasyKiConverter