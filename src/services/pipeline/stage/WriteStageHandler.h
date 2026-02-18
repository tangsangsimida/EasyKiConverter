#ifndef EASYKICONVERTER_WRITESTAGEHANDLER_H
#define EASYKICONVERTER_WRITESTAGEHANDLER_H

#include "../../../workers/WriteWorker.h"
#include "../../ExportService.h"  // ExportOptions 定义在此文件中
#include "../queue/PipelineQueueManager.h"
#include "StageHandler.h"

#include <QSet>
#include <QThreadPool>

namespace EasyKiConverter {

class WriteStageHandler : public StageHandler {
    Q_OBJECT
public:
    WriteStageHandler(QAtomicInt& isCancelled,
                      QThreadPool* threadPool,
                      PipelineQueueManager::StatusQueuePtr inputQueue,
                      const ExportOptions& options,
                      const QString& tempDir,
                      QObject* parent = nullptr);

    void start() override;
    void stop() override;

    void setInputQueue(PipelineQueueManager::StatusQueuePtr queue) {
        m_inputQueue = queue;
    }

    void setOptions(const ExportOptions& options) {
        m_options = options;
    }

    void setTempDir(const QString& tempDir) {
        m_tempDir = tempDir;
    }

signals:
    void componentWriteCompleted(QSharedPointer<ComponentExportStatus> status);

private:
    QThreadPool* m_threadPool;
    PipelineQueueManager::StatusQueuePtr m_inputQueue;
    ExportOptions m_options;
    QString m_tempDir;
    QSet<WriteWorker*> m_activeWorkers;
    QMutex m_workerMutex;
};

}  // namespace EasyKiConverter

#endif  // EASYKICONVERTER_WRITESTAGEHANDLER_H
