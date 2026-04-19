#include "ComponentQueueManager.h"

#include <QDebug>

namespace EasyKiConverter {

ComponentQueueManager::ComponentQueueManager(int maxConcurrent, QObject* parent)
    : QObject(parent)
    , m_maxConcurrentRequests(maxConcurrent)
    , m_checkTimer(new QTimer(this))
    , m_totalTimer(new QTimer(this))
    , m_isRunning(false) {
    m_checkTimer->setSingleShot(false);
    m_checkTimer->setInterval(QUEUE_CHECK_INTERVAL_MS);
    connect(m_checkTimer, &QTimer::timeout, this, &ComponentQueueManager::onCheckTimeout);

    m_totalTimer->setSingleShot(true);
    m_totalTimer->setInterval(TOTAL_TIMEOUT_MS);
    connect(m_totalTimer, &QTimer::timeout, this, &ComponentQueueManager::onTotalTimeout);
}

ComponentQueueManager::~ComponentQueueManager() {
    stop();
}

void ComponentQueueManager::start(const QStringList& componentIds) {
    if (m_isRunning) {
        qWarning() << "ComponentQueueManager: Already running, ignoring new request";
        return;
    }

    qDebug() << "ComponentQueueManager: Starting with" << componentIds.size()
             << "components, max concurrent:" << m_maxConcurrentRequests;

    m_requestQueue = componentIds;
    m_pendingComponents.clear();
    m_isRunning = true;

    m_totalTimer->start();
    m_checkTimer->start();

    checkAndProcessNext();
}

void ComponentQueueManager::stop() {
    if (!m_isRunning) {
        return;
    }

    qDebug() << "ComponentQueueManager: Stopping";

    m_checkTimer->stop();
    m_totalTimer->stop();

    m_requestQueue.clear();
    m_pendingComponents.clear();
    m_isRunning = false;
}

void ComponentQueueManager::setMaxConcurrentRequests(int maxConcurrent) {
    if (maxConcurrent <= 0) {
        return;
    }
    if (m_maxConcurrentRequests != maxConcurrent) {
        m_maxConcurrentRequests = maxConcurrent;
        qDebug() << "ComponentQueueManager: Max concurrent requests set to" << m_maxConcurrentRequests;
    }
}

void ComponentQueueManager::checkAndProcessNext() {
    if (!m_isRunning) {
        m_checkTimer->stop();
        return;
    }

    int activeCount = m_pendingComponents.size();

    // 如果所有槽位都满了，启用定时器作为安全网（防止某些请求完成但信号未触发的情况）
    // 否则禁用定时器（正常情况下 requestCompleted 会直接触发处理）
    if (activeCount >= m_maxConcurrentRequests && !m_requestQueue.isEmpty()) {
        if (!m_checkTimer->isActive()) {
            m_checkTimer->start();
        }
    } else {
        m_checkTimer->stop();
    }

    while (activeCount < m_maxConcurrentRequests && !m_requestQueue.isEmpty()) {
        QString componentId = m_requestQueue.takeFirst();

        if (m_pendingComponents.contains(componentId)) {
            qDebug() << "Component" << componentId << "already pending, skipping";
            continue;
        }

        m_pendingComponents.insert(componentId);
        activeCount++;

        qDebug() << "ComponentQueueManager: Request ready for:" << componentId << "(Active:" << activeCount << "/"
                 << m_maxConcurrentRequests << "Remaining:" << m_requestQueue.size() << ")";

        emit requestReady(componentId);
    }

    if (m_requestQueue.isEmpty() && activeCount == 0) {
        qDebug() << "ComponentQueueManager: All components processed. Queue empty.";
        m_checkTimer->stop();
        emit queueEmpty();
        stop();
    }
}

void ComponentQueueManager::reset() {
    stop();
}

void ComponentQueueManager::requestCompleted(const QString& componentId) {
    if (!m_isRunning) {
        return;
    }

    m_pendingComponents.remove(componentId);
    qDebug() << "ComponentQueueManager: Request completed for:" << componentId
             << "(Remaining active:" << m_pendingComponents.size() << ")";

    checkAndProcessNext();
}

int ComponentQueueManager::activeRequestCount() const {
    return m_pendingComponents.size();
}

bool ComponentQueueManager::isRunning() const {
    return m_isRunning;
}

int ComponentQueueManager::maxConcurrentRequests() const {
    return m_maxConcurrentRequests;
}

void ComponentQueueManager::onCheckTimeout() {
    checkAndProcessNext();
}

void ComponentQueueManager::onTotalTimeout() {
    qWarning() << "ComponentQueueManager: Queue timeout reached after" << TOTAL_TIMEOUT_MS << "ms";
    qWarning() << "ComponentQueueManager: Pending:" << m_pendingComponents.size()
               << "Remaining in queue:" << m_requestQueue.size();

    emit timeout();
    stop();
}

}  // namespace EasyKiConverter
