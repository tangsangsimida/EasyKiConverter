#include "WriteWorker.h"

#include "core/kicad/Exporter3DModel.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "core/utils/AtomicFileWriter.h"
#include "models/FootprintDataSerializer.h"
#include "models/SymbolDataSerializer.h"
#include "utils/PathSecurity.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>
#include <QThreadPool>
#include <QWaitCondition>

namespace EasyKiConverter {

const QRegularExpression INVALID_FILENAME_CHARS("[<>:\"/\\|?*]");

WriteWorker::WriteWorker(QSharedPointer<ComponentExportStatus> status,
                         const QString& outputPath,
                         const QString& libName,
                         bool exportSymbol,
                         bool exportFootprint,
                         bool exportModel3D,
                         bool exportPreviewImages,
                         bool exportDatasheet,
                         bool debugMode,
                         const QString& tempDir,
                         QObject* parent)
    : QObject(parent)
    , m_status(status)
    , m_outputPath(outputPath)
    , m_libName(libName)
    , m_exportSymbol(exportSymbol)
    , m_exportFootprint(exportFootprint)
    , m_exportModel3D(exportModel3D)
    , m_exportPreviewImages(exportPreviewImages)
    , m_exportDatasheet(exportDatasheet)
    , m_debugMode(debugMode)
    , m_tempDir(tempDir)
    , m_symbolExporter()
    , m_footprintExporter()
    , m_model3DExporter()
    , m_isAborted(0) {}

WriteWorker::~WriteWorker() {}

void WriteWorker::run() {
    // 检查是否已被取消 (v3.0.5+)
    if (m_isAborted.loadRelaxed()) {
        m_status->writeSuccess = false;
        m_status->writeMessage = "Export cancelled";
        m_status->isCancelled = true;
        m_status->addDebugLog(QString("WriteWorker cancelled for component: %1").arg(m_status->componentId));
        emit writeCompleted(m_status);
        return;
    }

    QElapsedTimer writeTimer;
    writeTimer.start();

    m_status->addDebugLog(QString("WriteWorker started for component: %1").arg(m_status->componentId));

    // 初始化所有写入状态

    m_status->symbolWritten = false;
    m_status->footprintWritten = false;
    m_status->model3DWritten = false;
    m_status->previewImageWritten = false;
    m_status->datasheetWritten = false;

    if (!m_status->fetchSuccess || !m_status->processSuccess) {
        m_status->writeDurationMs = 0;
        m_status->writeSuccess = false;
        m_status->writeMessage = "Skipped writing due to previous stage failure";
        m_status->addDebugLog("Skipping write stage because fetch or process failed.");
        emit writeCompleted(m_status);
        return;
    }

    if ((m_exportSymbol && (!m_status->symbolData || m_status->symbolData->info().name.isEmpty())) &&
        (m_exportFootprint && (!m_status->footprintData || m_status->footprintData->info().name.isEmpty()))) {
        m_status->writeDurationMs = writeTimer.elapsed();
        m_status->writeSuccess = false;
        m_status->writeMessage = "No valid symbol or footprint data to write";
        m_status->addDebugLog("ERROR: Symbol and Footprint data are empty or invalid.");
        emit writeCompleted(m_status);
        return;
    }

    if (!createOutputDirectory(m_outputPath)) {
        m_status->writeDurationMs = writeTimer.elapsed();
        m_status->writeSuccess = false;
        m_status->writeMessage = "Failed to create output directory";
        m_status->addDebugLog(
            QString("ERROR: Failed to create output directory, Duration: %1ms").arg(m_status->writeDurationMs));
        emit writeCompleted(m_status);
        return;
    }

    // 串行写入文件，每个函数独立更新自己的状态
    if (m_exportSymbol && m_status->symbolData) {
        writeSymbolFile(*m_status);
        if (m_isAborted.loadRelaxed())
            goto cleanup;
    }

    if (m_exportFootprint && m_status->footprintData) {
        writeFootprintFile(*m_status);
        if (m_isAborted.loadRelaxed())
            goto cleanup;
    }

    if (m_exportModel3D && m_status->model3DData &&
        (!m_status->model3DData->rawObj().isEmpty() || !m_status->cachedModel3DWrlPath.isEmpty())) {
        write3DModelFile(*m_status);
    }

    if (m_exportPreviewImages && !m_status->previewImageDataList.isEmpty()) {
        writePreviewImageFile(*m_status);
        if (m_isAborted.loadRelaxed())
            goto cleanup;
    }

    if (m_exportDatasheet && !m_status->datasheetData.isEmpty()) {
        writeDatasheetFile(*m_status);
        if (m_isAborted.loadRelaxed())
            goto cleanup;
    }

cleanup:
    if (m_isAborted.loadRelaxed()) {
        m_status->writeDurationMs = writeTimer.elapsed();
        m_status->writeSuccess = false;
        m_status->writeMessage = "Export cancelled";
        m_status->isCancelled = true;
        // 状态已在开头初始化为 false，无需再次设置
        m_status->addDebugLog(QString("WriteWorker cancelled for component: %1").arg(m_status->componentId));
        emit writeCompleted(m_status);
        return;
    }

    // 根据用户请求的导出项和实际完成情况，决定最终的成功状态
    // 注意：3D模型/预览图/手册导出失败不影响符号和封装的导出成功状态
    // 符号和封装是捆绑的核心输出，其他是可选附加输出
    bool coreSuccess = true;
    bool model3DFailed = false;
    bool previewImageFailed = false;
    bool datasheetFailed = false;
    if (m_exportSymbol && !m_status->symbolWritten) {
        coreSuccess = false;
    }
    if (m_exportFootprint && !m_status->footprintWritten) {
        coreSuccess = false;
    }
    if (m_exportModel3D && !m_status->model3DWritten) {
        model3DFailed = true;  // 仅记录，不影响核心成功状态
        m_status->addDebugLog("WARNING: 3D model export failed, but symbol/footprint export is not affected.");
    }
    if (m_exportPreviewImages && !m_status->previewImageWritten) {
        previewImageFailed = true;
        m_status->addDebugLog("WARNING: Preview image export failed.");
    }
    if (m_exportDatasheet && !m_status->datasheetWritten) {
        datasheetFailed = true;
        m_status->addDebugLog("WARNING: Datasheet export failed.");
    }

    m_status->writeSuccess = coreSuccess;
    if (coreSuccess && (model3DFailed || previewImageFailed || datasheetFailed)) {
        QStringList failedItems;
        if (model3DFailed)
            failedItems.append("3D");
        if (previewImageFailed)
            failedItems.append("preview");
        if (datasheetFailed)
            failedItems.append("datasheet");
        m_status->writeMessage =
            QString("Write completed (%1 export failed, symbol/footprint OK)").arg(failedItems.join(", "));
    } else if (coreSuccess) {
        m_status->writeMessage = "Write completed successfully";
    } else {
        m_status->writeMessage = "Failed to write one or more core files (symbol/footprint)";
    }

    if (m_debugMode) {
        exportDebugData(*m_status);
    }

    m_status->writeDurationMs = writeTimer.elapsed();
    m_status->addDebugLog(QString("WriteWorker completed for component: %1, Success: %2, Duration: %3ms")
                              .arg(m_status->componentId)
                              .arg(m_status->writeSuccess)
                              .arg(m_status->writeDurationMs));

    emit writeCompleted(m_status);
}

bool WriteWorker::writeSymbolFile(ComponentExportStatus& status) {
    if (!status.symbolData) {
        return true;
    }

    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    if (!PathSecurity::isSafePath(m_tempDir, m_outputPath)) {
        status.addDebugLog(QString("SECURITY ERROR: Temp dir outside output path: %1").arg(m_tempDir));
        return false;
    }

    QString finalFilePath = QString("%1/%2.kicad_sym").arg(m_tempDir, status.componentId);

    bool success = AtomicFileWriter::writeAtomically(
        m_tempDir, finalFilePath, ".kicad_sym.tmp", [this, &status](const QString& tempPath) -> bool {
            return m_symbolExporter.exportSymbol(*status.symbolData, tempPath);
        });

    if (success) {
        status.addDebugLog(QString("Symbol file written atomically: %1").arg(finalFilePath));
        status.symbolWritten = true;
        return true;
    } else {
        status.addDebugLog(QString("ERROR: Failed to write symbol file"));
        return false;
    }
}

bool WriteWorker::writeFootprintFile(ComponentExportStatus& status) {
    if (!status.footprintData) {
        return true;
    }

    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    if (!PathSecurity::isSafePath(m_tempDir, m_outputPath)) {
        status.addDebugLog(QString("SECURITY ERROR: Temp dir outside output path: %1").arg(m_tempDir));
        return false;
    }

    QString footprintLibPath = QString("%1/%2.pretty").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(footprintLibPath)) {
        status.addDebugLog(QString("ERROR: Failed to create footprint library directory: %1").arg(footprintLibPath));
        return false;
    }

    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (m_exportModel3D) {
        if (!createOutputDirectory(modelsDirPath)) {
            status.addDebugLog(QString("WARNING: Failed to create 3D models directory: %1").arg(modelsDirPath));
        }
    }

