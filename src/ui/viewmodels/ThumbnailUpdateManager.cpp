#include "ThumbnailUpdateManager.h"

#include <QDebug>

namespace EasyKiConverter {

ThumbnailUpdateManager::ThumbnailUpdateManager(QObject* parent) : QObject(parent), m_timer(new QTimer(this)) {
    m_timer->setSingleShot(true);
    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &ThumbnailUpdateManager::onTimerTimeout);
}

ThumbnailUpdateManager::~ThumbnailUpdateManager() {
    if (m_timer) {
        m_timer->stop();
    }
}

void ThumbnailUpdateManager::scheduleUpdate(const QString& componentId) {
    m_pendingIndices.insert(componentId);
    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

void ThumbnailUpdateManager::scheduleUpdates(const QSet<QString>& componentIds) {
    m_pendingIndices.unite(componentIds);
    if (!m_timer->isActive()) {
        m_timer->start();
    }
}

void ThumbnailUpdateManager::clear() {
    m_pendingIndices.clear();
    if (m_timer->isActive()) {
        m_timer->stop();
    }
}

void ThumbnailUpdateManager::setFlushCallback(std::function<void(const QSet<QString>&)> callback) {
    m_flushCallback = callback;
}

bool ThumbnailUpdateManager::hasPendingUpdates() const {
    return !m_pendingIndices.isEmpty();
}

int ThumbnailUpdateManager::pendingCount() const {
    return m_pendingIndices.size();
}

void ThumbnailUpdateManager::onTimerTimeout() {
    if (m_flushCallback && !m_pendingIndices.isEmpty()) {
        m_flushCallback(m_pendingIndices);
    }
    m_pendingIndices.clear();
}

}  // namespace EasyKiConverter