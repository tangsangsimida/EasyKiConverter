#include "ExportService_Pipeline.h"

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
    // 使用 QSharedPointer 创建新队列，自动管理内存
    m_fetchProcessQueue =
        QSharedPointer<BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>>::create(FIXED_QUEUE_SIZE);
    m_processWriteQueue =
        QSharedPointer<BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>>::create(FIXED_QUEUE_SIZE);

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

void ExportServicePipeline::handleFetchCompleted(QSharedPointer<ComponentExportStatus> status, FetchWorker* worker) {
    if (worker) {
        QMutexLocker locker(&m_workerMutex);
        m_activeFetchWorkers.remove(worker);
    }

    QMutexLocker locker(m_mutex);
    if (!m_isPipelineRunning)
        return;

    qDebug() << "Fetch completed for component:" << status->componentId << "Success:" << status->fetchSuccess
             << "fetch3DOnly:" << status->fetch3DOnly;
    m_pipelineProgress.fetchCompleted++;
    m_completedStatuses.append(status);

    // 安全检查队列
    if (!m_fetchProcessQueue)
        return;

    if (status->fetchSuccess) {
        // 使用 safePushToQueue 替代直接 push
        if (safePushToQueue(m_fetchProcessQueue, status)) {
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

                if (safePushToQueue(m_fetchProcessQueue, status)) {
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

void ExportServicePipeline::handleProcessCompleted(QSharedPointer<ComponentExportStatus> status,
                                                   ProcessWorker* worker) {
    if (worker) {
        QMutexLocker locker(&m_workerMutex);
        m_activeProcessWorkers.remove(worker);
    }

    QMutexLocker locker(m_mutex);
    if (!m_isPipelineRunning)
        return;

    qDebug() << "Process completed for component:" << status->componentId << "Success:" << status->processSuccess;
    m_pipelineProgress.processCompleted++;

    if (!m_processWriteQueue)
        return;

    if (status->processSuccess) {
        // 使用 safePushToQueue 替代直接 push
        if (safePushToQueue(m_processWriteQueue, status)) {
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

void ExportServicePipeline::handleWriteCompleted(QSharedPointer<ComponentExportStatus> status, WriteWorker* worker) {
    if (worker) {
        QMutexLocker locker(&m_workerMutex);
        m_activeWriteWorkers.remove(worker);
    }

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

            // 使用 safePushToQueue
            if (safePushToQueue(m_processWriteQueue, status)) {
                // 成功推送到写入队列
            } else {
                // 关键修复：如果推送失败，必须增加写入完成计数并发送失败信号，否则流水线会卡死
                m_failureCount++;
                m_pipelineProgress.writeCompleted++;
                status->writeSuccess = false;
                status->writeMessage = "Failed to push preloaded data to write queue";
                emit componentExported(status->componentId,
                                       false,
                                       "Export failed (Queue Error)",
                                       static_cast<int>(PipelineStage::Write),
                                       status->symbolWritten,
                                       status->footprintWritten,
                                       status->model3DWritten);
                emit pipelineProgressUpdated(m_pipelineProgress);
                checkPipelineCompletion();
            }

            continue;
        }

        FetchWorker* worker =
            new FetchWorker(componentId, m_networkAccessManager, m_options.exportModel3D, false, QString(), nullptr);

        // 标准化生命周期管理：确保 deleteLater 在主线程执行
        worker->moveToThread(this->thread());

        // 使用 Lambda 捕获 worker 指针，避免在槽函数中使用 sender()
        // Context 设为 this，确保 Service 销毁时连接自动断开，避免回调悬空指针
        connect(
            worker,
            &FetchWorker::fetchCompleted,
            this,
            [this, worker](QSharedPointer<ComponentExportStatus> status) { handleFetchCompleted(status, worker); },
            Qt::QueuedConnection);

        // 让 Worker 在信号处理完成后自动销毁
        connect(worker, &FetchWorker::fetchCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);

        {
            QMutexLocker locker(&m_workerMutex);
            m_activeFetchWorkers.insert(worker);
        }

        m_fetchThreadPool->start(worker);
    }
}

void ExportServicePipeline::startProcessStage() {
    // 捕获队列的共享指针副本，确保在 Service 被销毁时队列依然有效
    auto fetchProcessQueue = m_fetchProcessQueue;

    for (int i = 0; i < m_processThreadPool->maxThreadCount(); i++) {
        m_processThreadPool->start(QRunnable::create([this, fetchProcessQueue]() {
            while (true) {
                if (m_isCancelled.loadAcquire()) {  // Batch-level cancellation check
                    qDebug() << "Process stage worker exiting due to cancellation.";
                    break;
                }

                // 使用捕获的队列指针进行操作
                if (!fetchProcessQueue)
                    break;

                QSharedPointer<ComponentExportStatus> status;
                if (!fetchProcessQueue->pop(status, 1000)) {
                    if (fetchProcessQueue->isClosed())
                        break;
                    continue;
                }

                ProcessWorker* worker = new ProcessWorker(status, nullptr);

                // 关键修复：将 worker 移动到主线程，确保 deleteLater() 能在主线程事件循环中被执行
                // 线程池线程通常不运行事件循环，直接 deleteLater 可能导致内存泄漏
                worker->moveToThread(this->thread());

                // 注册活跃 worker
                {
                    QMutexLocker locker(&m_workerMutex);
                    m_activeProcessWorkers.insert(worker);
                }

                // 使用 Lambda 捕获 worker 指针，避免在槽函数中使用 sender()
                // Context 设为 this，确保 Service 销毁时连接自动断开
                connect(
                    worker,
                    &ProcessWorker::processCompleted,
                    this,
                    [this, worker](QSharedPointer<ComponentExportStatus> status) {
                        handleProcessCompleted(status, worker);
                    },
                    Qt::QueuedConnection);

                // 让 Worker 在信号处理完成后自动销毁
                connect(worker, &ProcessWorker::processCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);

                worker->run();
            }
        }));
    }
}

void ExportServicePipeline::startWriteStage() {
    // 捕获队列的共享指针副本，确保在 Service 被销毁时队列依然有效
    auto processWriteQueue = m_processWriteQueue;

    for (int i = 0; i < m_writeThreadPool->maxThreadCount(); i++) {
        m_writeThreadPool->start(QRunnable::create([this, processWriteQueue]() {
            while (true) {
                if (m_isCancelled.loadAcquire()) {  // Batch-level cancellation check
                    qDebug() << "Write stage worker exiting due to cancellation.";
                    break;
                }

                // 使用捕获的队列指针进行操作
                if (!processWriteQueue)
                    break;

                QSharedPointer<ComponentExportStatus> status;
                if (!processWriteQueue->pop(status, 1000)) {
                    if (processWriteQueue->isClosed())
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
                                                      m_tempDir,  // 传递临时文件夹路径
                                                      nullptr);

                // 关键修复：将 worker 移动到主线程，确保 deleteLater() 能在主线程事件循环中被执行
                worker->moveToThread(this->thread());

                // 注册活跃 worker
                {
                    QMutexLocker locker(&m_workerMutex);
                    m_activeWriteWorkers.insert(worker);
                }

                // 使用 Lambda 捕获 worker 指针，避免在槽函数中使用 sender()
                // Context 设为 this，确保 Service 销毁时连接自动断开
                connect(
                    worker,
                    &WriteWorker::writeCompleted,
                    this,
                    [this, worker](QSharedPointer<ComponentExportStatus> status) {
                        handleWriteCompleted(status, worker);
                    },
                    Qt::QueuedConnection);

                // 让 Worker 在信号处理完成后自动销毁
                connect(worker, &WriteWorker::writeCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);

                worker->run();
            }
        }));
    }
}

void ExportServicePipeline::checkPipelineCompletion() {
    // 防止重复触发：如果已经安排了完成处理，直接返回
    if (m_completionScheduled.loadAcquire()) {
        qDebug() << "Completion already scheduled, skipping.";
        return;
    }

    // 抓取阶段完成检查
    bool fetchDone = m_pipelineProgress.fetchCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire()) {
        bool fetchQueueEmpty = m_fetchProcessQueue ? m_fetchProcessQueue->isEmpty() : true;
        if (fetchQueueEmpty)
            fetchDone = true;
    }
    if (fetchDone && m_fetchProcessQueue && !m_fetchProcessQueue->isClosed())
        m_fetchProcessQueue->close();
    if (!fetchDone)
        return;

    // 处理阶段完成检查
    bool processDone = m_pipelineProgress.processCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire()) {
        bool processQueueEmpty = m_processWriteQueue ? m_processWriteQueue->isEmpty() : true;
        if (processQueueEmpty)
            processDone = true;
    }
    if (processDone && m_processWriteQueue && !m_processWriteQueue->isClosed())
        m_processWriteQueue->close();
    if (!processDone)
        return;

    // 写入阶段完成检查
    bool writeDone = m_pipelineProgress.writeCompleted >= m_pipelineProgress.totalTasks;
    if (m_isCancelled.loadAcquire()) {
        bool writeQueueEmpty = m_processWriteQueue ? m_processWriteQueue->isEmpty() : true;
        if (writeQueueEmpty)
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
        // 使用安全删除，防止意外的大规模删除
        if (PathSecurity::safeRemoveRecursively(m_tempDir)) {
            qDebug() << "Temp folder removed safely:" << m_tempDir;
        } else {
            qWarning() << "Failed to remove temp folder (or aborted for security):" << m_tempDir;
        }
    }
    m_tempDir.clear();

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
    m_completionScheduled.storeRelease(0);

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

    // 关键修复：移除 waitForDone() 调用
    // 之前这里调用 waitForDone() 会导致死锁，因为 worker 线程可能正试图通过 Qt::QueuedConnection 发送信号给主线程
    // 而主线程被 waitForDone() 阻塞，无法处理信号
    //
    // 现在我们采用以下策略：
    // 1. 在 cancelExport() 中已经发出了取消信号并 abort 了所有 worker
    // 2. 队列使用了 QSharedPointer，并被 worker 的 Lambda 捕获
    // 3. 这里我们只需要 reset 成员变量中的指针，不仅是安全的，而且是必须的
    // 4. 当最后一个 worker 结束时，队列的引用计数归零，自动销毁

    if (m_fetchProcessQueue) {
        m_fetchProcessQueue->close();
        m_fetchProcessQueue.reset();
    }
    if (m_processWriteQueue) {
        m_processWriteQueue->close();
        m_processWriteQueue.reset();
    }

    // 清理 worker 追踪集合
    {
        QMutexLocker locker(&m_workerMutex);
        m_activeFetchWorkers.clear();
        m_activeProcessWorkers.clear();
        m_activeWriteWorkers.clear();
    }

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

    // v3.0.5: 内存峰值统计
    // 无论是否重试模式，都使用当前记录的最大值
    statistics.peakMemoryUsage = m_originalStatistics.peakMemoryUsage;

    return statistics;
}

bool ExportServicePipeline::safePushToQueue(
    QSharedPointer<BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>> queue,
    QSharedPointer<ComponentExportStatus> status) {
    if (!queue || m_isCancelled.loadAcquire())
        return false;

    // 统计内存峰值
    MemorySnapshot snapshot = status->getMemorySnapshot();
    // 无论是否重试模式，只要当前峰值更高，就更新
    // 由于 safePushToQueue 是在主线程调用的（通过 handle...Completed），所以是线程安全的
    if (snapshot.totalSize > m_originalStatistics.peakMemoryUsage) {
        m_originalStatistics.peakMemoryUsage = snapshot.totalSize;
    }

    // 快速尝试
    if (queue->tryPush(status))
        return true;

    // 指数退避策略
    constexpr int MAX_RETRIES = 5;
    constexpr int BASE_DELAY_MS = 10;          // 减少初始延迟到 10ms
    constexpr int MAX_TOTAL_TIMEOUT_MS = 500;  // 减少总超时到 500ms

    int retryCount = 0;
    int totalDelay = 0;

    while (retryCount < MAX_RETRIES) {
        if (m_isCancelled.loadAcquire())
            return false;

        // 防止 UI 冻结
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        // 尝试非阻塞推送
        if (queue->tryPush(status)) {
            if (retryCount > 0) {
                qDebug() << "Queue backoff successful after" << retryCount << "retries for" << status->componentId;
            }
            return true;
        }

        // 计算延迟
        int delay = BASE_DELAY_MS * (1 << retryCount);
        delay += QRandomGenerator::global()->bounded(0, 20);  // 抖动

        if (totalDelay + delay > MAX_TOTAL_TIMEOUT_MS) {
            qWarning() << "Queue push timed out after" << totalDelay << "ms for component:" << status->componentId;
            return false;
        }

        // QThread::msleep 是阻塞当前线程（主线程）
        // 因为我们在循环中调用了 processEvents，所以界面不会完全死锁
        QThread::msleep(delay);
        totalDelay += delay;
        retryCount++;
    }

    return false;
}

void ExportServicePipeline::emergencyCleanup() {
    qDebug() << "Performing emergency cleanup...";

    QMutexLocker locker(m_mutex);

    // 清理已完成状态中的临时数据（这些数据可能占用大量内存）
    for (const auto& status : m_completedStatuses) {
        if (status) {
            status->clearIntermediateData(false);
            // 既然是紧急清理，说明不再继续，STEP 数据也可以清了
            status->clearStepData();
        }
    }
    m_completedStatuses.clear();

    // 清理预加载数据
    m_preloadedData.clear();

    // 清理符号数据
    m_symbols.clear();

    // 新增：清理临时目录，防止强行退出后残留大量临时文件
    if (!m_tempDir.isEmpty()) {
        if (PathSecurity::safeRemoveRecursively(m_tempDir)) {
            qDebug() << "Emergency cleanup: Temp folder removed safely:" << m_tempDir;
        }
        m_tempDir.clear();
    }

    qDebug() << "Emergency cleanup completed: Memory released and disk cleaned.";
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
    // v3.0.5 新增：内存峰值（转换为 MB 显示）
    overview["peakMemoryUsageMB"] = QString::number(statistics.peakMemoryUsage / (1024.0 * 1024.0), 'f', 2) + " MB";

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
    {
        QMutexLocker locker(&m_workerMutex);

        // 中断 Fetch 任务
        for (FetchWorker* worker : m_activeFetchWorkers) {
            if (worker)
                worker->abort();
        }
        m_activeFetchWorkers.clear();

        // 中断 Process 任务
        for (ProcessWorker* worker : m_activeProcessWorkers) {
            if (worker)
                worker->abort();
        }
        m_activeProcessWorkers.clear();

        // 中断 Write 任务
        for (WriteWorker* worker : m_activeWriteWorkers) {
            if (worker)
                worker->abort();
        }
        m_activeWriteWorkers.clear();
    }

    // 3. 启动异步紧急清理，释放内存 (在任务中断后执行，减少数据生成)
    // 直接调用紧急清理，不使用 QTimer，确保在退出时也能执行
    emergencyCleanup();

    // 4. 关闭队列（close() 是非阻塞的）
    if (m_fetchProcessQueue) {
        m_fetchProcessQueue->close();
    }
    if (m_processWriteQueue) {
        m_processWriteQueue->close();
    }

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

        if (!pool) return true;

        

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

    if (!safeWaitForPool(m_fetchThreadPool, "Fetch")) return false;


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