    QString footprintName = PathSecurity::sanitizeFilename(status.footprintData->info().name);
    QString filePath = QString("%1/%2.kicad_mod").arg(footprintLibPath, footprintName);

    QString model3DWrlPath;
    QString model3DStepPath;
    if (m_exportModel3D && status.model3DData && !status.model3DData->uuid().isEmpty()) {
        model3DWrlPath = QString("../%1.3dmodels/%2.wrl").arg(m_libName, footprintName);
        // 检查是否有STEP数据（无论是原始数据还是缓存路径）
        if (!status.model3DStepRaw.isEmpty() || !status.cachedModel3DStepPath.isEmpty()) {
            model3DStepPath = QString("../%1.3dmodels/%2.step").arg(m_libName, footprintName);
        }
    }

    bool success = AtomicFileWriter::writeAtomically(
        m_tempDir,
        filePath,
        ".kicad_mod.tmp",
        [this, &status, &model3DWrlPath, &model3DStepPath](const QString& tempPath) -> bool {
            if (!model3DStepPath.isEmpty()) {
                return m_footprintExporter.exportFootprint(
                    *status.footprintData, tempPath, model3DWrlPath, model3DStepPath);
            } else if (!model3DWrlPath.isEmpty()) {
                return m_footprintExporter.exportFootprint(*status.footprintData, tempPath, model3DWrlPath);
            } else {
                return m_footprintExporter.exportFootprint(*status.footprintData, tempPath);
            }
        });

