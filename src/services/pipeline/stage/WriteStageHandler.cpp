#include "WriteStageHandler.h"

#include <QDebug>
#include <QRunnable>

namespace EasyKiConverter {

WriteStageHandler::WriteStageHandler(QAtomicInt& isCancelled,
                                     QThreadPool* threadPool,
                                     PipelineQueueManager::StatusQueuePtr inputQueue,
                                     const ExportOptions& options,
                                     const QString& tempDir,
                                     QObject* parent)
    : StageHandler(isCancelled, parent)
    , m_threadPool(threadPool)
    , m_inputQueue(inputQueue)
    , m_options(options)
    , m_tempDir(tempDir) {}

void WriteStageHandler::start() {
    auto inputQueue = m_inputQueue;
    if (!inputQueue)
        return;

    for (int i = 0; i < m_threadPool->maxThreadCount(); i++) {
        m_threadPool->start(QRunnable::create([this, inputQueue]() {
            while (true) {
                if (m_isCancelled.loadAcquire() || !inputQueue)
                    break;

                QSharedPointer<ComponentExportStatus> status;
                if (!inputQueue->pop(status, 1000)) {
                    if (inputQueue->isClosed())
                        break;
                    continue;
                }

                WriteWorker* worker = new WriteWorker(status,
                                                      m_options.outputPath,
                                                      m_options.libName,
                                                      m_options.exportSymbol,
                                                      m_options.exportFootprint,
                                                      m_options.exportModel3D,
                                                      m_options.exportPreviewImages,
                                                      m_options.exportDatasheet,
                                                      m_options.debugMode,
                                                      m_tempDir,
                                                      nullptr);
                worker->moveToThread(this->thread());

                {
                    QMutexLocker locker(&m_workerMutex);
                    m_activeWorkers.insert(worker);
                }

                connect(
                    worker,
                    &WriteWorker::writeCompleted,
                    this,
                    [this, worker](QSharedPointer<ComponentExportStatus> s) {
                        {
                            QMutexLocker locker(&m_workerMutex);
                            m_activeWorkers.remove(worker);
                        }
                        // 即使取消了也要发出信号，让流水线能够正确完成
                        emit componentWriteCompleted(s);
                    },
                    Qt::QueuedConnection);

                // 连接单项导出完成信号，用于实时进度更新
                connect(worker,
                        &WriteWorker::itemWriteCompleted,
                        this,
                        &WriteStageHandler::itemWriteCompleted,
                        Qt::QueuedConnection);

                // 注意：不能使用 deleteLater()，因为 QThreadPool 已经拥有 worker 的所有权
                // QThreadPool::start() 会自动在 run() 完成后删除 worker

                // 使用线程池调度 WriteWorker，而不是直接调用 run()
                // 这样 WriteWorker::run() 会在线程池线程中执行
                m_threadPool->start(worker);
            }
        }));
    }
}

void WriteStageHandler::stop() {
    QMutexLocker locker(&m_workerMutex);
    for (WriteWorker* worker : m_activeWorkers) {
        if (worker)
            worker->abort();
    }
    m_activeWorkers.clear();
}

}  // namespace EasyKiConverter
