#include "ExportService_Pipeline.h"
#include "workers/FetchWorker.h"
#include "workers/ProcessWorker.h"
#include "workers/WriteWorker.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <algorithm>

namespace EasyKiConverter
{

    ExportServicePipeline::ExportServicePipeline(QObject *parent)
        : ExportService(parent), m_fetchThreadPool(new QThreadPool(this)), m_processThreadPool(new QThreadPool(this)), m_writeThreadPool(new QThreadPool(this)), m_fetchProcessQueue(new BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>(100)), m_processWriteQueue(new BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>(100)), m_networkAccessManager(new QNetworkAccessManager(this)), m_isPipelineRunning(false), m_mutex(new QMutex()), m_successCount(0), m_failureCount(0), m_exportStartTimeMs(0)
    {
        // 配置线程�?
        m_fetchThreadPool->setMaxThreadCount(32);                            // I/O密集型，32个线�?
        m_processThreadPool->setMaxThreadCount(QThread::idealThreadCount()); // CPU密集型，等于核心�?
        m_writeThreadPool->setMaxThreadCount(8);                             // 磁盘I/O密集型，8个线�?

        qDebug() << "ExportServicePipeline initialized with thread pools:"
                 << "Fetch:" << m_fetchThreadPool->maxThreadCount()
                 << "Process:" << m_processThreadPool->maxThreadCount()
                 << "Write:" << m_writeThreadPool->maxThreadCount();
    }

    ExportServicePipeline::~ExportServicePipeline()
    {
        cleanupPipeline();
    }

    void ExportServicePipeline::executeExportPipelineWithStages(const QStringList &componentIds, const ExportOptions &options)
    {
        qDebug() << "Starting pipeline export for" << componentIds.size() << "components";

        QMutexLocker locker(m_mutex);

        if (m_isPipelineRunning)
        {
            qWarning() << "Pipeline is already running";
            return;
        }

        // 使用固定队列大小�?4）以防止内存溢出
        // 固定大小提供背压（Backpressure）机制，当下游处理不过来时阻塞上�?
        const size_t FIXED_QUEUE_SIZE = 64;

        // 重新创建队列以应用固定的队列大小
        delete m_fetchProcessQueue;
        delete m_processWriteQueue;
        m_fetchProcessQueue = new BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>(FIXED_QUEUE_SIZE);
        m_processWriteQueue = new BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>(FIXED_QUEUE_SIZE);

        qDebug() << "Fixed queue size set to:" << FIXED_QUEUE_SIZE << "(prevents memory overflow for" << componentIds.size() << "tasks)";

        // 初始化流水线状�?
        m_componentIds = componentIds;
        m_options = options;
        m_pipelineProgress.totalTasks = componentIds.size();
        m_pipelineProgress.fetchCompleted = 0;
        m_pipelineProgress.processCompleted = 0;
        m_pipelineProgress.writeCompleted = 0;
        m_successCount = 0;
        m_failureCount = 0;
        m_tempSymbolFiles.clear();
        m_symbols.clear();
        m_completedStatuses.clear();
        m_isPipelineRunning = true;
        m_exportStartTimeMs = QDateTime::currentMSecsSinceEpoch();

        // 创建输出目录
        QDir dir;
        if (!dir.exists(m_options.outputPath))
        {
            dir.mkpath(m_options.outputPath);
        }

        // 释放互斥锁以允许其他线程工作
        locker.unlock();

        // 发出开始导出信�?
        emit exportProgress(0, m_pipelineProgress.totalTasks);

        // 启动抓取阶段
        startFetchStage();

        // 启动处理和写入阶�?
        startProcessStage();
        startWriteStage();

        qDebug() << "Pipeline export started";
    }

    PipelineProgress ExportServicePipeline::getPipelineProgress() const
    {
        QMutexLocker locker(m_mutex);
        return m_pipelineProgress;
    }

