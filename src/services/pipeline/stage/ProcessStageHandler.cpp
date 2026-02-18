#include "ProcessStageHandler.h"

#include <QDebug>
#include <QRunnable>

namespace EasyKiConverter {

ProcessStageHandler::ProcessStageHandler(QAtomicInt& isCancelled,
                                         QThreadPool* threadPool,
                                         PipelineQueueManager::StatusQueuePtr inputQueue,
                                         QObject* parent)
    : StageHandler(isCancelled, parent), m_threadPool(threadPool), m_inputQueue(inputQueue) {}

void ProcessStageHandler::start() {
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

                ProcessWorker* worker = new ProcessWorker(status, nullptr);
                worker->moveToThread(this->thread());

                {
                    QMutexLocker locker(&m_workerMutex);
                    m_activeWorkers.insert(worker);
                }

                connect(
                    worker,
                    &ProcessWorker::processCompleted,
                    this,
                    [this, worker](QSharedPointer<ComponentExportStatus> s) {
                        {
                            QMutexLocker locker(&m_workerMutex);
                            m_activeWorkers.remove(worker);
                        }
                        // 即使取消了也要发出信号，让流水线能够正确完成
                        emit componentProcessCompleted(s);
                    },
                    Qt::QueuedConnection);

                connect(worker, &ProcessWorker::processCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);

                worker->run();
            }
        }));
    }
}

void ProcessStageHandler::stop() {
    QMutexLocker locker(&m_workerMutex);
    for (ProcessWorker* worker : m_activeWorkers) {
        if (worker)
            worker->abort();
    }
    m_activeWorkers.clear();
}

}  // namespace EasyKiConverter
