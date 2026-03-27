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

void ComponentQueueManager::checkAndProcessNext() {
    if (!m_isRunning) {
        m_checkTimer->stop();
        return;
    }

    int activeCount = m_pendingComponents.size();

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
        emit queueEmpty();
        stop();
    }
}

void ComponentQueueManager::reset() {
    stop();
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