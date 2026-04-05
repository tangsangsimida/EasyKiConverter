#include "ComponentListViewModel.h"

#include "ui/viewmodels/ValidationStateManager.h"

#include <QBuffer>
#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

PreviewImageEncodeRunnable::PreviewImageEncodeRunnable(ComponentListItemData* item,
                                                       const QList<QImage>& images,
                                                       std::function<void(const QString&, const QStringList&)> callback)
    : m_item(item), m_images(images), m_callback(callback) {}

void PreviewImageEncodeRunnable::run() {
    QStringList encodedList;
    for (const QImage& img : m_images) {
        if (img.isNull()) {
            encodedList.append(QString());
            continue;
        }
        QByteArray byteArray;
        QBuffer buffer(&byteArray);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "PNG");
        encodedList.append(QString::fromLatin1(byteArray.toBase64().data()));
    }

    QString componentId = m_item ? m_item->componentId() : QString();
    if (m_callback) {
        m_callback(componentId, encodedList);
    }
}

ComponentListViewModel::ComponentListViewModel(ComponentService* service, QObject* parent)
    : QAbstractListModel(parent), m_service(service), m_componentList(), m_bomFilePath(), m_bomResult() {
    m_validationStateManager = new ValidationStateManager(this);

    m_previewImageUpdateTimer = new QTimer(this);
    m_previewImageUpdateTimer->setSingleShot(true);
    m_previewImageUpdateTimer->setInterval(100);
    connect(m_previewImageUpdateTimer, &QTimer::timeout, this, &ComponentListViewModel::batchUpdatePreviewImages);

    m_encodingThreadPool = new QThreadPool(this);
    m_encodingThreadPool->setMaxThreadCount(10);

    m_batchAddTimer = new QTimer(this);
    m_batchAddTimer->setSingleShot(false);
    m_batchAddTimer->setInterval(50);
    connect(m_batchAddTimer, &QTimer::timeout, this, &ComponentListViewModel::processNextBatchAdd);

    m_batchUpdateTimer = new QTimer(this);
    m_batchUpdateTimer->setSingleShot(true);
    m_batchUpdateTimer->setInterval(100);
    connect(m_batchUpdateTimer, &QTimer::timeout, this, [this]() {
        m_batchUpdateMode = false;
        for (const QPointer<ComponentListItemData>& item : m_batchUpdateItems) {
            if (item) {
                emit item->dataChanged();
            }
        }
        m_batchUpdateItems.clear();
    });

    connect(m_service, &ComponentService::componentInfoReady, this, &ComponentListViewModel::handleComponentInfoReady);
    connect(m_service, &ComponentService::cadDataReady, this, &ComponentListViewModel::handleCadDataReady);
    connect(m_service, &ComponentService::model3DReady, this, &ComponentListViewModel::handleModel3DReady);
    connect(m_service, &ComponentService::lcscDataUpdated, this, &ComponentListViewModel::handleLcscDataUpdated);
    connect(m_service, &ComponentService::datasheetReady, this, &ComponentListViewModel::handleDatasheetReady);
    connect(m_service, &ComponentService::fetchError, this, &ComponentListViewModel::handleFetchError);
    connect(m_service,
            &ComponentService::previewImageReady,
            this,
            [this](const QString& componentId, const QImage& image, int imageIndex) {
                auto item = findItemData(componentId);
                if (item) {
                    item->insertPreviewImageSilent(image, imageIndex);
                    qDebug() << "[ViewModel] previewImageReady - component:" << componentId << "index:" << imageIndex
                             << "image valid:" << !image.isNull();
                }
            });
    connect(m_service,
            &ComponentService::previewImageFailed,
            this,
            [this](const QString& componentId, const QString& error) {
                qDebug() << "Preview image fetch failed for component:" << componentId << "error:" << error;
            });
    connect(m_service, &ComponentService::allImagesReady, this, &ComponentListViewModel::handleAllImagesReady);
}

