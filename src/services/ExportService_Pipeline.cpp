#include "ExportService_Pipeline.h"

#include "workers/FetchWorker.h"
#include "workers/ProcessWorker.h"
#include "workers/WriteWorker.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QTimer>

#include <algorithm>

namespace EasyKiConverter {

ExportServicePipeline::ExportServicePipeline(QObject* parent)
    : ExportService(parent)
    , m_fetchThreadPool(new QThreadPool(this))
    , m_processThreadPool(new QThreadPool(this))
    , m_writeThreadPool(new QThreadPool(this))
    , m_fetchProcessQueue(new BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>(100))
    , m_processWriteQueue(new BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>(100))
    , m_networkAccessManager(new QNetworkAccessManager(this))
    , m_isPipelineRunning(false)
    , m_isCancelled(0)
    , m_mutex(new QMutex())
    , m_successCount(0)
    , m_failureCount(0)
    , m_exportStartTimeMs(0) {
    // 配置线程池
    m_fetchThreadPool->setMaxThreadCount(5);
    m_processThreadPool->setMaxThreadCount(QThread::idealThreadCount());
    m_writeThreadPool->setMaxThreadCount(3);

    qDebug() << "ExportServicePipeline initialized with thread pools:"
             << "Fetch:" << m_fetchThreadPool->maxThreadCount() << "Process:" << m_processThreadPool->maxThreadCount()
             << "Write:" << m_writeThreadPool->maxThreadCount();
}

ExportServicePipeline::~ExportServicePipeline() {
    cleanupPipeline();
}

void ExportServicePipeline::executeExportPipelineWithStages(const QStringList& componentIds,
                                                            const ExportOptions& options) {
    qDebug() << "Starting pipeline export for" << componentIds.size() << "components";

    QMutexLocker locker(m_mutex);

    if (m_isPipelineRunning) {
        qWarning() << "Pipeline is already running";
        return;
    }

    if (componentIds.isEmpty()) {
        qDebug() << "Component list is empty, nothing to do.";
        emit exportProgress(0, 0);
        emit exportCompleted(0, 0);
        return;
    }

    const size_t FIXED_QUEUE_SIZE = 64;
    delete m_fetchProcessQueue;
    delete m_processWriteQueue;
    m_fetchProcessQueue = new BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>(FIXED_QUEUE_SIZE);
    m_processWriteQueue = new BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>(FIXED_QUEUE_SIZE);

    m_componentIds = componentIds;
    m_options = options;
    m_pipelineProgress.totalTasks = componentIds.size();
    m_pipelineProgress.fetchCompleted = 0;
    m_pipelineProgress.processCompleted = 0;
    m_pipelineProgress.writeCompleted = 0;

    if (!options.updateMode) {
        m_successCount = 0;
        m_failureCount = 0;
        m_tempSymbolFiles.clear();
        m_symbols.clear();
        m_completedStatuses.clear();
    }

    // 确保 m_exportStartTimeMs 被正确初始化 (v3.0.5+ 修复)
    m_exportStartTimeMs = QDateTime::currentMSecsSinceEpoch();
    qDebug() << "Export started at:" << m_exportStartTimeMs;

    m_isPipelineRunning = true;
    m_isCancelled.storeRelease(0);
    m_isExporting.storeRelease(1);
    m_isStopping.storeRelease(0);

    QDir dir;
    if (!dir.exists(m_options.outputPath)) {
        dir.mkpath(m_options.outputPath);
    }

    locker.unlock();
    emit exportProgress(0, m_pipelineProgress.totalTasks);
    startFetchStage();
    startProcessStage();
    startWriteStage();
    qDebug() << "Pipeline export started";
}

void ExportServicePipeline::retryExport(const QStringList& componentIds, const ExportOptions& options) {
    ExportOptions retryOptions = options;
    retryOptions.overwriteExistingFiles = false;
    retryOptions.updateMode = true;
    executeExportPipelineWithStages(componentIds, retryOptions);
}

PipelineProgress ExportServicePipeline::getPipelineProgress() const {
    QMutexLocker locker(m_mutex);
    return m_pipelineProgress;
}

void ExportServicePipeline::setPreloadedData(const QMap<QString, QSharedPointer<ComponentData>>& data) {
    QMutexLocker locker(m_mutex);
    m_preloadedData = data;
}

void ExportServicePipeline::handleFetchCompleted(QSharedPointer<ComponentExportStatus> status) {
    if (FetchWorker* worker = qobject_cast<FetchWorker*>(sender())) {
        QMutexLocker locker(&m_workerMutex);
        m_activeFetchWorkers.remove(worker);
    }

    QMutexLocker locker(m_mutex);
    qDebug() << "Fetch completed for component:" << status->componentId << "Success:" << status->fetchSuccess;
    m_pipelineProgress.fetchCompleted++;
    m_completedStatuses.append(status);

    if (status->fetchSuccess) {
        if (m_fetchProcessQueue->push(status)) {
            emit componentExported(status->componentId,
                                   true,
                                   "Fetch completed",
                                   static_cast<int>(PipelineStage::Fetch),
                                   status->symbolWritten,
                                   status->footprintWritten,
                                   status->model3DWritten);
        } else {
            m_failureCount++;
            status->processSuccess = false;
            status->processMessage = "Export cancelled";
            emit componentExported(status->componentId,
                                   false,
                                   "Export cancelled",
                                   static_cast<int>(PipelineStage::Fetch),
                                   status->symbolWritten,
                                   status->footprintWritten,
                                   status->model3DWritten);
        }
    } else {
        m_failureCount++;
        m_pipelineProgress.processCompleted++;
        m_pipelineProgress.writeCompleted++;
        emit componentExported(status->componentId,
                               false,
                               status->fetchMessage,
                               static_cast<int>(PipelineStage::Fetch),
                               status->symbolWritten,
                               status->footprintWritten,
                               status->model3DWritten);
    }

    emit pipelineProgressUpdated(m_pipelineProgress);
    checkPipelineCompletion();
}

void ExportServicePipeline::handleProcessCompleted(QSharedPointer<ComponentExportStatus> status) {
    QMutexLocker locker(m_mutex);
    qDebug() << "Process completed for component:" << status->componentId << "Success:" << status->processSuccess;
    m_pipelineProgress.processCompleted++;

    if (status->processSuccess) {
        if (m_processWriteQueue->push(status)) {
            emit componentExported(status->componentId,
                                   true,
                                   "Process completed",
                                   static_cast<int>(PipelineStage::Process),
                                   status->symbolWritten,
                                   status->footprintWritten,
                                   status->model3DWritten);
        } else {
            m_failureCount++;
            m_pipelineProgress.writeCompleted++;
            status->writeSuccess = false;
            status->writeMessage = "Export cancelled";
            emit componentExported(status->componentId,
                                   false,
                                   "Export cancelled",
                                   static_cast<int>(PipelineStage::Process),
                                   status->symbolWritten,
                                   status->footprintWritten,
                                   status->model3DWritten);
        }
    } else {
        m_failureCount++;
        m_pipelineProgress.writeCompleted++;
        emit componentExported(status->componentId,
                               false,
                               status->processMessage,
                               static_cast<int>(PipelineStage::Process),
                               status->symbolWritten,
                               status->footprintWritten,
                               status->model3DWritten);
    }

    emit pipelineProgressUpdated(m_pipelineProgress);
    checkPipelineCompletion();
}

void ExportServicePipeline::handleWriteCompleted(QSharedPointer<ComponentExportStatus> status) {
    QMutexLocker locker(m_mutex);
    qDebug() << "Write completed for component:" << status->componentId << "Success:" << status->writeSuccess;
    m_pipelineProgress.writeCompleted++;

    // 无论整体是否成功，只要符号文件被写入了，就加入清理列表
    if (m_options.exportSymbol && status->symbolWritten) {
        QString tempFilePath = QString("%1/%2.kicad_sym.tmp").arg(m_options.outputPath, status->componentId);
        if (QFile::exists(tempFilePath) && !m_tempSymbolFiles.contains(tempFilePath)) {
            m_tempSymbolFiles.append(tempFilePath);
        }
    }

    if (status->writeSuccess && status->fetchSuccess && status->processSuccess) {
        m_successCount++;
        if (m_options.exportSymbol && status->symbolData && !status->symbolData->info().name.isEmpty()) {
            m_symbols.append(*status->symbolData);
        }
        emit componentExported(status->componentId,
                               true,
                               "Export completed successfully",
                               static_cast<int>(PipelineStage::Write),
                               status->symbolWritten,
                               status->footprintWritten,
                               status->model3DWritten);
    } else {
        m_failureCount++;
        emit componentExported(status->componentId,
                               false,
                               status->writeMessage,
                               static_cast<int>(PipelineStage::Write),
                               status->symbolWritten,
                               status->footprintWritten,
                               status->model3DWritten);
    }

    emit pipelineProgressUpdated(m_pipelineProgress);
    checkPipelineCompletion();
}

void ExportServicePipeline::startFetchStage() {
    for (const QString& componentId : m_componentIds) {
        // 检查是否有预加载数据
        bool canUsePreloaded = false;
        QSharedPointer<ComponentData> preloadedData;

        if (m_preloadedData.contains(componentId)) {
            preloadedData = m_preloadedData.value(componentId);
            if (preloadedData && preloadedData->isValid()) {
                // 如果需要导出3D模型，必须检查预加载数据中是否有有效的3D模型信息
                // 如果预加载数据没有3D信息（例如列表验证时没有勾选获取3D），则不能使用预加载数据，必须重新获取
                if (m_options.exportModel3D) {
                    if (preloadedData->model3DData() && !preloadedData->model3DData()->uuid().isEmpty()) {
                        canUsePreloaded = true;
                    } else {
                        qDebug() << "Preloaded data for" << componentId << "misses 3D model data, forcing fetch.";
                        canUsePreloaded = false;
                    }
                } else {
                    canUsePreloaded = true;
                }
            }
        }

        if (canUsePreloaded) {
            qDebug() << "Using preloaded data for component:" << componentId;

            QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
            status->componentId = componentId;
            status->need3DModel = m_options.exportModel3D;

            // 填充数据
            // 注意：这里我们需要把 ComponentData 转换回原始 JSON 或者直接填充 Status 对象
            // ProcessWorker 主要是从 status->cadDataRaw 解析，或者我们可以修改 ProcessWorker 接受 ComponentData

            // 现在的架构是 FetchWorker -> raw JSON -> ProcessWorker -> ComponentData -> WriteWorker
            // 如果我们已经有 ComponentData，我们应该跳过 Fetch 和 Process 阶段吗？
            // 或者我们可以把 ComponentData 序列化回 JSON (有点多余)，
            // 或者修改 Status 对象以支持直接持有 ComponentData，并让 ProcessWorker 识别它。

            // 既然 ComponentExportStatus 已经有 symbolData, footprintData 等成员，
            // 我们可以直接填充这些，并标记 fetchSuccess 和 processSuccess 为 true。

            status->fetchSuccess = true;
            status->fetchMessage = "Used preloaded data";
            status->fetchDurationMs = 0;

            // 填充 CAD 数据
            status->symbolData = preloadedData->symbolData();
            status->footprintData = preloadedData->footprintData();
            status->model3DData = preloadedData->model3DData();

            // 标记处理成功 (因为数据已经是处理过的 ComponentData)
            status->processSuccess = true;
            status->processMessage = "Preloaded data used";
            status->processDurationMs = 0;

            // 添加到完成状态列表以供统计 (Critical Fix)
            {
                QMutexLocker locker(m_mutex);
                m_completedStatuses.append(status);
            }

            // 直接推送到写入队列
            m_pipelineProgress.fetchCompleted++;
            m_pipelineProgress.processCompleted++;

            // 发送信号更新 UI
            emit componentExported(
                componentId, true, "Used preloaded data", static_cast<int>(PipelineStage::Fetch), false, false, false);
            emit componentExported(componentId,
                                   true,
                                   "Used preloaded data",
                                   static_cast<int>(PipelineStage::Process),
                                   false,
                                   false,
                                   false);
            emit pipelineProgressUpdated(m_pipelineProgress);

            if (m_processWriteQueue->push(status)) {
                // 成功推送到写入队列
            } else {
                m_failureCount++;
                // Handle failure
            }

            continue;
        }

        FetchWorker* worker = new FetchWorker(componentId, m_networkAccessManager, m_options.exportModel3D, nullptr);
        {
            QMutexLocker locker(&m_workerMutex);
            m_activeFetchWorkers.insert(worker);
        }
        connect(worker,
                &FetchWorker::fetchCompleted,
                this,
                &ExportServicePipeline::handleFetchCompleted,
                Qt::QueuedConnection);
        connect(worker, &FetchWorker::fetchCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);
        m_fetchThreadPool->start(worker);
    }
}

void ExportServicePipeline::startProcessStage() {
    for (int i = 0; i < m_processThreadPool->maxThreadCount(); i++) {
        m_processThreadPool->start(QRunnable::create([this]() {
            while (true) {
                QSharedPointer<ComponentExportStatus> status;
                if (!m_fetchProcessQueue->pop(status, 1000)) {
                    if (m_fetchProcessQueue->isClosed())
                        break;
                    continue;
                }
                ProcessWorker* worker = new ProcessWorker(status, nullptr);
                connect(worker,
                        &ProcessWorker::processCompleted,
                        this,
                        &ExportServicePipeline::handleProcessCompleted,
                        Qt::QueuedConnection);
                connect(worker, &ProcessWorker::processCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);
                worker->run();
            }
        }));
    }
}

void ExportServicePipeline::startWriteStage() {
    for (int i = 0; i < m_writeThreadPool->maxThreadCount(); i++) {
        m_writeThreadPool->start(QRunnable::create([this]() {
            while (true) {
                QSharedPointer<ComponentExportStatus> status;
                if (!m_processWriteQueue->pop(status, 1000)) {
                    if (m_processWriteQueue->isClosed())
                        break;
                    continue;
                }
                WriteWorker* worker = new WriteWorker(status,
                                                      m_options.outputPath,
                                                      m_options.libName,
                                                      m_options.exportSymbol,
                                                      m_options.exportFootprint,
                                                      m_options.exportModel3D,
                                                      m_options.debugMode,
                                                      nullptr);
                connect(worker,
                        &WriteWorker::writeCompleted,
                        this,
                        &ExportServicePipeline::handleWriteCompleted,
                        Qt::QueuedConnection);
                connect(worker, &WriteWorker::writeCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);
                worker->run();
            }
        }));
    }
}

void ExportServicePipeline::checkPipelineCompletion() {
    // 抓取阶段完成检查
    bool fetchDone = m_pipelineProgress.fetchCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire() && m_fetchProcessQueue->isEmpty())
        fetchDone = true;
    if (fetchDone && !m_fetchProcessQueue->isClosed())
        m_fetchProcessQueue->close();
    if (!fetchDone)
        return;

