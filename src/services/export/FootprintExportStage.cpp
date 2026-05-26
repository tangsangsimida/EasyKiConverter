#include "FootprintExportStage.h"

#include "KiCadLibraryTableManager.h"
#include "core/kicad/Exporter3DModel.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/utils/GeometryUtils.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThread>

#include <cmath>
#include <limits>

namespace EasyKiConverter {

namespace {
constexpr double MODEL_Z_OFFSET_EPSILON_MM = 0.01;
constexpr double EASYEDA_UNIT_TO_MM = 0.254;  // 逆向转换用，正向请用 GeometryUtils::convertToMm()
constexpr double EASYEDA_Z_OFFSET_BIAS = 0.000001;  // 避免对齐到精确零值导致 KiCad 忽略偏移
constexpr auto FP_TYPE_SMD = "smd";

bool calculateStepGeometryCenter(const QByteArray& stepData, Model3DBase* center) {
    if (stepData.isEmpty() || center == nullptr) {
        return false;
    }

    const QString content = QString::fromLatin1(stepData);
    // Thread-safe: const QRegularExpression; globalMatch() is reentrant.
    static const QRegularExpression pointRegex(
        QStringLiteral("#(\\d+)\\s*=\\s*CARTESIAN_POINT\\s*\\(\\s*('[^']*'|\\$)\\s*,\\s*\\(([^()]*)\\)\\s*\\)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression vertexPointRegex(QStringLiteral("VERTEX_POINT\\s*\\([^,]*,\\s*#(\\d+)\\s*\\)"),
                                                     QRegularExpression::CaseInsensitiveOption);

    // 单次遍历收集所有 CARTESIAN_POINT，避免对整个文件做两次正则扫描
    struct Vec3 {
        double x = 0, y = 0, z = 0;
    };

    QHash<int, Vec3> allPoints;
    QRegularExpressionMatchIterator pointMatches = pointRegex.globalMatch(content);
    while (pointMatches.hasNext()) {
        const QRegularExpressionMatch match = pointMatches.next();
        bool idOk = false;
        int pointId = match.captured(1).toInt(&idOk);
        if (!idOk) {
            continue;
        }

        const QStringList coordinateParts = match.captured(3).split(',', Qt::SkipEmptyParts);
        if (coordinateParts.size() < 3) {
            continue;
        }

        bool okX = false;
        bool okY = false;
        bool okZ = false;
        const double x = coordinateParts.at(0).trimmed().toDouble(&okX);
        const double y = coordinateParts.at(1).trimmed().toDouble(&okY);
        const double z = coordinateParts.at(2).trimmed().toDouble(&okZ);
        if (!okX || !okY || !okZ) {
            continue;
        }

        allPoints.insert(pointId, {x, y, z});
    }

    // 第二次遍历仅扫描 VERTEX_POINT（数量远少于 CARTESIAN_POINT）
    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    double minZ = std::numeric_limits<double>::max();
    bool hasGeometryPoint = false;

    QRegularExpressionMatchIterator vertexMatches = vertexPointRegex.globalMatch(content);
    while (vertexMatches.hasNext()) {
        bool ok = false;
        int id = vertexMatches.next().captured(1).toInt(&ok);
        if (!ok) {
            continue;
        }

        auto it = allPoints.constFind(id);
        if (it == allPoints.constEnd()) {
            continue;
        }

        const Vec3& p = it.value();
        minX = qMin(minX, p.x);
        maxX = qMax(maxX, p.x);
        minY = qMin(minY, p.y);
        maxY = qMax(maxY, p.y);
        minZ = qMin(minZ, p.z);
        hasGeometryPoint = true;
    }

    if (!hasGeometryPoint) {
        return false;
    }

    center->x = (minX + maxX) / 2.0;
    center->y = (minY + maxY) / 2.0;
    center->z = minZ;  // 有意取最小值而非中心——用于 Z 轴对齐
    return true;
}

double calculateStepZOffset(double wrlDisplayMinZ, double stepMinZ) {
    if (wrlDisplayMinZ == std::numeric_limits<double>::max()) {
        return stepMinZ > MODEL_Z_OFFSET_EPSILON_MM ? -stepMinZ : 0.0;
    }

    const double offset = wrlDisplayMinZ - stepMinZ;
    return qAbs(offset) < MODEL_Z_OFFSET_EPSILON_MM ? 0.0 : offset;
}

bool readModelSourceZMm(const QByteArray& cadJsonRaw, const QString& modelUuid, double* zMm) {
    if (cadJsonRaw.isEmpty() || modelUuid.isEmpty() || zMm == nullptr) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument cadDoc = QJsonDocument::fromJson(cadJsonRaw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !cadDoc.isObject()) {
        return false;
    }

    const QJsonObject packageData =
        cadDoc.object().value(QStringLiteral("packageDetail")).toObject().value(QStringLiteral("dataStr")).toObject();
    const QJsonArray shapes = packageData.value(QStringLiteral("shape")).toArray();
    for (const QJsonValue& shapeValue : shapes) {
        const QString shape = shapeValue.toString();
        if (!shape.startsWith(QStringLiteral("SVGNODE~"))) {
            continue;
        }

        const QByteArray svgNodeJson = shape.mid(QStringLiteral("SVGNODE~").size()).toUtf8();
        QJsonParseError svgParseError;
        const QJsonDocument svgDoc = QJsonDocument::fromJson(svgNodeJson, &svgParseError);
        if (svgParseError.error != QJsonParseError::NoError || !svgDoc.isObject()) {
            continue;
        }

        const QJsonObject attrs = svgDoc.object().value(QStringLiteral("attrs")).toObject();
        if (attrs.value(QStringLiteral("uuid")).toString() != modelUuid) {
            continue;
        }

        bool ok = false;
        double z = 0.0;
        const QJsonValue zValue = attrs.value(QStringLiteral("z"));
        if (zValue.isString()) {
            z = zValue.toString().toDouble(&ok);
        } else if (zValue.isDouble()) {
            z = zValue.toDouble();
            ok = true;
        }
        if (!ok) {
            return false;
        }

        *zMm = GeometryUtils::convertToMm(z);
        return true;
    }

    return false;
}

double calculateWrlBaseZOffset(const QString& fpType, double wrlDisplayMinZ, double sourceZMm) {
    if (fpType != QLatin1String(FP_TYPE_SMD) || wrlDisplayMinZ == std::numeric_limits<double>::max()) {
        return 0.0;
    }

    const double offset = -wrlDisplayMinZ + sourceZMm;
    return offset > MODEL_Z_OFFSET_EPSILON_MM ? offset : 0.0;
}

double zOffsetMmToEasyEdaUnits(double zOffsetMm) {
    const double roundedOffset = std::round(zOffsetMm * 100.0) / 100.0;
    return -(roundedOffset - EASYEDA_Z_OFFSET_BIAS) / EASYEDA_UNIT_TO_MM;
}
}  // namespace

FootprintExportStage::FootprintExportStage(QObject* parent)
    : ExportTypeStage("Footprint", 1, parent) {  // maxConcurrent=1 因为是库级别导出
}

FootprintExportStage::~FootprintExportStage() {
    waitForWorkerThread(m_workerThread, 30000);
}

void FootprintExportStage::start(const QStringList& componentIds,
                                 const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "FootprintExportStage: Export already in progress";
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "FootprintExportStage: No components to export";
        emit completed(0, 0, 0);
        return;
    }

    m_componentIds = componentIds;
    m_cachedData = cachedData;
    m_cancelled.store(false);
    m_isRunning.store(true);

    {
        QMutexLocker locker(&m_progressMutex);
        m_progress = ExportTypeProgress();
        m_progress.typeName = QStringLiteral("Footprint");
        m_progress.totalCount = componentIds.size();
        m_progress.completedCount = 0;
        m_progress.successCount = 0;
        m_progress.failedCount = 0;
        m_progress.skippedCount = 0;
        m_progress.inProgressCount = componentIds.size();
        for (const QString& componentId : componentIds) {
            ExportItemStatus itemStatus;
            itemStatus.status = ExportItemStatus::Status::InProgress;
            itemStatus.startTime = QDateTime::currentDateTime();
            m_progress.itemStatus[componentId] = itemStatus;
        }
    }

    m_tempManager.setOutputPath(m_options.outputPath);

    m_isExporting.store(true);
    m_workerThread = QThread::create([this, componentIds, cachedData]() { doLibraryExport(componentIds, cachedData); });
    connect(m_workerThread, &QThread::finished, this, [this]() { m_workerThread = nullptr; });
    m_workerThread->start();
}

void FootprintExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "FootprintExportStage: Cancelling...";

