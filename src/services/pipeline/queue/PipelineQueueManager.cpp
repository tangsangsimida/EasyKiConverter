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
    m_mediaFetchQueue = StatusQueuePtr::create(m_queueSize);
}

void PipelineQueueManager::closeAll() {
    if (m_fetchProcessQueue)
        m_fetchProcessQueue->close();
    if (m_processWriteQueue)
        m_processWriteQueue->close();
    if (m_mediaFetchQueue)
        m_mediaFetchQueue->close();
}

bool PipelineQueueManager::safePush(StatusQueuePtr queue,
                                    QSharedPointer<ComponentExportStatus> status,
                                    QAtomicInt& isCancelled) {
    if (!queue || isCancelled.loadAcquire())
        return false;

    // 1. 快速尝试（非阻塞）
    if (queue->tryPush(status))
        return true;

    // 2. 使用简单的重试策略（移除指数退避和抖动）
    constexpr int MAX_RETRIES = 3;
    constexpr int RETRY_DELAY_MS = 10;

    int retryCount = 0;

    while (retryCount < MAX_RETRIES) {
        if (isCancelled.loadAcquire())
            return false;

        // 短暂等待
        QThread::msleep(RETRY_DELAY_MS);

        if (queue->tryPush(status)) {
            return true;
        }

        retryCount++;
    }

    // 3. 超时返回
    qWarning() << "PipelineQueueManager: Push timed out for" << status->componentId;
    return false;
}

}  // namespace EasyKiConverter
