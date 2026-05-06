#include "FootprintExportStage.h"

#include "KiCadLibraryTableManager.h"
#include "core/kicad/ExporterFootprint.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QThread>

#include <limits>

namespace EasyKiConverter {

namespace {
bool calculateStepGeometryCenter(const QByteArray& stepData, Model3DBase* center) {
    if (stepData.isEmpty() || center == nullptr) {
        return false;
    }

    const QString content = QString::fromLatin1(stepData);
    static const QRegularExpression pointRegex(
        QStringLiteral("#(\\d+)\\s*=\\s*CARTESIAN_POINT\\s*\\(\\s*('[^']*'|\\$)\\s*,\\s*\\(([^()]*)\\)\\s*\\)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression vertexPointRegex(QStringLiteral("VERTEX_POINT\\s*\\([^,]*,\\s*#(\\d+)\\s*\\)"),
                                                     QRegularExpression::CaseInsensitiveOption);

    QSet<QString> vertexPointIds;
    QRegularExpressionMatchIterator vertexMatches = vertexPointRegex.globalMatch(content);
    while (vertexMatches.hasNext()) {
        vertexPointIds.insert(vertexMatches.next().captured(1));
    }

    double minX = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double minY = std::numeric_limits<double>::max();
    double maxY = std::numeric_limits<double>::lowest();
    double minZ = std::numeric_limits<double>::max();
    bool hasGeometryPoint = false;

    QRegularExpressionMatchIterator matches = pointRegex.globalMatch(content);
    while (matches.hasNext()) {
        const QRegularExpressionMatch match = matches.next();
        if (!vertexPointIds.isEmpty() && !vertexPointIds.contains(match.captured(1))) {
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

        minX = qMin(minX, x);
        maxX = qMax(maxX, x);
        minY = qMin(minY, y);
        maxY = qMax(maxY, y);
        minZ = qMin(minZ, z);
        hasGeometryPoint = true;
    }

    if (!hasGeometryPoint) {
        return false;
    }

    center->x = (minX + maxX) / 2.0;
    center->y = (minY + maxY) / 2.0;
    center->z = minZ;
    return true;
}
}  // namespace

FootprintExportStage::FootprintExportStage(QObject* parent)
    : ExportTypeStage("Footprint", 1, parent) {  // maxConcurrent=1 因为是库级别导出
}

FootprintExportStage::~FootprintExportStage() {
    cancel();
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

    // 设置临时文件管理器
    m_tempManager.setOutputPath(m_options.outputPath);

    // 在工作线程中执行库级别的导出
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

    // 设置取消标志
    m_cancelled.store(true);

    // 等待工作线程结束（最多等待 5 秒）
    if (m_workerThread && m_workerThread->isRunning()) {
        qDebug() << "FootprintExportStage: Waiting for worker thread to finish...";
        m_workerThread->quit();
        if (!m_workerThread->wait(5000)) {
            qWarning() << "FootprintExportStage: Thread did not finish in 5s, terminating";
            m_workerThread->terminate();
        }
        m_workerThread = nullptr;
    }

    // 清理临时文件
    m_tempManager.rollbackAll();

    m_isExporting.store(false);
    m_isRunning.store(false);

    qDebug() << "FootprintExportStage: Cancelled";
}

void FootprintExportStage::doLibraryExport(const QStringList& componentIds,
                                           const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    qDebug() << "FootprintExportStage: Starting library export in worker thread for" << componentIds.size()
             << "components";

    // 1. 收集所有有效的封装数据
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
            // 发射 itemStatusChanged 信号通知 ViewModel
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No component data";
            emit itemStatusChanged(componentId, status);
            continue;
        }

        QSharedPointer<ComponentData> data = it.value();

        // 检查封装数据
        if (!data->footprintData()) {
            qWarning() << "FootprintExportStage: No footprint data for component:" << componentId;
            failedIds.append(componentId);
            // 发射 itemStatusChanged 信号通知 ViewModel
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

                Model3DBase geometryCenter;
                if (calculateStepGeometryCenter(stepData, &geometryCenter)) {
                    Model3DBase stepOffset;
                    stepOffset.x = -geometryCenter.x;
                    stepOffset.y = -geometryCenter.y;
                    stepOffset.z = -geometryCenter.z;
                    model3D.setStepOffsetMm(stepOffset);
                    footprint.setModel3D(model3D);
                    qDebug() << "FootprintExportStage: STEP geometry center for" << componentId << "uuid"
                             << model3D.uuid() << "center:" << geometryCenter.x << geometryCenter.y << geometryCenter.z
                             << "offset:" << stepOffset.x << stepOffset.y << stepOffset.z;
                }
            }
        }

        // 添加到封装列表
        footprintList.append(footprint);
        successCount++;

        // 发射 itemStatusChanged 信号通知 ViewModel
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

    // 2. 构建输出路径
    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString dirName = libName + QStringLiteral(".pretty");
    QString outputDir = m_options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString finalDir = outputDir + QDir::separator() + dirName;

    // 3. 确保输出目录存在
    QDir dir;
    if (!dir.mkpath(outputDir)) {
        qCritical() << "FootprintExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    // 4. 创建临时目录路径并导出
    QString tempDirPath = m_tempManager.createTempDirectoryPath(dirName);
    if (tempDirPath.isEmpty()) {
        qCritical() << "FootprintExportStage: Failed to create temp dir path";
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    qDebug() << "FootprintExportStage: Exporting" << footprintList.size() << "footprints to temp:" << tempDirPath;

    // 5. 执行封装库导出
    bool exportSuccess = false;
    QString libraryDescription = m_options.footprintLibraryDescription;
    {
        ExporterFootprint exporter;
        // 传入3D模型格式标志：WRL和STEP可以同时导出，生成两个 (model ...) 块
        const bool preferWrl = m_options.needsModel3DWrl();
        const bool exportStep = m_options.needsModel3DStep();
        QString libraryKeywords = m_options.footprintLibraryKeywords;
        exportSuccess = exporter.exportFootprintLibrary(
            footprintList, libName, tempDirPath, preferWrl, exportStep, libraryDescription, libraryKeywords);
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

    // 6. 提交临时目录（重命名为最终路径）
    if (m_tempManager.commitDirectory(finalDir)) {
        qDebug() << "FootprintExportStage: Successfully exported to:" << finalDir;
    } else {
        qCritical() << "FootprintExportStage: Failed to commit temp dir";
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    // 6.1 生成 fp-lib-table 文件（如果提供了库描述）
    if (!libraryDescription.isEmpty()) {
        ExporterFootprint fpTableExporter;
        fpTableExporter.generateFpLibTable(libName, finalDir, outputDir, libraryDescription);
        KiCadLibraryTableManager::registerFootprintLibrary(outputDir, libName, finalDir, libraryDescription);
    }

    // 7. 清理临时目录
    m_tempManager.cleanupTempDirectory();

    // 8. 完成
    qDebug() << "FootprintExportStage: Completed. Success:" << successCount << "Failed:" << failedIds.size();

    m_isExporting.store(false);
    m_isRunning.store(false);
    emit completed(successCount, failedIds.size(), skippedCount);
}

}  // namespace EasyKiConverter