    void ExportServicePipeline::handleFetchCompleted(QSharedPointer<ComponentExportStatus> status)
    {
        QMutexLocker locker(m_mutex);

        qDebug() << "Fetch completed for component:" << status->componentId
                 << "Success:" << status->fetchSuccess;

        m_pipelineProgress.fetchCompleted++;

        // 保存状态到列表（用于生成统计报告）
        m_completedStatuses.append(status);

        if (status->fetchSuccess)
        {
            // 将数据放入处理队列（使用 QSharedPointer 避免拷贝�?
            m_fetchProcessQueue->push(status);
            // 发送抓取完成信号（包含阶段信息�?
            emit componentExported(status->componentId, true, "Fetch completed", static_cast<int>(PipelineStage::Fetch));
        }
        else
        {
            // 抓取失败，直接记录失�?
            m_failureCount++;
            qDebug() << "Fetch failed for component:" << status->componentId << "Error:" << status->fetchMessage;
            emit componentExported(status->componentId, false, status->fetchMessage, static_cast<int>(PipelineStage::Fetch));
        }

        emit pipelineProgressUpdated(m_pipelineProgress);

        qDebug() << "Pipeline progress emitted (fetch) - Fetch:" << m_pipelineProgress.fetchCompleted << "/" << m_pipelineProgress.totalTasks
                 << "Process:" << m_pipelineProgress.processCompleted << "/" << m_pipelineProgress.totalTasks
                 << "Write:" << m_pipelineProgress.writeCompleted << "/" << m_pipelineProgress.totalTasks;

        // 检查是否完�?
        checkPipelineCompletion();
    }

    void ExportServicePipeline::handleProcessCompleted(QSharedPointer<ComponentExportStatus> status)
    {
        QMutexLocker locker(m_mutex);

        qDebug() << "Process completed for component:" << status->componentId
                 << "Success:" << status->processSuccess;

        m_pipelineProgress.processCompleted++;

        // 更新状态（不重复添加，因为已经�?Fetch 阶段添加了）
        // ProcessWorker 会修改同一�?status 对象

        if (status->processSuccess)
        {
            // 将数据放入写入队列（使用 QSharedPointer 避免拷贝�?
            m_processWriteQueue->push(status);
            // 发送处理完成信号（包含阶段信息�?
            emit componentExported(status->componentId, true, "Process completed", static_cast<int>(PipelineStage::Process));
        }
        else
        {
            // 处理失败，直接记录失�?
            m_failureCount++;
            qDebug() << "Process failed for component:" << status->componentId << "Error:" << status->processMessage;
            emit componentExported(status->componentId, false, status->processMessage, static_cast<int>(PipelineStage::Process));
        }

        emit pipelineProgressUpdated(m_pipelineProgress);

        qDebug() << "Pipeline progress emitted (process) - Fetch:" << m_pipelineProgress.fetchCompleted << "/" << m_pipelineProgress.totalTasks
                 << "Process:" << m_pipelineProgress.processCompleted << "/" << m_pipelineProgress.totalTasks
                 << "Write:" << m_pipelineProgress.writeCompleted << "/" << m_pipelineProgress.totalTasks;

        // 检查是否完�?
        checkPipelineCompletion();
    }

    void ExportServicePipeline::handleWriteCompleted(QSharedPointer<ComponentExportStatus> status)
    {
        QMutexLocker locker(m_mutex);

        qDebug() << "Write completed for component:" << status->componentId
                 << "Success:" << status->writeSuccess;

        m_pipelineProgress.writeCompleted++;

        if (status->writeSuccess)
        {
            m_successCount++;

            // 如果导出了符号，将符号数据加入列�?
            if (m_options.exportSymbol && status->symbolData)
            {
                m_symbols.append(*status->symbolData);
                qDebug() << "Added symbol to merge list:" << status->symbolData->info().name;
            }

            // 如果导出了符号，将临时文件加入列表（用于清理�?
            if (m_options.exportSymbol && status->symbolData)
            {
                QString tempFilePath = QString("%1/%2.kicad_sym.tmp").arg(m_options.outputPath, status->componentId);
                if (QFile::exists(tempFilePath))
                {
                    m_tempSymbolFiles.append(tempFilePath);
                }
            }

            // 发送写入完成信号（包含阶段信息�?
            emit componentExported(status->componentId, true, "Export completed successfully", static_cast<int>(PipelineStage::Write));
        }
        else
        {
            m_failureCount++;
            qDebug() << "Write failed for component:" << status->componentId << "Error:" << status->writeMessage;
            emit componentExported(status->componentId, false, status->writeMessage, static_cast<int>(PipelineStage::Write));
        }

        emit pipelineProgressUpdated(m_pipelineProgress);

        qDebug() << "Pipeline progress emitted (write) - Fetch:" << m_pipelineProgress.fetchCompleted << "/" << m_pipelineProgress.totalTasks
                 << "Process:" << m_pipelineProgress.processCompleted << "/" << m_pipelineProgress.totalTasks
                 << "Write:" << m_pipelineProgress.writeCompleted << "/" << m_pipelineProgress.totalTasks;

        // 检查是否完�?
        checkPipelineCompletion();
    }