ComponentListViewModel::~ComponentListViewModel() {
    qDeleteAll(m_componentList);
    m_componentList.clear();
}

int ComponentListViewModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    return m_componentList.count();
}

QVariant ComponentListViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_componentList.count())
        return QVariant();

    if (role == ItemDataRole) {
        return QVariant::fromValue(m_componentList.at(index.row()));
    }

    return QVariant();
}

QHash<int, QByteArray> ComponentListViewModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[ItemDataRole] = "itemData";
    return roles;
}

void ComponentListViewModel::addComponent(const QString& componentId) {
    QString trimmedId = componentId.trimmed();

    if (trimmedId.isEmpty()) {
        qWarning() << "Component ID is empty";
        return;
    }

    // 统一转换为大写，支持用户输入小写 c
    trimmedId = trimmedId.toUpper();

    // 验证元件ID格式
    if (!validateComponentId(trimmedId)) {
        qWarning() << "Invalid component ID format:" << trimmedId;

        // 尝试从文本中智能提取元件编号
        QStringList extractedIds = extractComponentIdFromText(trimmedId);
        if (extractedIds.isEmpty()) {
            qWarning() << "Failed to extract component ID from text:" << trimmedId;
            emit componentAdded(trimmedId, false, "Invalid LCSC component ID format");
            return;
        }

        addComponentsBatch(extractedIds);
        return;
    }

    // 检查元件是否已存在
    if (componentExists(trimmedId)) {
        qWarning() << "Component already exists:" << trimmedId;
        emit componentAdded(trimmedId, false, "Component already exists");
        return;
    }

    beginInsertRows(QModelIndex(), m_componentList.count(), m_componentList.count());
    auto item = new ComponentListItemData(trimmedId, this);
    item->setFetching(true);
    item->setValid(false);
    m_componentList.append(item);
    m_componentIdIndex.insert(trimmedId, m_componentList.count() - 1);
    endInsertRows();

    m_service->fetchComponentData(trimmedId, false);

    m_validationStateManager->startValidation(1);

    emit componentCountChanged();
    emit filteredCountChanged();
    emit componentAdded(trimmedId, true, "Component added");
}

void ComponentListViewModel::removeComponent(int index) {
    if (index >= 0 && index < m_componentList.count()) {
        beginRemoveRows(QModelIndex(), index, index);
        auto item = m_componentList.takeAt(index);
        QString removedId = item->componentId();
        m_componentIdIndex.remove(removedId);
        m_validatedComponentIds.removeAll(removedId);
        if (item->isFetching()) {
            m_pendingValidationCount--;
        }

        delete item;
        endRemoveRows();

        rebuildComponentIdIndex();
        updateHasInvalidComponents();

        emit componentCountChanged();
        emit filteredCountChanged();
        emit componentRemoved(removedId);
    }
}

void ComponentListViewModel::removeComponentById(const QString& componentId) {
    for (int i = 0; i < m_componentList.count(); ++i) {
        if (m_componentList.at(i)->componentId() == componentId) {
            removeComponent(i);
            return;
        }
    }
}

void ComponentListViewModel::clearComponentList() {
    if (!m_componentList.isEmpty()) {
        if (m_service) {
            m_service->clearCache();
        }

        beginResetModel();
        qDeleteAll(m_componentList);
        m_componentList.clear();
        m_componentIdIndex.clear();
        endResetModel();

        m_validationStateManager->reset();
        updateHasInvalidComponents();

        emit componentCountChanged();
        emit filteredCountChanged();
        emit listCleared();
    }
}

void ComponentListViewModel::rebuildComponentIdIndex() {
    m_componentIdIndex.clear();
    for (int i = 0; i < m_componentList.count(); ++i) {
        QString id = m_componentList.at(i)->componentId();
        m_componentIdIndex.insert(id, i);
    }
}

