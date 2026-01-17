#include "ExportProgressViewModel.h"
#include <QDebug>

namespace EasyKiConverter
{

ExportProgressViewModel::ExportProgressViewModel(ExportService *service, QObject *parent)
    : QObject(parent)
    , m_service(service)
    , m_progress(0)
    , m_isExporting(false)
    , m_successCount(0)
    , m_failureCount(0)
    , m_status("Ready")
{
    // 连接 Service 信号
    if (m_service) {
        connect(m_service, &ExportService::exportProgress, this, &ExportProgressViewModel::handleExportProgress);
        connect(m_service, &ExportService::componentExported, this, &ExportProgressViewModel::handleComponentExported);
        connect(m_service, &ExportService::exportCompleted, this, &ExportProgressViewModel::handleExportCompleted);
        connect(m_service, &ExportService::exportFailed, this, &ExportProgressViewModel::handleExportFailed);
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
    
    if (!m_service) {
        qWarning() << "Export service not initialized";
        return;
    }
    
    m_isExporting = true;
    m_progress = 0;
    m_successCount = 0;
    m_failureCount = 0;
    m_status = "Exporting...";
    
    emit isExportingChanged();
    emit progressChanged();
    
    // 创建 ExportOptions 对象
    ExportOptions options;
    options.outputPath = outputPath;
    options.libName = libName;
    options.exportSymbol = exportSymbol;
    options.exportFootprint = exportFootprint;
    options.exportModel3D = exportModel3D;
    options.overwriteExistingFiles = overwriteExistingFiles;
    
    // 调用导出服务
    m_service->executeExportPipeline(componentIds, options);
    emit successCountChanged();
    emit failureCountChanged();
    emit statusChanged();
    
    // TODO: 调用 ExportService 执行导出
    // m_service->executeExportPipeline(componentIds, options);
}

void ExportProgressViewModel::cancelExport()
{
    qDebug() << "Canceling export";
    
    if (!m_service) {
        qWarning() << "Export service not initialized";
        return;
    }
    
    m_isExporting = false;
    m_status = "Export cancelled";
    
    emit isExportingChanged();
    emit statusChanged();
    
    // TODO: 调用 ExportService 取消导出
    // m_service->cancelExport();
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