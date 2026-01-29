#include "WriteWorker.h"

#include "core/kicad/Exporter3DModel.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QThreadPool>
#include <QWaitCondition>

namespace EasyKiConverter {


WriteWorker::WriteWorker(QSharedPointer<ComponentExportStatus> status,
                         const QString& outputPath,
                         const QString& libName,
                         bool exportSymbol,
                         bool exportFootprint,
                         bool exportModel3D,
                         bool debugMode,
                         QObject* parent)
    : QObject(parent)
    , m_status(status)
    , m_outputPath(outputPath)
    , m_libName(libName)
    , m_exportSymbol(exportSymbol)
    , m_exportFootprint(exportFootprint)
    , m_exportModel3D(exportModel3D)
    , m_debugMode(debugMode)
    , m_symbolExporter()
    , m_footprintExporter()
    , m_model3DExporter() {}

WriteWorker::~WriteWorker() {}

void WriteWorker::run() {
    QElapsedTimer writeTimer;
    writeTimer.start();

    m_status->addDebugLog(QString("WriteWorker started for component: %1").arg(m_status->componentId));


    if (!createOutputDirectory(m_outputPath)) {
        m_status->writeDurationMs = writeTimer.elapsed();
        m_status->writeSuccess = false;
        m_status->writeMessage = "Failed to create output directory";
        m_status->addDebugLog(
            QString("ERROR: Failed to create output directory, Duration: %1ms").arg(m_status->writeDurationMs));
        emit writeCompleted(m_status);
        return;
    }

    bool allSuccess = true;

    // 串行写入文件，避免创建线程池的高开销
    if (m_exportSymbol && m_status->symbolData) {
        if (!writeSymbolFile(*m_status)) {
            allSuccess = false;
        }
    }

    if (m_exportFootprint && m_status->footprintData) {
        if (!writeFootprintFile(*m_status)) {
            allSuccess = false;
        }
    }

    if (m_exportModel3D && m_status->model3DData && !m_status->model3DObjRaw.isEmpty()) {
        if (!write3DModelFile(*m_status)) {
            allSuccess = false;
        }
    }

    if (!allSuccess) {
        m_status->writeDurationMs = writeTimer.elapsed();
        m_status->writeSuccess = false;
        m_status->writeMessage = "Failed to write one or more files";
        m_status->addDebugLog(
            QString("ERROR: Failed to write one or more files, Duration: %1ms").arg(m_status->writeDurationMs));
        emit writeCompleted(m_status);
        return;
    }


    if (m_debugMode) {
        exportDebugData(*m_status);
    }

    m_status->writeDurationMs = writeTimer.elapsed();
    m_status->writeSuccess = true;
    m_status->writeMessage = "Write completed successfully";
    m_status->addDebugLog(QString("WriteWorker completed successfully for component: %1, Duration: %2ms")
                              .arg(m_status->componentId)
                              .arg(m_status->writeDurationMs));

    emit writeCompleted(m_status);
}

bool WriteWorker::writeSymbolFile(ComponentExportStatus& status) {
    if (!status.symbolData) {
        return true;
    }


    QString tempFilePath = QString("%1/%2.kicad_sym.tmp").arg(m_outputPath, status.componentId);

    if (!m_symbolExporter.exportSymbol(*status.symbolData, tempFilePath)) {
        status.addDebugLog(QString("ERROR: Failed to write symbol file: %1").arg(tempFilePath));
        return false;
    }

    status.addDebugLog(QString("Symbol file written: %1").arg(tempFilePath));
    return true;
}

bool WriteWorker::writeFootprintFile(ComponentExportStatus& status) {
    if (!status.footprintData) {
        return true;
    }

    QString footprintLibPath = QString("%1/%2.pretty").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(footprintLibPath)) {
        status.addDebugLog(QString("ERROR: Failed to create footprint library directory: %1").arg(footprintLibPath));
        return false;
    }

    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (m_exportModel3D) {
        createOutputDirectory(modelsDirPath);
    }

    QString footprintName = status.footprintData->info().name;
    QString filePath = QString("%1/%2.kicad_mod").arg(footprintLibPath, footprintName);

    QString model3DWrlPath;
    QString model3DStepPath;
    if (m_exportModel3D && status.model3DData && !status.model3DData->uuid().isEmpty()) {
        model3DWrlPath = QString("../%1.3dmodels/%2.wrl").arg(m_libName, footprintName);
        if (!status.model3DStepRaw.isEmpty()) {
            model3DStepPath = QString("../%1.3dmodels/%2.step").arg(m_libName, footprintName);
        }
    }


    if (!model3DStepPath.isEmpty()) {
        if (!m_footprintExporter.exportFootprint(*status.footprintData, filePath, model3DWrlPath, model3DStepPath)) {
            status.addDebugLog(QString("ERROR: Failed to write footprint file: %1").arg(filePath));
            return false;
        }
    } else {
        if (!m_footprintExporter.exportFootprint(*status.footprintData, filePath, model3DWrlPath)) {
            status.addDebugLog(QString("ERROR: Failed to write footprint file: %1").arg(filePath));
            return false;
        }
    }

    status.addDebugLog(QString("Footprint file written: %1").arg(filePath));
    return true;
}

