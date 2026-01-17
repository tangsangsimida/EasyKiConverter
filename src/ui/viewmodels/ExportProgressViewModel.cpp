#include "ExportProgressViewModel.h"
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
{
    // 连接 ExportService 信号
    if (m_exportService) {
        connect(m_exportService, &ExportService::exportProgress, this, &ExportProgressViewModel::handleExportProgress);
        connect(m_exportService, &ExportService::componentExported, this, &ExportProgressViewModel::handleComponentExported);
        connect(m_exportService, &ExportService::exportCompleted, this, &ExportProgressViewModel::handleExportCompleted);
        connect(m_exportService, &ExportService::exportFailed, this, &ExportProgressViewModel::handleExportFailed);
    }
    
    // 连接 ComponentService 信号
    if (m_componentService) {
        connect(m_componentService, &ComponentService::componentDataFetched, this, &ExportProgressViewModel::handleComponentDataFetched);
    }
}

ExportProgressViewModel::~ExportProgressViewModel()
{
}

void ExportProgressViewModel::startExport(const QStringList &componentIds, const QString &outputPath, const QString &libName, bool exportSymbol, bool exportFootprint, bool exportModel3D, bool overwriteExistingFiles)
{
    qDebug() << "Starting export for" << componentIds.size() << "components";
    qDebug() << "Options - Path:" << outputPath << "Lib:" << libName << "Symbol:" << exportSymbol << "Footprint:" << exportFootprint << "3D:" << exportModel3D << "Overwrite:" << overwriteExistingFiles;
    
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
    m_componentIds = componentIds;
    m_status = "Fetching component data...";
    
    emit isExportingChanged();
    emit progressChanged();
    emit statusChanged();
    
    // 设置组件服务的输出路径
    m_componentService->setOutputPath(outputPath);
    
    // 开始收集所有元件的数据
    for (const QString &componentId : componentIds) {
        m_componentService->fetchComponentData(componentId, exportModel3D);
    }
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
    qDebug() << "Component exported:" << componentId << "Success:" << success;
    
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
    qDebug() << "Component data fetched for:" << componentId;
    
    m_fetchedCount++;
    
    // 更新进度
    int progress = (m_fetchedCount * 50) / m_componentIds.size(); // 数据收集占50%的进度
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged();
    }
    
    // 检查是否所有元件数据都已收集完成
    if (m_fetchedCount >= m_componentIds.size()) {
        m_status = "Exporting components...";
        emit statusChanged();
        
        // 构建导出选项（这里使用默认值，实际应该在调用时传递）
        ExportOptions options;
        options.outputPath = m_componentService->getOutputPath();
        options.libName = "MyLibrary";
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = true;
        options.overwriteExistingFiles = false;
        
        // 调用 ExportService 执行导出流程
        m_exportService->executeExportPipeline(m_componentIds, options);
    }
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

} // namespace EasyKiConverter