    void ExportServicePipeline::startFetchStage()
    {
        qDebug() << "Starting fetch stage for" << m_componentIds.size() << "components";

        for (const QString &componentId : m_componentIds)
        {
            FetchWorker *worker = new FetchWorker(
                componentId,
                m_networkAccessManager,
                m_options.exportModel3D,
                nullptr); // 不设置parent，避免线程问�?

            connect(worker, &FetchWorker::fetchCompleted,
                    this, &ExportServicePipeline::handleFetchCompleted,
                    Qt::QueuedConnection); // 使用队列连接确保线程安全

            connect(worker, &FetchWorker::fetchCompleted,
                    worker, &QObject::deleteLater,
                    Qt::QueuedConnection); // 自动删除worker

            m_fetchThreadPool->start(worker);
        }
    }

    void ExportServicePipeline::startProcessStage()
    {
        qDebug() << "Starting process stage";

        // 创建持续运行的处理工作线�?
        for (int i = 0; i < m_processThreadPool->maxThreadCount(); i++)
        {
            QRunnable *task = QRunnable::create([this]()
                                                {
            while (true) {
                QSharedPointer<ComponentExportStatus> status;
                if (!m_fetchProcessQueue->pop(status, 1000)) {
                    // 超时，检查队列是否已关闭
                    if (m_fetchProcessQueue->isClosed()) {
                        break;
                    }
                    continue;
                }

                // 创建ProcessWorker处理数据（不设置parent以避免线程警告）
                ProcessWorker *worker = new ProcessWorker(status, nullptr);
                connect(worker, &ProcessWorker::processCompleted,
                        this, &ExportServicePipeline::handleProcessCompleted,
                        Qt::QueuedConnection);
                connect(worker, &ProcessWorker::processCompleted,
                        worker, &QObject::deleteLater,
                        Qt::QueuedConnection);
                worker->run();
            } });

            m_processThreadPool->start(task);
        }
    }

    void ExportServicePipeline::startWriteStage()
    {
        qDebug() << "Starting write stage";

        // 创建持续运行的写入工作线�?
        for (int i = 0; i < m_writeThreadPool->maxThreadCount(); i++)
        {
            QRunnable *task = QRunnable::create([this]()
                                                {
            while (true) {
                QSharedPointer<ComponentExportStatus> status;
                if (!m_processWriteQueue->pop(status, 1000)) {
                    // 超时，检查队列是否已关闭
                    if (m_processWriteQueue->isClosed()) {
                        break;
                    }
                    continue;
                }

                // 创建WriteWorker写入数据（不设置parent以避免线程警告）
                WriteWorker *worker = new WriteWorker(
                    status,
                    m_options.outputPath,
                    m_options.libName,
                    m_options.exportSymbol,
                    m_options.exportFootprint,
                    m_options.exportModel3D,
                    m_options.debugMode,
                    nullptr);

                connect(worker, &WriteWorker::writeCompleted,
                        this, &ExportServicePipeline::handleWriteCompleted,
                        Qt::QueuedConnection);
                connect(worker, &WriteWorker::writeCompleted,
                        worker, &QObject::deleteLater,
                        Qt::QueuedConnection);
                worker->run();
            } });

            m_writeThreadPool->start(task);
        }
    }