    if (!waitForWorkerThread(m_workerThread)) {
        return;  // 超时，交由析构函数再次等待
    }

    m_tempManager.rollbackAll();
    m_isExporting.store(false);
    m_isRunning.store(false);

    qDebug() << "FootprintExportStage: Cancelled";
}

void FootprintExportStage::doLibraryExport(const QStringList& componentIds,
                                           const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    qDebug() << "FootprintExportStage: Starting library export in worker thread for" << componentIds.size()
             << "components";

    QList<FootprintData> footprintList;
    QStringList failedIds;
    int successCount = 0;
    int skippedCount = 0;

    for (const QString& componentId : componentIds) {
        if (m_cancelled.load()) {
            qDebug() << "FootprintExportStage: Export cancelled during data collection";
            break;
        }

        auto it = cachedData.find(componentId);
        if (it == cachedData.end() || !it.value()) {
            qWarning() << "FootprintExportStage: No data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No component data";
            emit itemStatusChanged(componentId, status);
            continue;
        }

        QSharedPointer<ComponentData> data = it.value();

        if (!data->footprintData()) {
            qWarning() << "FootprintExportStage: No footprint data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No footprint data";
            emit itemStatusChanged(componentId, status);
            continue;
        }

        FootprintData footprint = *data->footprintData();
        if (m_options.needsModel3DStep()) {
            Model3DData model3D = footprint.model3D();
            if (!model3D.uuid().isEmpty()) {
                QByteArray stepData;
                if (data->model3DData() && !data->model3DData()->step().isEmpty()) {
                    stepData = data->model3DData()->step();
                }
                if (stepData.isEmpty()) {
                    stepData = ComponentCacheService::instance()->loadModel3D(model3D.uuid(), QStringLiteral("step"));
                }
                if (stepData.isEmpty()) {
                    Exporter3DModel modelExporter;
                    if (modelExporter.downloadStepDataSync(model3D.uuid(), &stepData) && !stepData.isEmpty()) {
                        ComponentCacheService::instance()->saveModel3D(
                            model3D.uuid(), stepData, QStringLiteral("step"));
                    }
                }

                Model3DBase geometryCenter;
                if (calculateStepGeometryCenter(stepData, &geometryCenter)) {
                    QByteArray objData = data->model3DObjRaw();
                    if (objData.isEmpty() && data->model3DData()) {
                        objData = data->model3DData()->rawObj().toUtf8();
                    }
                    if (objData.isEmpty()) {
                        objData = ComponentCacheService::instance()->loadModel3D(model3D.uuid(), QStringLiteral("obj"));
                    }
                    QByteArray wrlData;
                    if (objData.isEmpty()) {
                        wrlData = ComponentCacheService::instance()->loadModel3D(model3D.uuid(), QStringLiteral("wrl"));
                    }
                    if (objData.isEmpty() && wrlData.isEmpty()) {
                        Exporter3DModel modelExporter;
                        if (modelExporter.downloadObjDataSync(model3D.uuid(), &objData) && !objData.isEmpty()) {
                            ComponentCacheService::instance()->saveModel3D(
                                model3D.uuid(), objData, QStringLiteral("obj"));
                        }
                    }

                    double wrlDisplayMinZ = std::numeric_limits<double>::max();
                    if (!objData.isEmpty()) {
                        // OBJ 全部顶点在 Z>0 时，模型视为平放在板面上，钳位到 0
                        const double rawMinZ = Exporter3DModel::calculateObjMinZ(objData);
                        wrlDisplayMinZ = rawMinZ > 0.0 ? 0.0 : rawMinZ;
                    } else if (!wrlData.isEmpty()) {
                        wrlDisplayMinZ = Exporter3DModel::calculateWrlDisplayMinZ(wrlData);
                    }
                    QByteArray cadJsonRaw = data->cadJsonRaw();
                    if (cadJsonRaw.isEmpty()) {
                        cadJsonRaw = ComponentCacheService::instance()->loadCadDataJson(componentId);
                    }
                    double sourceZMm = GeometryUtils::convertToMm(model3D.translation().z);
                    (void)readModelSourceZMm(cadJsonRaw, model3D.uuid(), &sourceZMm);
                    const double wrlBaseZOffset =
                        calculateWrlBaseZOffset(footprint.info().type, wrlDisplayMinZ, sourceZMm);
                    if (wrlBaseZOffset > 0.0) {
                        Model3DBase translation = model3D.translation();
                        translation.z = zOffsetMmToEasyEdaUnits(wrlBaseZOffset);
                        model3D.setTranslation(translation);
                    }
                    const double stepMinZ = geometryCenter.z;

                    Model3DBase stepOffset;
                    stepOffset.x = -geometryCenter.x;
                    stepOffset.y = -geometryCenter.y;
                    stepOffset.z = calculateStepZOffset(wrlDisplayMinZ, stepMinZ);
                    model3D.setStepOffsetMm(stepOffset);
                    footprint.setModel3D(model3D);
                    qDebug() << "FootprintExportStage: STEP offset for" << componentId << "uuid" << model3D.uuid()
                             << "xyCenter:" << geometryCenter.x << geometryCenter.y << "minZ:" << geometryCenter.z
                             << "wrlDisplayMinZ:" << wrlDisplayMinZ << "stepMinZ:" << stepMinZ
                             << "sourceZMm:" << sourceZMm << "wrlBaseZOffset:" << wrlBaseZOffset
                             << "offset:" << stepOffset.x << stepOffset.y << stepOffset.z;
                }
            }
        }

        footprintList.append(footprint);
        successCount++;

        ExportItemStatus status;
        status.status = ExportItemStatus::Status::Success;
        emit itemStatusChanged(componentId, status);

        qDebug() << "FootprintExportStage: Collected footprint for" << componentId;
    }

    if (m_cancelled.load()) {
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    if (footprintList.isEmpty()) {
        qWarning() << "FootprintExportStage: No valid footprints to export";
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString dirName = libName + QStringLiteral(".pretty");
    QString outputDir = m_options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString finalDir = outputDir + QDir::separator() + dirName;

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        qCritical() << "FootprintExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    QString tempDirPath = m_tempManager.createTempDirectoryPath(dirName);
    if (tempDirPath.isEmpty()) {
        qCritical() << "FootprintExportStage: Failed to create temp dir path";
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    // 追加/更新封装库时，先将已有 .kicad_mod 复制到临时目录，避免提交临时目录时丢失旧封装。
    const bool preserveExistingFootprints =
        QDir(finalDir).exists() && (!m_options.overwriteExistingFiles || m_options.updateMode || m_options.retryMode);
    if (preserveExistingFootprints) {
        if (!QDir().mkpath(tempDirPath)) {
            qCritical() << "FootprintExportStage: Failed to create temp dir for merge:" << tempDirPath;
            m_isExporting.store(false);
            m_isRunning.store(false);
            emit completed(0, footprintList.size(), 0);
            return;
        }
        const QStringList existingFiles = QDir(finalDir).entryList({"*.kicad_mod"}, QDir::Files);
        for (const QString& file : existingFiles) {
            if (!QFile::copy(finalDir + QDir::separator() + file, tempDirPath + QDir::separator() + file)) {
                qWarning() << "FootprintExportStage: Failed to copy existing footprint:" << file;
            }
        }
        qDebug() << "FootprintExportStage: Preserved" << existingFiles.size() << "existing footprints";
    }

    qDebug() << "FootprintExportStage: Exporting" << footprintList.size() << "footprints to temp:" << tempDirPath;

    bool exportSuccess = false;
    QString libraryDescription = m_options.footprintLibraryDescription;
    {
        ExporterFootprint exporter;
        const bool preferWrl = m_options.needsModel3DWrl();
        const bool exportStep = m_options.needsModel3DStep();
        QString libraryKeywords = m_options.footprintLibraryKeywords;
        exportSuccess =
            exporter.exportFootprintLibrary(footprintList,
                                            libName,
                                            tempDirPath,
                                            preferWrl,
                                            exportStep,
                                            libraryDescription,
                                            libraryKeywords,
                                            m_options.exportModel3DPathMode == ExportOptions::MODEL_3D_PATH_ABSOLUTE,
                                            outputDir);
    }

    if (m_cancelled.load()) {
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    if (!exportSuccess) {
        qCritical() << "FootprintExportStage: Failed to export footprint library";
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    if (m_tempManager.commitDirectoryWithBackup(tempDirPath, finalDir)) {
        qDebug() << "FootprintExportStage: Successfully exported to:" << finalDir;
    } else {
        qCritical() << "FootprintExportStage: Failed to commit temp dir";
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    if (!libraryDescription.isEmpty()) {
        ExporterFootprint fpTableExporter;
        fpTableExporter.generateFpLibTable(libName, finalDir, outputDir, libraryDescription);
        KiCadLibraryTableManager::registerFootprintLibrary(outputDir, libName, finalDir, libraryDescription);
    }

    m_tempManager.cleanupTempDirectory();

    qDebug() << "FootprintExportStage: Completed. Success:" << successCount << "Failed:" << failedIds.size();

    m_isExporting.store(false);
    m_isRunning.store(false);
    emit completed(successCount, failedIds.size(), skippedCount);
}

}  // namespace EasyKiConverter