void ComponentListViewModel::addComponentsBatch(const QStringList& componentIds) {
    QStringList newIds;
    for (const QString& rawId : componentIds) {
        QString id = rawId.trimmed().toUpper();
        if (id.isEmpty())
            continue;

        if (!validateComponentId(id)) {
            QStringList extracted = extractComponentIdFromText(id);
            for (const QString& extractedId : extracted) {
                if (!componentExists(extractedId) && !newIds.contains(extractedId)) {
                    newIds.append(extractedId);
                }
            }
            continue;
        }

        if (!componentExists(id) && !newIds.contains(id)) {
            newIds.append(id);
        }
    }

    if (newIds.isEmpty())
        return;

    m_pendingComponentIds.append(newIds);

    if (!m_batchAddTimer->isActive()) {
        m_batchAddTimer->start();
    }
}

void ComponentListViewModel::processNextBatchAdd() {
    if (m_pendingComponentIds.isEmpty()) {
        m_batchAddTimer->stop();
        return;
    }

    QStringList batch;
    int count = qMin(BATCH_ADD_SIZE, m_pendingComponentIds.size());
    for (int i = 0; i < count; ++i) {
        batch.append(m_pendingComponentIds.takeFirst());
    }

    int pendingCount = batch.count();
    int startIndex = m_componentList.count();

    beginInsertRows(QModelIndex(), startIndex, startIndex + pendingCount - 1);
    for (const QString& id : batch) {
        auto item = new ComponentListItemData(id, this);
        item->setFetching(true);
        item->setValid(false);
        m_componentList.append(item);
        m_componentIdIndex.insert(id, m_componentList.count() - 1);
    }
    endInsertRows();

    emit componentCountChanged();
    emit filteredCountChanged();

    if (m_validationPendingCount == 0 && !m_batchAddTimer->isActive()) {
        m_validationStateManager->startValidation(pendingCount);
        startValidationQueue();
    }

    if (m_pendingComponentIds.isEmpty()) {
        m_batchAddTimer->stop();
        m_validationStateManager->startValidation(pendingCount);
        startValidationQueue();
    }
}

void ComponentListViewModel::startValidationQueue() {
    const int CONCURRENT_WORKERS = 5;

    m_validationQueue.clear();
    for (auto item : m_componentList) {
        if (item->isFetching()) {
            m_validationQueue.append(item->componentId());
        }
    }
    m_validationPendingCount = 0;
    m_validationCompletedCount = 0;
    m_validationTotalCount = m_validationQueue.count();

    if (m_validationQueue.isEmpty()) {
        return;
    }

    int initialCount = qMin(CONCURRENT_WORKERS, m_validationQueue.count());
    for (int i = 0; i < initialCount; ++i) {
        QString componentId = m_validationQueue.takeFirst();
        m_service->fetchComponentData(componentId, false);
        m_validationPendingCount++;
    }
}

void ComponentListViewModel::processNextValidation() {
    if (m_validationQueue.isEmpty()) {
        return;
    }

    QString componentId = m_validationQueue.takeFirst();
    m_service->fetchComponentData(componentId, false);
}

void ComponentListViewModel::onValidationComplete(const QString& componentId) {
    Q_UNUSED(componentId);
    m_validationCompletedCount++;

    if (m_validationPendingCount > 0) {
        m_validationPendingCount--;
    }

    qDebug() << "onValidationComplete:" << componentId << "pending:" << m_validationPendingCount
             << "completed:" << m_validationCompletedCount << "queue size:" << m_validationQueue.size();

    emit filteredCountChanged();

    if (!m_validationQueue.isEmpty()) {
        processNextValidation();
    } else if (m_validationPendingCount <= 0) {
        qDebug() << "All validation complete, fetching preview images";
        fetchAllPreviewImages();
    }
}

