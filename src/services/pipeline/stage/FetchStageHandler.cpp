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

            status->componentData = preData;  // 设置完整的 ComponentData，包括预览图和手册

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

    qDebug() << "FetchStageHandler stopping, active workers:" << m_activeWorkers.size();

    // 1. 标记所有 worker 为中止
    for (FetchWorker* worker : m_activeWorkers) {
        if (worker) {
            worker->abort();
        }
    }

    // 2. 等待所有 worker 完成（最多 200ms）
    QElapsedTimer timer;
    timer.start();

    while (!m_activeWorkers.isEmpty() && timer.elapsed() < 200) {
        QThread::msleep(10);
        locker.unlock();  // 临时解锁允许 worker 完成
        locker.relock();
    }

    // 3. 清空列表
    if (!m_activeWorkers.isEmpty()) {
        qWarning() << "FetchStageHandler: Some workers did not finish in 200ms";
    }
    m_activeWorkers.clear();

    qDebug() << "FetchStageHandler stopped in" << timer.elapsed() << "ms";
}

void FetchStageHandler::onWorkerCompleted(QSharedPointer<ComponentExportStatus> status, FetchWorker* worker) {
    if (worker) {
        QMutexLocker locker(&m_workerMutex);
        m_activeWorkers.remove(worker);
    }

    // 即使取消了也要发出信号，让流水线能够正确完成
    // 外部通过检查 status 中的状态来判断是否真的成功
    emit componentFetchCompleted(status, false);
}

}  // namespace EasyKiConverter
