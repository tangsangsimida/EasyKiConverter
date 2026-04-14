#include "ExportTypeStage.h"

#include "IExportWorker.h"

#include <QDebug>
#include <QMutexLocker>
#include <QPointer>

namespace EasyKiConverter {

ExportTypeStage::ExportTypeStage(const QString& typeName, int maxConcurrent, QObject* parent)
    : QObject(parent), m_typeName(typeName) {
    m_threadPool.setMaxThreadCount(maxConcurrent);
    m_threadPool.setExpiryTimeout(60000);
    m_progress.typeName = typeName;
}

ExportTypeStage::~ExportTypeStage() {
    // Don't call cancel() or waitForDone() in destructor
    // as it can cause crashes during application shutdown

    // Clear the active workers list to prevent accessing deleted objects
    {
        QMutexLocker locker(&m_workerMutex);
        m_activeWorkers.clear();
    }
}

void ExportTypeStage::start(const QStringList& componentIds,
                            const QMap<QString, QSharedPointer<ComponentData>>& cachedData) {
    if (m_isRunning.load()) {
        qWarning() << "ExportTypeStage:" << m_typeName << "already running";
        return;
    }

    m_componentIds = componentIds;
    m_cachedData = cachedData;
    m_cancelled.store(false);
    m_isRunning.store(true);

    {
        QMutexLocker locker(&m_progressMutex);
        m_progress = ExportTypeProgress();
        m_progress.typeName = m_typeName;
        m_progress.totalCount = componentIds.size();
        m_progress.completedCount = 0;
        m_progress.successCount = 0;
        m_progress.failedCount = 0;
        m_progress.skippedCount = 0;
        m_progress.inProgressCount = 0;
    }

    // 清空待处理队列和追踪集合
    {
        QMutexLocker locker(&m_workerMutex);
        m_pendingComponents.clear();
        m_activeWorkers.clear();
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "starting for" << componentIds.size() << "components";

    // 如果没有组件需要处理，立即发送完成信号
    if (componentIds.isEmpty()) {
        qDebug() << "ExportTypeStage:" << m_typeName << "no components to process, completing immediately";
        m_isRunning.store(false);
        emit completed(0, 0, 0);
        return;
    }

    // 将所有组件加入待处理队列
    for (const QString& componentId : componentIds) {
        initItemProgress(componentId);
        m_pendingComponents.enqueue(componentId);
    }

    // 启动初始批次：不超过 maxConcurrent 个 worker
    int initialBatch = qMin(componentIds.size(), m_threadPool.maxThreadCount());
    for (int i = 0; i < initialBatch; ++i) {
        if (m_cancelled.load()) {
            break;
        }
        startNextWorker();
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "initial batch started, pending:" << m_pendingComponents.size();
}

void ExportTypeStage::startNextWorker() {
    if (m_cancelled.load()) {
        return;
    }

    QString componentId;
    {
        QMutexLocker locker(&m_workerMutex);
        if (m_pendingComponents.isEmpty()) {
            return;
        }
        componentId = m_pendingComponents.dequeue();
    }

    QSharedPointer<ComponentData> data = m_cachedData.value(componentId);
    QObject* worker = createWorker();

    {
        QMutexLocker locker(&m_workerMutex);
        m_activeWorkers.insert(worker);  // 使用 QSet 而非 QList，O(1) 插入和删除
    }

    ExportItemStatus statusSnapshot;
    ExportTypeProgress progressSnapshot;
    {
        QMutexLocker locker(&m_progressMutex);
        auto it = m_progress.itemStatus.find(componentId);
        if (it != m_progress.itemStatus.end()) {
            it->status = ExportItemStatus::Status::InProgress;
            it->startTime = QDateTime::currentDateTime();
            statusSnapshot = it.value();
        }
        m_progress.inProgressCount++;
        progressSnapshot = m_progress;
    }

    qInfo() << "ExportTypeStage::startNextWorker: Emitting itemStatusChanged for" << componentId
            << "status:" << (int)statusSnapshot.status;
    emit itemStatusChanged(componentId, statusSnapshot);
    emit progressChanged(progressSnapshot);

    startWorker(worker, componentId, data);
}

void ExportTypeStage::cancel() {
    if (!m_isRunning.load()) {
        return;
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "cancelling...";
    m_cancelled.store(true);

    // 取消所有活跃worker（在锁内完成以避免与 completeItemProgress 竞争）
    // 使用 QPointer 保护 worker 指针，防止在 cancel 过程中 worker 被删除导致悬空指针
    {
        QMutexLocker locker(&m_workerMutex);
        for (QObject* worker : m_activeWorkers) {
            QPointer<QObject> workerPtr(worker);  // 使用 QPointer 保护
            if (!workerPtr) {
                continue;  // worker 已删除，跳过
            }
            if (auto* exportWorker = dynamic_cast<IExportWorker*>(workerPtr.data())) {
                exportWorker->cancel();
            }
        }
        // 清空待处理队列
        m_pendingComponents.clear();

        // 在锁内设置 m_isRunning = false，避免与 completeItemProgress 竞争
        // 注意：completeItemProgress 也可能在所有 worker 被取消后到达 completedCount >= totalCount
        // 的状态并设置 m_isRunning = false，这是安全的（原子变量的重复写入是无害的）
        m_isRunning.store(false);
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "cancelled";
}

ExportTypeProgress ExportTypeStage::getProgress() const {
    QMutexLocker locker(&m_progressMutex);
    return m_progress;
}

bool ExportTypeStage::isRunning() const {
    return m_isRunning.load();
}

void ExportTypeStage::initItemProgress(const QString& componentId) {
    QMutexLocker locker(&m_progressMutex);
    ExportItemStatus status;
    status.status = ExportItemStatus::Status::Pending;
    m_progress.itemStatus[componentId] = status;
    qInfo() << "ExportTypeStage::initItemProgress:" << m_typeName << "componentId:" << componentId
            << "total items now:" << m_progress.itemStatus.size();
}

void ExportTypeStage::completeItemProgress(QObject* worker,
                                           const QString& componentId,
                                           bool success,
                                           const QString& error) {
    // 1. 从活跃worker集合移除
    bool shouldStartNext = false;
    {
        QMutexLocker workerLocker(&m_workerMutex);
        if (worker) {
            m_activeWorkers.remove(worker);
        }
        // 检查是否还有待处理的worker
        shouldStartNext = !m_pendingComponents.isEmpty();
    }

    // 2. 更新进度
    {
        QMutexLocker locker(&m_progressMutex);

        auto it = m_progress.itemStatus.find(componentId);
        if (it == m_progress.itemStatus.end()) {
            qInfo() << "ExportTypeStage::completeItemProgress: componentId not found:" << componentId;
            return;
        }

        ExportItemStatus& status = it.value();
        status.status = success ? ExportItemStatus::Status::Success : ExportItemStatus::Status::Failed;
        status.endTime = QDateTime::currentDateTime();
        if (!success) {
            status.errorMessage = error;
        }
        const ExportItemStatus statusSnapshot = status;

        m_progress.completedCount++;
        if (success) {
            m_progress.successCount++;
        } else {
            m_progress.failedCount++;
        }
        m_progress.inProgressCount--;
        const ExportTypeProgress progressSnapshot = m_progress;

        if (m_progress.completedCount >= m_progress.totalCount) {
            locker.unlock();
            qInfo() << "ExportTypeStage::completeItemProgress: Emitting itemStatusChanged for" << componentId
                    << "status:" << (int)statusSnapshot.status;
            emit itemStatusChanged(componentId, statusSnapshot);
            emit progressChanged(progressSnapshot);
            qDebug() << "ExportTypeStage:" << m_typeName << "completed. Success:" << progressSnapshot.successCount
                     << "Failed:" << progressSnapshot.failedCount << "Skipped:" << progressSnapshot.skippedCount;
            emit completed(progressSnapshot.successCount, progressSnapshot.failedCount, progressSnapshot.skippedCount);
            m_isRunning.store(false);
            return;
        } else {
            locker.unlock();
            emit itemStatusChanged(componentId, statusSnapshot);
            emit progressChanged(progressSnapshot);
        }
    }

    // 3. 启动下一个待处理的worker（按需创建）
    if (shouldStartNext && !m_cancelled.load()) {
        startNextWorker();
    }
}

}  // namespace EasyKiConverter