void ComponentListViewModel::pasteFromClipboard() {
    QClipboard* clipboard = QGuiApplication::clipboard();
    QString text = clipboard->text();

    if (text.isEmpty()) {
        qWarning() << "Clipboard is empty";
        return;
    }

    QStringList extractedIds = extractComponentIdFromText(text);
    if (extractedIds.isEmpty()) {
        qWarning() << "No valid component IDs found in clipboard";
        emit pasteCompleted(0, 0);
        return;
    }

    int skipped = 0;
    QStringList newIds;
    for (const QString& id : extractedIds) {
        if (componentExists(id)) {
            skipped++;
        } else {
            newIds.append(id);
        }
    }

    if (!newIds.isEmpty()) {
        addComponentsBatch(newIds);
    }

    emit pasteCompleted(newIds.count(), skipped);
}

void ComponentListViewModel::copyAllComponentIds() {
    if (m_componentList.isEmpty()) {
        qWarning() << "Component list is empty, nothing to copy";
        return;
    }

    QStringList componentIds;
    for (const auto* item : m_componentList) {
        if (item) {
            componentIds.append(item->componentId());
        }
    }

    QString textToCopy = componentIds.join("\n");

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(textToCopy);

    qDebug() << "Copied" << componentIds.size() << "component IDs to clipboard";
}

void ComponentListViewModel::selectBomFile(const QString& filePath) {
    qDebug() << "BOM file selected:" << filePath;

    if (m_bomFilePath != filePath) {
        m_bomFilePath = filePath;
        emit bomFilePathChanged();
    }

    QString localPath = filePath;
    if (filePath.startsWith("file:///")) {
        localPath = QUrl(filePath).toLocalFile();
        qDebug() << "Converted URL to local path:" << localPath;
    }

    m_bomResult = "Parsing BOM file...";
    emit bomResultChanged();

    QFuture<QStringList> future = QtConcurrent::run([this, localPath]() { return m_service->parseBomFile(localPath); });

    QFutureWatcher<QStringList>* watcher = new QFutureWatcher<QStringList>(this);
    QPointer<ComponentListViewModel> self(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [self, watcher, localPath]() {
        if (!self) {
            return;
        }
        QStringList componentIds = watcher->result();
        watcher->deleteLater();

        if (componentIds.isEmpty()) {
            self->m_bomResult = "No valid component IDs found in BOM file";
            qWarning() << "No component IDs found in BOM file:" << localPath;
            emit self->bomResultChanged();
            return;
        }

        QStringList newIds;
        int skipped = 0;

        for (const QString& id : componentIds) {
            if (!self->componentExists(id)) {
                newIds.append(id);
            } else {
                skipped++;
            }
        }

        if (!newIds.isEmpty()) {
            self->addComponentsBatch(newIds);
        }

        QString resultMsg =
            QString("BOM file imported: %1 components added, %2 skipped").arg(newIds.count()).arg(skipped);
        self->m_bomResult = resultMsg;
        qDebug() << resultMsg;
        emit self->bomResultChanged();
    });
    watcher->setFuture(future);
}

void ComponentListViewModel::fetchComponentData(const QString& componentId, bool fetch3DModel) {
    qDebug() << "Fetching component data for:" << componentId;
    m_service->setOutputPath(m_outputPath);
    m_service->fetchComponentData(componentId, fetch3DModel);
}

void ComponentListViewModel::setOutputPath(const QString& path) {
    if (m_outputPath != path) {
        m_outputPath = path;
        emit outputPathChanged();
    }
}

bool ComponentListViewModel::validateComponentId(const QString& componentId) const {
    return m_service->validateComponentId(componentId);
}

QStringList ComponentListViewModel::extractComponentIdFromText(const QString& text) const {
    return m_service->extractComponentIdFromText(text);
}

bool ComponentListViewModel::componentExists(const QString& componentId) const {
    return m_componentIdIndex.contains(componentId);
}

ComponentListItemData* ComponentListViewModel::findItemData(const QString& componentId) const {
    auto it = m_componentIdIndex.find(componentId);
    if (it != m_componentIdIndex.end()) {
        int index = it.value();
        if (index >= 0 && index < m_componentList.count()) {
            return m_componentList.at(index);
        }
    }
    return nullptr;
}

