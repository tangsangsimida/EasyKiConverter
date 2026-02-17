#include "ExportService_Pipeline.h"

#include "pipeline/cleanup/PipelineCleanup.h"
#include "pipeline/stats/PipelineStatistics.h"
#include "utils/PathSecurity.h"
#include "workers/FetchWorker.h"
#include "workers/ProcessWorker.h"
#include "workers/WriteWorker.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTextStream>
#include <QTimer>

#include <algorithm>

namespace EasyKiConverter {

ExportServicePipeline::ExportServicePipeline(QObject* parent)
    : ExportService(parent)
    , m_fetchThreadPool(nullptr)
    , m_processThreadPool(nullptr)
    , m_writeThreadPool(nullptr)
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

    // 初始化处理器
    m_fetchHandler = new FetchStageHandler(m_isCancelled, m_networkAccessManager, m_fetchThreadPool, this);
    m_processHandler = new ProcessStageHandler(m_isCancelled, m_processThreadPool, nullptr, this);
    m_writeHandler = new WriteStageHandler(m_isCancelled, m_writeThreadPool, nullptr, m_options, QString(), this);

    // 绑定处理器信号
    connect(m_fetchHandler,
            &FetchStageHandler::componentFetchCompleted,
            this,
            [this](QSharedPointer<ComponentExportStatus> s, bool p) {
                Q_UNUSED(p);
                handleFetchCompleted(s);
            });
    connect(m_processHandler,
            &ProcessStageHandler::componentProcessCompleted,
            this,
            [this](QSharedPointer<ComponentExportStatus> s) { handleProcessCompleted(s); });
    connect(m_writeHandler,
            &WriteStageHandler::componentWriteCompleted,
            this,
            [this](QSharedPointer<ComponentExportStatus> s) { handleWriteCompleted(s); });

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
    m_queueManager.reset();

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
        m_symbols.clear();
        m_completedStatuses.clear();
    }

    // 确保 m_exportStartTimeMs 被正确初始化 (v3.0.5+ 修复)
    m_exportStartTimeMs = QDateTime::currentMSecsSinceEpoch();
    qDebug() << "Export started at:" << m_exportStartTimeMs;

    m_isPipelineRunning = true;
    m_isCancelled.storeRelease(0);          // 重置取消标志
    m_completionScheduled.storeRelease(0);  // 重置完成安排标志
    m_isExporting.storeRelease(1);
    m_isStopping.storeRelease(0);
    m_completionScheduled.storeRelease(0);  // 重置完成处理标志

    QDir dir;
    // 1. 确保输出目录存在
    if (!dir.exists(m_options.outputPath)) {
        if (!dir.mkpath(m_options.outputPath)) {
            qCritical() << "FATAL: Failed to create output directory:" << m_options.outputPath;
            m_isPipelineRunning = false;
            locker.unlock();
            emit exportFailed("Failed to create output directory");
            return;
        }
        qDebug() << "Created output directory:" << m_options.outputPath;
    }

    // 2. 创建临时目录
    m_tempDir = QString("%1/temp").arg(m_options.outputPath);
    if (!dir.exists(m_tempDir)) {
        if (!dir.mkpath(m_tempDir)) {
            qCritical() << "FATAL: Failed to create temp directory:" << m_tempDir;
            m_isPipelineRunning = false;
            locker.unlock();
            emit exportFailed("Failed to create temp directory");
            return;
        }
    }

    // 3. 验证临时目录是否可写
    QFileInfo tempInfo(m_tempDir);
    if (!tempInfo.isWritable()) {
        qCritical() << "FATAL: Temp directory is not writable:" << m_tempDir;
        m_isPipelineRunning = false;
        locker.unlock();
        emit exportFailed("Temp directory is not writable");
        return;
    }

    // 更新处理器依赖
    m_processHandler->setInputQueue(m_queueManager.fetchProcessQueue());
    m_writeHandler->setInputQueue(m_queueManager.processWriteQueue());
    m_writeHandler->setOptions(m_options);
    m_writeHandler->setTempDir(m_tempDir);

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
    QMutexLocker locker(m_mutex);
    if (!m_isPipelineRunning)
        return;

    qDebug() << "Fetch completed for component:" << status->componentId << "Success:" << status->fetchSuccess
             << "fetch3DOnly:" << status->fetch3DOnly;
    m_pipelineProgress.fetchCompleted++;
    m_completedStatuses.append(status);

    // 安全检查队列
    auto fetchProcessQueue = m_queueManager.fetchProcessQueue();
    if (!fetchProcessQueue)
        return;

    if (status->fetchSuccess) {
        updateMemoryPeak(status);

        // 使用 safePush 替代直接 push
        if (PipelineQueueManager::safePush(fetchProcessQueue, status, m_isCancelled)) {
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
            status->processMessage = "Export cancelled or Queue Timeout";
            emit componentExported(status->componentId,
                                   false,
                                   "Export cancelled or Queue Timeout",
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

                updateMemoryPeak(status);

                if (PipelineQueueManager::safePush(fetchProcessQueue, status, m_isCancelled)) {
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
                    status->processMessage = "Export cancelled or Queue Timeout";
                    emit componentExported(status->componentId,
                                           false,
                                           "Export cancelled or Queue Timeout",
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
    if (!m_isPipelineRunning)
        return;

    qDebug() << "Process completed for component:" << status->componentId << "Success:" << status->processSuccess;
    m_pipelineProgress.processCompleted++;

    auto processWriteQueue = m_queueManager.processWriteQueue();
    if (!processWriteQueue)
        return;

    if (status->processSuccess) {
        updateMemoryPeak(status);

        // 使用 safePush 替代直接 push
        if (PipelineQueueManager::safePush(processWriteQueue, status, m_isCancelled)) {
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
            status->writeMessage = "Export cancelled or Queue Timeout";
            emit componentExported(status->componentId,
                                   false,
                                   "Export cancelled or Queue Timeout",
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
    if (!m_isPipelineRunning)
        return;

    qDebug() << "Write completed for component:" << status->componentId << "Success:" << status->writeSuccess;
    m_pipelineProgress.writeCompleted++;

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
    m_fetchHandler->setComponentIds(m_componentIds);
    m_fetchHandler->setOptions(m_options);
    m_fetchHandler->setPreloadedData(m_preloadedData);
    m_fetchHandler->start();
}

void ExportServicePipeline::startProcessStage() {
    m_processHandler->start();
}

void ExportServicePipeline::startWriteStage() {
    m_writeHandler->start();
}

void ExportServicePipeline::checkPipelineCompletion() {
    // 防止重复触发：如果已经安排了完成处理，直接返回
    if (m_completionScheduled.loadAcquire()) {
        qDebug() << "Completion already scheduled, skipping.";
        return;
    }

    auto fetchQueue = m_queueManager.fetchProcessQueue();
    auto writeQueue = m_queueManager.processWriteQueue();

    // 抓取阶段完成检查
    bool fetchDone = m_pipelineProgress.fetchCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire()) {
        if (fetchQueue ? fetchQueue->isEmpty() : true)
            fetchDone = true;
    }
    if (fetchDone && fetchQueue && !fetchQueue->isClosed())
        fetchQueue->close();
    if (!fetchDone)
        return;

    // 处理阶段完成检查
    bool processDone = m_pipelineProgress.processCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire()) {
        if (writeQueue ? writeQueue->isEmpty() : true)
            processDone = true;
    }
    if (processDone && writeQueue && !writeQueue->isClosed())
        writeQueue->close();
    if (!processDone)
        return;

    // 写入阶段完成检查
    bool writeDone = m_pipelineProgress.writeCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire()) {
        if (writeQueue ? writeQueue->isEmpty() : true)
            writeDone = true;
    }
    if (!writeDone)
        return;

    // 标记完成处理已安排，防止重复调用
    m_completionScheduled.storeRelease(1);

    // 所有阶段都完成，开始清理和统计
    qDebug() << "Pipeline completed. Success:" << m_successCount << "Failed:" << m_failureCount;

    // 直接同步执行清理和统计，不使用 QTimer
    // 这样可以避免异步回调导致的崩溃和状态不一致

    // 检查是否已被取消
    bool wasCancelled = m_isCancelled.loadAcquire();
    if (wasCancelled) {
        qDebug() << "Pipeline was cancelled, performing cleanup...";
    }

    // 1. 合并符号库（如果有符号数据）
    if (m_options.exportSymbol && !m_symbols.isEmpty() && !m_options.outputPath.isEmpty() &&
        !m_options.libName.isEmpty()) {
        bool mergeSuccess = mergeSymbolLibrary();
        if (!mergeSuccess) {
            qWarning() << "Failed to merge symbol library during cleanup.";
        }
    }

    // 2. 清理临时文件夹（无论合并是否成功都要清理）
    if (!m_tempDir.isEmpty()) {
        PipelineCleanup::removeTempDir(m_tempDir);
        m_tempDir.clear();
    }

    // 4. 只在非取消模式下生成统计报告
    if (!wasCancelled) {
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
    }

    // 所有清理完成后，发出完成信号
    emit exportCompleted(m_pipelineProgress.totalTasks, m_successCount);
    m_isExporting.storeRelease(0);
    m_isStopping.storeRelease(0);
    m_isCancelled.storeRelease(0);
    // 注意：这里不再重置 m_completionScheduled，让它保持为 1
    // 这样可以防止在 cleanupPipeline 执行期间有任何异步调用重复触发完成逻辑
    // 下一次导出开始时会显式重置这个标志

    cleanupPipeline();
    qDebug() << "Cleanup and statistics generation completed.";
}

void ExportServicePipeline::cleanupPipeline() {
    // 使用原子标志检查，避免锁带来的死锁问题
    // cleanupPipeline 只在主线程的事件循环中被调用，此时所有 worker 已停止
    if (!m_isPipelineRunning) {
        return;
    }
    m_isPipelineRunning = false;

    m_queueManager.closeAll();

    // 清理 worker 追踪集合 (现在由 StageHandler 内部管理)

    qDebug() << "Pipeline resources cleaned up (queues released, threads detached).";
}

bool ExportServicePipeline::mergeSymbolLibrary() {
    QString libraryPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, m_options.libName);
    // 使用统一的临时文件夹
    QString tempPath = QString("%1/%2.kicad_sym.tmp").arg(m_tempDir, m_options.libName);

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
            Q_UNUSED(QFile::remove(libraryPath));
        if (!QFile::rename(tempPath, libraryPath)) {
            Q_UNUSED(QFile::remove(tempPath));
            success = false;
        } else {
            // 重命名成功后，验证并清理可能残留的临时文件
            if (QFile::exists(tempPath)) {
                Q_UNUSED(QFile::remove(tempPath));
            }
        }
    } else {
        if (QFile::exists(tempPath))
            Q_UNUSED(QFile::remove(tempPath));
    }
    return success;
}

void ExportServicePipeline::updateMemoryPeak(const QSharedPointer<ComponentExportStatus>& status) {
    if (!status)
        return;
    MemorySnapshot snapshot = status->getMemorySnapshot();
    if (snapshot.totalSize > m_originalStatistics.peakMemoryUsage) {
        m_originalStatistics.peakMemoryUsage = snapshot.totalSize;
    }
}

ExportStatistics ExportServicePipeline::generateStatistics() {
    return PipelineStatistics::generate(
        m_completedStatuses,
        m_options,
        m_pipelineProgress.totalTasks,
        m_originalExportStartTimeMs > 0 ? m_originalExportStartTimeMs : m_exportStartTimeMs,
        m_isRetryMode,
        m_originalStatistics);
}

void ExportServicePipeline::emergencyCleanup() {
    PipelineCleanup::emergencyCleanup(m_completedStatuses, m_preloadedData, m_symbols, m_tempDir);
}

bool ExportServicePipeline::saveStatisticsReport(const ExportStatistics& statistics, const QString& reportPath) {
    return PipelineStatistics::saveReport(statistics, m_options.outputPath, m_options.libName, reportPath);
}

void ExportServicePipeline::cancelExport() {
    // 防止重复取消
    if (m_isCancelled.loadAcquire()) {
        qDebug() << "ExportPipeline already cancelled, skipping.";
        return;
    }

    qDebug() << "Cancelling ExportPipeline...";

    // 1. 设置取消标志（原子操作，不阻塞）
    m_isCancelled.storeRelease(1);
    m_isStopping.storeRelease(1);
    ExportService::cancelExport();

    // 2. 中断所有任务
    m_fetchHandler->stop();
    m_processHandler->stop();
    m_writeHandler->stop();

    // 3. 启动异步紧急清理，释放内存 (在任务中断后执行，减少数据生成)
    // 直接调用紧急清理，不使用 QTimer，确保在退出时也能执行
    emergencyCleanup();

    // 4. 关闭队列（close() 是非阻塞的）
    m_queueManager.closeAll();

    // 5. 清空线程池（clear() 是非阻塞的）
    if (m_fetchThreadPool) {
        m_fetchThreadPool->clear();
    }
    if (m_processThreadPool) {
        m_processThreadPool->clear();
    }
    if (m_writeThreadPool) {
        m_writeThreadPool->clear();
    }

    // 注意：不在这里发出 exportCompleted 信号
    // 也不在这里做任何清理
    // 所有清理和信号发出都在 checkPipelineCompletion() 中统一处理
    // 这样可以避免竞争条件和状态不一致

    qDebug() << "ExportPipeline cancellation requested. Cleanup will be done by checkPipelineCompletion.";
}

bool ExportServicePipeline::waitForCompletion(int timeoutMs) {
    if (!m_isPipelineRunning)

        return true;

    // 检查 QApplication 实例是否存在

    if (!QCoreApplication::instance()) {
        qWarning() << "QCoreApplication instance not available, skipping graceful wait.";

        return false;
    }

    qDebug() << "Waiting for pipeline completion (timeout:" << timeoutMs << "ms)...";

    // 确保已设置取消标志，加速 Worker 退出

    if (!m_isCancelled.loadAcquire()) {
        m_isCancelled.storeRelease(1);
    }

    QElapsedTimer timer;

    timer.start();

    // 缓存主线程指针

    QThread* mainThread = QCoreApplication::instance()->thread();

    // 定义一个辅助 lambda 用于安全等待线程池

    auto safeWaitForPool = [&](QThreadPool* pool, const char* name) -> bool {
        if (!pool)
            return true;

        while (pool->activeThreadCount() > 0) {
            if (timer.elapsed() > timeoutMs) {
                qWarning() << name << "thread pool wait timed out";

                return false;
            }

            // 尝试非阻塞等待一小段时间

            if (pool->waitForDone(50)) {
                break;
            }

            // 关键：仅在主线程处理事件循环，防止死锁并确保线程安全

            if (QThread::currentThread() == mainThread) {
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            }
        }

        return true;
    };

    // 1. 等待 fetch 线程池

    if (!safeWaitForPool(m_fetchThreadPool, "Fetch"))
        return false;

    // 2. 等待 process 线程池

    if (!safeWaitForPool(m_processThreadPool, "Process"))
        return false;

    // 3. 等待 write 线程池

    if (!safeWaitForPool(m_writeThreadPool, "Write"))
        return false;

    qDebug() << "Pipeline wait finished successfully in" << timer.elapsed() << "ms";

    return true;
}

}  // namespace EasyKiConverter