    void ExportServicePipeline::checkPipelineCompletion()
    {
        if (m_pipelineProgress.fetchCompleted < m_pipelineProgress.totalTasks)
        {
            return; // 还在抓取
        }

        if (m_pipelineProgress.processCompleted < m_pipelineProgress.totalTasks)
        {
            return; // 还在处理
        }

        if (m_pipelineProgress.writeCompleted < m_pipelineProgress.totalTasks)
        {
            return; // 还在写入
        }

        // 所有阶段都完成�?
        qDebug() << "Pipeline completed. Success:" << m_successCount << "Failed:" << m_failureCount;

        // 合并符号�?
        if (m_options.exportSymbol && !m_symbols.isEmpty())
        {
            mergeSymbolLibrary();
        }

        // 生成统计报告
        ExportStatistics statistics = generateStatistics();
        QString reportPath = QString("%1/export_report_%2.json")
                                 .arg(m_options.outputPath)
                                 .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        saveStatisticsReport(statistics, reportPath);
        qDebug() << "Statistics report generated:" << reportPath;

        // 发送统计报告生成信�?
        emit statisticsReportGenerated(reportPath, statistics);

        // 清理临时文件
        for (const QString &tempFile : m_tempSymbolFiles)
        {
            QFile::remove(tempFile);
        }
        m_tempSymbolFiles.clear();
        m_symbols.clear();

        // 清理状态列�?
        m_completedStatuses.clear();

        // 发送完成信�?
        emit exportCompleted(m_pipelineProgress.totalTasks, m_successCount);

        // 清理流水线（使用QTimer::singleShot延迟执行，避免在信号处理中清理）
        QTimer::singleShot(0, this, [this]()
                           { cleanupPipeline(); });
    }

    void ExportServicePipeline::cleanupPipeline()
    {
        QMutexLocker locker(m_mutex);

        if (!m_isPipelineRunning)
        {
            return;
        }

        qDebug() << "Cleaning up pipeline";

        // 关闭队列
        m_fetchProcessQueue->close();
        m_processWriteQueue->close();

        // 等待线程池完�?
        m_fetchThreadPool->waitForDone();
        m_processThreadPool->waitForDone();
        m_writeThreadPool->waitForDone();

        m_isPipelineRunning = false;
        m_tempSymbolFiles.clear();
    }

    bool ExportServicePipeline::mergeSymbolLibrary()
    {
        qDebug() << "Merging symbol library from" << m_symbols.size() << "symbols";

        if (m_symbols.isEmpty())
        {
            qDebug() << "No symbols to merge";
            return true;
        }

        // 导出合并后的符号�?
        QString libraryPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, m_options.libName);
        
        // 正确设置 appendMode �?updateMode
        // appendMode: true = 追加模式（跳过已存在的符号）
        // updateMode: true = 更新模式（覆盖已存在的符号）
        bool appendMode = !m_options.overwriteExistingFiles;
        bool updateMode = m_options.updateMode;

        qDebug() << "Merge settings - Append mode:" << appendMode << "Update mode:" << updateMode;

        // 使用ExporterSymbol直接导出
        ExporterSymbol exporter;
        bool success = exporter.exportSymbolLibrary(m_symbols, m_options.libName, libraryPath, appendMode, updateMode);

        if (success)
        {
            qDebug() << "Symbol library merged successfully:" << libraryPath;
        }
        else
        {
            qWarning() << "Failed to merge symbol library:" << libraryPath;
        }

