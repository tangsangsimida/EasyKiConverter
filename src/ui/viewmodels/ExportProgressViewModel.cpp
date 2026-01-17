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
        connect(m_componentService, &ComponentService::cadDataReady, this, &ExportProgressViewModel::handleComponentDataFetched);
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
    m_collectedData.clear();
    m_status = "Fetching component data...";
    
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
    
    // 设置组件服务的输出路径
    m_componentService->setOutputPath(outputPath);
    
    // 串行收集数据（由于 EasyedaApi 不支持并发）
    if (!m_componentIds.isEmpty()) {
        m_componentService->fetchComponentData(m_componentIds.first(), exportModel3D);
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
    bool hasSymbol = data.symbolData() && !data.symbolData()->info().name.isEmpty();
    bool hasFootprint = data.footprintData() && !data.footprintData()->info().name.isEmpty();
    bool hasModel3D = data.model3DData() && !data.model3DData()->uuid().isEmpty();
    
    qDebug() << "Component data fetched for:" << componentId
             << "Symbol:" << hasSymbol
             << "Footprint:" << hasFootprint
             << "3D Model:" << hasModel3D;
    
    // 存储收集到的数据
    m_collectedData.append(data);
    m_fetchedCount++;
    
    qDebug() << "Fetched count:" << m_fetchedCount << "/" << m_componentIds.size();
    
    // 更新进度
    int progress = (m_fetchedCount * 50) / m_componentIds.size(); // 数据收集占50%的进度
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged();
    }
    
    // 检查是否还有更多元件需要收集
    if (m_fetchedCount < m_componentIds.size()) {
        qDebug() << "Fetching next component, index:" << m_fetchedCount;
        // 收集下一个元件的数据
        int nextIndex = m_fetchedCount;
        if (nextIndex < m_componentIds.size()) {
            m_componentService->fetchComponentData(m_componentIds[nextIndex], m_exportOptions.exportModel3D);
        }
    } else {
        qDebug() << "All components collected, starting export...";
        // 所有元件数据都已收集完成
        m_status = "Exporting components...";
        emit statusChanged();
        
        qDebug() << "All component data collected, starting export with" << m_collectedData.size() << "components";
        
        // 使用收集到的数据执行导出流程
        m_exportService->executeExportPipelineWithData(m_collectedData, m_exportOptions);
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