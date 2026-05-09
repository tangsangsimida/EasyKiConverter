#include "SymbolExportStage.h"

#include "DebugExportHelper.h"
#include "KiCadLibraryTableManager.h"
#include "core/kicad/ExporterSymbol.h"
#include "models/ComponentData.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QThread>
#include <QThreadPool>

namespace EasyKiConverter {

SymbolExportStage::SymbolExportStage(QObject* parent)
    : ExportTypeStage("Symbol", 1, parent) {  // maxConcurrent=1 因为是库级别导出
}

SymbolExportStage::~SymbolExportStage() {
    m_cancelled.store(true);
    if (m_workerThread && m_workerThread->isRunning()) {
        m_workerThread->wait();
    }
}

void SymbolExportStage::start(const QStringList& componentIds,
                              const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isExporting.load()) {
        qWarning() << "SymbolExportStage: Export already in progress";
        return;
    }

    if (componentIds.isEmpty()) {
        qWarning() << "SymbolExportStage: No components to export";
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
        m_progress.typeName = QStringLiteral("Symbol");
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

void SymbolExportStage::cancel() {
    if (!m_isExporting.load()) {
        return;
    }

    qDebug() << "SymbolExportStage: Cancelling...";

    // 设置取消标志
    m_cancelled.store(true);

    // 等待工作线程协作式退出（最多等待 5 秒）。不能 terminate()：强杀线程可能中断 Qt/C++ 对象析构或持锁区。
    if (m_workerThread && m_workerThread->isRunning()) {
        qDebug() << "SymbolExportStage: Waiting for worker thread to finish...";
        m_workerThread->quit();
        if (!m_workerThread->wait(5000)) {
            qWarning() << "SymbolExportStage: Thread did not finish in 5s, leaving it to finish asynchronously";
            return;
        }
        m_workerThread = nullptr;
    }

    // 清理临时文件
    m_tempManager.rollbackAll();

    m_isExporting.store(false);
    m_isRunning.store(false);

    qDebug() << "SymbolExportStage: Cancelled";
}

void SymbolExportStage::doLibraryExport(const QStringList& componentIds,
                                        const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    qDebug() << "SymbolExportStage: Starting library export in worker thread for" << componentIds.size()
             << "components";

    // 1. 收集所有有效的符号数据
    QList<SymbolData> symbolList;
    QStringList collectedIds;
    QStringList failedIds;
    int successCount = 0;
    int skippedCount = 0;

    for (const QString& componentId : componentIds) {
        if (m_cancelled.load()) {
            qDebug() << "SymbolExportStage: Export cancelled during data collection";
            break;
        }

        auto it = cachedData.find(componentId);
        if (it == cachedData.end() || !it.value()) {
            qWarning() << "SymbolExportStage: No data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No component data";
            emit itemStatusChanged(componentId, status);
            continue;
        }

        QSharedPointer<ComponentData> data = it.value();

        if (!data->symbolData()) {
            qWarning() << "SymbolExportStage: No symbol data for component:" << componentId;
            failedIds.append(componentId);
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = "No symbol data";
            emit itemStatusChanged(componentId, status);
            continue;
        }

        symbolList.append(*data->symbolData());
        collectedIds.append(componentId);
        successCount++;

        ExportItemStatus status;
        status.status = ExportItemStatus::Status::Success;
        emit itemStatusChanged(componentId, status);

        if (m_options.debugMode) {
            QThreadPool::globalInstance()->start([componentId, data, outputPath = m_options.outputPath]() {
                DebugExportHelper::exportDebugData(componentId, data, outputPath);
            });
        }

        qDebug() << "SymbolExportStage: Collected symbol for" << componentId;
    }

    if (m_cancelled.load()) {
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    if (symbolList.isEmpty()) {
        qWarning() << "SymbolExportStage: No valid symbols to export";
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, componentIds.size(), 0);
        return;
    }

    const auto failCollectedSymbols = [this, &collectedIds, &failedIds, &successCount](const QString& errorMessage) {
        for (const QString& componentId : collectedIds) {
            failedIds.append(componentId);
            ExportItemStatus status;
            status.status = ExportItemStatus::Status::Failed;
            status.errorMessage = errorMessage;
            status.endTime = QDateTime::currentDateTime();
            emit itemStatusChanged(componentId, status);
        }
        successCount = 0;
    };

    const auto abortExport = [&](const QString& errorMessage) {
        qCritical() << "SymbolExportStage:" << errorMessage;
        failCollectedSymbols(errorMessage);
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, failedIds.size(), 0);
    };

    // 2. 构建输出路径
    QString libName = m_options.libName.isEmpty() ? QStringLiteral("EasyKiConverter") : m_options.libName;
    QString fileName = libName + QStringLiteral(".kicad_sym");
    QString outputDir = m_options.outputPath;
    if (outputDir.isEmpty()) {
        outputDir = QDir::currentPath() + QStringLiteral("/export");
    }
    QString finalPath = outputDir + QDir::separator() + fileName;

    // 3. 确保输出目录存在
    QDir dir;
    if (!dir.mkpath(outputDir)) {
        abortExport(QStringLiteral("Failed to create output directory: %1").arg(outputDir));
        return;
    }

    const bool finalFileExists = QFile::exists(finalPath);

    // 4. 创建临时文件并导出
    QString tempPath = m_tempManager.createSymbolTempPath(libName, QStringLiteral(".kicad_sym"));
    if (tempPath.isEmpty()) {
        abortExport(QStringLiteral("Failed to create temp file path"));
        return;
    }

    // 追加/更新到现有符号库时，先把最终文件复制到临时文件，再在临时文件上执行 merge。
    if (finalFileExists && (!m_options.overwriteExistingFiles || m_options.updateMode || m_options.retryMode)) {
        const QString tempDirPath = QFileInfo(tempPath).absolutePath();
        if (!QDir().mkpath(tempDirPath)) {
            abortExport(QStringLiteral("Failed to create temp symbol library directory: %1").arg(tempDirPath));
            return;
        }

        QFile tempFile(tempPath);
        if (tempFile.exists() && !tempFile.remove()) {
            abortExport(QStringLiteral("Failed to remove stale temp symbol library: %1").arg(tempPath));
            return;
        }

        QFile sourceFile(finalPath);
        if (!sourceFile.open(QIODevice::ReadOnly)) {
            if (!QFileInfo::exists(finalPath)) {
                qWarning()
                    << "SymbolExportStage: Existing symbol library disappeared before copy, creating new library:"
                    << finalPath;
            } else {
                abortExport(QStringLiteral("Failed to open existing symbol library for copy: %1").arg(finalPath));
                return;
            }
        } else if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            abortExport(QStringLiteral("Failed to create temp symbol library for copy: %1").arg(tempPath));
            return;
        } else {
            constexpr qint64 kChunkSize = 65536;
            char buf[kChunkSize];
            bool copyError = false;
            while (!sourceFile.atEnd()) {
                qint64 bytesRead = sourceFile.read(buf, kChunkSize);
                if (bytesRead < 0) {
                    abortExport(QStringLiteral("Failed to read existing symbol library: %1").arg(finalPath));
                    copyError = true;
                    break;
                }
                if (tempFile.write(buf, bytesRead) != bytesRead) {
                    abortExport(QStringLiteral("Failed to write temp symbol library copy: %1").arg(tempPath));
                    copyError = true;
                    break;
                }
            }
            if (copyError) {
                return;
            }
            tempFile.close();
        }
    }