void ComponentListViewModel::handleComponentInfoReady(const QString& componentId, const ComponentData& data) {
    auto item = findItemData(componentId);
    if (item) {
        item->setNameSilent(data.name());
        item->setPackageSilent(data.package());
        if (!m_batchUpdateItems.contains(item)) {
            m_batchUpdateItems.append(item);
        }
        if (!m_batchUpdateTimer->isActive()) {
            m_batchUpdateTimer->start();
        }
    }
}

void ComponentListViewModel::handleCadDataReady(const QString& componentId, const ComponentData& data) {
    qDebug() << "CAD data ready for:" << componentId;
    auto item = findItemData(componentId);
    if (!item) {
        qWarning() << "handleCadDataReady: item not found for componentId:" << componentId
                   << ", m_componentIdIndex size:" << m_componentIdIndex.size();
        return;
    } else {
        QSharedPointer<ComponentData> dataPtr = QSharedPointer<ComponentData>::create(data);
        item->setComponentData(dataPtr);
        item->setFetching(false);
        item->setValid(true);
        item->setErrorMessage("");

        m_validationStateManager->onComponentValidated(componentId);
        QTimer::singleShot(0, this, [this, componentId]() { onValidationComplete(componentId); });
    }
}

void ComponentListViewModel::handleModel3DReady(const QString& uuid, const QString& filePath) {
    qDebug() << "3D model ready for UUID:" << uuid << "at:" << filePath;
}

void ComponentListViewModel::handleFetchError(const QString& componentId, const QString& error) {
    qWarning() << "Fetch error for:" << componentId << "-" << error;

    auto item = findItemData(componentId);

    if (item) {
        item->setFetching(false);
        item->setValid(false);

        if (error.contains("Request timeout", Qt::CaseInsensitive) || error.contains("timeout", Qt::CaseInsensitive)) {
            item->setErrorMessage("验证超时（网络不稳定）");
        } else if (error.contains("No result", Qt::CaseInsensitive) || error.contains("404") ||
                   error.contains("not found", Qt::CaseInsensitive)) {
            item->setErrorMessage("元件不存在");
        } else {
            item->setErrorMessage(error);
        }

        updateHasInvalidComponents();
        emit filteredCountChanged();

        m_validationStateManager->onComponentFailed(componentId);
        QTimer::singleShot(0, this, [this, componentId]() { onValidationComplete(componentId); });
    }
}

void ComponentListViewModel::handleLcscDataUpdated(const QString& componentId,
                                                   const QString& manufacturerPart,
                                                   const QString& datasheetUrl,
                                                   const QStringList& imageUrls) {
    auto item = findItemData(componentId);
    if (item) {
        if (item->componentData()) {
            auto data = item->componentData();
            if (!manufacturerPart.isEmpty()) {
                data->setManufacturerPart(manufacturerPart);
            }
            if (!datasheetUrl.isEmpty()) {
                data->setDatasheet(datasheetUrl);

                QString format = "pdf";
                if (datasheetUrl.toLower().contains(".html")) {
                    format = "html";
                }
                data->setDatasheetFormat(format);
            }
            if (!imageUrls.isEmpty()) {
                data->setPreviewImages(imageUrls);
            }
        }

        if (!manufacturerPart.isEmpty()) {
            item->setNameSilent(manufacturerPart);
            if (!m_batchUpdateItems.contains(item)) {
                m_batchUpdateItems.append(item);
            }
            if (!m_batchUpdateTimer->isActive()) {
                m_batchUpdateTimer->start();
            }
        }
    } else {
        qWarning() << "Component" << componentId << "not found in list, cannot update LCSC data";
    }
}

