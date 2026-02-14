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
    , m_fetchThreadPool(nullptr)
    , m_processThreadPool(nullptr)
    , m_writeThreadPool(nullptr)
    , m_fetchProcessQueue(nullptr)
    , m_processWriteQueue(nullptr)
    , m_networkAccessManager(new QNetworkAccessManager(this))
    , m_isPipelineRunning(false)
    , m_isCancelled(0)
    , m_mutex(new QMutex())
    , m_successCount(0)
    , m_failureCount(0)
    , m_exportStartTimeMs(0)
    , m_originalExportStartTimeMs(0)
    , m_originalTotalTasks(0)
    , m_isRetryMode(false) {
    qDebug() << "ExportServicePipeline: Initializing thread pools...";
    // 线程池初始化
    m_fetchThreadPool = new QThreadPool(this);
    m_fetchThreadPool->setMaxThreadCount(5);  // I/O 密集型
    m_fetchThreadPool->setExpiryTimeout(60000);

    m_processThreadPool = new QThreadPool(this);
    m_processThreadPool->setMaxThreadCount(QThread::idealThreadCount());  // CPU 密集型

    m_writeThreadPool = new QThreadPool(this);
    m_writeThreadPool->setMaxThreadCount(3);  // 磁盘 I/O 密集型

    // 状态统计初始化
    m_pipelineProgress = PipelineProgress();

    qDebug() << "ExportServicePipeline initialized with thread pools:"
             << "Fetch:" << m_fetchThreadPool->maxThreadCount() << "Process:" << m_processThreadPool->maxThreadCount()
             << "Write:" << m_writeThreadPool->maxThreadCount();
}

ExportServicePipeline::~ExportServicePipeline() {
    cancelExport();  // 确保退出时停止所有请求
    cleanupPipeline();
    delete m_mutex;
}

