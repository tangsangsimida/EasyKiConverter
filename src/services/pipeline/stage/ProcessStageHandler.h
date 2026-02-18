#ifndef EASYKICONVERTER_PROCESSSTAGEHANDLER_H
#define EASYKICONVERTER_PROCESSSTAGEHANDLER_H

#include "../../../models/ComponentExportStatus.h"
#include "../../../workers/ProcessWorker.h"
#include "../queue/PipelineQueueManager.h"
#include "StageHandler.h"

#include <QSet>
#include <QThreadPool>

namespace EasyKiConverter {

class ProcessStageHandler : public StageHandler {
    Q_OBJECT
public:
    ProcessStageHandler(QAtomicInt& isCancelled,
                        QThreadPool* threadPool,
                        PipelineQueueManager::StatusQueuePtr inputQueue,
                        QObject* parent = nullptr);

    void start() override;
    void stop() override;

    void setInputQueue(PipelineQueueManager::StatusQueuePtr queue) {
        m_inputQueue = queue;
    }

signals:
    void componentProcessCompleted(QSharedPointer<ComponentExportStatus> status);

private:
    QThreadPool* m_threadPool;
    PipelineQueueManager::StatusQueuePtr m_inputQueue;
    QSet<ProcessWorker*> m_activeWorkers;
    QMutex m_workerMutex;
};

}  // namespace EasyKiConverter

#endif  // EASYKICONVERTER_PROCESSSTAGEHANDLER_H
