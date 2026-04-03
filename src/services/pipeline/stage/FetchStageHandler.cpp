#include "FetchStageHandler.h"

#include "services/ComponentCacheService.h"

#include <QDebug>

namespace EasyKiConverter {

FetchStageHandler::FetchStageHandler(QAtomicInt& isCancelled, QThreadPool* threadPool, QObject* parent)
    : StageHandler(isCancelled, parent), m_threadPool(threadPool) {}

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
        bool needFetch3DOnly = false;
        QString existing3DUuid;

        if (m_preloadedData.contains(componentId)) {
            auto data = m_preloadedData.value(componentId);
            qDebug() << "FetchStage: Found preloaded data for" << componentId << "isValid:" << (data && data->isValid())
                     << "exportModel3D:" << m_options.exportModel3D;

            if (data && data->isValid()) {
                if (m_options.exportModel3D) {
                    // 需要导出3D模型：检查是否有UUID
                    if (data->model3DData() && !data->model3DData()->uuid().isEmpty()) {
                        // 有UUID，检查缓存中是否有3D模型数据
                        ComponentCacheService* cache = ComponentCacheService::instance();
                        QString uuid = data->model3DData()->uuid();
                        bool hasStepCached = cache->hasModel3DCached(uuid, "step");
                        bool hasWrlCached = cache->hasModel3DCached(uuid, "wrl");
                        // 只有当两个格式都缓存了，才完全使用预加载数据
                        if (hasStepCached && hasWrlCached) {
                            // 3D模型已在缓存中，完全使用预加载数据
                            canUsePreloaded = true;
                        } else {
                            // 有UUID但3D数据不完整，需要下载缺失的格式
                            canUsePreloaded = true;
                            needFetch3DOnly = true;
                            existing3DUuid = uuid;
                        }
                    } else {
                        // 元器件没有3D模型数据，但有符号和封装，使用预加载数据
                        // 3D导出将失败，但不影响符号和封装导出
                        canUsePreloaded = true;
                    }
                } else {
                    // 不需要导出3D模型，使用预加载数据
                    canUsePreloaded = true;
                }
            }
        }

        if (canUsePreloaded) {
            auto preData = m_preloadedData.value(componentId);

            qDebug() << "FetchStage: Using preloaded data for" << componentId
                     << "model3DData valid:" << (preData && preData->model3DData() != nullptr)
                     << "uuid:" << (preData && preData->model3DData() ? preData->model3DData()->uuid() : "null")
                     << "needFetch3DOnly:" << needFetch3DOnly;

            // 如果需要3D模型但缓存中没有，先下载3D模型再合并
            if (needFetch3DOnly && !existing3DUuid.isEmpty()) {
                // 创建基础status（包含symbol和footprint）
                auto baseStatus = QSharedPointer<ComponentExportStatus>::create();
                baseStatus->componentId = componentId;
                baseStatus->componentData = preData;
                baseStatus->need3DModel = m_options.exportModel3D;
                baseStatus->symbolData = preData->symbolData();
                baseStatus->footprintData = preData->footprintData();
                baseStatus->model3DData = preData->model3DData();
                baseStatus->fetchSuccess = false;  // 等待3D下载完成
                baseStatus->fetchMessage = "Downloading 3D model from UUID";

                // 启动3D下载worker
                FetchWorker* worker = new FetchWorker(componentId, false, true, existing3DUuid, nullptr);

                // 使用lambda捕获baseStatus，在3D下载完成后合并数据并发送
                connect(
                    worker,
                    &FetchWorker::fetchCompleted,
                    this,
                    [this, worker, baseStatus, existing3DUuid](QSharedPointer<ComponentExportStatus> downloadedStatus) {
                        // 合并3D数据到baseStatus
                        baseStatus->model3DObjRaw = downloadedStatus->model3DObjRaw;
                        baseStatus->model3DStepRaw = downloadedStatus->model3DStepRaw;
                        baseStatus->fetchSuccess = downloadedStatus->fetchSuccess;
                        baseStatus->fetchMessage = downloadedStatus->fetchSuccess ? "Preloaded + 3D downloaded"
                                                                                  : "Preloaded + 3D download failed";
                        baseStatus->processSuccess = true;
                        baseStatus->processMessage = "Preloaded data used";

                        // 合并网络诊断信息（用于统计）
                        baseStatus->networkDiagnostics = downloadedStatus->networkDiagnostics;

                        // 保存3D模型到缓存
                        if (downloadedStatus->fetchSuccess && !existing3DUuid.isEmpty()) {
                            ComponentCacheService* cache = ComponentCacheService::instance();
                            if (!downloadedStatus->model3DObjRaw.isEmpty()) {
                                cache->saveModel3D(existing3DUuid, downloadedStatus->model3DObjRaw, "wrl");
                                qDebug() << "Saved 3D model (WRL) to cache for UUID:" << existing3DUuid;
                            }
                            if (!downloadedStatus->model3DStepRaw.isEmpty()) {
                                cache->saveModel3D(existing3DUuid, downloadedStatus->model3DStepRaw, "step");
                                qDebug() << "Saved 3D model (STEP) to cache for UUID:" << existing3DUuid;
                            }
                        }

                        qDebug() << "FetchStageHandler: Merged 3D data for" << baseStatus->componentId
                                 << "success:" << baseStatus->fetchSuccess;

                        // 清理worker
                        {
                            QMutexLocker locker(&m_workerMutex);
                            m_activeWorkers.remove(worker);
                        }
                        worker->deleteLater();

                        // 发送合并后的status
                        emit componentFetchCompleted(baseStatus, true);
                    },
                    Qt::QueuedConnection);

                {
                    QMutexLocker locker(&m_workerMutex);
                    m_activeWorkers.insert(worker);
                }

                m_threadPool->start(worker);
                continue;
            }

            // 不需要下载3D模型，直接使用预加载数据
            auto status = QSharedPointer<ComponentExportStatus>::create();
            status->componentId = componentId;
            status->componentData = preData;
            status->need3DModel = m_options.exportModel3D;
            status->symbolData = preData->symbolData();
            status->footprintData = preData->footprintData();
            status->model3DData = preData->model3DData();

            // 转移预览图和手册数据（如果内存中有的话）
            if (!preData->previewImageData().isEmpty()) {
                status->previewImageDataList = preData->previewImageData();
            }
            if (!preData->datasheetData().isEmpty()) {
                status->datasheetData = preData->datasheetData();
            }

            // 如果需要导出3D模型，设置缓存文件路径（WriteWorker将直接拷贝，避免大文件经过内存）
            if (m_options.exportModel3D && preData->model3DData() && !preData->model3DData()->uuid().isEmpty()) {
                ComponentCacheService* cache = ComponentCacheService::instance();
                QString uuid = preData->model3DData()->uuid();

                // WRL文件：直接从缓存拷贝到目标位置（缓存的WRL已经是处理好的可用文件）
                if (cache->hasModel3DCached(uuid, "wrl")) {
                    status->cachedModel3DWrlPath = cache->model3DPath(uuid, "wrl");
                }

                // STEP文件：WriteWorker会直接从缓存拷贝，不需要加载到内存
                if (cache->hasModel3DCached(uuid, "step")) {
                    status->cachedModel3DStepPath = cache->model3DPath(uuid, "step");
                }
            }

            status->fetchSuccess = true;
            status->fetchMessage = "Used preloaded data";
            status->processSuccess = true;
            status->processMessage = "Preloaded data used";

            // 检查并获取预览图/手册
            fetchMediaIfNeeded(componentId, preData, status);
            continue;
        }

