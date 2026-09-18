#include "ComponentMediaCallbackCoordinator.h"

#include "ComponentCacheService.h"
#include "ComponentQueueManager.h"
#include "ComponentService.h"
#include "PreviewImageDataEncoder.h"

#include <QDebug>
#include <QFutureWatcher>
#include <QImage>
#include <QMutexLocker>
#include <QtConcurrent>

namespace EasyKiConverter {

/** @brief 合并单张预览图数据并转发图像和原始数据事件。 */
void ComponentMediaCallbackCoordinator::handleImageReady(ComponentService& owner,
                                                         const QString& componentId,
                                                         const QByteArray& imageData,
                                                         int imageIndex) {
    QImage image = QImage::fromData(imageData);
    if (image.isNull()) {
        qWarning() << "Failed to load image from data for component:" << componentId << "index:" << imageIndex
                   << "data size:" << imageData.size();
        return;
    }

    const QString normalizedId = componentId.toUpper();
    ComponentData updatedData;
    bool hasValidUpdate = false;
    uint64_t gen = 0;
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        const auto it = owner.m_fetchingComponents.find(normalizedId);
        if (it == owner.m_fetchingComponents.end() ||
            it->cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
            qDebug() << "ComponentService: Discarding stale image callback for" << componentId;
            return;
        }
        it->data.addPreviewImageData(imageData, imageIndex);
        updatedData = it->data;
        gen = it->cacheGeneration;
        hasValidUpdate = true;
    }

    if (hasValidUpdate) {
        emit owner.previewImageReady(normalizedId, image, imageIndex);
        emit owner.previewImageDataReady(normalizedId, imageData, imageIndex);
        if (ComponentCacheService::instance()->currentGeneration() != gen)
            return;
        owner.m_componentCache.replaceIfPresent(normalizedId, updatedData);
    }
}

/** @brief 合并 LCSC 元数据并异步持久化最新组件快照。 */
void ComponentMediaCallbackCoordinator::handleLcscDataReady(ComponentService& owner,
                                                            const QString& componentId,
                                                            const QString& manufacturerPart,
                                                            const QString& datasheetUrl,
                                                            const QStringList& imageUrls) {
    qDebug() << "LCSC data ready for component:" << componentId
             << "Manufacturer Part:" << (manufacturerPart.isEmpty() ? "none" : manufacturerPart)
             << "Datasheet:" << (datasheetUrl.isEmpty() ? "none" : datasheetUrl) << "Images:" << imageUrls.size();

    ComponentData updatedData;
    bool hasValidUpdate = false;
    uint64_t gen = 0;
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        const QString normalizedId = componentId.toUpper();
        if (owner.m_fetchingComponents.contains(normalizedId)) {
            ComponentService::FetchingComponent& fetchingComponent = owner.m_fetchingComponents[normalizedId];
            if (fetchingComponent.cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
                qDebug() << "ComponentService: Discarding stale LCSC data callback for" << componentId;
                return;
            }

            if (!manufacturerPart.isEmpty()) {
                fetchingComponent.data.setManufacturerPart(manufacturerPart);
                qDebug() << "Manufacturer part saved to ComponentData:" << manufacturerPart;
            }

            if (!datasheetUrl.isEmpty()) {
                fetchingComponent.data.setDatasheet(datasheetUrl);
                QString format =
                    datasheetUrl.toLower().contains(".html") ? QStringLiteral("html") : QStringLiteral("pdf");
                fetchingComponent.data.setDatasheetFormat(format);
                qDebug() << "Datasheet saved to ComponentData:" << datasheetUrl << "format:" << format;
            }

            if (!imageUrls.isEmpty()) {
                fetchingComponent.data.setPreviewImages(imageUrls);
                qDebug() << "Preview images saved to ComponentData:" << imageUrls.size() << "images";
                QList<QByteArray> emptyImageDataList;
                emptyImageDataList.resize(imageUrls.size());
                fetchingComponent.data.setPreviewImageData(emptyImageDataList);
                qDebug() << "Pre-allocated" << imageUrls.size() << "empty image data slots";
            }

            updatedData = fetchingComponent.data;
            gen = fetchingComponent.cacheGeneration;
            hasValidUpdate = true;
        } else {
            qWarning() << "Component" << componentId << "not found in m_fetchingComponents, cannot update LCSC data";
        }
    }

    if (hasValidUpdate) {
        if (ComponentCacheService::instance()->currentGeneration() != gen) {
            qDebug() << "ComponentService: Discarding stale LCSC data update for" << componentId;
            return;
        }
        const QString normalizedId = componentId.toUpper();
        emit owner.lcscDataUpdated(normalizedId, manufacturerPart, datasheetUrl, imageUrls);
        ComponentCacheService::instance()->saveComponentMetadataAsync(normalizedId, updatedData, gen);
        owner.updateComponentCache(normalizedId, updatedData);
    }
}

