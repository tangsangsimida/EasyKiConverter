#include "ParallelFetchContext.h"

#include "ComponentData.h"

#include <QMutexLocker>

namespace EasyKiConverter {

ParallelFetchContext::ParallelFetchContext(QObject* parent)
    : QObject(parent), m_totalCount(0), m_completedCount(0), m_isAllDone(false) {}

ParallelFetchContext::~ParallelFetchContext() = default;

void ParallelFetchContext::start(int totalCount) {
    QMutexLocker locker(&m_mutex);
    m_totalCount = totalCount;
    m_completedCount = 0;
    m_isAllDone = false;
    m_collectedData.clear();
}

void ParallelFetchContext::markCompleted(const QString& componentId, const ComponentData& data) {
    {
        QMutexLocker locker(&m_mutex);
        m_collectedData.insert(componentId, data);
        ++m_completedCount;
    }
    checkCompletion();
}

void ParallelFetchContext::markFailed(const QString& componentId) {
    Q_UNUSED(componentId);
    {
        QMutexLocker locker(&m_mutex);
        ++m_completedCount;
    }
    checkCompletion();
}

bool ParallelFetchContext::isAllDone() const {
    QMutexLocker locker(&m_mutex);
    return m_isAllDone;
}

int ParallelFetchContext::completedCount() const {
    QMutexLocker locker(&m_mutex);
    return m_completedCount;
}

int ParallelFetchContext::totalCount() const {
    QMutexLocker locker(&m_mutex);
    return m_totalCount;
}

QList<ComponentData> ParallelFetchContext::collectedData() const {
    QMutexLocker locker(&m_mutex);
    return m_collectedData.values();
}

void ParallelFetchContext::checkCompletion() {
    QList<ComponentData> dataToEmit;
    bool shouldEmit = false;

    {
        QMutexLocker locker(&m_mutex);
        if (m_completedCount >= m_totalCount && !m_isAllDone) {
            m_isAllDone = true;
            shouldEmit = true;
            if (m_totalCount > 0) {
                dataToEmit = m_collectedData.values();
            }
        }
    }

    if (shouldEmit) {
        Q_EMIT allCompleted(dataToEmit);
    }
}

}  // namespace EasyKiConverter