    if (success) {
        status.addDebugLog(QString("Footprint file written atomically: %1").arg(filePath));
        status.footprintWritten = true;
        return true;
    } else {
        status.addDebugLog(QString("ERROR: Failed to write footprint file"));
        return false;
    }
}

bool WriteWorker::write3DModelFile(ComponentExportStatus& status) {
    // 如果既没有原始数据也没有缓存路径，则跳过
    if (!status.model3DData || (status.model3DData->rawObj().isEmpty() && status.cachedModel3DWrlPath.isEmpty())) {
        return true;
    }

    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    if (!PathSecurity::isSafePath(m_tempDir, m_outputPath)) {
        status.addDebugLog(
            QString("SECURITY ERROR: Temp dir %1 is outside output path %2").arg(m_tempDir, m_outputPath));
        return false;
    }

    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(modelsDirPath)) {
        status.addDebugLog(QString("ERROR: Failed to create 3D models directory: %1").arg(modelsDirPath));
        return false;
    }

    QString footprintName = status.footprintData ? status.footprintData->info().name : status.componentId;
    footprintName = PathSecurity::sanitizeFilename(footprintName);

    bool wrlSuccess = false;

    // WRL文件写入：优先使用缓存直接拷贝（避免大文件经过内存）
    if (!status.cachedModel3DWrlPath.isEmpty()) {
        QString wrlFilePath = QString("%1/%2.wrl").arg(modelsDirPath, footprintName);
        wrlSuccess = AtomicFileWriter::copyAtomically(status.cachedModel3DWrlPath, wrlFilePath, m_tempDir);
        if (wrlSuccess) {
            status.addDebugLog(QString("3D model WRL file copied from cache: %1").arg(status.cachedModel3DWrlPath));
        } else {
            status.addDebugLog(QString("ERROR: Failed to copy WRL file from cache"));
        }
    } else {
        // 使用导出器生成WRL文件
        QString wrlFilePath = QString("%1/%2.wrl").arg(modelsDirPath, footprintName);
        wrlSuccess = AtomicFileWriter::writeAtomically(
            m_tempDir, wrlFilePath, ".wrl.tmp", [this, &status](const QString& tempPath) -> bool {
                return m_model3DExporter.exportToWrl(*status.model3DData, tempPath);
            });
        if (wrlSuccess) {
            status.addDebugLog(QString("3D model WRL file written atomically: %1").arg(wrlFilePath));
        } else {
            status.addDebugLog(QString("ERROR: Failed to write WRL file"));
        }
    }

    // STEP文件写入：优先使用缓存直接拷贝（避免大文件经过内存）
    if (!status.cachedModel3DStepPath.isEmpty()) {
        // 使用直接拷贝模式（不经过内存）
        QString stepFilePath = QString("%1/%2.step").arg(modelsDirPath, footprintName);
        bool stepWriteSuccess = AtomicFileWriter::copyAtomically(status.cachedModel3DStepPath, stepFilePath, m_tempDir);

        if (stepWriteSuccess) {
            status.addDebugLog(QString("3D model STEP file copied from cache: %1").arg(status.cachedModel3DStepPath));
        } else {
            status.addDebugLog(QString("WARNING: Failed to copy STEP file from cache: %1").arg(stepFilePath));
        }
    } else if (!status.model3DStepRaw.isEmpty()) {
        // 使用内存模式（原有逻辑）
        QString stepFilePath = QString("%1/%2.step").arg(modelsDirPath, footprintName);
        bool stepWriteSuccess = AtomicFileWriter::writeAtomically(
            m_tempDir, stepFilePath, ".step.tmp", [&status](const QString& tempPath) -> bool {
                QFile file(tempPath);
                if (!file.open(QIODevice::WriteOnly)) {
                    return false;
                }
                qint64 written = file.write(status.model3DStepRaw);
                file.close();
                return written == status.model3DStepRaw.size();
            });

        if (stepWriteSuccess) {
            status.addDebugLog(QString("3D model STEP file written"));
        } else {
            status.addDebugLog(QString("WARNING: Failed to write STEP file: %1").arg(stepFilePath));
        }
        status.clearStepData();
    }

    status.model3DWritten = wrlSuccess;
    return wrlSuccess;
}