void ExportServicePipeline::executeExportPipelineWithStages(const QStringList& componentIds,
                                                            const ExportOptions& options,
                                                            bool isRetry) {
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

    // 设置重试模式标志（使用明确的 isRetry 参数，而不是 options.updateMode）
    m_isRetryMode = isRetry;
    qDebug() << "Retry mode set to:" << m_isRetryMode;

    // 只在非重试模式下重置并设置原始总数量和开始时间
    if (!m_isRetryMode) {
        // 重置之前的原始统计信息（新的导出流程）
        m_originalTotalTasks = 0;
        m_originalExportStartTimeMs = 0;
        m_originalStatistics = ExportStatistics();  // 重置原始统计信息

        // 设置新的原始统计信息
        m_originalTotalTasks = componentIds.size();
        m_originalExportStartTimeMs = QDateTime::currentMSecsSinceEpoch();
        qDebug() << "New export: Original export start time and total tasks set to:" << m_originalExportStartTimeMs
                 << m_originalTotalTasks;
    } else {
        qDebug() << "Retry mode: Preserving original export statistics from first export.";
    }

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
    executeExportPipelineWithStages(componentIds, retryOptions, true);  // 明确标识这是重试操作
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
    qDebug() << "Fetch completed for component:" << status->componentId << "Success:" << status->fetchSuccess
             << "fetch3DOnly:" << status->fetch3DOnly;
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
        // 特殊处理 fetch3DOnly 模式：如果 3D 获取失败但符号和封装数据完整，仍然继续处理
        if (status->fetch3DOnly && status->symbolData && status->footprintData) {
            bool hasSymbolData = !status->symbolData->pins().isEmpty();
            bool hasFootprintData = !status->footprintData->info().name.isEmpty();

            if (hasSymbolData && hasFootprintData) {
                // 符号和封装数据完整，3D 模型获取失败，仍然继续处理
                status->addDebugLog(
                    "3D model fetch failed, but symbol/footprint data is complete. Continuing export...");
                status->fetchSuccess = true;  // 标记为成功（符号和封装部分）
                status->fetchMessage = "3D model fetch failed, but symbol/footprint OK";
                status->need3DModel = false;  // 禁用 3D 导出，避免后续写入失败

                if (m_fetchProcessQueue->push(status)) {
                    emit componentExported(status->componentId,
                                           true,
                                           "Fetch completed (3D failed)",
                                           static_cast<int>(PipelineStage::Fetch),
                                           status->symbolWritten,
                                           status->footprintWritten,
                                           false);
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
                                           false);
                }
            } else {
                // 符号或封装数据不完整，完全失败
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
        } else {
            // 非 fetch3DOnly 模式或数据不完整，完全失败
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
        QString finalFilePath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, status->componentId);
        QString tempFilePath = finalFilePath + ".tmp";
        // 优先清理最终文件（.kicad_sym），如果存在临时文件也加入清理列表
        if (QFile::exists(finalFilePath) && !m_tempSymbolFiles.contains(finalFilePath)) {
            m_tempSymbolFiles.append(finalFilePath);
        }
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
        if (m_isCancelled.loadAcquire()) {  // Batch-level cancellation check
            qDebug() << "Fetch stage cancelled early for component:" << componentId;
            QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
            status->componentId = componentId;
            status->fetchSuccess = false;
            status->fetchMessage = "Export cancelled";
            m_completedStatuses.append(status);  // Add to completed statuses for proper count
            m_pipelineProgress.fetchCompleted++;
            m_pipelineProgress.processCompleted++;
            m_pipelineProgress.writeCompleted++;
            m_failureCount++;
            emit componentExported(
                componentId, false, "Export cancelled", static_cast<int>(PipelineStage::Fetch), false, false, false);
            emit pipelineProgressUpdated(m_pipelineProgress);
            checkPipelineCompletion();
            continue;
        }

        // 创建导出状态对象
        QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
        status->componentId = componentId;
        status->need3DModel = m_options.exportModel3D;

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

            // 填充 CAD 数据
            status->symbolData = preloadedData->symbolData();
            status->footprintData = preloadedData->footprintData();
            status->model3DData = preloadedData->model3DData();

            // 标记处理成功 (因为数据已经是处理过的 ComponentData)
            status->fetchSuccess = true;
            status->fetchMessage = "Used preloaded data";
            status->fetchDurationMs = 0;
            status->processSuccess = true;
            status->processMessage = "Preloaded data used";
            status->processDurationMs = 0;

            // 添加到完成状态列表以供统计
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
            }

            continue;
        }

        FetchWorker* worker = new FetchWorker(componentId, m_networkAccessManager, m_options.exportModel3D, false, QString(), nullptr);
        connect(worker,
                &FetchWorker::fetchCompleted,
                this,
                &ExportServicePipeline::handleFetchCompleted,
                Qt::QueuedConnection);
        connect(worker, &FetchWorker::fetchCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);

        {
            QMutexLocker locker(&m_workerMutex);
            m_activeFetchWorkers.insert(worker);
        }

        m_fetchThreadPool->start(worker);
    }
}

