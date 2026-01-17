#include "ExportSettingsViewModel.h"
#include <QDebug>

namespace EasyKiConverter
{

ExportSettingsViewModel::ExportSettingsViewModel(QObject *parent)
    : QObject(parent)
    , m_outputPath("")
    , m_libName("MyLibrary")
    , m_exportSymbol(true)
    , m_exportFootprint(true)
    , m_exportModel3D(true)
    , m_overwriteExistingFiles(false)
{
    loadFromConfig();
}

ExportSettingsViewModel::~ExportSettingsViewModel()
{
}

void ExportSettingsViewModel::setOutputPath(const QString &path)
{
    if (m_outputPath != path) {
        m_outputPath = path;
        emit outputPathChanged();
    }
}

void ExportSettingsViewModel::setLibName(const QString &name)
{
    if (m_libName != name) {
        m_libName = name;
        emit libNameChanged();
    }
}

void ExportSettingsViewModel::setExportSymbol(bool enabled)
{
    if (m_exportSymbol != enabled) {
        m_exportSymbol = enabled;
        emit exportSymbolChanged();
    }
}

void ExportSettingsViewModel::setExportFootprint(bool enabled)
{
    if (m_exportFootprint != enabled) {
        m_exportFootprint = enabled;
        emit exportFootprintChanged();
    }
}

void ExportSettingsViewModel::setExportModel3D(bool enabled)
{
    if (m_exportModel3D != enabled) {
        m_exportModel3D = enabled;
        emit exportModel3DChanged();
    }
}

void ExportSettingsViewModel::setOverwriteExistingFiles(bool enabled)
{
    if (m_overwriteExistingFiles != enabled) {
        m_overwriteExistingFiles = enabled;
        emit overwriteExistingFilesChanged();
    }
}

void ExportSettingsViewModel::startExport(const QStringList &componentIds)
{
    qDebug() << "Starting export for" << componentIds.size() << "components";
    
    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }
    
    // TODO: 需要添加 ExportService 的支持
    // 目前先设置为导出状态
    setIsExporting(true);
    setStatus("Export started");
    setProgress(0);
}

void ExportSettingsViewModel::cancelExport()
{
    qDebug() << "Canceling export";
    
    // TODO: 需要添加 ExportService 的支持
    setIsExporting(false);
    setStatus("Export cancelled");
}

void ExportSettingsViewModel::handleExportProgress(int current, int total)
{
    int newProgress = total > 0 ? (current * 100 / total) : 0;
    if (m_progress != newProgress) {
        m_progress = newProgress;
        emit progressChanged();
    }
}

void ExportSettingsViewModel::handleComponentExported(const QString &componentId, bool success, const QString &message)
{
    qDebug() << "Component exported:" << componentId << "Success:" << success;
    // TODO: 需要添加信号声明
}

void ExportSettingsViewModel::handleExportCompleted(bool success)
{
    qDebug() << "Export completed:" << success;
    
    setIsExporting(false);
    setProgress(100);
    setStatus(success ? "Export completed successfully" : "Export completed with errors");
}

void ExportSettingsViewModel::setIsExporting(bool exporting)
{
    if (m_isExporting != exporting) {
        m_isExporting = exporting;
        emit isExportingChanged();
    }
}

void ExportSettingsViewModel::setProgress(int progress)
{
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged();
    }
}

void ExportSettingsViewModel::setStatus(const QString &status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void ExportSettingsViewModel::handleExportFailed(const QString &error)
{
    qWarning() << "Export failed:" << error;
    
    setIsExporting(false);
    setStatus("Export failed: " + error);
}

void ExportSettingsViewModel::loadFromConfig()
{
    // TODO: 从配置文件加载设置
    qDebug() << "Loading configuration";
}

void ExportSettingsViewModel::saveConfig()
{
    // TODO: 保存配置到文件
    qDebug() << "Saving configuration";
}

void ExportSettingsViewModel::resetConfig()
{
    // 重置为默认值
    m_outputPath = "";
    m_libName = "MyLibrary";
    m_exportSymbol = true;
    m_exportFootprint = true;
    m_exportModel3D = true;
    m_overwriteExistingFiles = false;
    
    emit outputPathChanged();
    emit libNameChanged();
    emit exportSymbolChanged();
    emit exportFootprintChanged();
    emit exportModel3DChanged();
    emit overwriteExistingFilesChanged();
    
    qDebug() << "Configuration reset to defaults";
}

} // namespace EasyKiConverter