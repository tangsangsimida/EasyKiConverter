#include "DatasheetExportStage.h"

#include "DatasheetExportWorker.h"
#include "models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QPointer>

namespace EasyKiConverter {

DatasheetExportStage::DatasheetExportStage(QObject* parent) : ExportTypeStage("Datasheet", 2, parent) {}

DatasheetExportStage::~DatasheetExportStage() {
    cancel();
}

void DatasheetExportStage::start(const QStringList& componentIds,
                                 const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "DatasheetExportStage: Export already in progress";
        return;
    }

    m_threadPool.setMaxThreadCount(m_options.weakNetworkSupport ? 1 : 2);

    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".datasheet");

    QDir dir;
    if (!dir.mkpath(outputDir)) {
        qCritical() << "DatasheetExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        ExportTypeStage::start(QStringList(), cachedData);
        return;
    }

    m_tempManager.setOutputPath(outputDir);

    m_tempPaths.clear();
    m_finalPaths.clear();
    for (const QString& componentId : componentIds) {
        QString format = QStringLiteral("pdf");
        auto it = cachedData.find(componentId);
        if (it != cachedData.end() && it.value()) {
            QString dataFormat = it.value()->datasheetFormat();
            if (!dataFormat.isEmpty()) {
                format = dataFormat;
            }
        }

        QString fileName = componentId + QStringLiteral(".") + format;
        QString finalPath = outputDir + QDir::separator() + fileName;
        QString tempPath = m_tempManager.createTempFilePath(componentId, QStringLiteral(".") + format);

        if (!tempPath.isEmpty()) {
            m_tempPaths[componentId] = tempPath;
            m_finalPaths[componentId] = finalPath;
        }
    }

    m_isExporting.store(true);

    ExportTypeStage::start(componentIds, cachedData);
}

void DatasheetExportStage::cancel() {
    cancelWithTempRollback(m_tempManager);
}

QObject* DatasheetExportStage::createWorker() {
    return new DatasheetExportWorker();
}

void DatasheetExportStage::startWorker(QObject* worker,
                                       const QString& componentId,
                                       const QSharedPointer<ComponentData>& data) {
    auto* exportWorker = qobject_cast<DatasheetExportWorker*>(worker);
    if (!exportWorker) {
        qWarning() << "DatasheetExportStage: Failed to cast worker to DatasheetExportWorker";
        return;
    }

    exportWorker->setOptions(m_options);
    exportWorker->setData(componentId, data, m_options);

    if (m_tempPaths.contains(componentId)) {
        exportWorker->setTempPath(m_tempPaths[componentId]);
    }

    QPointer<DatasheetExportStage> stagePtr(this);
    connect(
        exportWorker,
        &DatasheetExportWorker::completed,
        this,
        [stagePtr, exportWorker, componentId](const QString&, bool success, const QString& error) {
            if (!stagePtr) {
                exportWorker->deleteLater();
                return;
            }

            if (success && stagePtr->m_finalPaths.contains(componentId)) {
                QString tempPath = stagePtr->m_tempPaths.value(componentId);
                QString finalPath = stagePtr->m_finalPaths.value(componentId);
                success = stagePtr->m_tempManager.commitWithBackup(tempPath, finalPath);
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