    // 处理阶段完成检查
    bool processDone = m_pipelineProgress.processCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire() && m_fetchProcessQueue->isEmpty())
        processDone = true;
    if (processDone && !m_processWriteQueue->isClosed())
        m_processWriteQueue->close();
    if (!processDone)
        return;

    // 写入阶段完成检查
    bool writeDone = m_pipelineProgress.writeCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire() && m_processWriteQueue->isEmpty())
        writeDone = true;
    if (!writeDone)
        return;

    // 所有阶段都完成，开始清理和统计
    qDebug() << "Pipeline completed. Success:" << m_successCount << "Failed:" << m_failureCount;

    // 将合并符号库、生成统计报告和清理临时文件的耗时操作放到异步线程中执行
    QThreadPool::globalInstance()->start(QRunnable::create([this]() {
        qDebug() << "Starting asynchronous cleanup and statistics generation...";
        bool mergeSuccess = true;
        if (m_options.exportSymbol && !m_symbols.isEmpty()) {
            mergeSuccess = mergeSymbolLibrary();
            if (!mergeSuccess) {
                qWarning() << "Failed to merge symbol library during cleanup.";
            }
        }

        // 临时文件清理
        for (const QString& tempFile : m_tempSymbolFiles) {
            if (QFile::exists(tempFile)) {
                if (!QFile::remove(tempFile)) {
                    qWarning() << "Failed to remove temporary symbol file:" << tempFile;
                }
            }
        }
        m_tempSymbolFiles.clear();

        // 生成和保存统计报告
        ExportStatistics statistics = generateStatistics();
        QString reportPath = QString("%1/export_report_%2.json")
                                 .arg(m_options.outputPath)
                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        if (saveStatisticsReport(statistics, reportPath)) {
            emit statisticsReportGenerated(reportPath, statistics);
        } else {
            qWarning() << "Failed to save statistics report.";
        }

        // 所有清理和统计完成后，才真正标记导出完成
        emit exportCompleted(m_pipelineProgress.totalTasks, m_successCount);
        m_isExporting.storeRelease(0);
        m_isStopping.storeRelease(0);
        m_isCancelled.storeRelease(0);

        QTimer::singleShot(0, this, [this]() { cleanupPipeline(); });
        qDebug() << "Asynchronous cleanup and statistics generation completed.";
    }));
}

