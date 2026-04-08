/*
 * 我发现我在学校，不谈恋爱也不学习，不打游戏也不逃课，每天就是处于认真听课，然后因为听不懂走神了，
 * 反应过来以后再继续认真听课的循环，进入这种状态的时候，我一般就是左脑和右脑已经聊美了，
 * 完全就是一个放飞自我，意淫我有多少多少家产，然后怕班里有人会读心术紧急撤回一条意淫，
 * 那这在恋爱小说里我不就是那种…背景板吗，就每天在学校挂机任务就完成的npc
 */

#include "ExportSettingsViewModel.h"

#include "services/export/ParallelExportService.h"
#include "utils/FileUtils.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace EasyKiConverter {

ExportSettingsViewModel::ExportSettingsViewModel(ParallelExportService* exportService, QObject* parent)
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
    , m_exportMode(0)
    , m_debugMode(false)
    , m_isExporting(false)
    , m_status("Ready") {
    loadFromConfig();

    if (m_exportService) {
        connect(m_exportService,
                &ParallelExportService::preloadProgressChanged,
                this,
                &ExportSettingsViewModel::handlePreloadProgressChanged);
        connect(m_exportService,
                &ParallelExportService::preloadCompleted,
                this,
                &ExportSettingsViewModel::handlePreloadCompleted);
        connect(m_exportService,
                &ParallelExportService::progressChanged,
                this,
                &ExportSettingsViewModel::handleProgressChanged);
        connect(m_exportService,
                &ParallelExportService::typeCompleted,
                this,
                &ExportSettingsViewModel::handleTypeCompleted);
        connect(m_exportService, &ParallelExportService::completed, this, &ExportSettingsViewModel::handleCompleted);
        connect(m_exportService, &ParallelExportService::cancelled, this, &ExportSettingsViewModel::handleCancelled);
        connect(m_exportService, &ParallelExportService::failed, this, &ExportSettingsViewModel::handleFailed);
        qDebug() << "ExportSettingsViewModel: Connected to ParallelExportService signals";
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
    }
}

void ExportSettingsViewModel::setDebugMode(bool enabled) {
    bool envDebugMode = qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE");

    if (envDebugMode) {
        qDebug()
            << "Debug mode is controlled by environment variable EASYKICONVERTER_DEBUG_MODE, ignoring manual setting";
        return;
    }

    if (m_debugMode != enabled) {
        m_debugMode = enabled;
        emit debugModeChanged();
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
        qWarning() << "ParallelExportService is not available";
        setStatus("Export service not available");
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "No components to export";
        setStatus("No components to export");
        return;
    }

    // Save component IDs for after preload
    m_pendingComponentIds = componentIds;

    // Build and set export options
    buildExportOptions();

    setIsExporting(true);
    setStatus("Preloading component data...");

    // Start preload first
    m_exportService->startPreload(componentIds);
}

