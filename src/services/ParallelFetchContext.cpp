#include "ParallelFetchContext.h"

#include "ComponentData.h"

#include <QMutexLocker>

namespace EasyKiConverter {

// 初始化并行获取上下文及其线程安全状态。
ParallelFetchContext::ParallelFetchContext(QObject* parent)
    // 初始化计数器和完成状态。
    : QObject(parent), m_totalCount(0), m_completedCount(0), m_isAllDone(false) {}

ParallelFetchContext::~ParallelFetchContext() = default;

// 开始新一轮批量获取并清理上一轮结果。
void ParallelFetchContext::start(int totalCount) {
    bool shouldEmit = false;
    {
        QMutexLocker locker(&m_mutex);
        m_totalCount = totalCount;
        m_completedCount = 0;
        m_isAllDone = totalCount == 0;
        m_collectedData.clear();
        m_failedComponents.clear();
        shouldEmit = m_isAllDone;
    }
    if (shouldEmit) {
        Q_EMIT allCompleted({});
    }
}

// 记录一个成功结果并检查批量任务是否完成。
void ParallelFetchContext::markCompleted(const QString& componentId, const ComponentData& data) {
    {
        QMutexLocker locker(&m_mutex);
        m_collectedData.insert(componentId, data);
        ++m_completedCount;
    }
    checkCompletion();
}

// 记录一个失败原因并检查批量任务是否完成。
void ParallelFetchContext::markFailed(const QString& componentId, const QString& error) {
    {
        QMutexLocker locker(&m_mutex);
        m_failedComponents.insert(componentId, error);
        ++m_completedCount;
    }
    checkCompletion();
}

// 返回批量获取是否已经发出完成信号。
bool ParallelFetchContext::isAllDone() const {
    QMutexLocker locker(&m_mutex);
    return m_isAllDone;
}

// 返回已经结束的请求数量。
int ParallelFetchContext::completedCount() const {
    QMutexLocker locker(&m_mutex);
    return m_completedCount;
}

// 返回本轮批量获取的请求总数。
int ParallelFetchContext::totalCount() const {
    QMutexLocker locker(&m_mutex);
    return m_totalCount;
}

// 返回当前已成功收集的元件数据。
QList<ComponentData> ParallelFetchContext::collectedData() const {
    QMutexLocker locker(&m_mutex);
    return m_collectedData.values();
}

QMap<QString, QString> ParallelFetchContext::failedComponents() const {
    QMutexLocker locker(&m_mutex);
    return m_failedComponents;
}

// 在所有请求结束后发出一次批量完成信号。
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
