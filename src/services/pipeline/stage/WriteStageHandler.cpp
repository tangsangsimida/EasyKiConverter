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
                        if (!m_isCancelled.loadAcquire())
                            emit componentWriteCompleted(s);
                    },
                    Qt::QueuedConnection);

                connect(worker, &WriteWorker::writeCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);

                worker->run();
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