void ExportSettingsViewModel::buildExportOptions() {
    ExportOptions options;

    // Handle output path
    QString absoluteOutputPath = m_outputPath;
    if (absoluteOutputPath.isEmpty()) {
        QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
        QDir exportDir(documentsPath);

        if (!exportDir.exists("EasyKiConverter")) {
            exportDir.mkdir("EasyKiConverter");
        }
        exportDir.cd("EasyKiConverter");
        absoluteOutputPath = exportDir.absoluteFilePath(m_libName);
    } else {
        QDir dir(absoluteOutputPath);
        if (dir.isAbsolute()) {
            absoluteOutputPath = dir.cleanPath(absoluteOutputPath);
        } else {
            QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
            QDir exportDir(documentsPath);
            if (!exportDir.exists("EasyKiConverter")) {
                exportDir.mkdir("EasyKiConverter");
            }
            exportDir.cd("EasyKiConverter");
            absoluteOutputPath = exportDir.absoluteFilePath(absoluteOutputPath);
        }
    }

    if (absoluteOutputPath.isEmpty()) {
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
    options.updateMode = (m_exportMode == 1);
    options.debugMode = m_debugMode;

    qDebug() << "Export options:" << "OutputPath:" << options.outputPath << "LibName:" << options.libName
             << "Symbol:" << options.exportSymbol << "Footprint:" << options.exportFootprint
             << "3D Model:" << options.exportModel3D << "Preview Images:" << options.exportPreviewImages
             << "Datasheet:" << options.exportDatasheet << "Update Mode:" << options.updateMode
             << "Debug Mode:" << options.debugMode;

    m_exportService->setOptions(options);
    m_exportService->setOutputPath(absoluteOutputPath);
}

void ExportSettingsViewModel::cancelExport() {
    qDebug() << "Cancelling export";

    if (!m_exportService) {
        return;
    }

    m_exportService->cancelExport();
    setIsExporting(false);
    setStatus("Export cancelled");
}

bool ExportSettingsViewModel::openOutputFolder() {
    if (m_outputPath.isEmpty()) {
        return false;
    }

    QString pathToOpen = m_outputPath;
    if (!QDir(pathToOpen).exists()) {
        QDir().mkpath(pathToOpen);
    }

    FileUtils utils;
    return utils.openFolder(pathToOpen);
}

void ExportSettingsViewModel::handlePreloadProgressChanged(const PreloadProgress& progress) {
    Q_UNUSED(progress);
    setStatus(QString("Preloading... %1/%2").arg(progress.completedCount).arg(progress.totalCount));
}

void ExportSettingsViewModel::handlePreloadCompleted(int successCount, int failedCount) {
    qDebug() << "Preload completed: success=" << successCount << "failed=" << failedCount;

    if (failedCount > 0) {
        setStatus(QString("Preload completed with errors: %1 failed").arg(failedCount));
    }

    // Now start the actual export
    setStatus("Starting export...");
    emit exportStarted();
    m_exportService->startExport();
}

void ExportSettingsViewModel::handleProgressChanged(const ExportOverallProgress& progress) {
    Q_UNUSED(progress);
    setStatus("Exporting...");
}

void ExportSettingsViewModel::handleTypeCompleted(const QString& typeName,
                                                  int successCount,
                                                  int failedCount,
                                                  int skippedCount) {
    Q_UNUSED(skippedCount);
    qDebug() << "Type completed:" << typeName << "success=" << successCount << "failed=" << failedCount;
}

void ExportSettingsViewModel::handleCompleted(int successCount, int failedCount) {
    qDebug() << "Export completed: success=" << successCount << "failed=" << failedCount;
    setIsExporting(false);

    if (failedCount > 0) {
        setStatus(QString("Export completed with errors: %1 failed").arg(failedCount));
    } else {
        setStatus("Export completed successfully");
    }
}

void ExportSettingsViewModel::handleCancelled() {
    qDebug() << "Export cancelled";
    setIsExporting(false);
    setStatus("Export cancelled");
}

void ExportSettingsViewModel::handleFailed(const QString& error) {
    qWarning() << "Export failed:" << error;
    setIsExporting(false);
    setStatus(QString("Export failed: %1").arg(error));
}

void ExportSettingsViewModel::setIsExporting(bool exporting) {
    if (m_isExporting != exporting) {
        m_isExporting = exporting;
        emit isExportingChanged();
    }
}

void ExportSettingsViewModel::setStatus(const QString& status) {
    if (m_status != status) {
        m_status = status;
        emit statusChanged();
    }
}

void ExportSettingsViewModel::loadFromConfig() {
    m_outputPath = m_configService->getOutputPath();
    m_libName = m_configService->getLibName();
    m_exportSymbol = m_configService->getExportSymbol();
    m_exportFootprint = m_configService->getExportFootprint();
    m_exportModel3D = m_configService->getExportModel3D();
    m_exportPreviewImages = m_configService->getExportPreviewImages();
    m_exportDatasheet = m_configService->getExportDatasheet();
    m_overwriteExistingFiles = m_configService->getOverwriteExistingFiles();

    bool envDebugMode = qEnvironmentVariableIsSet("EASYKICONVERTER_DEBUG_MODE");
    if (envDebugMode) {
        QString debugValue = qEnvironmentVariable("EASYKICONVERTER_DEBUG_MODE", "false").toLower();
        m_debugMode = (debugValue == "true" || debugValue == "1" || debugValue == "yes");
    } else {
        m_debugMode = m_configService->getDebugMode();
    }

    emit outputPathChanged();
    emit libNameChanged();
    emit exportSymbolChanged();
    emit exportFootprintChanged();
    emit exportModel3DChanged();
    emit exportPreviewImagesChanged();
    emit exportDatasheetChanged();
    emit overwriteExistingFilesChanged();
    emit debugModeChanged();
}

void ExportSettingsViewModel::saveConfig() {
    m_configService->saveConfig();
}

void ExportSettingsViewModel::resetConfig() {
    m_outputPath = "";
    m_libName = "MyLibrary";
    m_exportSymbol = true;
    m_exportFootprint = true;
    m_exportModel3D = true;
    m_exportPreviewImages = false;
    m_exportDatasheet = false;
    m_overwriteExistingFiles = false;
    m_exportMode = 0;
    m_debugMode = false;

    m_configService->setOutputPath(m_outputPath);
    m_configService->setLibName(m_libName);
    m_configService->setExportSymbol(m_exportSymbol);
    m_configService->setExportFootprint(m_exportFootprint);
    m_configService->setExportModel3D(m_exportModel3D);
    m_configService->setExportPreviewImages(m_exportPreviewImages);
    m_configService->setExportDatasheet(m_exportDatasheet);
    m_configService->setOverwriteExistingFiles(m_overwriteExistingFiles);
    m_configService->setDebugMode(m_debugMode);

    emit outputPathChanged();
    emit libNameChanged();
    emit exportSymbolChanged();
    emit exportFootprintChanged();
    emit exportModel3DChanged();
    emit exportPreviewImagesChanged();
    emit exportDatasheetChanged();
    emit overwriteExistingFilesChanged();
    emit exportModeChanged();
    emit debugModeChanged();
}

}  // namespace EasyKiConverter
