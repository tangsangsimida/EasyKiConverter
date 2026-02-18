#ifndef EASYKICONVERTER_PIPELINEQUEUEMANAGER_H
#define EASYKICONVERTER_PIPELINEQUEUEMANAGER_H

#include "../../../models/ComponentExportStatus.h"
#include "../../../utils/BoundedThreadSafeQueue.h"

#include <QAtomicInt>
#include <QObject>
#include <QSharedPointer>

namespace EasyKiConverter {

/**
 * @brief 流水线队列管理器
 *
 * 负责管理 Fetch->Process 和 Process->Write 之间的有界并发队列，
 * 并提供线程安全的“安全推送”机制。
 */
class PipelineQueueManager {
public:
    using StatusQueue = BoundedThreadSafeQueue<QSharedPointer<ComponentExportStatus>>;
    using StatusQueuePtr = QSharedPointer<StatusQueue>;

    PipelineQueueManager(size_t queueSize = 64);

    void reset();
    void closeAll();

    StatusQueuePtr fetchProcessQueue() const {
        return m_fetchProcessQueue;
    }

    StatusQueuePtr processWriteQueue() const {
        return m_processWriteQueue;
    }

    /**
     * @brief 安全地将状态推送到队列，包含指数退避和 UI 事件处理
     */
    static bool safePush(StatusQueuePtr queue, QSharedPointer<ComponentExportStatus> status, QAtomicInt& isCancelled);

private:
    size_t m_queueSize;
    StatusQueuePtr m_fetchProcessQueue;
    StatusQueuePtr m_processWriteQueue;
};

}  // namespace EasyKiConverter

#endif  // EASYKICONVERTER_PIPELINEQUEUEMANAGER_H
