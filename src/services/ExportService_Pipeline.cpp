#include "ExportService_Pipeline.h"
#include "src/workers/FetchWorker.h"
#include "src/workers/ProcessWorker.h"
#include "src/workers/WriteWorker.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

namespace EasyKiConverter {

ExportServicePipeline::ExportServicePipeline(QObject *parent)
    : ExportService(parent)
    , m_fetchThreadPool(new QThreadPool(this))
    , m_processThreadPool(new QThreadPool(this))
    , m_writeThreadPool(new QThreadPool(this))
    , m_fetchProcessQueue(new BoundedThreadSafeQueue<ComponentExportStatus>(100))
    , m_processWriteQueue(new BoundedThreadSafeQueue<ComponentExportStatus>(100))
    , m_networkAccessManager(new QNetworkAccessManager(this))
    , m_isPipelineRunning(false)
    , m_mutex(new QMutex())
    , m_successCount(0)
    , m_failureCount(0)
{
    // 配置线程池
    m_fetchThreadPool->setMaxThreadCount(32);      // I/O密集型，32个线程
    m_processThreadPool->setMaxThreadCount(QThread::idealThreadCount());  // CPU密集型，等于核心数
    m_writeThreadPool->setMaxThreadCount(8);      // 磁盘I/O密集型，8个线程

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

    if (m_isPipelineRunning) {
        qWarning() << "Pipeline is already running";
        return;
    }

    // 初始化流水线状态
    m_componentIds = componentIds;
    m_options = options;
    m_pipelineProgress.totalTasks = componentIds.size();
    m_pipelineProgress.fetchCompleted = 0;
    m_pipelineProgress.processCompleted = 0;
    m_pipelineProgress.writeCompleted = 0;
    m_successCount = 0;
    m_failureCount = 0;
    m_tempSymbolFiles.clear();
    m_isPipelineRunning = true;

    // 创建输出目录
    QDir dir;
    if (!dir.exists(m_options.outputPath)) {
        dir.mkpath(m_options.outputPath);
    }

    // 启动抓取阶段
    startFetchStage();

    // 启动处理和写入阶段
    startProcessStage();
    startWriteStage();

    qDebug() << "Pipeline export started";
}

PipelineProgress ExportServicePipeline::getPipelineProgress() const
{
    QMutexLocker locker(m_mutex);
    return m_pipelineProgress;
}

void ExportServicePipeline::handleFetchCompleted(const ComponentExportStatus &status)
{
    QMutexLocker locker(m_mutex);

    qDebug() << "Fetch completed for component:" << status.componentId
             << "Success:" << status.fetchSuccess;

    m_pipelineProgress.fetchCompleted++;

    if (status.fetchSuccess) {
        // 将数据放入处理队列
        m_fetchProcessQueue->push(status);
    } else {
        // 抓取失败，直接记录失败
        m_failureCount++;
        qDebug() << "Fetch failed for component:" << status.componentId << "Error:" << status.fetchMessage;
    }

    emit pipelineProgressUpdated(m_pipelineProgress);

    // 检查是否完成
    checkPipelineCompletion();
}

void ExportServicePipeline::handleProcessCompleted(const ComponentExportStatus &status)
{
    QMutexLocker locker(m_mutex);

    qDebug() << "Process completed for component:" << status.componentId
             << "Success:" << status.processSuccess;

    m_pipelineProgress.processCompleted++;

    if (status.processSuccess) {
        // 将数据放入写入队列
        m_processWriteQueue->push(status);
    } else {
        // 处理失败，直接记录失败
        m_failureCount++;
        qDebug() << "Process failed for component:" << status.componentId << "Error:" << status.processMessage;
    }

    emit pipelineProgressUpdated(m_pipelineProgress);

    // 检查是否完成
    checkPipelineCompletion();
}

void ExportServicePipeline::handleWriteCompleted(const ComponentExportStatus &status)
{
    QMutexLocker locker(m_mutex);

    qDebug() << "Write completed for component:" << status.componentId
             << "Success:" << status.writeSuccess;

    m_pipelineProgress.writeCompleted++;

    if (status.writeSuccess) {
        m_successCount++;

        // 如果导出了符号，将临时文件加入列表
        if (m_options.exportSymbol && status.symbolData) {
            QString tempFilePath = QString("%1/%2.kicad_sym.tmp").arg(m_options.outputPath, status.componentId);
            if (QFile::exists(tempFilePath)) {
                m_tempSymbolFiles.append(tempFilePath);
            }
        }
    } else {
        m_failureCount++;
        qDebug() << "Write failed for component:" << status.componentId << "Error:" << status.writeMessage;
    }

    emit pipelineProgressUpdated(m_pipelineProgress);

    // 检查是否完成
    checkPipelineCompletion();
}

void ExportServicePipeline::startFetchStage()
{
    qDebug() << "Starting fetch stage for" << m_componentIds.size() << "components";

    for (const QString &componentId : m_componentIds) {
        FetchWorker *worker = new FetchWorker(
            componentId,
            m_networkAccessManager,
            m_options.exportModel3D,
            this);

        connect(worker, &FetchWorker::fetchCompleted,
                this, &ExportServicePipeline::handleFetchCompleted);

        m_fetchThreadPool->start(worker);
    }
}

void ExportServicePipeline::startProcessStage()
{
    qDebug() << "Starting process stage";

    // 创建持续运行的处理工作线程
    for (int i = 0; i < m_processThreadPool->maxThreadCount(); i++) {
        QRunnable *task = QRunnable::create([this]() {
            while (true) {
                ComponentExportStatus status;
                if (!m_fetchProcessQueue->pop(status, 1000)) {
                    // 超时，检查队列是否已关闭
                    if (m_fetchProcessQueue->isClosed()) {
                        break;
                    }
                    continue;
                }

                // 创建ProcessWorker处理数据
                ProcessWorker *worker = new ProcessWorker(status, this);
                connect(worker, &ProcessWorker::processCompleted,
                        this, &ExportServicePipeline::handleProcessCompleted);
                worker->run();
                worker->deleteLater();
            }
        });

        m_processThreadPool->start(task);
    }
}

void ExportServicePipeline::startWriteStage()
{
    qDebug() << "Starting write stage";

    // 创建持续运行的写入工作线程
    for (int i = 0; i < m_writeThreadPool->maxThreadCount(); i++) {
        QRunnable *task = QRunnable::create([this]() {
            while (true) {
                ComponentExportStatus status;
                if (!m_processWriteQueue->pop(status, 1000)) {
                    // 超时，检查队列是否已关闭
                    if (m_processWriteQueue->isClosed()) {
                        break;
                    }
                    continue;
                }

                // 创建WriteWorker写入数据
                WriteWorker *worker = new WriteWorker(
                    status,
                    m_options.outputPath,
                    m_options.libName,
                    m_options.exportSymbol,
                    m_options.exportFootprint,
                    m_options.exportModel3D,
                    this);

                connect(worker, &WriteWorker::writeCompleted,
                        this, &ExportServicePipeline::handleWriteCompleted);
                worker->run();
                worker->deleteLater();
            }
        });

        m_writeThreadPool->start(task);
    }
}

void ExportServicePipeline::checkPipelineCompletion()
{
    if (m_pipelineProgress.fetchCompleted < m_pipelineProgress.totalTasks) {
        return;  // 还在抓取
    }

    if (m_pipelineProgress.processCompleted < m_pipelineProgress.totalTasks) {
        return;  // 还在处理
    }

    if (m_pipelineProgress.writeCompleted < m_pipelineProgress.totalTasks) {
        return;  // 还在写入
    }

    // 所有阶段都完成了
    qDebug() << "Pipeline completed. Success:" << m_successCount << "Failed:" << m_failureCount;

    // 合并符号库
    if (m_options.exportSymbol && !m_tempSymbolFiles.isEmpty()) {
        mergeSymbolLibrary();
    }

    // 清理临时文件
    for (const QString &tempFile : m_tempSymbolFiles) {
        QFile::remove(tempFile);
    }

    // 发送完成信号
    emit exportCompleted(m_pipelineProgress.totalTasks, m_successCount);

    // 清理流水线
    cleanupPipeline();
}

void ExportServicePipeline::cleanupPipeline()
{
    QMutexLocker locker(m_mutex);

    if (!m_isPipelineRunning) {
        return;
    }

    qDebug() << "Cleaning up pipeline";

    // 关闭队列
    m_fetchProcessQueue->close();
    m_processWriteQueue->close();

    // 等待线程池完成
    m_fetchThreadPool->waitForDone();
    m_processThreadPool->waitForDone();
    m_writeThreadPool->waitForDone();

    m_isPipelineRunning = false;
    m_tempSymbolFiles.clear();
}

bool ExportServicePipeline::mergeSymbolLibrary()
{
    qDebug() << "Merging symbol library from" << m_tempSymbolFiles.size() << "temp files";

    QList<SymbolData> symbols;

    // 读取所有临时文件
    for (const QString &tempFile : m_tempSymbolFiles) {
        QFile file(tempFile);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open temp file:" << tempFile;
            continue;
        }

        QTextStream in(&file);
        QString content = in.readAll();
        file.close();

        // 解析符号数据
        QRegularExpression symbolRegex(R"(\(symbol\s+\"([^\"]+)\"\s*(.*?)\s*\))", QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator it = symbolRegex.globalMatch(content);

        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString symbolName = match.captured(1);
            QString symbolContent = match.captured(2);

            // 创建SymbolData（这里简化处理，实际应该完整解析）
            SymbolData symbol;
            SymbolInfo info;
            info.name = symbolName;
            symbol.setInfo(info);

            symbols.append(symbol);
        }
    }

    if (symbols.isEmpty()) {
        qDebug() << "No symbols to merge";
        return true;
    }

    // 导出合并后的符号库
    QString libraryPath = QString("%1/%2.kicad_sym").arg(m_options.outputPath, m_options.libName);
    bool appendMode = !m_options.overwriteExistingFiles;

    // 使用ExporterSymbol直接导出
    ExporterSymbol exporter;
    bool success = exporter.exportSymbolLibrary(symbols, m_options.libName, libraryPath, appendMode);

    if (success) {
        qDebug() << "Symbol library merged successfully:" << libraryPath;
    } else {
        qWarning() << "Failed to merge symbol library:" << libraryPath;
    }

    return success;
}

} // namespace EasyKiConverter