bool WriteWorker::createOutputDirectory(const QString& path) {
    QDir dir;
    if (!dir.exists(path)) {
        if (!dir.mkpath(path)) {
            qWarning() << "Failed to create directory:" << path;
            return false;
        }
    }
    return true;
}

bool WriteWorker::exportDebugData(ComponentExportStatus& status) {
    QString debugDirPath = QString("%1/debug").arg(m_outputPath);
    if (!createOutputDirectory(debugDirPath)) {
        status.addDebugLog(QString("ERROR: Failed to create debug directory: %1").arg(debugDirPath));
        return false;
    }

    QString componentDebugDir = QString("%1/%2").arg(debugDirPath, status.componentId);
    if (!createOutputDirectory(componentDebugDir)) {
        status.addDebugLog(QString("ERROR: Failed to create component debug directory: %1").arg(componentDebugDir));
        return false;
    }

    status.addDebugLog(QString("Exporting debug data to: %1").arg(componentDebugDir));

    if (!status.cinfoJsonRaw.isEmpty()) {
        QString cinfoFilePath = QString("%1/cinfo_raw.json").arg(componentDebugDir);
        QFile cinfoFile(cinfoFilePath);
        if (cinfoFile.open(QIODevice::WriteOnly)) {
            cinfoFile.write(status.cinfoJsonRaw);
            cinfoFile.close();
            status.addDebugLog("Debug: cinfo_raw.json written");
        }
    }

    if (!status.cadJsonRaw.isEmpty()) {
        QString cadFilePath = QString("%1/cad_raw.json").arg(componentDebugDir);
        QFile cadFile(cadFilePath);
        if (cadFile.open(QIODevice::WriteOnly)) {
            cadFile.write(status.cadJsonRaw);
            cadFile.close();
            status.addDebugLog("Debug: cad_raw.json written");
        }
    }

    if (!status.advJsonRaw.isEmpty()) {
        QString advFilePath = QString("%1/adv_raw.json").arg(componentDebugDir);
        QFile advFile(advFilePath);
        if (advFile.open(QIODevice::WriteOnly)) {
            advFile.write(status.advJsonRaw);
            advFile.close();
            qDebug() << "Debug: adv_raw.json written";
        }
    }

    if (!status.model3DObjRaw.isEmpty()) {
        QString objFilePath = QString("%1/model3d_raw.obj").arg(componentDebugDir);
        QFile objFile(objFilePath);
        if (objFile.open(QIODevice::WriteOnly)) {
            objFile.write(status.model3DObjRaw);
            objFile.close();
            status.addDebugLog("Debug: model3d_raw.obj written");
        }
    }

    if (!status.model3DStepRaw.isEmpty()) {
        QString stepFilePath = QString("%1/model3d_raw.step").arg(componentDebugDir);
        QFile stepFile(stepFilePath);
        if (stepFile.open(QIODevice::WriteOnly)) {
            stepFile.write(status.model3DStepRaw);
            stepFile.close();
            status.addDebugLog("Debug: model3d_raw.step written");
        }
    }

    QJsonObject debugInfo;
    debugInfo["componentId"] = status.componentId;
    debugInfo["fetchSuccess"] = status.fetchSuccess;
    debugInfo["fetchMessage"] = status.fetchMessage;
    debugInfo["processSuccess"] = status.processSuccess;
    debugInfo["processMessage"] = status.processMessage;
    debugInfo["writeSuccess"] = status.writeSuccess;
    debugInfo["writeMessage"] = status.writeMessage;

    if (!status.debugLog.isEmpty()) {
        QJsonArray logArray;
        for (const QString& log : status.debugLog) {
            logArray.append(log);
        }
        debugInfo["debugLog"] = logArray;
    }

    if (status.symbolData) {
        QJsonObject symbolInfo = SymbolDataSerializer::toJson(status.symbolData->info());
        symbolInfo["pinCount"] = status.symbolData->pins().size();
        symbolInfo["rectangleCount"] = status.symbolData->rectangles().size();
        symbolInfo["circleCount"] = status.symbolData->circles().size();
        symbolInfo["arcCount"] = status.symbolData->arcs().size();
        symbolInfo["polylineCount"] = status.symbolData->polylines().size();
        symbolInfo["polygonCount"] = status.symbolData->polygons().size();
        symbolInfo["pathCount"] = status.symbolData->paths().size();
        symbolInfo["ellipseCount"] = status.symbolData->ellipses().size();

        QJsonObject bbox;
        bbox["x"] = status.symbolData->bbox().x;
        bbox["y"] = status.symbolData->bbox().y;
        bbox["width"] = status.symbolData->bbox().width;
        bbox["height"] = status.symbolData->bbox().height;
        symbolInfo["bbox"] = bbox;

        QJsonArray pinsArray;
        for (const SymbolPin& pin : status.symbolData->pins()) {
            pinsArray.append(SymbolDataSerializer::toJson(pin));
        }
        symbolInfo["pins"] = pinsArray;

        QJsonArray rectanglesArray;
        for (const SymbolRectangle& rect : status.symbolData->rectangles()) {
            rectanglesArray.append(SymbolDataSerializer::toJson(rect));
        }
        symbolInfo["rectangles"] = rectanglesArray;

        QJsonArray circlesArray;
        for (const SymbolCircle& circle : status.symbolData->circles()) {
            circlesArray.append(SymbolDataSerializer::toJson(circle));
        }
        symbolInfo["circles"] = circlesArray;

        QJsonArray arcsArray;
        for (const SymbolArc& arc : status.symbolData->arcs()) {
            arcsArray.append(SymbolDataSerializer::toJson(arc));
        }
        symbolInfo["arcs"] = arcsArray;

        QJsonArray polylinesArray;
        for (const SymbolPolyline& polyline : status.symbolData->polylines()) {
            polylinesArray.append(SymbolDataSerializer::toJson(polyline));
        }
        symbolInfo["polylines"] = polylinesArray;

        QJsonArray polygonsArray;
        for (const SymbolPolygon& polygon : status.symbolData->polygons()) {
            polygonsArray.append(SymbolDataSerializer::toJson(polygon));
        }
        symbolInfo["polygons"] = polygonsArray;

        QJsonArray pathsArray;
        for (const SymbolPath& path : status.symbolData->paths()) {
            pathsArray.append(SymbolDataSerializer::toJson(path));
        }
        symbolInfo["paths"] = pathsArray;

        QJsonArray ellipsesArray;
        for (const SymbolEllipse& ellipse : status.symbolData->ellipses()) {
            ellipsesArray.append(SymbolDataSerializer::toJson(ellipse));
        }
        symbolInfo["ellipses"] = ellipsesArray;

        debugInfo["symbolData"] = symbolInfo;
    }

    if (status.footprintData) {
        QJsonObject footprintInfo = FootprintDataSerializer::toJson(status.footprintData->info());
        footprintInfo["padCount"] = status.footprintData->pads().size();
        footprintInfo["trackCount"] = status.footprintData->tracks().size();
        footprintInfo["holeCount"] = status.footprintData->holes().size();
        footprintInfo["circleCount"] = status.footprintData->circles().size();
        footprintInfo["arcCount"] = status.footprintData->arcs().size();
        footprintInfo["rectangleCount"] = status.footprintData->rectangles().size();
        footprintInfo["textCount"] = status.footprintData->texts().size();
        footprintInfo["solidRegionCount"] = status.footprintData->solidRegions().size();
        footprintInfo["outlineCount"] = status.footprintData->outlines().size();

        QJsonObject bbox;
        bbox["x"] = status.footprintData->bbox().x;
        bbox["y"] = status.footprintData->bbox().y;
        bbox["width"] = status.footprintData->bbox().width;
        bbox["height"] = status.footprintData->bbox().height;
        footprintInfo["bbox"] = bbox;

        QJsonArray padsArray;
        for (const FootprintPad& pad : status.footprintData->pads()) {
            padsArray.append(FootprintDataSerializer::toJson(pad));
        }
        footprintInfo["pads"] = padsArray;

        QJsonArray tracksArray;
        for (const FootprintTrack& track : status.footprintData->tracks()) {
            tracksArray.append(FootprintDataSerializer::toJson(track));
        }
        footprintInfo["tracks"] = tracksArray;

        QJsonArray holesArray;
        for (const FootprintHole& hole : status.footprintData->holes()) {
            holesArray.append(FootprintDataSerializer::toJson(hole));
        }
        footprintInfo["holes"] = holesArray;

        QJsonArray circlesArray;
        for (const FootprintCircle& circle : status.footprintData->circles()) {
            circlesArray.append(FootprintDataSerializer::toJson(circle));
        }
        footprintInfo["circles"] = circlesArray;

        QJsonArray arcsArray;
        for (const FootprintArc& arc : status.footprintData->arcs()) {
            arcsArray.append(FootprintDataSerializer::toJson(arc));
        }
        footprintInfo["arcs"] = arcsArray;

        QJsonArray rectanglesArray;
        for (const FootprintRectangle& rect : status.footprintData->rectangles()) {
            rectanglesArray.append(FootprintDataSerializer::toJson(rect));
        }
        footprintInfo["rectangles"] = rectanglesArray;

        QJsonArray textsArray;
        for (const FootprintText& text : status.footprintData->texts()) {
            textsArray.append(FootprintDataSerializer::toJson(text));
        }
        footprintInfo["texts"] = textsArray;

        QJsonArray solidRegionsArray;
        for (const FootprintSolidRegion& region : status.footprintData->solidRegions()) {
            solidRegionsArray.append(FootprintDataSerializer::toJson(region));
        }
        footprintInfo["solidRegions"] = solidRegionsArray;

        QJsonArray outlinesArray;
        for (const FootprintOutline& outline : status.footprintData->outlines()) {
            outlinesArray.append(FootprintDataSerializer::toJson(outline));
        }
        footprintInfo["outlines"] = outlinesArray;

        debugInfo["footprintData"] = footprintInfo;
    }

    if (status.model3DData) {
        QJsonObject model3DInfo;
        model3DInfo["uuid"] = status.model3DData->uuid();
        model3DInfo["objSize"] = status.model3DObjRaw.size();
        model3DInfo["stepSize"] = status.model3DStepRaw.size();
        debugInfo["model3DData"] = model3DInfo;
    }

    QString debugInfoFilePath = QString("%1/%2_debug_info.json").arg(componentDebugDir, status.componentId);
    QFile debugInfoFile(debugInfoFilePath);
    if (debugInfoFile.open(QIODevice::WriteOnly)) {
        QJsonDocument debugDoc(debugInfo);
        debugInfoFile.write(debugDoc.toJson(QJsonDocument::Indented));
        debugInfoFile.close();
        status.addDebugLog("Debug: debug_info.json written");
    }

    status.addDebugLog(QString("Debug data export completed for component: %1").arg(status.componentId));
    return true;
}