/** @brief 合并数据手册、推进异步下载完成状态并转发结果。 */
void ComponentMediaCallbackCoordinator::handleDatasheetReady(ComponentService& owner,
                                                             const QString& componentId,
                                                             const QByteArray& datasheetData) {
    qDebug() << "Datasheet downloaded for component:" << componentId << "size:" << datasheetData.size() << "bytes";

    QString format;
    bool hasValidUpdate = false;
    bool shouldMarkCompleted = false;
    ComponentData completedData;
    uint64_t gen = 0;
    const QString normalizedId = componentId.toUpper();
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        if (owner.m_fetchingComponents.contains(normalizedId)) {
            ComponentService::FetchingComponent& fetchingComponent = owner.m_fetchingComponents[normalizedId];
            gen = fetchingComponent.cacheGeneration;
            if (gen != ComponentCacheService::instance()->currentGeneration()) {
                qDebug() << "ComponentService: Discarding stale datasheet callback for" << componentId;
                return;
            }
            fetchingComponent.data.setDatasheetData(datasheetData);
            format = fetchingComponent.data.datasheetFormat();
            if (format == "pdf" && !owner.isPDF(datasheetData)) {
                format = "html";
                fetchingComponent.data.setDatasheetFormat(format);
            }
            qDebug() << "Datasheet data saved to ComponentData, size:" << datasheetData.size() << "bytes"
                     << "format:" << format;
            if (fetchingComponent.pendingAsyncDownloads > 0) {
                fetchingComponent.pendingAsyncDownloads--;
                qDebug() << "Datasheet download complete, pending async downloads:"
                         << fetchingComponent.pendingAsyncDownloads;
                if (fetchingComponent.pendingAsyncDownloads == 0) {
                    shouldMarkCompleted = true;
                    completedData = fetchingComponent.data;
                }
            }
            hasValidUpdate = true;
        } else {
            qWarning() << "Component" << componentId
                       << "not found in m_fetchingComponents, datasheet data will be saved directly to cache";
        }
    }

    if (shouldMarkCompleted) {
        if (ComponentCacheService::instance()->currentGeneration() != gen) {
            qDebug() << "ComponentService: Discarding stale datasheet completion for" << componentId;
            return;
        }
        ParallelFetchContext* parallelContext = nullptr;
        {
            QMutexLocker locker(&owner.m_parallelContextMutex);
            parallelContext = owner.m_parallelContext;
        }
        if (parallelContext != nullptr)
            parallelContext->markCompleted(normalizedId, completedData);
        if (owner.m_queueManager != nullptr)
            owner.m_queueManager->requestCompleted(normalizedId);
    }

    if (hasValidUpdate) {
        ComponentData dataCopy;
        bool hasDataCopy = false;
        {
            QMutexLocker fetchLocker(&owner.m_fetchingComponentsMutex);
            if (owner.m_fetchingComponents.contains(normalizedId) &&
                owner.m_fetchingComponents[normalizedId].cacheGeneration == gen) {
                dataCopy = owner.m_fetchingComponents[normalizedId].data;
                hasDataCopy = true;
            }
        }
        if (hasDataCopy && owner.m_componentCache.replaceIfPresent(normalizedId, dataCopy))
            qDebug() << "ComponentService: Updated cache with datasheet data for" << normalizedId;
    } else {
        qDebug() << "ComponentService: Discarding datasheet callback without active request for" << componentId;
    }

    if (hasValidUpdate)
        emit owner.datasheetReady(componentId, datasheetData);
}

