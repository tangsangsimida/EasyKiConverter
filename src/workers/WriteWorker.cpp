#include "WriteWorker.h"

#include "core/kicad/Exporter3DModel.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
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
#include <QThread>
#include <QThreadPool>
#include <QWaitCondition>

namespace EasyKiConverter {

// 初始化静态互斥锁
QMutex WriteWorker::s_fileWriteMutex;

WriteWorker::WriteWorker(QSharedPointer<ComponentExportStatus> status,
                         const QString& outputPath,
                         const QString& libName,
                         bool exportSymbol,
                         bool exportFootprint,
                         bool exportModel3D,
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

    if (m_exportModel3D && m_status->model3DData && !m_status->model3DData->rawObj().isEmpty()) {
        write3DModelFile(*m_status);
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
    // 注意：3D模型导出失败不影响符号和封装的导出成功状态
    // 符号和封装是捆绑的核心输出，3D模型是可选附加输出
    bool coreSuccess = true;
    bool model3DFailed = false;
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

    m_status->writeSuccess = coreSuccess;
    if (coreSuccess && model3DFailed) {
        m_status->writeMessage = "Write completed (3D model export failed, symbol/footprint OK)";
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
        return true;  // 没有数据可写，不应视为失败
    }

    // 1. 检查临时目录是否存在
    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    // 2. 检查路径安全性
    if (!PathSecurity::isSafePath(m_tempDir, m_outputPath)) {
        status.addDebugLog(QString("SECURITY ERROR: Temp dir outside output path: %1").arg(m_tempDir));
        return false;
    }

    // 使用线程ID和当前时间生成唯一的临时文件名，避免并发冲突
    qint64 threadId = reinterpret_cast<qint64>(QThread::currentThreadId());
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    // 使用统一的临时文件夹
    QString tempFilePath =
        QString("%1/%2_%3_%4.kicad_sym.tmp").arg(m_tempDir, status.componentId).arg(threadId).arg(timestamp);
    QString finalFilePath = QString("%1/%2.kicad_sym").arg(m_tempDir, status.componentId);

    // 先写入临时文件
    if (!m_symbolExporter.exportSymbol(*status.symbolData, tempFilePath)) {
        status.addDebugLog(QString("ERROR: Failed to write symbol file: %1").arg(tempFilePath));
        QFile::remove(tempFilePath);  // 清理临时文件
        return false;                 // 导出器报告失败
    }

    // 验证临时文件是否真的被写入
    if (!QFile::exists(tempFilePath)) {
        status.addDebugLog(QString("ERROR: Symbol file not found after export: %1").arg(tempFilePath));
        return false;  // 文件未找到，视为失败
    }

    // 使用互斥锁保护文件删除和重命名操作，防止并发竞态条件
    {
        QMutexLocker locker(&s_fileWriteMutex);

        // 删除旧文件（如果存在）
        if (QFile::exists(finalFilePath)) {
            if (!QFile::remove(finalFilePath)) {
                status.addDebugLog(QString("WARNING: Failed to remove old symbol file: %1").arg(finalFilePath));
                // 继续尝试重命名，可能操作系统会处理
            }
        }

        // 原子性地重命名临时文件到最终文件
        if (!QFile::rename(tempFilePath, finalFilePath)) {
            status.addDebugLog(
                QString("ERROR: Failed to rename symbol file: %1 -> %2").arg(tempFilePath, finalFilePath));
            QFile::remove(tempFilePath);  // 清理临时文件
            return false;
        }
    }

    // 验证最终文件是否存在
    if (QFile::exists(finalFilePath)) {
        status.addDebugLog(QString("Symbol file written atomically: %1").arg(finalFilePath));
        status.symbolWritten = true;
        return true;
    } else {
        status.addDebugLog(QString("ERROR: Final symbol file not found after rename: %1").arg(finalFilePath));
        return false;
    }
}

bool WriteWorker::writeFootprintFile(ComponentExportStatus& status) {
    if (!status.footprintData) {
        return true;  // 没有数据可写，不应视为失败
    }

    // 1. 检查临时目录是否存在
    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    // 2. 检查路径安全性
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
        // 确保3D模型目录存在，但它的创建失败不直接导致封装写入失败
        if (!createOutputDirectory(modelsDirPath)) {
            status.addDebugLog(QString("WARNING: Failed to create 3D models directory: %1").arg(modelsDirPath));
        }
    }

    QString footprintName = PathSecurity::sanitizeFilename(status.footprintData->info().name);
    QString filePath = QString("%1/%2.kicad_mod").arg(footprintLibPath, footprintName);

    // 使用线程ID和当前时间生成唯一的临时文件名，避免并发冲突
    qint64 threadId = reinterpret_cast<qint64>(QThread::currentThreadId());
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    // 使用统一的临时文件夹
    QString tempFilePath =
        QString("%1/%2_%3_%4.kicad_mod.tmp").arg(m_tempDir, footprintName).arg(threadId).arg(timestamp);

    QString model3DWrlPath;
    QString model3DStepPath;
    if (m_exportModel3D && status.model3DData && !status.model3DData->uuid().isEmpty()) {
        // 始终使用封装名作为 3D 模型的文件名和引用路径，以确保与 write3DModelFile 写入磁盘的命名一致
        model3DWrlPath = QString("../%1.3dmodels/%2.wrl").arg(m_libName, footprintName);
        if (!status.model3DStepRaw.isEmpty()) {
            model3DStepPath = QString("../%1.3dmodels/%2.step").arg(m_libName, footprintName);
        }
    }

    // 先写入临时文件
    bool exportSuccess = false;
    if (!model3DStepPath.isEmpty()) {
        // 有 WRL 和 STEP 两个模型，调用双参数版本
        exportSuccess =
            m_footprintExporter.exportFootprint(*status.footprintData, tempFilePath, model3DWrlPath, model3DStepPath);
    } else if (!model3DWrlPath.isEmpty()) {
        // 只有 WRL 模型，调用单参数版本
        exportSuccess = m_footprintExporter.exportFootprint(*status.footprintData, tempFilePath, model3DWrlPath);
    } else {
        // 没有 3D 模型，调用无参数版本
        exportSuccess = m_footprintExporter.exportFootprint(*status.footprintData, tempFilePath);
    }

    if (!exportSuccess) {
        status.addDebugLog(QString("ERROR: Failed to write footprint file (exporter): %1").arg(tempFilePath));
        QFile::remove(tempFilePath);  // 清理临时文件
        return false;                 // 导出器报告失败
    }

    // 验证临时文件是否真的被写入
    if (!QFile::exists(tempFilePath)) {
        status.addDebugLog(QString("ERROR: Footprint file not found after export: %1").arg(tempFilePath));
        return false;  // 文件未找到，视为失败
    }

    // 使用互斥锁保护文件删除和重命名操作，防止并发竞态条件
    {
        QMutexLocker locker(&s_fileWriteMutex);

        // 删除旧文件（如果存在）
        if (QFile::exists(filePath)) {
            if (!QFile::remove(filePath)) {
                status.addDebugLog(QString("WARNING: Failed to remove old footprint file: %1").arg(filePath));
                // 继续尝试重命名
            }
        }

        // 原子性地重命名临时文件到最终文件
        if (!QFile::rename(tempFilePath, filePath)) {
            status.addDebugLog(QString("ERROR: Failed to rename footprint file: %1 -> %2").arg(tempFilePath, filePath));
            QFile::remove(tempFilePath);  // 清理临时文件
            return false;
        }
    }

    // 验证最终文件是否存在
    if (QFile::exists(filePath)) {
        status.addDebugLog(QString("Footprint file written atomically: %1").arg(filePath));
        status.footprintWritten = true;
        return true;
    } else {
        status.addDebugLog(QString("ERROR: Final footprint file not found after rename: %1").arg(filePath));
        return false;
    }
}

bool WriteWorker::write3DModelFile(ComponentExportStatus& status) {
    // 注意：model3DObjRaw 可能在 Process 阶段被 clearIntermediateData 清理
    // 所以必须检查 model3DData 对象中的 rawObj 字符串
    if (!status.model3DData || status.model3DData->rawObj().isEmpty()) {
        return true;  // 没有 WRL 数据可写，不应视为失败 (如果只请求 STEP，则应在ProcessWorker中处理)
    }

    // 1. 检查临时目录是否存在
    if (!QDir(m_tempDir).exists()) {
        status.addDebugLog(QString("ERROR: Temp directory does not exist: %1").arg(m_tempDir));
        return false;
    }
    // 2. 检查路径安全性（仅对临时目录路径检查）
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
    bool stepSuccess = false;

    // 导出 WRL 文件（使用 Write-Temp-Move 模式）
    QString wrlFilePath = QString("%1/%2.wrl").arg(modelsDirPath, footprintName);

    // 使用线程ID和当前时间生成唯一的临时文件名，避免并发冲突
    qint64 threadId = reinterpret_cast<qint64>(QThread::currentThreadId());
    qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    // 使用统一的临时文件夹
    QString wrlTempFilePath = QString("%1/%2_%3_%4.wrl.tmp").arg(m_tempDir, footprintName).arg(threadId).arg(timestamp);

    wrlSuccess = m_model3DExporter.exportToWrl(*status.model3DData, wrlTempFilePath);

    if (wrlSuccess && QFile::exists(wrlTempFilePath)) {
        // 使用互斥锁保护文件删除和重命名操作
        QMutexLocker locker(&s_fileWriteMutex);

        // 删除旧文件（如果存在）
        if (QFile::exists(wrlFilePath) && !QFile::remove(wrlFilePath)) {
            status.addDebugLog(QString("WARNING: Failed to remove old WRL file: %1").arg(wrlFilePath));
        }
        // 原子性地重命名
        if (QFile::rename(wrlTempFilePath, wrlFilePath)) {
            status.addDebugLog(QString("3D model WRL file written atomically: %1").arg(wrlFilePath));
        } else {
            status.addDebugLog(QString("ERROR: Failed to rename WRL file: %1 -> %2").arg(wrlTempFilePath, wrlFilePath));
            QFile::remove(wrlTempFilePath);
            wrlSuccess = false;
        }
    } else if (wrlSuccess) {
        status.addDebugLog(QString("ERROR: WRL file not found after export: %1").arg(wrlTempFilePath));
        wrlSuccess = false;
    } else {
        status.addDebugLog(QString("ERROR: Failed to write WRL file: %1").arg(wrlTempFilePath));
        QFile::remove(wrlTempFilePath);
    }

    // 导出 STEP 文件（如果有，使用 Write-Temp-Move 模式）
    if (!status.model3DStepRaw.isEmpty()) {
        QString stepFilePath = QString("%1/%2.step").arg(modelsDirPath, footprintName);

        // 使用线程ID和当前时间生成唯一的临时文件名，避免并发冲突
        qint64 threadId = reinterpret_cast<qint64>(QThread::currentThreadId());
        qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
        // 使用统一的临时文件夹
        QString stepTempFilePath =
            QString("%1/%2_%3_%4.step.tmp").arg(m_tempDir, footprintName).arg(threadId).arg(timestamp);

        QFile stepFile(stepTempFilePath);
        if (stepFile.open(QIODevice::WriteOnly)) {
            stepFile.write(status.model3DStepRaw);
            stepFile.close();

            if (QFile::exists(stepTempFilePath)) {
                // 使用互斥锁保护文件删除和重命名操作
                QMutexLocker locker(&s_fileWriteMutex);

                // 删除旧文件（如果存在）
                if (QFile::exists(stepFilePath) && !QFile::remove(stepFilePath)) {
                    status.addDebugLog(QString("WARNING: Failed to remove old STEP file: %1").arg(stepFilePath));
                }
                // 原子性地重命名
                if (QFile::rename(stepTempFilePath, stepFilePath)) {
                    status.addDebugLog(QString("3D model STEP file written atomically: %1").arg(stepFilePath));
                    stepSuccess = true;
                } else {
                    status.addDebugLog(
                        QString("ERROR: Failed to rename STEP file: %1 -> %2").arg(stepTempFilePath, stepFilePath));
                    QFile::remove(stepTempFilePath);
                }
            }
        } else {
            status.addDebugLog(QString("ERROR: Failed to write STEP file: %1").arg(stepTempFilePath));
            // STEP 文件失败不影响整体 3D 模型成功
        }

        // 内存优化：写入完成后立即清理 STEP 数据
        status.clearStepData();
    }

    // 只有在 WRL 文件成功导出且存在时才设置 model3DWritten 标志
    status.model3DWritten = wrlSuccess;

    // 整个3D模型导出成功，取决于 WRL 是否成功。STEP 失败不影响此结果。
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

    QString debugInfoFilePath = QString("%1/debug_info.json").arg(componentDebugDir);
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

void WriteWorker::abort() {
    m_isAborted.storeRelaxed(1);
    m_status->addDebugLog(QString("WriteWorker abort requested for component: %1").arg(m_status->componentId));
}

}  // namespace EasyKiConverter