void ComponentListViewModel::handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData) {
    auto item = findItemData(componentId);
    if (item) {
        if (item->componentData()) {
            auto data = item->componentData();
            data->setDatasheetData(datasheetData);

            QString format = data->datasheetFormat();
            if (format.isEmpty()) {
                if (datasheetData.startsWith("%PDF-")) {
                    format = "pdf";
                } else {
                    format = "html";
                }
                data->setDatasheetFormat(format);
            }
        }
    } else {
        qWarning() << "Component" << componentId << "not found in list, cannot update datasheet data";
    }
}

void ComponentListViewModel::handleAllImagesReady(const QString& componentId, const QStringList& imagePaths) {
    qDebug() << "All images ready for component:" << componentId << "paths:" << imagePaths.size();
    auto item = findItemData(componentId);
    if (item) {
        qDebug() << "[ViewModel] handleAllImagesReady - component:" << componentId
                 << "current raw images count:" << item->previewImagesRaw().size();
        if (!m_pendingPreviewImageItems.contains(item)) {
            m_pendingPreviewImageItems.append(item);
        }
        if (!m_previewImageUpdateTimer->isActive()) {
            m_previewImageUpdateTimer->start();
        }
    }
    if (item && item->componentData()) {
        QFutureWatcher<QList<QByteArray>>* watcher = new QFutureWatcher<QList<QByteArray>>(this);
        connect(watcher, &QFutureWatcher<QList<QByteArray>>::finished, this, [this, watcher, item, componentId]() {
            QList<QByteArray> imageDataList = watcher->result();
            watcher->deleteLater();
            if (!imageDataList.isEmpty()) {
                item->componentData()->setPreviewImageData(imageDataList);
            }
        });

        QFuture<QList<QByteArray>> future = QtConcurrent::run([imagePaths]() -> QList<QByteArray> {
            QList<QByteArray> imageDataList;
            for (const QString& path : imagePaths) {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly)) {
                    imageDataList.append(file.readAll());
                }
            }
            return imageDataList;
        });
        watcher->setFuture(future);
    }
}

void ComponentListViewModel::batchUpdatePreviewImages() {
    qDebug() << "Batch updating preview images for" << m_pendingPreviewImageItems.size() << "items";

    if (m_pendingPreviewImageItems.isEmpty()) {
        return;
    }

    for (const QPointer<ComponentListItemData>& item : m_pendingPreviewImageItems) {
        if (item) {
            QList<QImage> images;
            for (const QImage& img : item->previewImagesRaw()) {
                images.append(img);
            }

            auto callback = [this](const QString& cid, const QStringList& encoded) {
                onPreviewImageEncodingDone(cid, encoded);
            };
            PreviewImageEncodeRunnable* runnable = new PreviewImageEncodeRunnable(item, images, callback);

            m_encodingThreadPool->start(runnable);
        }
    }

    m_pendingPreviewImageItems.clear();
}

void ComponentListViewModel::onPreviewImageEncodingDone(const QString& componentId, const QStringList& encodedImages) {
    qDebug() << "[ViewModel] Encoding completed for:" << componentId << "count:" << encodedImages.size();

    auto item = findItemData(componentId);
    if (item) {
        item->setEncodedPreviewImages(encodedImages);
    }
}

void ComponentListViewModel::refreshComponentInfo(int index) {
    if (index >= 0 && index < m_componentList.count()) {
        auto item = m_componentList.at(index);
        item->setFetching(true);
        item->setValid(false);
        item->setErrorMessage("");
        m_service->fetchComponentData(item->componentId(), false);

        m_validationStateManager->startValidation(1);
        emit filteredCountChanged();
    }
}

void ComponentListViewModel::retryAllInvalidComponents() {
    int retryCount = 0;
    for (int i = 0; i < m_componentList.count(); ++i) {
        auto item = m_componentList.at(i);
        if (item && !item->isValid() && !item->isFetching()) {
            item->setFetching(true);
            item->setValid(false);
            item->setErrorMessage("");
            m_service->fetchComponentData(item->componentId(), false);
            retryCount++;
        }
    }

    if (retryCount > 0) {
        m_validationStateManager->startValidation(retryCount);
    }

    emit filteredCountChanged();
}

