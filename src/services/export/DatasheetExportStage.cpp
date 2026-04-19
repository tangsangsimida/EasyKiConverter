#include "DatasheetExportStage.h"

#include "DatasheetExportWorker.h"
#include "models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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

    // 构建输出目录：outputPath/libName.datasheet/
    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString baseOutputDir = m_options.outputPath;
    if (baseOutputDir.isEmpty()) {
        baseOutputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString outputDir = baseOutputDir + QDir::separator() + libName + QStringLiteral(".datasheet");

    // 确保输出目录存在
    QDir dir;
    if (!dir.mkpath(outputDir)) {
        qCritical() << "DatasheetExportStage: Failed to create output directory:" << outputDir;
        m_isExporting.store(false);
        // 不直接发送completed，让基类处理
        ExportTypeStage::start(QStringList(), cachedData);
        return;
    }

    // 设置临时文件管理器
    m_tempManager.setOutputPath(outputDir);

    // 为每个组件预创建临时文件路径
    m_tempPaths.clear();
    m_finalPaths.clear();
    for (const QString& componentId : componentIds) {
        // 获取数据手册格式
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

    // 调用基类的初始化方法（设置状态，并处理空列表情况）
    ExportTypeStage::start(componentIds, cachedData);

    m_isExporting.store(true);
}

void DatasheetExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "DatasheetExportStage: Cancelling...";

    // 设置取消标志
    m_cancelled.store(true);

    // 回滚所有临时文件
    m_tempManager.rollbackAll();

    m_isExporting.store(false);
    m_isRunning.store(false);

    qDebug() << "DatasheetExportStage: Cancelled";
}

void DatasheetExportStage::commitTempFile(const QString& tempPath, const QString& finalPath) {
    if (tempPath.isEmpty() || finalPath.isEmpty()) {
        return;
    }

    QFileInfo finalInfo(finalPath);
    if (!finalInfo.absoluteDir().exists() && !QDir().mkpath(finalInfo.absolutePath())) {
        qWarning() << "DatasheetExportStage: Failed to create output dir for:" << finalPath;
        return;
    }

    if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
        qWarning() << "DatasheetExportStage: Failed to remove existing file:" << finalPath;
        return;
    }

    if (QFile::rename(tempPath, finalPath)) {
        qDebug() << "DatasheetExportStage: Committed temp file:" << finalPath;
    } else {
        qWarning() << "DatasheetExportStage: Failed to commit temp file:" << tempPath;
    }
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

    // 设置临时文件路径
    if (m_tempPaths.contains(componentId)) {
        exportWorker->setTempPath(m_tempPaths[componentId]);
    }

    // 使用QPointer防止stage已销毁时lambda访问悬空指针
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

            // 如果成功，提交临时文件
            if (success && stagePtr->m_finalPaths.contains(componentId)) {
                QString tempPath = stagePtr->m_tempPaths.value(componentId);
                QString finalPath = stagePtr->m_finalPaths.value(componentId);
                stagePtr->commitTempFile(tempPath, finalPath);
            }

            stagePtr->completeItemProgress(exportWorker, componentId, success, error);
            exportWorker->deleteLater();
        },
        Qt::QueuedConnection);

    m_threadPool.start(exportWorker);
}

}  // namespace EasyKiConverter
