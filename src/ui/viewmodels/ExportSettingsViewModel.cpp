#include "ExportSettingsViewModel.h"

#include <QDebug>

namespace EasyKiConverter {

ExportSettingsViewModel::ExportSettingsViewModel(QObject* parent)
    : QObject(parent),
      m_configService(ConfigService::instance()),
      m_outputPath(""),
      m_libName("MyLibrary"),
      m_exportSymbol(true),
      m_exportFootprint(true),
      m_exportModel3D(true),
      m_overwriteExistingFiles(false),
      m_exportMode(0)  // 默认为追加模�?
      ,
      m_debugMode(false) {
    loadFromConfig();
}

ExportSettingsViewModel::~ExportSettingsViewModel() {}

void ExportSettingsViewModel::setOutputPath(const QString& path) {
    if (m_outputPath != path) {
        m_outputPath = path;
        emit outputPathChanged();
    }
}

void ExportSettingsViewModel::setLibName(const QString& name) {
    if (m_libName != name) {
        m_libName = name;
        emit libNameChanged();
    }
}

void ExportSettingsViewModel::setExportSymbol(bool enabled) {
    if (m_exportSymbol != enabled) {
        m_exportSymbol = enabled;
        emit exportSymbolChanged();
    }
}

void ExportSettingsViewModel::setExportFootprint(bool enabled) {
    if (m_exportFootprint != enabled) {
        m_exportFootprint = enabled;
        emit exportFootprintChanged();
    }
}

void ExportSettingsViewModel::setExportModel3D(bool enabled) {
    if (m_exportModel3D != enabled) {
        m_exportModel3D = enabled;
        emit exportModel3DChanged();
    }
}

void ExportSettingsViewModel::setOverwriteExistingFiles(bool enabled) {
    if (m_overwriteExistingFiles != enabled) {
        m_overwriteExistingFiles = enabled;
        emit overwriteExistingFilesChanged();
    }
}

void ExportSettingsViewModel::setExportMode(int mode) {
    if (m_exportMode != mode) {
        m_exportMode = mode;
        emit exportModeChanged();
        qDebug() << "Export mode changed to:" << mode << "(0=append, 1=update)";

        // 同步更新 overwriteExistingFiles
        // 0 = 追加模式（保留已存在的元器件，跳过重复的�?> overwriteExistingFiles = false
        // 1 = 更新模式（替换相同的元器件，保留不同的元器件，添加新的元器件�?> overwriteExistingFiles = false
        // 注意：更新模式不删除整个文件，而是智能合并
        bool newOverwrite = false;  // 两种模式都不删除整个文件
        if (m_overwriteExistingFiles != newOverwrite) {
            m_overwriteExistingFiles = newOverwrite;
            emit overwriteExistingFilesChanged();
            qDebug() << "overwriteExistingFiles changed to:" << newOverwrite;
        }
    }
}

void ExportSettingsViewModel::setDebugMode(bool enabled) {
    if (m_debugMode != enabled) {
        m_debugMode = enabled;
        emit debugModeChanged();
        qDebug() << "Debug mode changed to:" << enabled;

        // 同步更新配置服务
        m_configService->setDebugMode(enabled);
    }
}
void ExportSettingsViewModel::startExport(const QStringList& componentIds) {
    qDebug() << "Starting export for" << componentIds.size() << "components";

    if (m_isExporting) {
        qWarning() << "Export already in progress";
        return;
    }

    // TODO: 需要添�?ExportService 的支�?
    // 目前先设置为导出状�?
    setIsExporting(true);
    setStatus("Export started");
    setProgress(0);
}

void ExportSettingsViewModel::cancelExport() {
    qDebug() << "Canceling export";

    // TODO: 需要添�?ExportService 的支�?
    setIsExporting(false);
    setStatus("Export cancelled");
}

void ExportSettingsViewModel::handleExportProgress(int current, int total) {
    int newProgress = total > 0 ? (current * 100 / total) : 0;
    if (m_progress != newProgress) {
        m_progress = newProgress;
        emit progressChanged();
    }
}

void ExportSettingsViewModel::handleComponentExported(const QString& componentId,
                                                      bool success,
                                                      const QString& message) {
    Q_UNUSED(message);
    qDebug() << "Component exported:" << componentId << "Success:" << success;
    // TODO: 需要添加信号声�?
}
void ExportSettingsViewModel::handleExportCompleted(bool success) {
    qDebug() << "Export completed:" << success;

    setIsExporting(false);
    setProgress(100);
    setStatus(success ? "Export completed successfully" : "Export completed with errors");
}

void ExportSettingsViewModel::setIsExporting(bool exporting) {
    if (m_isExporting != exporting) {
        m_isExporting = exporting;
        emit isExportingChanged();
    }
}

void ExportSettingsViewModel::setProgress(int progress) {
    if (m_progress != progress) {
        m_progress = progress;
        emit progressChanged();
    }
}

void ExportSettingsViewModel::setStatus(const QString& status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void ExportSettingsViewModel::handleExportFailed(const QString& error) {
    qWarning() << "Export failed:" << error;

    setIsExporting(false);
    setStatus("Export failed: " + error);
}

void ExportSettingsViewModel::loadFromConfig() {
    // 从配置服务加载设�?
    m_outputPath = m_configService->getOutputPath();
    m_libName = m_configService->getLibName();
    m_exportSymbol = m_configService->getExportSymbol();
    m_exportFootprint = m_configService->getExportFootprint();
    m_exportModel3D = m_configService->getExportModel3D();
    m_overwriteExistingFiles = m_configService->getOverwriteExistingFiles();
    m_debugMode = m_configService->getDebugMode();

    qDebug() << "Loading configuration from ConfigService";
}
void ExportSettingsViewModel::saveConfig() {
    // 保存配置到配置服�?
    m_configService->setOutputPath(m_outputPath);
    m_configService->setLibName(m_libName);
    m_configService->setExportSymbol(m_exportSymbol);
    m_configService->setExportFootprint(m_exportFootprint);
    m_configService->setExportModel3D(m_exportModel3D);
    m_configService->setOverwriteExistingFiles(m_overwriteExistingFiles);
    m_configService->setDebugMode(m_debugMode);

    m_configService->saveConfig();
    qDebug() << "Saving configuration to ConfigService";
}
void ExportSettingsViewModel::resetConfig() {
    // 重置为默认�?
    m_outputPath = "";
    m_libName = "MyLibrary";
    m_exportSymbol = true;
    m_exportFootprint = true;
    m_exportModel3D = true;
    m_overwriteExistingFiles = false;
    m_exportMode = 0;  // 重置为追加模�?
    m_debugMode = false;

    emit outputPathChanged();
    emit libNameChanged();
    emit exportSymbolChanged();
    emit exportFootprintChanged();
    emit exportModel3DChanged();
    emit overwriteExistingFilesChanged();
    emit exportModeChanged();
    emit debugModeChanged();

    qDebug() << "Configuration reset to defaults";
}
}  // namespace EasyKiConverter