bool WriteWorker::writePreviewImageFile(ComponentExportStatus& status) {
    QDir outputDir(m_outputPath);
    QString imagesDir = outputDir.filePath("images");
    if (!createOutputDirectory(imagesDir)) {
        status.addDebugLog(QString("ERROR: Failed to create images directory: %1").arg(imagesDir));
        return false;
    }

    QString safeName = status.componentId;
    safeName.replace(INVALID_FILENAME_CHARS, "_");

    bool allSuccess = true;
    int exportedCount = 0;

    for (int i = 0; i < status.previewImageDataList.size(); ++i) {
        if (status.previewImageDataList[i].isEmpty()) {
            continue;
        }

        QString filename = QString("%1_%2.jpg").arg(safeName).arg(i);
        QString filePath = QDir(imagesDir).filePath(filename);

        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(status.previewImageDataList[i]);
            file.close();
            exportedCount++;
        } else {
            allSuccess = false;
            status.addDebugLog(QString("ERROR: Failed to write preview image: %1").arg(filePath));
        }
    }

    if (exportedCount > 0) {
        status.addDebugLog(QString("Preview images written: %1 files to %2").arg(exportedCount).arg(imagesDir));
    }
    status.previewImageWritten = allSuccess;
    return allSuccess;
}

bool WriteWorker::writeDatasheetFile(ComponentExportStatus& status) {
    if (status.datasheetData.isEmpty()) {
        return true;
    }

    QDir outputDir(m_outputPath);
    QString datasheetsDir = outputDir.filePath("datasheets");
    if (!createOutputDirectory(datasheetsDir)) {
        status.addDebugLog(QString("ERROR: Failed to create datasheets directory: %1").arg(datasheetsDir));
        return false;
    }

    QString safeName = status.componentId;
    safeName.replace(INVALID_FILENAME_CHARS, "_");

    // 根据数据内容判断格式
    QString extension = "pdf";
    if (status.datasheetData.startsWith("<!DOCTYPE html") || status.datasheetData.startsWith("<html")) {
        extension = "html";
    }

    QString filename = QString("%1.%2").arg(safeName).arg(extension);
    QString filePath = QDir(datasheetsDir).filePath(filename);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(status.datasheetData);
        file.close();
        status.addDebugLog(QString("Datasheet written: %1").arg(filePath));
        status.datasheetWritten = true;
        return true;
    }

    status.addDebugLog(QString("ERROR: Failed to write datasheet: %1").arg(filePath));
    return false;
}

void WriteWorker::abort() {
    m_isAborted.storeRelaxed(1);
    m_status->addDebugLog(QString("WriteWorker abort requested for component: %1").arg(m_status->componentId));
}

}  // namespace EasyKiConverter
