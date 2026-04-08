#include "ExportTypeStage.h"

#include "IExportWorker.h"

#include <QDebug>
#include <QMutexLocker>

namespace EasyKiConverter {

ExportTypeStage::ExportTypeStage(const QString& typeName, int maxConcurrent, QObject* parent)
    : QObject(parent), m_typeName(typeName) {
    m_threadPool.setMaxThreadCount(maxConcurrent);
    m_threadPool.setExpiryTimeout(60000);
    m_progress.typeName = typeName;
}

ExportTypeStage::~ExportTypeStage() {
    cancel();
    m_threadPool.waitForDone(5000);
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

    qDebug() << "ExportTypeStage:" << m_typeName << "starting for" << componentIds.size() << "components";

    for (const QString& componentId : componentIds) {
        if (m_cancelled.load()) {
            qDebug() << "ExportTypeStage:" << m_typeName << "cancelled";
            break;
        }

        initItemProgress(componentId);

        QSharedPointer<ComponentData> data = m_cachedData.value(componentId);

        QObject* worker = createWorker();
        {
            QMutexLocker locker(&m_workerMutex);
            m_activeWorkers.append(worker);
        }

        startWorker(worker, componentId, data);
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "all workers started";
}

void ExportTypeStage::cancel() {
    if (!m_isRunning.load()) {
        return;
    }

    qDebug() << "ExportTypeStage:" << m_typeName << "cancelling...";
    m_cancelled.store(true);

    QMutexLocker locker(&m_workerMutex);
    for (QObject* worker : m_activeWorkers) {
        if (auto* exportWorker = dynamic_cast<IExportWorker*>(worker)) {
            exportWorker->cancel();
        }
    }
    locker.unlock();

    m_threadPool.waitForDone(3000);
    m_isRunning.store(false);
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
}

void ExportTypeStage::completeItemProgress(const QString& componentId, bool success, const QString& error) {
    QMutexLocker locker(&m_progressMutex);

    auto it = m_progress.itemStatus.find(componentId);
    if (it == m_progress.itemStatus.end()) {
        return;
    }

    ExportItemStatus& status = it.value();
    status.status = success ? ExportItemStatus::Status::Success : ExportItemStatus::Status::Failed;
    status.endTime = QDateTime::currentDateTime();
    if (!success) {
        status.errorMessage = error;
    }

    m_progress.completedCount++;
    if (success) {
        m_progress.successCount++;
    } else {
        m_progress.failedCount++;
    }
    m_progress.inProgressCount--;

    if (m_progress.completedCount >= m_progress.totalCount) {
        locker.unlock();
        qDebug() << "ExportTypeStage:" << m_typeName << "completed. Success:" << m_progress.successCount
                 << "Failed:" << m_progress.failedCount << "Skipped:" << m_progress.skippedCount;
        emit completed(m_progress.successCount, m_progress.failedCount, m_progress.skippedCount);
        m_isRunning.store(false);
    } else {
        locker.unlock();
        emit itemStatusChanged(componentId, status);
        emit progressChanged(m_progress);
    }
}

}  // namespace EasyKiConverter