QStringList ComponentListViewModel::getAllComponentIds() const {
    QStringList ids;
    for (const auto& item : m_componentList) {
        ids.append(item->componentId());
    }
    return ids;
}

QSharedPointer<ComponentData> ComponentListViewModel::getPreloadedData(const QString& componentId) const {
    auto item = findItemData(componentId);
    if (item && item->isValid() && item->componentData()) {
        return item->componentData();
    }
    return nullptr;
}

QMap<QString, QSharedPointer<ComponentData>> ComponentListViewModel::getAllPreloadedData() const {
    QMap<QString, QSharedPointer<ComponentData>> result;
    for (auto item : m_componentList) {
        if (item && item->isValid() && item->componentData()) {
            result.insert(item->componentId(), item->componentData());
        }
    }
    return result;
}

void ComponentListViewModel::copyToClipboard(const QString& text) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);
    qDebug() << "Copied to clipboard:" << text;
}

void ComponentListViewModel::fetchAllPreviewImages() {
    QStringList validIds;
    for (auto item : m_componentList) {
        if (item && item->isValid()) {
            validIds.append(item->componentId());
        }
    }

    if (validIds.isEmpty()) {
        qDebug() << "No valid components to fetch preview images for";
        return;
    }

    qDebug() << "Fetching preview images for" << validIds.count() << "valid components";

    m_service->fetchBatchPreviewImages(validIds);
}

void ComponentListViewModel::updateHasInvalidComponents() {
    bool hasInvalid = false;
    for (const auto& item : m_componentList) {
        if (item && !item->isValid() && !item->isFetching()) {
            hasInvalid = true;
            break;
        }
    }
    if (hasInvalid != m_hasInvalidComponents) {
        m_hasInvalidComponents = hasInvalid;
        emit hasInvalidComponentsChanged();
    }
}

void ComponentListViewModel::setFilterMode(const QString& mode) {
    if (m_filterMode != mode) {
        m_filterMode = mode;
        emit filterModeChanged();
        emit filteredCountChanged();
    }
}

void ComponentListViewModel::setScrolling(bool scrolling) {
    if (m_isScrolling != scrolling) {
        m_isScrolling = scrolling;
        emit isScrollingChanged();
    }
}

void ComponentListViewModel::updateExportStatus(const QString& componentId,
                                                int previewImageExported,
                                                int datasheetExported) {
    ComponentListItemData* item = findItemData(componentId);
    if (item) {
        if (previewImageExported >= 0) {
            item->setPreviewImageExported(previewImageExported > 0);
        }
        if (datasheetExported >= 0) {
            item->setDatasheetExported(datasheetExported > 0);
        }
    }
}

int ComponentListViewModel::filteredCount() const {
    int count = 0;
    for (const auto& item : m_componentList) {
        if (!item)
            continue;
        if (m_filterMode == "all") {
            count++;
        } else if (m_filterMode == "validating") {
            if (item->isFetching())
                count++;
        } else if (m_filterMode == "valid") {
            if (!item->isFetching() && item->isValid())
                count++;
        } else if (m_filterMode == "invalid") {
            if (!item->isFetching() && !item->isValid())
                count++;
        }
    }
    return count;
}

int ComponentListViewModel::validatingCount() const {
    int count = 0;
    for (const auto& item : m_componentList) {
        if (item && item->isFetching())
            count++;
    }
    return count;
}

int ComponentListViewModel::validCount() const {
    int count = 0;
    for (const auto& item : m_componentList) {
        if (item && !item->isFetching() && item->isValid())
            count++;
    }
    return count;
}

int ComponentListViewModel::invalidCount() const {
    int count = 0;
    for (const auto& item : m_componentList) {
        if (item && !item->isFetching() && !item->isValid())
            count++;
    }
    return count;
}

}  // namespace EasyKiConverter
