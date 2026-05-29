#include "Model3DExportStage.h"

#include "Model3DExportWorker.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/ComponentData.h"
#include "services/ComponentCacheService.h"
#include "utils/PathSecurity.h"

#include <QDebug>
#include <QDir>
#include <QPointer>

namespace EasyKiConverter {

Model3DExportStage::Model3DExportStage(QObject* parent) : ExportTypeStage("Model3D", 2, parent) {}

Model3DExportStage::~Model3DExportStage() {
    cancel();
}

void Model3DExportStage::start(const QStringList& componentIds,
                               const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "Model3DExportStage: Export already in progress";
        return;
    }

    m_threadPool.setMaxThreadCount(m_options.weakNetworkSupport ? 1 : 2);

    if (componentIds.isEmpty()) {
        qWarning() << "Model3DExportStage: No components to export";
        emit completed(0, 0, 0);
        return;
    }

    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".3dmodels");

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        qCritical() << "Model3DExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        emit completed(0, 0, 0);
        return;
    }

    m_tempManager.setOutputPath(outputDir);

    const bool needWrl = m_options.needsModel3DWrl();
    const bool needStep = m_options.needsModel3DStep();

    qInfo() << "Model3DExportStage::start() - exportModel3DFormat:" << m_options.exportModel3DFormat
            << "needWrl:" << needWrl << "needStep:" << needStep;

    const auto hasModel3DUuid = [&cachedData](const QString& componentId) {
        auto it = cachedData.constFind(componentId);
        if (it != cachedData.constEnd() && it.value() && it.value()->model3DData() &&
            !it.value()->model3DData()->uuid().isEmpty()) {
            return true;
        }

        QSharedPointer<ComponentData> cachedComponent =
            ComponentCacheService::instance()->loadComponentData(componentId);
        return cachedComponent && cachedComponent->model3DData() && !cachedComponent->model3DData()->uuid().isEmpty();
    };

    m_componentPaths.clear();
    for (const QString& componentId : componentIds) {
        if ((needWrl || needStep) && !hasModel3DUuid(componentId)) {
            qDebug() << "Model3DExportStage: No 3D model UUID, skipping temp paths for" << componentId;
            continue;
        }

        TempFilePaths paths;
        if (needWrl) {
            paths.wrlTempPath =
                m_tempManager.createTempFilePath(componentId + QStringLiteral("_wrl"), QStringLiteral(".wrl"));
        }
        if (needStep) {
            paths.stepTempPath =
                m_tempManager.createTempFilePath(componentId + QStringLiteral("_step"), QStringLiteral(".step"));
        }

        if ((needWrl && !paths.wrlTempPath.isEmpty()) || (needStep && !paths.stepTempPath.isEmpty())) {
            m_componentPaths[componentId] = paths;
        } else if (needWrl || needStep) {
            qWarning() << "Model3DExportStage: Failed to create temp path for component:" << componentId
                       << "needWrl:" << needWrl << "needStep:" << needStep;
        }
    }

    m_isExporting.store(true);

    // Paths must be ready before workers read m_componentPaths.
    ExportTypeStage::start(componentIds, cachedData);
}

void Model3DExportStage::cancel() {
    if (!m_isRunning.load() && !m_isExporting.load()) {
        return;
    }

    qDebug() << "Model3DExportStage: cancelling...";
    m_cancelled.store(true);

    bool hasActiveWorkers = false;
    {
        QMutexLocker locker(&m_workerMutex);
        m_pendingComponents.clear();
        hasActiveWorkers = !m_activeWorkers.isEmpty();
        if (!hasActiveWorkers) {
            m_isRunning.store(false);
        }
    }

    m_tempManager.rollbackAll();
    m_isExporting.store(false);

    qDebug() << "Model3DExportStage: cancelled";
}

QObject* Model3DExportStage::createWorker() {
    return new Model3DExportWorker();
}

void Model3DExportStage::startWorker(QObject* worker,
                                     const QString& componentId,
                                     const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<Model3DExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "Model3DExportStage: Failed to cast worker to Model3DExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    QString modelName;
    if (data && data->model3DData()) {
        modelName = data->model3DData()->name();
    }
    if (modelName.isEmpty() && data && data->footprintData()) {
        modelName = data->footprintData()->info().name;
    }
    if (modelName.isEmpty()) {
        modelName = componentId;
    }
    modelName = PathSecurity::sanitizeFilename(modelName);

    const bool needWrl = m_options.needsModel3DWrl();
    const bool needStep = m_options.needsModel3DStep();
    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".3dmodels");

    if (m_componentPaths.contains(componentId)) {
        TempFilePaths& paths = m_componentPaths[componentId];
        paths.wrlFinalPath = needWrl ? (outputDir + QDir::separator() + modelName + QStringLiteral(".wrl")) : QString();
        paths.stepFinalPath =
            needStep ? (outputDir + QDir::separator() + modelName + QStringLiteral(".step")) : QString();
        exportWorker->setOutputPaths({paths.wrlTempPath, paths.stepTempPath});
    }

    QPointer<Model3DExportStage> stagePtr(this);
    qInfo() << "Model3DExportStage: Connecting worker completed signal for" << componentId;
    connect(
        exportWorker,
        &Model3DExportWorker::completed,
        this,
        [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
            qInfo() << "Model3DExportStage: Worker completed signal received for" << componentId
                    << "success:" << success;
            if (!stagePtr) {
                qInfo() << "Model3DExportStage: stagePtr is null!";
                exportWorker->deleteLater();
                return;
            }

            if (success && !stagePtr->m_cancelled.load() && stagePtr->m_componentPaths.contains(componentId)) {
                const TempFilePaths paths = stagePtr->m_componentPaths.value(componentId);
                const bool needWrl = stagePtr->m_options.needsModel3DWrl();
                const bool needStep = stagePtr->m_options.needsModel3DStep();

                QVector<TempFileManager::CommitItem> commitItems;
                if (needWrl) {
                    commitItems.append({paths.wrlTempPath, paths.wrlFinalPath, false});
                }
                if (needStep) {
                    commitItems.append({paths.stepTempPath, paths.stepFinalPath, false});
                }
                if (!stagePtr->m_tempManager.commitBatch(commitItems)) {
                    success = false;
                }
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