void ExportServicePipeline::cleanupPipeline() {
    QMutexLocker locker(m_mutex);
    if (!m_isPipelineRunning)
        return;
    m_isPipelineRunning = false;
    if (m_fetchProcessQueue)
        m_fetchProcessQueue->close();
    if (m_processWriteQueue)
        m_processWriteQueue->close();
}

bool ExportServicePipeline::mergeSymbolLibrary() {
    QString libraryPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, m_options.libName);
    QString tempPath = libraryPath + ".tmp";
    bool appendMode = !m_options.overwriteExistingFiles;
    bool updateMode = m_options.updateMode;

    if ((appendMode || updateMode) && QFile::exists(libraryPath)) {
        if (QFile::exists(tempPath))
            QFile::remove(tempPath);
        if (!QFile::copy(libraryPath, tempPath))
            return false;
    }

    ExporterSymbol exporter;
    bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, tempPath, appendMode, updateMode);
    if (success) {
        if (QFile::exists(libraryPath))
            QFile::remove(libraryPath);
        if (!QFile::rename(tempPath, libraryPath)) {
            QFile::remove(tempPath);
            success = false;
        }
    } else {
        if (QFile::exists(tempPath))
            QFile::remove(tempPath);
    }
    return success;
}

ExportStatistics ExportServicePipeline::generateStatistics() {
    ExportStatistics statistics;
    statistics.total = m_pipelineProgress.totalTasks;
    statistics.success = m_successCount;
    statistics.failed = m_failureCount;

    // 计算总耗时，添加边界检查 (v3.0.5+ 修复)
    qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
    if (m_exportStartTimeMs > 0) {
        statistics.totalDurationMs = currentTimeMs - m_exportStartTimeMs;
        qDebug() << "Total duration calculated:" << statistics.totalDurationMs << "ms"
                 << "(start:" << m_exportStartTimeMs << ", end:" << currentTimeMs << ")";
    } else {
        qWarning() << "m_exportStartTimeMs is not initialized, setting totalDurationMs to 0";
        statistics.totalDurationMs = 0;
    }

    qint64 totalFetchTime = 0, totalProcessTime = 0, totalWriteTime = 0;
    for (const QSharedPointer<ComponentExportStatus>& status : m_completedStatuses) {
        // 只统计启用的导出选项
        if (m_options.exportSymbol && status->symbolWritten)
            statistics.successSymbol++;
        if (m_options.exportFootprint && status->footprintWritten)
            statistics.successFootprint++;
        if (m_options.exportModel3D && status->model3DWritten)
            statistics.successModel3D++;

        if (!status->isCompleteSuccess()) {
            statistics.stageFailures[status->getFailedStage()]++;
            statistics.failureReasons[status->getFailureReason()]++;
        }
        totalFetchTime += status->fetchDurationMs;
        totalProcessTime += status->processDurationMs;
        totalWriteTime += status->writeDurationMs;
        for (const auto& diag : status->networkDiagnostics) {
            statistics.totalNetworkRequests++;
            statistics.totalRetries += diag.retryCount;
            statistics.avgNetworkLatencyMs += diag.latencyMs;
            if (diag.wasRateLimited)
                statistics.rateLimitHitCount++;
            statistics.statusCodeDistribution[diag.statusCode]++;
        }
    }
    if (statistics.totalNetworkRequests > 0)
        statistics.avgNetworkLatencyMs /= statistics.totalNetworkRequests;
    if (statistics.total > 0) {
        statistics.avgFetchTimeMs = totalFetchTime / statistics.total;
        statistics.avgProcessTimeMs = totalProcessTime / statistics.total;
        statistics.avgWriteTimeMs = totalWriteTime / statistics.total;
    }
    return statistics;
}