/** @brief 校验预览图错误回调代次并转发可用性错误。 */
void ComponentMediaCallbackCoordinator::handlePreviewImageError(ComponentService& owner,
                                                                const QString& componentId,
                                                                const QString& error) {
    const QString normalizedId = componentId.toUpper();
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        const auto it = owner.m_fetchingComponents.find(normalizedId);
        if (it == owner.m_fetchingComponents.end() ||
            it->cacheGeneration != ComponentCacheService::instance()->currentGeneration()) {
            qDebug() << "ComponentService: Discarding stale preview error for" << componentId;
            return;
        }
    }

    if (error == QLatin1String("Image not found") || error == QLatin1String("Preview image URL not found") ||
        error == QLatin1String("No images downloaded") || error == QLatin1String("No preview image URLs available"))
        qDebug() << "Preview image unavailable for component:" << componentId << "error:" << error;
    else
        qWarning() << "Preview image fetch error for component:" << componentId << "error:" << error;

    emit owner.previewImageFailed(normalizedId, error);
}

/** @brief 后台编码全部预览图并合并结果。 */
void ComponentMediaCallbackCoordinator::handleAllImagesReady(ComponentService& owner,
                                                             const QString& componentId,
                                                             const QStringList& imagePaths) {
    qDebug() << "All images ready for component:" << componentId << "paths:" << imagePaths.size();
    uint64_t gen = 0;
    {
        QMutexLocker locker(&owner.m_fetchingComponentsMutex);
        const auto it = owner.m_fetchingComponents.find(componentId.toUpper());
        if (it == owner.m_fetchingComponents.end()) {
            qDebug() << "ComponentService: Discarding all-images callback without active request for" << componentId;
            return;
        }
        gen = it->cacheGeneration;
    }

    QFuture<PreviewImageDataResult> future =
        QtConcurrent::run([imagePaths]() { return PreviewImageDataEncoder::encodeFiles(imagePaths); });
    auto* watcher = new QFutureWatcher<PreviewImageDataResult>(&owner);
    QObject::connect(watcher,
                     &QFutureWatcher<PreviewImageDataResult>::finished,
                     &owner,
                     [&owner, watcher, componentId, imagePaths, gen]() {
                         const PreviewImageDataResult imageResult = watcher->result();
                         watcher->deleteLater();
                         if (ComponentCacheService::instance()->currentGeneration() != gen) {
                             qDebug() << "ComponentService: Discarding stale all-images result for" << componentId;
                             return;
                         }
                         const QString normalizedId = componentId.toUpper();
                         {
                             QMutexLocker locker(&owner.m_fetchingComponentsMutex);
                             const auto it = owner.m_fetchingComponents.find(normalizedId);
                             if (it == owner.m_fetchingComponents.end() || it->cacheGeneration != gen) {
                                 qDebug() << "ComponentService: Discarding all-images result for replaced request"
                                          << componentId;
                                 return;
                             }
                             it->data.setPreviewImageData(imageResult.imageData);
                             qDebug() << "All image data updated in ComponentData for component:" << componentId
                                      << "count:" << imageResult.imageData.size();
                         }
                         emit owner.previewImagesReady(componentId, imageResult.encodedImages);
                         emit owner.allImagesReady(componentId, imagePaths);
                     });
    watcher->setFuture(future);
}

}  // namespace EasyKiConverter
