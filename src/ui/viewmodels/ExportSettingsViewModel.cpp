#include "ExportSettingsViewModel.h"

#include "services/ExportService_Pipeline.h"
#include "utils/FileUtils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

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
    , m_exportPreviewImages(false)
    , m_exportDatasheet(false)
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

void ExportSettingsViewModel::setExportPreviewImages(bool enabled) {
    if (m_exportPreviewImages != enabled) {
        m_exportPreviewImages = enabled;
        m_configService->setExportPreviewImages(enabled);
        emit exportPreviewImagesChanged();
    }
}

void ExportSettingsViewModel::setExportDatasheet(bool enabled) {
    if (m_exportDatasheet != enabled) {
        m_exportDatasheet = enabled;
        m_configService->setExportDatasheet(enabled);
        emit exportDatasheetChanged();
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
        qDebug()
            << "Debug mode is controlled by environment variable EASYKICONVERTER_DEBUG_MODE, ignoring manual setting";
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

    // 调试：打印原始输入值
    qDebug() << "Original input values - outputPath:" << m_outputPath << "libName:" << m_libName;

    // 处理输出路径
    QString absoluteOutputPath = m_outputPath;
    if (absoluteOutputPath.isEmpty()) {
        // 如果导出路径为空，使用默认路径：~/Documents/EasyKiConverter/库名称
        QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        QDir exportDir(documentsPath);

        // 创建 EasyKiConverter 目录
        if (!exportDir.exists("EasyKiConverter")) {
            exportDir.mkdir("EasyKiConverter");
        }
        exportDir.cd("EasyKiConverter");

        // 使用库名称作为导出路径
        absoluteOutputPath = exportDir.absoluteFilePath(m_libName);
        qDebug() << "Using default export path (empty input):" << absoluteOutputPath;
    } else {
        QDir dir(absoluteOutputPath);

        if (dir.isAbsolute()) {
            // 如果是绝对路径，规范化路径（处理 .. 和 . 等）
            absoluteOutputPath = dir.cleanPath(absoluteOutputPath);
            qDebug() << "Normalized absolute path:" << m_outputPath << "->" << absoluteOutputPath;
        } else {
            // 如果是相对路径，相对于 ~/Documents/EasyKiConverter/ 转换为绝对路径
            QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
            QDir exportDir(documentsPath);

            // 创建 EasyKiConverter 目录
            if (!exportDir.exists("EasyKiConverter")) {
                exportDir.mkdir("EasyKiConverter");
            }
            exportDir.cd("EasyKiConverter");

            // 直接拼接相对路径和库名称，支持多级目录（如 test/dennis）
            absoluteOutputPath = exportDir.absoluteFilePath(absoluteOutputPath + "/" + m_libName);
            qDebug() << "Converted relative path to absolute path (Documents/EasyKiConverter/userPath/libName):"
                     << m_outputPath << "->" << absoluteOutputPath;
        }
    }

    // 最终验证路径
    if (absoluteOutputPath.isEmpty()) {
        qWarning() << "Final output path is still empty! Using fallback to Desktop.";
        absoluteOutputPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        absoluteOutputPath = QDir(absoluteOutputPath).absoluteFilePath(m_libName);
    }

    options.outputPath = absoluteOutputPath;
    options.libName = m_libName;
    options.exportSymbol = m_exportSymbol;
    options.exportFootprint = m_exportFootprint;
    options.exportModel3D = m_exportModel3D;
    options.exportPreviewImages = m_exportPreviewImages;
    options.exportDatasheet = m_exportDatasheet;
    options.overwriteExistingFiles = m_overwriteExistingFiles;
    options.updateMode = (m_exportMode == 1);  // 1 = 更新模式
    options.debugMode = m_debugMode;

    qDebug() << "Export options:" << "OutputPath:" << options.outputPath << "LibName:" << options.libName
             << "Symbol:" << options.exportSymbol << "Footprint:" << options.exportFootprint
             << "3D Model:" << options.exportModel3D << "Preview Images:" << options.exportPreviewImages
             << "Datasheet:" << options.exportDatasheet << "Update Mode:" << options.updateMode
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

bool ExportSettingsViewModel::openOutputFolder() {
    qDebug() << "openOutputFolder called (ExportSettingsViewModel - deprecated, use ExportProgressViewModel instead)";

    // 此方法已弃用，现在使用 ExportProgressViewModel::openLastExportedFolder()
    // 为了向后兼容，保留此方法但不执行任何操作
    qWarning() << "ExportSettingsViewModel::openOutputFolder() is deprecated. Use "
                  "ExportProgressViewModel::openLastExportedFolder() instead.";
    return false;
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
    m_exportPreviewImages = m_configService->getExportPreviewImages();
    m_exportDatasheet = m_configService->getExportDatasheet();
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
    emit exportPreviewImagesChanged();
    emit exportDatasheetChanged();
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