        // 2. 启动 Worker

        FetchWorker* worker = new FetchWorker(componentId, m_options.exportModel3D, false, QString(), nullptr);

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

void FetchStageHandler::onMediaFetchCompleted(const QString& componentId,
                                              const QList<QByteArray>& previewImageDataList,
                                              const QByteArray& datasheetData) {
    QMutexLocker locker(&m_workerMutex);

    MediaFetchWorker* worker = qobject_cast<MediaFetchWorker*>(sender());
    if (worker) {
        m_activeMediaWorkers.remove(worker);
        worker->deleteLater();
    }

    auto statusIt = m_pendingMediaStatuses.find(componentId);
    if (statusIt == m_pendingMediaStatuses.end()) {
        return;
    }

    auto status = statusIt.value();

    // 更新媒体数据
    if (!previewImageDataList.isEmpty()) {
        status->previewImageDataList = previewImageDataList;
    }
    if (!datasheetData.isEmpty()) {
        status->datasheetData = datasheetData;
    }

    m_pendingMediaStatuses.erase(statusIt);

    qDebug() << "FetchStageHandler: Media fetch completed for" << componentId
             << "previewImages:" << previewImageDataList.size() << "datasheetSize:" << datasheetData.size();

    // 发送完成信号
    emit componentFetchCompleted(status, true);
}

void FetchStageHandler::fetchMediaIfNeeded(const QString& componentId,
                                           const QSharedPointer<ComponentData>& preData,
                                           QSharedPointer<ComponentExportStatus> status) {
    QStringList previewUrls;
    QString datasheetUrl;

    // 检查是否需要获取预览图
    if (m_options.exportPreviewImages && preData) {
        previewUrls = preData->previewImages();
        if (!previewUrls.isEmpty() && preData->previewImageData().isEmpty()) {
            status->needPreviewImages = true;
        }
    }

    // 检查是否需要获取手册
    if (m_options.exportDatasheet && preData) {
        datasheetUrl = preData->datasheet();
        if (!datasheetUrl.isEmpty() && preData->datasheetData().isEmpty()) {
            status->needDatasheet = true;
        }
    }

    // 如果需要获取媒体，启动 MediaFetchWorker
    if (status->needPreviewImages || status->needDatasheet) {
        qDebug() << "FetchStageHandler: Starting media fetch for" << componentId
                 << "needPreviewImages:" << status->needPreviewImages << "needDatasheet:" << status->needDatasheet;

        // 保存 status 等待媒体下载完成
        m_pendingMediaStatuses[componentId] = status;

        MediaFetchWorker* worker = new MediaFetchWorker(componentId, previewUrls, datasheetUrl, this);

        connect(worker,
                &MediaFetchWorker::fetchCompleted,
                this,
                &FetchStageHandler::onMediaFetchCompleted,
                Qt::QueuedConnection);
        connect(worker, &MediaFetchWorker::fetchCompleted, worker, &QObject::deleteLater, Qt::QueuedConnection);

        m_activeMediaWorkers.insert(worker);
        m_threadPool->start(worker);
    } else {
        // 不需要获取媒体，直接发送完成信号
        emit componentFetchCompleted(status, true);
    }
}

}  // namespace EasyKiConverter