        return success;
    }

    ExportStatistics ExportServicePipeline::generateStatistics()
    {
        ExportStatistics statistics;

        statistics.total = m_pipelineProgress.totalTasks;
        statistics.success = m_successCount;
        statistics.failed = m_failureCount;
        statistics.totalDurationMs = QDateTime::currentMSecsSinceEpoch() - m_exportStartTimeMs;

        qint64 totalFetchTime = 0;
        qint64 totalProcessTime = 0;
        qint64 totalWriteTime = 0;

        // 遍历所有完成的状态，收集详细统计
        for (const QSharedPointer<ComponentExportStatus> &status : m_completedStatuses)
        {
            // 收集失败原因和阶�?
            if (!status->isCompleteSuccess())
            {
                QString failedStage = status->getFailedStage();
                QString failureReason = status->getFailureReason();

                statistics.stageFailures[failedStage]++;
                statistics.failureReasons[failureReason]++;
            }

            // 收集时间统计
            totalFetchTime += status->fetchDurationMs;
            totalProcessTime += status->processDurationMs;
            totalWriteTime += status->writeDurationMs;

            // 收集最慢的组件
            qint64 totalDuration = status->getTotalDurationMs();
            statistics.slowestComponents.append(qMakePair(status->componentId, totalDuration));
        }

        // 计算平均时间
        if (statistics.total > 0)
        {
            statistics.avgFetchTimeMs = totalFetchTime / statistics.total;
            statistics.avgProcessTimeMs = totalProcessTime / statistics.total;
            statistics.avgWriteTimeMs = totalWriteTime / statistics.total;
        }

        // 排序最慢的组件（取�?0个）
        std::sort(statistics.slowestComponents.begin(), statistics.slowestComponents.end(),
                  [](const QPair<QString, qint64> &a, const QPair<QString, qint64> &b)
                  {
                      return a.second > b.second;
                  });

        if (statistics.slowestComponents.size() > 10)
        {
            statistics.slowestComponents = statistics.slowestComponents.mid(0, 10);
        }

        return statistics;
    }

    bool ExportServicePipeline::saveStatisticsReport(const ExportStatistics &statistics, const QString &reportPath)
    {
        QJsonObject reportObj;

        // 概览信息
        QJsonObject overviewObj;
        overviewObj["total"] = statistics.total;
        overviewObj["success"] = statistics.success;
        overviewObj["failed"] = statistics.failed;
        overviewObj["successRate"] = QString::number(statistics.getSuccessRate(), 'f', 2) + "%";
        overviewObj["totalDurationMs"] = statistics.totalDurationMs;
        overviewObj["totalDurationSeconds"] = QString::number(statistics.totalDurationMs / 1000.0, 'f', 2);
        reportObj["overview"] = overviewObj;

        // 时间统计
        QJsonObject timingObj;
        timingObj["avgFetchTimeMs"] = statistics.avgFetchTimeMs;
        timingObj["avgProcessTimeMs"] = statistics.avgProcessTimeMs;
        timingObj["avgWriteTimeMs"] = statistics.avgWriteTimeMs;
        timingObj["slowestStage"] = statistics.getSlowestStage();
        reportObj["timing"] = timingObj;

        // 失败统计
        QJsonObject failuresObj;
        QJsonObject stageFailuresObj;
        for (auto it = statistics.stageFailures.constBegin(); it != statistics.stageFailures.constEnd(); ++it)
        {
            stageFailuresObj[it.key()] = it.value();
        }
        failuresObj["stageFailures"] = stageFailuresObj;

        QJsonObject failureReasonsObj;
        for (auto it = statistics.failureReasons.constBegin(); it != statistics.failureReasons.constEnd(); ++it)
        {
            failureReasonsObj[it.key()] = it.value();
        }
        failuresObj["failureReasons"] = failureReasonsObj;
        reportObj["failures"] = failuresObj;

        // 最慢的组件
        QJsonArray slowestArray;
        for (const auto &item : statistics.slowestComponents)
        {
            QJsonObject componentObj;
            componentObj["componentId"] = item.first;
            componentObj["durationMs"] = item.second;
            componentObj["durationSeconds"] = QString::number(item.second / 1000.0, 'f', 2);
            slowestArray.append(componentObj);
        }
        reportObj["slowestComponents"] = slowestArray;

        // 导出选项
        QJsonObject optionsObj;
        optionsObj["outputPath"] = m_options.outputPath;
        optionsObj["libName"] = m_options.libName;
        optionsObj["exportSymbol"] = m_options.exportSymbol;
        optionsObj["exportFootprint"] = m_options.exportFootprint;
        optionsObj["exportModel3D"] = m_options.exportModel3D;
        optionsObj["overwriteExistingFiles"] = m_options.overwriteExistingFiles;
        optionsObj["updateMode"] = m_options.updateMode;
        optionsObj["debugMode"] = m_options.debugMode;
        reportObj["exportOptions"] = optionsObj;

        // 时间�?
        reportObj["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        reportObj["exportStartTime"] = QDateTime::fromMSecsSinceEpoch(m_exportStartTimeMs).toString(Qt::ISODate);
        reportObj["exportEndTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);

        // 保存到文�?
        QJsonDocument doc(reportObj);
        QFile file(reportPath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qWarning() << "Failed to open report file for writing:" << reportPath;
            return false;
        }

        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        qDebug() << "Statistics report saved to:" << reportPath;
        return true;
    }

} // namespace EasyKiConverter