bool ExportServicePipeline::saveStatisticsReport(const ExportStatistics& statistics, const QString& reportPath) {
    QJsonObject reportObj, overview, timing, failures, stageFailures, failureReasons, options;
    overview["total"] = statistics.total;
    overview["success"] = statistics.success;
    overview["failed"] = statistics.failed;
    overview["successSymbol"] = statistics.successSymbol;
    overview["successFootprint"] = statistics.successFootprint;
    overview["successModel3D"] = statistics.successModel3D;
    overview["successRate"] = QString::number(statistics.getSuccessRate(), 'f', 2) + "%";
    overview["totalDurationMs"] = statistics.totalDurationMs;
    reportObj["overview"] = overview;
    timing["avgFetchTimeMs"] = statistics.avgFetchTimeMs;
    timing["avgProcessTimeMs"] = statistics.avgProcessTimeMs;
    timing["avgWriteTimeMs"] = statistics.avgWriteTimeMs;
    reportObj["timing"] = timing;
    for (auto it = statistics.stageFailures.constBegin(); it != statistics.stageFailures.constEnd(); ++it)
        stageFailures[it.key()] = it.value();
    failures["stageFailures"] = stageFailures;
    for (auto it = statistics.failureReasons.constBegin(); it != statistics.failureReasons.constEnd(); ++it)
        failureReasons[it.key()] = it.value();
    failures["failureReasons"] = failureReasons;
    reportObj["failures"] = failures;
    options["outputPath"] = m_options.outputPath;
    options["libName"] = m_options.libName;
    reportObj["exportOptions"] = options;
    reportObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonDocument doc(reportObj);
    QFile file(reportPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

void ExportServicePipeline::cancelExport() {
    if (m_isCancelled.loadAcquire())
        return;
    m_isCancelled.storeRelease(1);
    ExportService::cancelExport();

    qDebug() << "Starting pipeline cancellation...";

    // 异步执行清理操作，确保不阻塞 UI 线程
    QThreadPool::globalInstance()->start(QRunnable::create([this]() {
        qDebug() << "Executing cancellation cleanup (async)...";

        // 清空线程池队列中尚未开始的任务
        if (m_fetchThreadPool) {
            m_fetchThreadPool->clear();
            qDebug() << "Cleared FetchWorker thread pool queue.";
        }
        if (m_processThreadPool) {
            m_processThreadPool->clear();
            qDebug() << "Cleared ProcessWorker thread pool queue.";
        }
        if (m_writeThreadPool) {
            m_writeThreadPool->clear();
            qDebug() << "Cleared WriteWorker thread pool queue.";
        }

        // 关闭队列（唤醒所有等待的线程，使它们退出循环）
        if (m_fetchProcessQueue) {
            m_fetchProcessQueue->close();
            qDebug() << "Closed FetchProcessQueue.";
        }
        if (m_processWriteQueue) {
            m_processWriteQueue->close();
            qDebug() << "Closed ProcessWriteQueue.";
        }

        qDebug() << "Cancellation cleanup completed.";

        // 触发完成检查，这将最终发出 exportCompleted 信号
        checkPipelineCompletion();
    }));

    // 移除 10 秒超时保护，因为异步清理应该足够快且 Worker 内部会响应取消
    // QTimer::singleShot(10000, this, [this]() { ... });
}

}  // namespace EasyKiConverter
