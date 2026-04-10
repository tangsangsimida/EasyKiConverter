#include "FootprintExportStage.h"

#include "core/kicad/ExporterFootprint.h"
#include "models/ComponentData.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QThread>

namespace EasyKiConverter {

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

    // 调用基类的初始化方法（设置状态）
    ExportTypeStage::start(componentIds, cachedData);

    // 设置临时文件管理器
    m_tempManager.setOutputPath(m_options.outputPath);

    // 在工作线程中执行库级别的导出
    m_isExporting.store(true);
    QThread* thread = QThread::create([this, componentIds, cachedData]() {
        doLibraryExport(componentIds, cachedData);
    });
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);
    thread->start();
}

void FootprintExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "FootprintExportStage: Cancelling...";

    // 设置取消标志
    m_cancelled.store(true);

    // 清理临时文件
    m_tempManager.rollbackAll();

    m_isExporting.store(false);
    m_isRunning.store(false);

    qDebug() << "FootprintExportStage: Cancelled";
}

void FootprintExportStage::doLibraryExport(const QStringList& componentIds,
                                          const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    qDebug() << "FootprintExportStage: Starting library export in worker thread for" << componentIds.size() << "components";

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
            continue;
        }

        QSharedPointer<ComponentData> data = it.value();

        // 检查封装数据
        if (!data->footprintData()) {
            qWarning() << "FootprintExportStage: No footprint data for component:" << componentId;
            failedIds.append(componentId);
            continue;
        }

        // 添加到封装列表
        footprintList.append(*data->footprintData());
        successCount++;

        qDebug() << "FootprintExportStage: Collected footprint for" << componentId;
    }

    if (m_cancelled.load()) {
        m_isExporting.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    if (footprintList.isEmpty()) {
        qWarning() << "FootprintExportStage: No valid footprints to export";
        m_isExporting.store(false);
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
        emit completed(0, footprintList.size(), 0);
        return;
    }

    // 4. 创建临时目录路径并导出
    QString tempDirPath = m_tempManager.createTempDirectoryPath(dirName);
    if (tempDirPath.isEmpty()) {
        qCritical() << "FootprintExportStage: Failed to create temp dir path";
        m_isExporting.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    qDebug() << "FootprintExportStage: Exporting" << footprintList.size() << "footprints to temp:" << tempDirPath;

    // 5. 执行封装库导出
    bool exportSuccess = false;
    {
        ExporterFootprint exporter;
        exportSuccess = exporter.exportFootprintLibrary(footprintList, libName, tempDirPath);
    }

    if (m_cancelled.load()) {
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        emit completed(0, footprintList.size(), 0);
        return;
    }

    if (!exportSuccess) {
        qCritical() << "FootprintExportStage: Failed to export footprint library";
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
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
        emit completed(0, footprintList.size(), 0);
        return;
    }

    // 7. 清理临时目录
    m_tempManager.cleanupTempDirectory();

    // 8. 完成
    qDebug() << "FootprintExportStage: Completed. Success:" << successCount
             << "Failed:" << failedIds.size();

    m_isExporting.store(false);
    m_isRunning.store(false);
    emit completed(successCount, failedIds.size(), skippedCount);
}

}  // namespace EasyKiConverter
