#include "PipelineQueueManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QRandomGenerator>
#include <QThread>

namespace EasyKiConverter {

PipelineQueueManager::PipelineQueueManager(size_t queueSize) : m_queueSize(queueSize) {
    reset();
}

void PipelineQueueManager::reset() {
    m_fetchProcessQueue = StatusQueuePtr::create(m_queueSize);
    m_processWriteQueue = StatusQueuePtr::create(m_queueSize);
}

void PipelineQueueManager::closeAll() {
    if (m_fetchProcessQueue)
        m_fetchProcessQueue->close();
    if (m_processWriteQueue)
        m_processWriteQueue->close();
}

bool PipelineQueueManager::safePush(StatusQueuePtr queue,
                                    QSharedPointer<ComponentExportStatus> status,
                                    QAtomicInt& isCancelled) {
    if (!queue || isCancelled.loadAcquire())
        return false;

    // 1. 快速尝试
    if (queue->tryPush(status))
        return true;

    // 2. 指数退避策略
    constexpr int MAX_RETRIES = 5;
    constexpr int BASE_DELAY_MS = 10;
    constexpr int MAX_TOTAL_TIMEOUT_MS = 500;

    int retryCount = 0;
    int totalDelay = 0;

    while (retryCount < MAX_RETRIES) {
        if (isCancelled.loadAcquire())
            return false;

        // 防止 UI 冻结
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

        if (queue->tryPush(status)) {
            return true;
        }

        int delay = BASE_DELAY_MS * (1 << retryCount);
        delay += QRandomGenerator::global()->bounded(0, 20);  // 抖动

        if (totalDelay + delay > MAX_TOTAL_TIMEOUT_MS) {
            qWarning() << "PipelineQueueManager: Push timed out for" << status->componentId;
            return false;
        }

        QThread::msleep(delay);
        totalDelay += delay;
        retryCount++;
    }

    return false;
}

}  // namespace EasyKiConverter
