#include "FetchStageHandler.h"

#include <QDebug>

namespace EasyKiConverter {

FetchStageHandler::FetchStageHandler(QAtomicInt& isCancelled,
                                     QNetworkAccessManager* networkManager,
                                     QThreadPool* threadPool,
                                     QObject* parent)
    : StageHandler(isCancelled, parent), m_networkManager(networkManager), m_threadPool(threadPool) {}

void FetchStageHandler::start() {
    for (const QString& componentId : m_componentIds) {
        if (m_isCancelled.loadAcquire()) {
            qDebug() << "FetchStage: Cancelled early for" << componentId;

            auto status = QSharedPointer<ComponentExportStatus>::create();

            status->componentId = componentId;

            status->fetchSuccess = false;

            status->fetchMessage = "Export cancelled";

            emit componentFetchCompleted(status, false);

            continue;
        }

        // 1. 检查预加载数据

        bool canUsePreloaded = false;

        if (m_preloadedData.contains(componentId)) {
            auto data = m_preloadedData.value(componentId);

            if (data && data->isValid()) {
                if (m_options.exportModel3D) {
                    if (data->model3DData() && !data->model3DData()->uuid().isEmpty()) {
                        canUsePreloaded = true;
                    }

                } else {
                    canUsePreloaded = true;
                }
            }
        }

        if (canUsePreloaded) {
            auto preData = m_preloadedData.value(componentId);

            auto status = QSharedPointer<ComponentExportStatus>::create();

            status->componentId = componentId;

            status->need3DModel = m_options.exportModel3D;

            status->symbolData = preData->symbolData();

            status->footprintData = preData->footprintData();

            status->model3DData = preData->model3DData();

            status->fetchSuccess = true;

            status->fetchMessage = "Used preloaded data";

            status->processSuccess = true;

            status->processMessage = "Preloaded data used";

            emit componentFetchCompleted(status, true);

            continue;
        }

        // 2. 启动 Worker

        FetchWorker* worker =
            new FetchWorker(componentId, m_networkManager, m_options.exportModel3D, false, QString(), nullptr);

        worker->moveToThread(this->thread());

        connect(
            worker,
            &FetchWorker::fetchCompleted,
            this,
            [this, worker](QSharedPointer<ComponentExportStatus> status) { onWorkerCompleted(status, worker); },
            Qt::QueuedConnection);

        connect(worker, &FetchWorker::fetchCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);

        {
            QMutexLocker locker(&m_workerMutex);

            m_activeWorkers.insert(worker);
        }

        m_threadPool->start(worker);
    }
}

void FetchStageHandler::stop() {
    QMutexLocker locker(&m_workerMutex);
    for (FetchWorker* worker : m_activeWorkers) {
        if (worker)
            worker->abort();
    }
    m_activeWorkers.clear();
}

void FetchStageHandler::onWorkerCompleted(QSharedPointer<ComponentExportStatus> status, FetchWorker* worker) {
    if (worker) {
        QMutexLocker locker(&m_workerMutex);
        m_activeWorkers.remove(worker);
    }

    if (m_isCancelled.loadAcquire())
        return;

    emit componentFetchCompleted(status, false);
}

}  // namespace EasyKiConverter