    qDebug() << "SymbolExportStage: Exporting" << symbolList.size() << "symbols to temp:" << tempPath;

    // 5. 执行符号库导出
    bool exportSuccess = false;
    QString libraryDescription = m_options.symbolLibraryDescription;
    {
        ExporterSymbol exporter;
        bool appendMode = !m_options.overwriteExistingFiles;
        exportSuccess = exporter.exportSymbolLibrary(
            symbolList, libName, tempPath, appendMode, m_options.updateMode, libraryDescription);
    }

    if (m_cancelled.load()) {
        m_tempManager.rollbackAll();
        m_isExporting.store(false);
        m_isRunning.store(false);
        emit completed(0, symbolList.size(), 0);
        return;
    }

    if (!exportSuccess) {
        abortExport(QStringLiteral("Failed to export symbol library"));
        return;
    }

    // 6. 提交临时文件（重命名为最终路径）
    if (!m_tempManager.commit(finalPath)) {
        abortExport(QStringLiteral("Failed to commit temp file"));
        return;
    }
    qDebug() << "SymbolExportStage: Successfully exported to:" << finalPath;

    // 7. 生成 sym-lib-table 文件（如果提供了库描述）
    if (!libraryDescription.isEmpty()) {
        ExporterSymbol symTableExporter;
        symTableExporter.generateSymLibTable(libName, finalPath, outputDir, libraryDescription);
        KiCadLibraryTableManager::registerSymbolLibrary(outputDir, libName, finalPath, libraryDescription);
    }

    // 8. 清理临时目录
    m_tempManager.cleanupTempDirectory();

    // 9. 完成
    qDebug() << "SymbolExportStage: Completed. Success:" << successCount << "Failed:" << failedIds.size();

    m_isExporting.store(false);
    m_isRunning.store(false);
    emit completed(successCount, failedIds.size(), skippedCount);
}

}  // namespace EasyKiConverter