void ExportServicePipeline::startProcessStage() {
    for (int i = 0; i < m_processThreadPool->maxThreadCount(); i++) {
        m_processThreadPool->start(QRunnable::create([this]() {
            while (true) {
                if (m_isCancelled.loadAcquire()) {  // Batch-level cancellation check
                    qDebug() << "Process stage worker exiting due to cancellation.";
                    break;
                }
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
                if (m_isCancelled.loadAcquire()) {  // Batch-level cancellation check
                    qDebug() << "Write stage worker exiting due to cancellation.";
                    break;
                }
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

        // 清理符号库合并时的临时文件
        QString libraryTempPath = QString("%1/%2.kicad_sym.tmp").arg(m_options.outputPath, m_options.libName);
        if (QFile::exists(libraryTempPath)) {
            if (!QFile::remove(libraryTempPath)) {
                qWarning() << "Failed to remove temporary symbol library file:" << libraryTempPath;
            }
        }

        // 生成统计信息（始终生成，用于显示基本统计卡片）
        ExportStatistics statistics = generateStatistics();

        // 在非重试模式下保存原始统计信息（用于重试时保留时间数据）
        if (!m_isRetryMode) {
            m_originalStatistics = statistics;
            qDebug() << "Original export statistics saved for potential retries";
        }

        // 只在调试模式下保存详细报告到文件
        QString reportPath;
        if (m_options.debugMode) {
            reportPath = QString("%1/export_report_%2.json")
                             .arg(m_options.outputPath)
                             .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
            if (saveStatisticsReport(statistics, reportPath)) {
                qDebug() << "Statistics report saved to:" << reportPath;
            } else {
                qWarning() << "Failed to save statistics report.";
                reportPath.clear();
            }
        }

        // 始终发送统计信号，使统计卡片显示
        emit statisticsReportGenerated(reportPath, statistics);

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
    if (m_fetchProcessQueue) {
        m_fetchProcessQueue->close();
        delete m_fetchProcessQueue;
        m_fetchProcessQueue = nullptr;
    }
    if (m_processWriteQueue) {
        m_processWriteQueue->close();
        delete m_processWriteQueue;
        m_processWriteQueue = nullptr;
    }
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
        } else {
            // 重命名成功后，验证并清理可能残留的临时文件
            if (QFile::exists(tempPath)) {
                QFile::remove(tempPath);
            }
        }
    } else {
        if (QFile::exists(tempPath))
            QFile::remove(tempPath);
    }
    return success;
}

ExportStatistics ExportServicePipeline::generateStatistics() {
    ExportStatistics statistics;

    // 使用原始总数量（包括重试），如果未设置则使用当前批次数量
    statistics.total = (m_originalTotalTasks > 0) ? m_originalTotalTasks : m_pipelineProgress.totalTasks;

    // 计算成功和失败数量
    // 需要去重统计，因为重试的元器件可能在 m_completedStatuses 中有多个状态
    QSet<QString> uniqueComponentIds;
    int actualSuccessCount = 0;
    int actualFailureCount = 0;

    for (const QSharedPointer<ComponentExportStatus>& status : m_completedStatuses) {
        QString componentId = status->componentId;

        // 只统计每个元器件的最新状态（最后一次导出结果）
        if (uniqueComponentIds.contains(componentId)) {
            continue;  // 跳过重复的元器件
        }
        uniqueComponentIds.insert(componentId);

        // 统计启用的导出选项
        if (m_options.exportSymbol && status->symbolWritten)
            statistics.successSymbol++;
        if (m_options.exportFootprint && status->footprintWritten)
            statistics.successFootprint++;
        if (m_options.exportModel3D && status->model3DWritten)
            statistics.successModel3D++;

        if (status->isCompleteSuccess()) {
            actualSuccessCount++;
        } else {
            actualFailureCount++;
            statistics.stageFailures[status->getFailedStage()]++;
            statistics.failureReasons[status->getFailureReason()]++;
        }
    }

    statistics.success = actualSuccessCount;
    statistics.failed = actualFailureCount;

    // 只在非重试模式下计算时间统计（避免重试时覆盖原始导出的时间信息）
    if (!m_isRetryMode) {
        // 计算总耗时
        qint64 currentTimeMs = QDateTime::currentMSecsSinceEpoch();
        if (m_originalExportStartTimeMs > 0) {
            statistics.totalDurationMs = currentTimeMs - m_originalExportStartTimeMs;
            qDebug() << "Total duration calculated:" << statistics.totalDurationMs << "ms"
                     << "(start:" << m_originalExportStartTimeMs << ", end:" << currentTimeMs << ")";
        } else if (m_exportStartTimeMs > 0) {
            // 如果原始开始时间未设置（向后兼容），使用当前开始时间
            statistics.totalDurationMs = currentTimeMs - m_exportStartTimeMs;
            qDebug() << "Total duration calculated (using current start):" << statistics.totalDurationMs << "ms"
                     << "(start:" << m_exportStartTimeMs << ", end:" << currentTimeMs << ")";
        } else {
            qWarning() << "Neither m_originalExportStartTimeMs nor m_exportStartTimeMs is initialized, setting "
                          "totalDurationMs to 0";
            statistics.totalDurationMs = 0;
        }

        // 计算平均耗时（基于所有完成的元器件状态）
        qint64 totalFetchTime = 0, totalProcessTime = 0, totalWriteTime = 0;
        int completedCount = uniqueComponentIds.size();

        for (const QSharedPointer<ComponentExportStatus>& status : m_completedStatuses) {
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
        if (completedCount > 0) {
            statistics.avgFetchTimeMs = totalFetchTime / completedCount;
            statistics.avgProcessTimeMs = totalProcessTime / completedCount;
            statistics.avgWriteTimeMs = totalWriteTime / completedCount;
        }
    } else {
        // 重试模式：保留原始统计信息的时间数据，但使用最新的基本统计
        if (m_originalStatistics.total > 0) {
            statistics.totalDurationMs = m_originalStatistics.totalDurationMs;
            statistics.avgFetchTimeMs = m_originalStatistics.avgFetchTimeMs;
            statistics.avgProcessTimeMs = m_originalStatistics.avgProcessTimeMs;
            statistics.avgWriteTimeMs = m_originalStatistics.avgWriteTimeMs;
            statistics.totalNetworkRequests = m_originalStatistics.totalNetworkRequests;
            statistics.totalRetries = m_originalStatistics.totalRetries;
            statistics.avgNetworkLatencyMs = m_originalStatistics.avgNetworkLatencyMs;
            statistics.rateLimitHitCount = m_originalStatistics.rateLimitHitCount;
            statistics.statusCodeDistribution = m_originalStatistics.statusCodeDistribution;
            qDebug() << "Retry mode: Preserving original timing statistics";
        } else {
            qDebug() << "Retry mode: No original statistics available, using default values";
        }
    }

    qDebug() << "Statistics generated - Total:" << statistics.total << "Success:" << statistics.success
             << "Failed:" << statistics.failed << "Unique components:" << uniqueComponentIds.size()
             << "Retry mode:" << m_isRetryMode;

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

    qDebug() << "Cancelling ExportPipeline...";

    // 1. 同步中断所有正在运行的抓取任务 (确保立即起效)
    {
        QMutexLocker locker(&m_workerMutex);
        qDebug() << "Aborting" << m_activeFetchWorkers.size() << "active fetch workers immediately.";
        for (FetchWorker* worker : m_activeFetchWorkers) {
            if (worker) {
                worker->abort();
            }
        }
    }

    // 2. 异步清理剩余资源，防止阻塞 UI
    QThreadPool::globalInstance()->start(QRunnable::create([this]() {
        // 使用 QPointer 检查 this 是否还存活？
        // 其实在 QThreadPool 里的 Lambda 访问 this 比较危险，
        // 但由于我们使用了 QThreadPool::globalInstance() 且 lambda 内部逻辑较快，
        // 且上面的同步 abort 已经解决了大部分请求残留问题。

        if (m_fetchThreadPool) {
            m_fetchThreadPool->clear();
        }
        if (m_processThreadPool) {
            m_processThreadPool->clear();
        }
        if (m_writeThreadPool) {
            m_writeThreadPool->clear();
        }

        if (m_fetchProcessQueue) {
            m_fetchProcessQueue->close();
        }
        if (m_processWriteQueue) {
            m_processWriteQueue->close();
        }

        qDebug() << "ExportPipeline cancelled and queues closed.";
        // 触发完成检查，这将最终发出 exportCompleted 信号
        checkPipelineCompletion();
    }));

    // 移除 10 秒超时保护，因为异步清理应该足够快且 Worker 内部会响应取消
    // QTimer::singleShot(10000, this, [this]() { ... });
}

}  // namespace EasyKiConverter
