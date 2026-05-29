#include "PreviewImagesExportStage.h"

#include "PreviewImagesExportWorker.h"
#include "models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QPointer>

namespace EasyKiConverter {

PreviewImagesExportStage::PreviewImagesExportStage(QObject* parent) : ExportTypeStage("PreviewImages", 4, parent) {}

PreviewImagesExportStage::~PreviewImagesExportStage() {
    cancel();
}

void PreviewImagesExportStage::start(const QStringList& componentIds,
                                     const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "PreviewImagesExportStage: Export already in progress";
        return;
    }

    m_threadPool.setMaxThreadCount(m_options.weakNetworkSupport ? 2 : 4);

    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".preview");

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        qCritical() << "PreviewImagesExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        ExportTypeStage::start(QStringList(), cachedData);
        return;
    }

    m_tempManager.setOutputPath(outputDir);

    m_outputDirs.clear();
    m_componentTempPaths.clear();
    for (const QString& componentId : componentIds) {
        m_outputDirs[componentId] = outputDir;
    }

    m_isExporting.store(true);

    ExportTypeStage::start(componentIds, cachedData);
}

void PreviewImagesExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "PreviewImagesExportStage: Cancelling...";
    m_cancelled.store(true);
    m_tempManager.rollbackAll();
    m_isExporting.store(false);
    qDebug() << "PreviewImagesExportStage: Cancelled";
}

QMap<QString, QString> PreviewImagesExportStage::createTempPathsForComponent(const QString& componentId,
                                                                             int previewCount) {
    QMap<QString, QString> paths;

    if (previewCount <= 0) {
        qWarning() << "PreviewImagesExportStage: No preview images to export for" << componentId;
        return paths;
    }

    QString outputDir = m_outputDirs.value(componentId);
    if (outputDir.isEmpty()) {
        qWarning() << "PreviewImagesExportStage: No output directory for" << componentId;
        return paths;
    }

    QString tempDir = m_tempManager.createTempDirectoryPath(componentId + QStringLiteral(".preview.tmp"));
    if (tempDir.isEmpty()) {
        qWarning() << "PreviewImagesExportStage: Failed to create temp dir path for" << componentId;
        return paths;
    }
    QDir dir;
    if (!dir.mkpath(tempDir)) {
        qWarning() << "PreviewImagesExportStage: Failed to create temp dir for" << componentId << ":" << tempDir;
        return paths;
    }

    for (int i = 1; i <= previewCount; ++i) {
        QString fileName = QStringLiteral("%1_preview_%2.png").arg(componentId).arg(i);
        QString tempPath = tempDir + QDir::separator() + fileName;
        paths[fileName] = tempPath;
    }
    m_componentTempPaths[componentId] = paths;

    if (paths.size() != previewCount) {
        qWarning() << "PreviewImagesExportStage: Created" << paths.size() << "paths but expected" << previewCount;
    }

    return paths;
}

QObject* PreviewImagesExportStage::createWorker() {
    return new PreviewImagesExportWorker();
}

void PreviewImagesExportStage::startWorker(QObject* worker,
                                           const QString& componentId,
                                           const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<PreviewImagesExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "PreviewImagesExportStage: Failed to cast worker to PreviewImagesExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    int previewCount = data->previewImageData().size();
    QMap<QString, QString> tempPaths = createTempPathsForComponent(componentId, previewCount);
    if (!tempPaths.isEmpty()) {
        exportWorker->setTempPaths(tempPaths);
    } else if (previewCount > 0) {
        qWarning() << "PreviewImagesExportStage: Temp path creation failed for" << componentId
                   << "- will use direct write";
    }

    QPointer<PreviewImagesExportStage> stagePtr(this);
    connect(
        exportWorker,
        &PreviewImagesExportWorker::completed,
        this,
        [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
            if (!stagePtr) {
                exportWorker->deleteLater();
                return;
            }

            if (success && !stagePtr->m_cancelled.load()) {
                QString outputDir = stagePtr->m_outputDirs.value(componentId);
                const QMap<QString, QString> tempPaths = stagePtr->m_componentTempPaths.value(componentId);

                QVector<TempFileManager::CommitItem> commitItems;
                for (auto it = tempPaths.constBegin(); it != tempPaths.constEnd(); ++it) {
                    commitItems.append({it.value(), outputDir + QDir::separator() + it.key(), false});
                }

                if (!commitItems.isEmpty()) {
                    success = stagePtr->m_tempManager.commitBatch(commitItems);
                    const QString tempDirPath = QFileInfo(commitItems.first().tempPath).absolutePath();
                    QDir(tempDirPath).removeRecursively();
                }
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
