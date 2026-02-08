#include "ExportSettingsViewModel.h"

#include "services/ExportService_Pipeline.h"

#include <QDebug>

namespace EasyKiConverter {

ExportSettingsViewModel::ExportSettingsViewModel(ExportService* exportService, QObject* parent)
    : QObject(parent)
    , m_exportService(exportService)
    , m_configService(ConfigService::instance())
    , m_outputPath("")
    , m_libName("MyLibrary")
    , m_exportSymbol(true)
    , m_exportFootprint(true)
    , m_exportModel3D(true)
    , m_overwriteExistingFiles(false)
    , m_exportMode(0)  // 默认为追加模式
    , m_debugMode(false) {
    loadFromConfig();

    // 连接 ExportService 信号
    if (m_exportService) {
        connect(m_exportService, &ExportService::exportProgress, this, &ExportSettingsViewModel::handleExportProgress);
        connect(m_exportService,
                &ExportService::componentExported,
                this,
                &ExportSettingsViewModel::handleComponentExported);
        connect(
            m_exportService, &ExportService::exportCompleted, this, &ExportSettingsViewModel::handleExportCompleted);
        connect(m_exportService, &ExportService::exportFailed, this, &ExportSettingsViewModel::handleExportFailed);
        qDebug() << "ExportSettingsViewModel: Connected to ExportService signals";
    }
}

ExportSettingsViewModel::~ExportSettingsViewModel() {}

void ExportSettingsViewModel::setOutputPath(const QString& path) {
    if (m_outputPath != path) {
        m_outputPath = path;
        m_configService->setOutputPath(path);
        emit outputPathChanged();
    }
}

void ExportSettingsViewModel::setLibName(const QString& name) {
    if (m_libName != name) {
        m_libName = name;
        m_configService->setLibName(name);
        emit libNameChanged();
    }
}

void ExportSettingsViewModel::setExportSymbol(bool enabled) {
    if (m_exportSymbol != enabled) {
        m_exportSymbol = enabled;
        m_configService->setExportSymbol(enabled);
        emit exportSymbolChanged();
    }
}

void ExportSettingsViewModel::setExportFootprint(bool enabled) {
    if (m_exportFootprint != enabled) {
        m_exportFootprint = enabled;
        m_configService->setExportFootprint(enabled);
        emit exportFootprintChanged();
    }
}

void ExportSettingsViewModel::setExportModel3D(bool enabled) {
    if (m_exportModel3D != enabled) {
        m_exportModel3D = enabled;
        m_configService->setExportModel3D(enabled);
        emit exportModel3DChanged();
    }
}

void ExportSettingsViewModel::setOverwriteExistingFiles(bool enabled) {
    if (m_overwriteExistingFiles != enabled) {
        m_overwriteExistingFiles = enabled;
        m_configService->setOverwriteExistingFiles(enabled);
        emit overwriteExistingFilesChanged();
    }
}

void ExportSettingsViewModel::setExportMode(int mode) {
    if (m_exportMode != mode) {
        m_exportMode = mode;
        emit exportModeChanged();
        qDebug() << "Export mode changed to:" << mode << "(0=append, 1=update)";

        // 同步更新 overwriteExistingFiles
        // 0 = 追加模式（保留已存在的元器件，跳过重复的）-> overwriteExistingFiles = false
        // 1 = 更新模式（替换相同的元器件，保留不同的元器件，添加新的元器件）-> overwriteExistingFiles = false
        // 注意：更新模式不删除整个文件，而是智能合并
        bool newOverwrite = false;  // 两种模式都不删除整个文件
        if (m_overwriteExistingFiles != newOverwrite) {
            m_overwriteExistingFiles = newOverwrite;
            m_configService->setOverwriteExistingFiles(newOverwrite);
            emit overwriteExistingFilesChanged();
            qDebug() << "overwriteExistingFiles changed to:" << newOverwrite;
        }
    }
}

void ExportSettingsViewModel::setDebugMode(bool enabled) {
    // 检查是否通过环境变量设置
    bool envDebugMode = qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE");
    
    if (envDebugMode) {
        // 如果设置了环境变量，禁用手动设置
        qDebug() << "Debug mode is controlled by environment variable EASYKICONVERTER_DEBUG_MODE, ignoring manual setting";
        return;
    }
    
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

    if (!m_exportService) {
        qWarning() << "ExportService is not available";
        setStatus("Export service not available");
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "No components to export";
        setStatus("No components to export");
        return;
    }

    // 设置导出选项
    ExportOptions options;
    options.outputPath = m_outputPath;
    options.libName = m_libName;
    options.exportSymbol = m_exportSymbol;
    options.exportFootprint = m_exportFootprint;
    options.exportModel3D = m_exportModel3D;
    options.updateMode = (m_exportMode == 1);  // 1 = 更新模式
    options.debugMode = m_debugMode;

    qDebug() << "Export options:"
             << "OutputPath:" << options.outputPath << "LibName:" << options.libName
             << "Symbol:" << options.exportSymbol << "Footprint:" << options.exportFootprint
             << "3D Model:" << options.exportModel3D << "Update Mode:" << options.updateMode
             << "Debug Mode:" << options.debugMode;

    // 设置导出状态
    setIsExporting(true);
    setStatus("Export started");
    setProgress(0);

    // 调用 ExportService 执行导出
    m_exportService->executeExportPipeline(componentIds, options);
}

void ExportSettingsViewModel::cancelExport() {
    qDebug() << "Canceling export";

    if (!m_exportService) {
        qWarning() << "ExportService is not available";
        return;
    }

    // 调用 ExportService 取消导出
    m_exportService->cancelExport();

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
    // TODO: 需要添加信号声明
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
    // 从配置服务加载设置
    m_outputPath = m_configService->getOutputPath();
    m_libName = m_configService->getLibName();
    m_exportSymbol = m_configService->getExportSymbol();
    m_exportFootprint = m_configService->getExportFootprint();
    m_exportModel3D = m_configService->getExportModel3D();
    m_overwriteExistingFiles = m_configService->getOverwriteExistingFiles();
    
    // 从环境变量读取调试模式（优先级高于配置文件）
    bool envDebugMode = qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE");
    if (envDebugMode) {
        QString debugValue = qEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "false").toLower();
        m_debugMode = (debugValue == "true" || debugValue == "1" || debugValue == "yes");
        qDebug() << "Debug mode enabled via environment variable EASYKICONVERTER_DEBUG_MODE:" << m_debugMode;
    } else {
        m_debugMode = m_configService->getDebugMode();
    }