bool WriteWorker::write3DModelFile(ComponentExportStatus& status) {
    if (!status.model3DData || status.model3DObjRaw.isEmpty()) {
        return true;
    }


    QString modelsDirPath = QString("%1/%2.3dmodels").arg(m_outputPath, m_libName);
    if (!createOutputDirectory(modelsDirPath)) {
        status.addDebugLog(QString("ERROR: Failed to create 3D models directory: %1").arg(modelsDirPath));
        return false;
    }


    QString footprintName = status.footprintData ? status.footprintData->info().name : status.componentId;


    QString wrlFilePath = QString("%1/%2.wrl").arg(modelsDirPath, footprintName);
    if (!m_model3DExporter.exportToWrl(*status.model3DData, wrlFilePath)) {
        status.addDebugLog(QString("ERROR: Failed to write WRL file: %1").arg(wrlFilePath));
    } else {
        status.addDebugLog(QString("3D model WRL file written: %1").arg(wrlFilePath));
    }


    if (!status.model3DStepRaw.isEmpty()) {
        QString stepFilePath = QString("%1/%2.step").arg(modelsDirPath, footprintName);
        QFile stepFile(stepFilePath);
        if (stepFile.open(QIODevice::WriteOnly)) {
            stepFile.write(status.model3DStepRaw);
            stepFile.close();
            status.addDebugLog(QString("3D model STEP file written: %1").arg(stepFilePath));
        } else {
            status.addDebugLog(QString("ERROR: Failed to write STEP file: %1").arg(stepFilePath));
        }
    }

    return true;
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
        QJsonObject symbolInfo = status.symbolData->info().toJson();
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
            pinsArray.append(pin.toJson());
        }
        symbolInfo["pins"] = pinsArray;


        QJsonArray rectanglesArray;
        for (const SymbolRectangle& rect : status.symbolData->rectangles()) {
            rectanglesArray.append(rect.toJson());
        }
        symbolInfo["rectangles"] = rectanglesArray;


        QJsonArray circlesArray;
        for (const SymbolCircle& circle : status.symbolData->circles()) {
            circlesArray.append(circle.toJson());
        }
        symbolInfo["circles"] = circlesArray;


        QJsonArray arcsArray;
        for (const SymbolArc& arc : status.symbolData->arcs()) {
            arcsArray.append(arc.toJson());
        }
        symbolInfo["arcs"] = arcsArray;


        QJsonArray polylinesArray;
        for (const SymbolPolyline& polyline : status.symbolData->polylines()) {
            polylinesArray.append(polyline.toJson());
        }
        symbolInfo["polylines"] = polylinesArray;


        QJsonArray polygonsArray;
        for (const SymbolPolygon& polygon : status.symbolData->polygons()) {
            polygonsArray.append(polygon.toJson());
        }
        symbolInfo["polygons"] = polygonsArray;


        QJsonArray pathsArray;
        for (const SymbolPath& path : status.symbolData->paths()) {
            pathsArray.append(path.toJson());
        }
        symbolInfo["paths"] = pathsArray;


        QJsonArray ellipsesArray;
        for (const SymbolEllipse& ellipse : status.symbolData->ellipses()) {
            ellipsesArray.append(ellipse.toJson());
        }
        symbolInfo["ellipses"] = ellipsesArray;

        debugInfo["symbolData"] = symbolInfo;
    }


    if (status.footprintData) {
        QJsonObject footprintInfo = status.footprintData->info().toJson();
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
            padsArray.append(pad.toJson());
        }
        footprintInfo["pads"] = padsArray;


        QJsonArray tracksArray;
        for (const FootprintTrack& track : status.footprintData->tracks()) {
            tracksArray.append(track.toJson());
        }
        footprintInfo["tracks"] = tracksArray;


        QJsonArray holesArray;
        for (const FootprintHole& hole : status.footprintData->holes()) {
            holesArray.append(hole.toJson());
        }
        footprintInfo["holes"] = holesArray;


        QJsonArray circlesArray;
        for (const FootprintCircle& circle : status.footprintData->circles()) {
            circlesArray.append(circle.toJson());
        }
        footprintInfo["circles"] = circlesArray;


        QJsonArray arcsArray;
        for (const FootprintArc& arc : status.footprintData->arcs()) {
            arcsArray.append(arc.toJson());
        }
        footprintInfo["arcs"] = arcsArray;


        QJsonArray rectanglesArray;
        for (const FootprintRectangle& rect : status.footprintData->rectangles()) {
            rectanglesArray.append(rect.toJson());
        }
        footprintInfo["rectangles"] = rectanglesArray;


        QJsonArray textsArray;
        for (const FootprintText& text : status.footprintData->texts()) {
            textsArray.append(text.toJson());
        }
        footprintInfo["texts"] = textsArray;


        QJsonArray solidRegionsArray;
        for (const FootprintSolidRegion& region : status.footprintData->solidRegions()) {
            solidRegionsArray.append(region.toJson());
        }
        footprintInfo["solidRegions"] = solidRegionsArray;


        QJsonArray outlinesArray;
        for (const FootprintOutline& outline : status.footprintData->outlines()) {
            outlinesArray.append(outline.toJson());
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

}  // namespace EasyKiConverter