    // 发送变更信号以更新 UI
    emit outputPathChanged();
    emit libNameChanged();
    emit exportSymbolChanged();
    emit exportFootprintChanged();
    emit exportModel3DChanged();
    emit overwriteExistingFilesChanged();
    emit debugModeChanged();

    qDebug() << "Loading configuration from ConfigService";
}

void ExportSettingsViewModel::saveConfig() {
    // 保存配置到配置服务
    m_configService->saveConfig();
    qDebug() << "Saving configuration to ConfigService";
}

void ExportSettingsViewModel::resetConfig() {
    // 重置为默认值
    m_outputPath = "";
    m_libName = "MyLibrary";
    m_exportSymbol = true;
    m_exportFootprint = true;
    m_exportModel3D = true;
    m_overwriteExistingFiles = false;
    m_exportMode = 0;  // 重置为追加模式
    m_debugMode = false;

    // 同步到 ConfigService
    m_configService->setOutputPath(m_outputPath);
    m_configService->setLibName(m_libName);
    m_configService->setExportSymbol(m_exportSymbol);
    m_configService->setExportFootprint(m_exportFootprint);
    m_configService->setExportModel3D(m_exportModel3D);
    m_configService->setOverwriteExistingFiles(m_overwriteExistingFiles);
    m_configService->setDebugMode(m_debugMode);

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