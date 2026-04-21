#include "ComponentListViewModel.h"

#include "services/ConfigService.h"
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
    : QAbstractListModel(parent), m_service(service) {
    m_validationStateManager = new ValidationStateManager(this);

    m_previewImageUpdateTimer = new QTimer(this);
    m_previewImageUpdateTimer->setSingleShot(true);
    m_previewImageUpdateTimer->setInterval(100);
    connect(m_previewImageUpdateTimer, &QTimer::timeout, this, &ComponentListViewModel::batchUpdatePreviewImages);

    // 缓存预览图批量更新定时器。
    // 这里用固定时间窗而不是每次来图都重启，保证预览图可以分批渐进显示。
    m_cachePreviewImageTimer = new QTimer(this);
    m_cachePreviewImageTimer->setSingleShot(true);
    m_cachePreviewImageTimer->setInterval(120);
    connect(m_cachePreviewImageTimer, &QTimer::timeout, this, &ComponentListViewModel::processCachePreviewImages);

    m_encodingThreadPool = new QThreadPool(this);
    m_encodingThreadPool->setMaxThreadCount(10);

    m_batchAddTimer = new QTimer(this);
    m_batchAddTimer->setSingleShot(false);
    m_batchAddTimer->setInterval(50);  // 普通模式使用 50ms 间隔，BOM 导入模式使用单独的计时器
    connect(m_batchAddTimer, &QTimer::timeout, this, &ComponentListViewModel::processNextBatchAdd);

    m_batchUpdateTimer = new QTimer(this);
    m_batchUpdateTimer->setSingleShot(true);
    m_batchUpdateTimer->setInterval(100);
    connect(m_batchUpdateTimer, &QTimer::timeout, this, [this]() {
        for (const QPointer<ComponentListItemData>& item : m_batchUpdateItems) {
            if (item) {
                emit item->dataChanged();
            }
        }
        m_batchUpdateItems.clear();
    });

    // 列表更新批处理定时器（更短的间隔，减少批量操作时的延迟）
    m_batchListUpdateTimer = new QTimer(this);
    m_batchListUpdateTimer->setSingleShot(true);
    m_batchListUpdateTimer->setInterval(150);
    connect(m_batchListUpdateTimer, &QTimer::timeout, this, [this]() {
        m_batchListUpdateMode = false;
        m_listUpdatePending = false;
        recomputeStateCounters();
        updateHasInvalidComponents();
        emit componentCountChanged();
        emit filteredCountChanged();
    });

    // BOM 导入模式定时器（降低 UI 更新频率）
    m_bomImportUpdateTimer = new QTimer(this);
    m_bomImportUpdateTimer->setSingleShot(true);
    m_bomImportUpdateTimer->setInterval(300);
    connect(m_bomImportUpdateTimer, &QTimer::timeout, this, [this]() {
        // BOM 导入模式结束，恢复正常更新
        m_bomImportMode = false;
        m_listUpdatePending = false;
        // 处理累积的验证完成计数
        if (m_bomImportPendingUpdates > 0) {
            m_bomImportPendingUpdates = 0;
            scheduleListUpdate();
        }
    });

    // 延迟获取预览图定时器（给验证留出时间完成）
    m_delayedFetchPreviewTimer = new QTimer(this);
    m_delayedFetchPreviewTimer->setSingleShot(true);
    m_delayedFetchPreviewTimer->setInterval(100);  // 100ms 延迟
    connect(m_delayedFetchPreviewTimer, &QTimer::timeout, this, &ComponentListViewModel::delayedFetchPreviewImages);

    // 监听验证完成信号，等待所有元器件验证完成后才开始获取预览图
    connect(m_validationStateManager,
            &ValidationStateManager::validationCompleted,
            this,
            [this](const QStringList& validatedIds) {
                qDebug() << "All validations completed, validated component count:" << validatedIds.size();
                const int totalCount = componentCount();
                m_validationReadyHint = !validatedIds.isEmpty() && validatedIds.size() == totalCount;
                m_previewReadyHint = false;
                emit attentionStateChanged();
                emit previewFetchRequested();
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
                if (image.isNull()) {
                    return;
                }

                QByteArray byteArray;
                QBuffer buffer(&byteArray);
                if (!buffer.open(QIODevice::WriteOnly)) {
                    return;
                }
                image.save(&buffer, "PNG");

                {
                    QMutexLocker locker(&m_cachePreviewMutex);
                    m_pendingIncrementalPreviewImages[componentId][imageIndex] =
                        QString::fromLatin1(byteArray.toBase64().constData());
                }
                if (!m_cachePreviewImageTimer->isActive()) {
                    m_cachePreviewImageTimer->start();
                }
            });
    connect(m_service,
            &ComponentService::previewImageFailed,
            this,
            [this](const QString& componentId, const QString& error) {
                // 预览图获取失败不影响验证状态，只记录日志
                // 元件仍然保持验证成功状态，避免误导用户
                auto item = findItemData(componentId);
                if (item && item->isValid()) {
                    // 预览图失败也应该标记为 completed（不影响验证）
                    if (item->validationPhase() == "fetching_preview") {
                        item->setValidationPhase("completed");
                        scheduleListUpdate();
                    }
                }
                markPreviewFetchCompleted(componentId);
            });
    connect(m_service,
            &ComponentService::previewImagesReady,
            this,
            [this](const QString& componentId, const QStringList& encodedImages) {
                // 收集到待处理列表，使用防抖避免频繁 UI 更新
                {
                    QMutexLocker locker(&m_cachePreviewMutex);
                    m_pendingCachePreviewImages.insert(componentId, encodedImages);
                }
                if (!m_cachePreviewImageTimer->isActive()) {
                    m_cachePreviewImageTimer->start();
                }
                markPreviewFetchCompleted(componentId);
            });
}

ComponentListViewModel::~ComponentListViewModel() {
    qDeleteAll(m_componentList);
    m_componentList.clear();
}

bool ComponentListViewModel::isNonRetryableValidationError(const QString& error) {
    return error.contains("HTTP 404", Qt::CaseInsensitive) || error.contains("404 Not Found", Qt::CaseInsensitive) ||
           error.contains("component not found", Qt::CaseInsensitive) ||
           error.contains("元器件不存在", Qt::CaseInsensitive) || error.contains("[NO_RETRY]", Qt::CaseInsensitive) ||
           error.contains("No result", Qt::CaseInsensitive);
}

int ComponentListViewModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    QMutexLocker locker(&m_listMutex);
    return m_componentList.count();
}

QVariant ComponentListViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_componentList.count())
        return QVariant();

    QMutexLocker locker(&m_listMutex);
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
    clearAttentionHints();
    m_bomImportComplete = false;
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

    // 检查元件是否已存在（需要锁保护）
    {
        QMutexLocker locker(&m_listMutex);
        if (m_componentIdIndex.contains(trimmedId)) {
            qWarning() << "Component already exists:" << trimmedId;
            emit componentAdded(trimmedId, false, "Component already exists");
            return;
        }
    }

    // 创建新元件项（在锁外创建，因为不涉及共享数据）
    auto item = new ComponentListItemData(trimmedId, this);
    item->setFetching(true);
    item->setValid(false);
    item->setValidationPhase("validating");

    // 获取插入位置
    int insertIndex;
    {
        QMutexLocker locker(&m_listMutex);
        insertIndex = m_componentList.count();
    }

    // 通知视图即将插入
    beginInsertRows(QModelIndex(), insertIndex, insertIndex);

    // 修改列表（需要锁保护）
    {
        QMutexLocker locker(&m_listMutex);
        m_componentList.append(item);
        m_componentIdIndex.insert(trimmedId, m_componentList.count() - 1);
    }

    // 通知视图插入完成
    endInsertRows();

    m_service->fetchComponentData(trimmedId, false);

    m_validationStateManager->addValidation(1);

    scheduleListUpdate();
    emit componentAdded(trimmedId, true, "Component added");
}

void ComponentListViewModel::removeComponent(int index) {
    clearAttentionHints();

    // 检查是否是最后一个元器件（需要锁保护）
    int listCount;
    {
        QMutexLocker locker(&m_listMutex);
        listCount = m_componentList.count();
        if (index < 0 || index >= listCount) {
            return;
        }
    }

    // 当删除最后一个元器件时，触发与清空列表相同的效果
    if (listCount == 1) {
        clearComponentList();
        return;
    }

    // 获取要删除的 item 和 ID（需要锁保护）
    ComponentListItemData* item = nullptr;
    QString removedId;

    {
        QMutexLocker locker(&m_listMutex);
        item = m_componentList.takeAt(index);
        removedId = item->componentId();
        m_componentIdIndex.remove(removedId);
        m_validatedComponentIds.removeAll(removedId);
    }

    // 检查是否正在获取中（锁外操作，因为 m_pendingValidationCount 有自己的逻辑）
    if (item->isFetching()) {
        m_pendingValidationCount--;
    }

    beginRemoveRows(QModelIndex(), index, index);
    delete item;
    endRemoveRows();

    rebuildComponentIdIndex();
    updateHasInvalidComponents();

    scheduleListUpdate();
    emit componentRemoved(removedId);
}

void ComponentListViewModel::removeComponentById(const QString& componentId) {
    int indexToRemove = -1;
    {
        QMutexLocker locker(&m_listMutex);
        auto it = m_componentIdIndex.find(componentId);
        if (it != m_componentIdIndex.end()) {
            indexToRemove = it.value();
        }
    }
    if (indexToRemove >= 0) {
        removeComponent(indexToRemove);
    }
}

void ComponentListViewModel::clearComponentList() {
    clearAttentionHints();
    m_bomImportComplete = false;
    m_listUpdatePending = false;
    m_bomImportMode = false;
    m_bomImportPendingUpdates = 0;
    m_pendingPreviewFetchIds.clear();
    {
        QMutexLocker locker(&m_cachePreviewMutex);
        m_pendingCachePreviewImages.clear();
        m_pendingIncrementalPreviewImages.clear();
    }
    // 获取列表大小（需要锁保护）
    int listCount;
    {
        QMutexLocker locker(&m_listMutex);
        listCount = m_componentList.count();
    }

    if (listCount > 0) {
        if (m_service) {
            // 先取消所有正在进行的请求，防止悬空响应
            m_service->cancelAllPendingRequests();
            m_service->clearCache();
        }

        beginResetModel();
        {
            QMutexLocker locker(&m_listMutex);
            qDeleteAll(m_componentList);
            m_componentList.clear();
            m_componentIdIndex.clear();
        }
        endResetModel();

        m_validationStateManager->reset();
        recomputeStateCounters();
        updateHasInvalidComponents();

        scheduleListUpdate();
        emit listCleared();
    }
}

void ComponentListViewModel::rebuildComponentIdIndex() {
    QMutexLocker locker(&m_listMutex);
    m_componentIdIndex.clear();
    for (int i = 0; i < m_componentList.count(); ++i) {
        QString id = m_componentList.at(i)->componentId();
        m_componentIdIndex.insert(id, i);
    }
}

void ComponentListViewModel::addComponentsBatch(const QStringList& componentIds) {
    clearAttentionHints();
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

    m_bomImportComplete = false;

    // 大量元器件导入时启用 BOM 导入模式，降低 UI 更新频率
    if (newIds.count() >= 20 && !m_bomImportMode) {
        m_bomImportMode = true;
    }

    m_pendingComponentIds.append(newIds);
    m_pendingBatchValidationCount += newIds.count();

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

    // 获取起始索引（需要锁保护）
    int startIndex;
    bool isFirstBatch;
    bool isLastBatch;
    {
        QMutexLocker locker(&m_listMutex);
        startIndex = m_componentList.count();
        isFirstBatch = (startIndex == 0);
        isLastBatch = m_pendingComponentIds.isEmpty();
    }

    // 创建新的 items（在锁外创建，因为不涉及共享数据）
    QList<ComponentListItemData*> newItems;
    for (const QString& id : batch) {
        auto item = new ComponentListItemData(id, this);
        item->setFetching(true);
        item->setValid(false);
        item->setValidationPhase("validating");
        newItems.append(item);
    }

    // 通知视图即将插入
    beginInsertRows(QModelIndex(), startIndex, startIndex + pendingCount - 1);

    // 修改列表（需要锁保护）
    {
        QMutexLocker locker(&m_listMutex);
        for (auto item : newItems) {
            m_componentList.append(item);
            m_componentIdIndex.insert(item->componentId(), m_componentList.count() - 1);
        }
    }

    // 通知视图插入完成
    endInsertRows();

    // 只有在最后一批时才启动验证和刷新统计，避免 BOM 导入时 UI 与验证并发抖动
    if (isLastBatch) {
        const int validationCount = m_pendingBatchValidationCount;
        m_pendingBatchValidationCount = 0;

        // BOM 导入模式结束，恢复正常 UI 更新
        if (m_bomImportMode) {
            // 保存状态以便后续处理
            bool hadPendingUpdates = (m_bomImportPendingUpdates > 0);
            m_bomImportMode = false;
            // 如果有累积的验证完成计数，触发一次 UI 更新
            if (hadPendingUpdates) {
                m_bomImportPendingUpdates = 0;
                scheduleListUpdate();
            }
        }

        if (validationCount > 0) {
            if (m_validationStateManager->pendingCount() > 0 || !m_validationQueue.isEmpty() ||
                !m_inFlightComponentIds.isEmpty()) {
                m_validationStateManager->addValidation(validationCount);
            } else {
                m_validationStateManager->startValidation(validationCount);
            }
        }

        // BOM 导入完成，标记并启动验证队列
        m_bomImportComplete = true;
        startValidationQueue();
    }

    scheduleListUpdate();
}

void ComponentListViewModel::startValidationQueue() {
    const int CONCURRENT_WORKERS = ConfigService::instance()->getValidationConcurrentCount();

    // 检查需要添加到队列的新组件
    for (auto item : m_componentList) {
        // 只添加正在验证的组件，排除已验证完成或已验证失败的组件
        // 注意：isFetching() 在 handleCadDataReady 时立即设为 false，
        // 但 onValidationComplete 通过 singleShot(0) 延迟调用，
        // 所以需要额外检查 isValid() 来排除已完成的组件
        if (item->isFetching() && !item->isValid()) {
            QString componentId = item->componentId();
            // 检查是否已经在队列中或正在处理中（飞行中），避免重复添加
            if (!m_validationQueue.contains(componentId) && !m_inFlightComponentIds.contains(componentId)) {
                m_validationQueue.append(componentId);
            }
        }
    }

    // 如果没有待验证的组件，直接返回
    if (m_validationQueue.isEmpty()) {
        return;
    }

    // 如果 m_validationTotalCount 为 0，说明是首次启动，设置总数
    // 否则说明是延续之前的处理，不需要额外设置（已在 pending 中）
    if (m_validationTotalCount == 0) {
        m_validationTotalCount = m_validationQueue.count();
    }

    // 启动并发验证 worker（如果当前没有活跃的 worker）
    int initialCount = qMin(CONCURRENT_WORKERS, m_validationQueue.count());
    for (int i = 0; i < initialCount; ++i) {
        QString componentId = m_validationQueue.takeFirst();
        m_inFlightComponentIds.insert(componentId);  // 标记为飞行中
        m_service->fetchComponentData(componentId, false);
        m_validationPendingCount++;
    }
}

void ComponentListViewModel::processNextValidation() {
    if (m_validationQueue.isEmpty()) {
        return;
    }

    QString componentId = m_validationQueue.takeFirst();
    m_inFlightComponentIds.insert(componentId);  // 标记为飞行中
    m_service->fetchComponentData(componentId, false);
    m_validationPendingCount++;
}

void ComponentListViewModel::onValidationComplete(const QString& componentId) {
    m_validationCompletedCount++;
    m_inFlightComponentIds.remove(componentId);  // 从飞行中移除

    if (m_validationPendingCount > 0) {
        m_validationPendingCount--;
    }

    // BOM 导入完成后，跳过 scheduleListUpdate 和 startValidationQueue
    // 避免频繁调用导致 O(n²) 复杂度和 UI 阻塞
    // 验证流程已在 processNextBatchAdd -> startValidationQueue 中启动
    if (m_bomImportComplete) {
        // BOM 导入已完成，只处理队列中的下一个验证
        if (!m_validationQueue.isEmpty()) {
            processNextValidation();
        } else if (m_validationPendingCount == 0) {
            m_bomImportComplete = false;
        }
        return;
    }

    // BOM 导入模式下，跳过频繁的 scheduleListUpdate，避免 UI 阻塞
    // 只在最后所有验证完成时再更新 UI
    if (!m_bomImportMode) {
        scheduleListUpdate();
    } else {
        m_bomImportPendingUpdates++;
    }

    if (!m_validationQueue.isEmpty()) {
        processNextValidation();
    } else {
        // 队列为空，尝试补充新组件（可能在验证期间新添加的）
        startValidationQueue();
        // 预览图获取现在由 ValidationStateManager::validationCompleted 信号触发
        // 不再使用计数器判断，因为计数器存在溢出问题
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
    QMutexLocker locker(&m_listMutex);
    return m_componentIdIndex.contains(componentId);
}

ComponentListItemData* ComponentListViewModel::findItemData(const QString& componentId) const {
    QMutexLocker locker(&m_listMutex);
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
    auto item = findItemData(componentId);
    if (!item) {
        qWarning() << "handleCadDataReady: item not found for componentId:" << componentId
                   << ", m_componentIdIndex size:" << m_componentIdIndex.size();
        return;
    }

    // 如果组件已经验证通过，说明 CAD 数据之前已经处理过了，
    // 只需要更新 componentData，不要重复触发验证完成逻辑
    if (item->isValid()) {
        QSharedPointer<ComponentData> dataPtr = QSharedPointer<ComponentData>::create(data);
        item->setComponentData(dataPtr);
        return;
    }

    QSharedPointer<ComponentData> dataPtr = QSharedPointer<ComponentData>::create(data);
    item->setComponentData(dataPtr);
    item->setFetching(false);
    item->setValid(true);
    item->setRetryable(true);
    item->setValidationPhase("fetching_preview");  // 预览图获取阶段
    item->setErrorMessage("");
    scheduleListUpdate();

    m_validationStateManager->onComponentValidated(componentId);
    QTimer::singleShot(0, this, [this, componentId]() { onValidationComplete(componentId); });
}

void ComponentListViewModel::handleModel3DReady(const QString& uuid, const QString& filePath) {
    qDebug() << "3D model ready for UUID:" << uuid << "at:" << filePath;
}

void ComponentListViewModel::handleFetchError(const QString& componentId, const QString& error) {
    qWarning() << "Fetch error for:" << componentId << "-" << error;

    auto item = findItemData(componentId);

    if (item) {
        // 重要：预览图获取失败不应该改变元器件的验证状态
        // 元器件的验证状态只由 CAD 数据获取结果决定
        // 如果组件已经验证通过（isValid 为 true），保持验证状态不变
        bool wasAlreadyValid = item->isValid();

        item->setFetching(false);

        if (!wasAlreadyValid) {
            // 组件尚未验证通过，说明 CAD 数据获取失败或还未完成
            // 判断是否是 CAD 数据获取失败
            // 扩大判断范围：包括网络错误和 HTTP 错误，因为这些也可能导致 CAD 数据获取失败
            bool isCadDataFailure =
                error.contains("CAD data") || error.contains("Symbol data") || error.contains("Footprint data") ||
                error.contains("Empty CAD") || error.contains("parse.*EasyEDA") ||
                error.contains("No result", Qt::CaseInsensitive) ||
                // 网络相关错误（CAD 数据获取也可能触发这些）
                error.contains("403") || error.contains("404") || error.contains("timeout", Qt::CaseInsensitive) ||
                error.contains("access denied", Qt::CaseInsensitive) ||
                error.contains("forbidden", Qt::CaseInsensitive) || error.contains("not found", Qt::CaseInsensitive) ||
                error.contains("connection closed", Qt::CaseInsensitive) ||
                error.contains("operation canceled", Qt::CaseInsensitive) ||
                error.contains("network error", Qt::CaseInsensitive) ||
                error.contains("fetch error", Qt::CaseInsensitive);

            if (isCadDataFailure) {
                // CAD 数据获取失败，标记为验证失败
                item->setValid(false);
                item->setValidationPhase("failed");
                const bool nonRetryable = ComponentListViewModel::isNonRetryableValidationError(error);
                item->setRetryable(!nonRetryable);
                if (error.contains("No result", Qt::CaseInsensitive) ||
                    error.contains("not found", Qt::CaseInsensitive) || error.contains("404", Qt::CaseInsensitive)) {
                    item->setErrorMessage(QStringLiteral("元器件不存在（404）"));
                } else if (nonRetryable) {
                    item->setErrorMessage(error);
                } else {
                    item->setErrorMessage(error);
                }
                m_validationStateManager->onComponentFailed(componentId);
                scheduleListUpdate();
                QTimer::singleShot(0, this, [this, componentId]() { onValidationComplete(componentId); });
            } else {
                // 预览图获取失败，保持验证状态（可能还未验证完成）
                // 只更新错误消息，不改变验证状态
                if (error.contains("Request timeout", Qt::CaseInsensitive) ||
                    error.contains("timeout", Qt::CaseInsensitive)) {
                    item->setErrorMessage("预览图获取超时（网络不稳定）");
                } else if (error.contains("No result", Qt::CaseInsensitive) || error.contains("404") ||
                           error.contains("not found", Qt::CaseInsensitive)) {
                    item->setErrorMessage("预览图不存在");
                } else if (error.contains("403")) {
                    item->setErrorMessage("预览图获取被拒绝");
                } else {
                    item->setErrorMessage("预览图获取失败");
                }
            }
        } else {
            // 组件已经验证通过，只是预览图获取失败，保持验证状态不变
        }

        scheduleListUpdate();
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

void ComponentListViewModel::batchUpdatePreviewImages() {
    // 获取并清空待处理列表（需要锁保护）
    QList<QPointer<ComponentListItemData>> itemsToProcess;
    {
        QMutexLocker locker(&m_previewImageMutex);
        itemsToProcess = m_pendingPreviewImageItems;
        m_pendingPreviewImageItems.clear();
    }

    qDebug() << "Batch updating preview images for" << itemsToProcess.size() << "items";

    if (itemsToProcess.isEmpty()) {
        return;
    }

    for (const QPointer<ComponentListItemData>& item : itemsToProcess) {
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
}

void ComponentListViewModel::processCachePreviewImages() {
    // 获取并清空待处理映射（需要锁保护）
    QMap<QString, QStringList> pending;
    QMap<QString, QMap<int, QString>> incrementalPending;
    {
        QMutexLocker locker(&m_cachePreviewMutex);
        pending = m_pendingCachePreviewImages;
        incrementalPending = m_pendingIncrementalPreviewImages;
        m_pendingCachePreviewImages.clear();
        m_pendingIncrementalPreviewImages.clear();
    }

    if (pending.isEmpty() && incrementalPending.isEmpty()) {
        return;
    }

    for (auto it = incrementalPending.cbegin(); it != incrementalPending.cend(); ++it) {
        auto item = findItemData(it.key());
        if (!item) {
            continue;
        }

        bool changed = false;
        for (auto imageIt = it.value().cbegin(); imageIt != it.value().cend(); ++imageIt) {
            item->setEncodedPreviewImageAt(imageIt.value(), imageIt.key(), false);
            changed = true;
        }

        if (changed) {
            item->notifyPreviewImagesChanged();
        }
    }

    for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
        auto item = findItemData(it.key());
        if (item) {
            item->setEncodedPreviewImages(it.value());
            if (item->validationPhase() == "fetching_preview") {
                item->setValidationPhase("completed");
            }
        }
    }

    scheduleListUpdate();
}

void ComponentListViewModel::onPreviewImageEncodingDone(const QString& componentId, const QStringList& encodedImages) {
    auto item = findItemData(componentId);
    if (item) {
        item->setEncodedPreviewImages(encodedImages);
    }
}

void ComponentListViewModel::refreshComponentInfo(int index) {
    ComponentListItemData* item = nullptr;
    QString componentId;
    {
        QMutexLocker locker(&m_listMutex);
        if (index >= 0 && index < m_componentList.count()) {
            item = m_componentList.at(index);
            componentId = item->componentId();
        }
    }

    if (item) {
        clearAttentionHints();
        m_bomImportComplete = false;
        item->setFetching(true);
        item->setValid(false);
        item->setRetryable(true);
        item->setValidationPhase("validating");
        item->setErrorMessage("");
        m_service->fetchComponentData(componentId, false);

        m_validationStateManager->startValidation(1);
        emit filteredCountChanged();
    }
}

void ComponentListViewModel::retryAllInvalidComponents() {
    clearAttentionHints();
    // 先收集需要重试的组件 ID 列表
    QStringList idsToRetry;
    {
        QMutexLocker locker(&m_listMutex);
        for (int i = 0; i < m_componentList.count(); ++i) {
            auto item = m_componentList.at(i);
            if (item && !item->isValid() && !item->isFetching() && item->retryable()) {
                idsToRetry.append(item->componentId());
            }
        }
    }

    // 在锁外执行重试操作
    for (const QString& id : idsToRetry) {
        auto item = findItemData(id);
        if (item) {
            m_bomImportComplete = false;
            item->setFetching(true);
            item->setValid(false);
            item->setRetryable(true);
            item->setValidationPhase("validating");
            item->setErrorMessage("");
            m_service->fetchComponentData(id, false);
        }
    }

    if (!idsToRetry.isEmpty()) {
        m_validationStateManager->startValidation(idsToRetry.count());
    }

    emit filteredCountChanged();
}

QStringList ComponentListViewModel::getAllComponentIds() const {
    QStringList ids;
    {
        QMutexLocker locker(&m_listMutex);
        for (const auto& item : m_componentList) {
            ids.append(item->componentId());
        }
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
    {
        QMutexLocker locker(&m_listMutex);
        for (auto item : m_componentList) {
            if (item && item->isValid() && item->componentData()) {
                result.insert(item->componentId(), item->componentData());
            }
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
    qDebug() << "fetchAllPreviewImages() is deprecated - use fetchPreviewImages(componentIds) instead";
}

void ComponentListViewModel::fetchPreviewImages(const QStringList& componentIds) {
    QStringList validIds;
    QSet<QString> seenIds;
    for (const QString& componentId : componentIds) {
        if (componentId.isEmpty() || seenIds.contains(componentId)) {
            continue;
        }
        seenIds.insert(componentId);

        auto item = findItemData(componentId);
        if (item && item->isValid()) {
            validIds.append(componentId);
        }
    }

    if (validIds.isEmpty()) {
        qDebug() << "No valid components to fetch preview images for";
        m_previewReadyHint = false;
        m_validationReadyHint = false;
        emit attentionStateChanged();
        return;
    }

    qDebug() << "Fetching preview images for" << validIds.count() << "valid components";
    m_pendingPreviewFetchIds = QSet<QString>(validIds.begin(), validIds.end());
    m_previewReadyHint = false;
    emit attentionStateChanged();

    // 使用批量获取预览图接口，避免为每个组件创建单独的定时器
    // LcscImageService 会自动处理缓存加载和队列管理
    m_service->fetchBatchPreviewImages(validIds);
}

void ComponentListViewModel::delayedFetchPreviewImages() {
    // 预览图获取现在由 ValidationStateManager::validationCompleted 信号触发
    // 此函数不再需要，使用信号机制避免了计数器溢出的问题
    qDebug() << "delayedFetchPreviewImages called but ignored - using signal-based trigger instead";
}

void ComponentListViewModel::updateHasInvalidComponents() {
    const bool hasInvalid = m_retryableInvalidCountCache > 0;
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

void ComponentListViewModel::markPreviewFetchCompleted(const QString& componentId) {
    if (componentId.isEmpty() || !m_pendingPreviewFetchIds.contains(componentId)) {
        return;
    }

    m_pendingPreviewFetchIds.remove(componentId);
    if (m_pendingPreviewFetchIds.isEmpty()) {
        m_validationReadyHint = false;
        m_previewReadyHint = true;
        emit attentionStateChanged();
    }
}

void ComponentListViewModel::dismissAttentionHints() {
    clearAttentionHints();
}

void ComponentListViewModel::clearAttentionHints() {
    const bool changed = m_validationReadyHint || m_previewReadyHint || !m_pendingPreviewFetchIds.isEmpty();
    m_validationReadyHint = false;
    m_previewReadyHint = false;
    m_pendingPreviewFetchIds.clear();
    if (changed) {
        emit attentionStateChanged();
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
    if (m_filterMode == "validating") {
        return m_validatingCountCache;
    }
    if (m_filterMode == "valid") {
        return m_validCountCache;
    }
    if (m_filterMode == "invalid") {
        return m_invalidCountCache;
    }
    return componentCount();
}

int ComponentListViewModel::validatingCount() const {
    return m_validatingCountCache;
}

int ComponentListViewModel::validCount() const {
    return m_validCountCache;
}

int ComponentListViewModel::invalidCount() const {
    return m_invalidCountCache;
}

void ComponentListViewModel::scheduleListUpdate() {
    // 防抖：防止定时器级联重启
    // 如果已经有待处理的列表更新，直接跳过
    if (m_listUpdatePending) {
        return;
    }
    if (!m_batchListUpdateMode) {
        m_batchListUpdateMode = true;
    }
    // 安全检查：如果 BOM 导入模式已启用但没有待处理组件，重置标志
    // 这处理了批量添加计时器被外部停止或待处理列表被清空的情况
    if (m_bomImportMode && m_pendingComponentIds.isEmpty()) {
        m_bomImportMode = false;
        m_bomImportPendingUpdates = 0;
    }
    // BOM 导入模式下使用较长的更新间隔，减少 UI 抖动
    if (m_bomImportMode) {
        m_listUpdatePending = true;
        m_bomImportUpdateTimer->start();
    } else {
        m_listUpdatePending = true;
        m_batchListUpdateTimer->start();
    }
}

void ComponentListViewModel::recomputeStateCounters() {
    int validatingCount = 0;
    int validCount = 0;
    int invalidCount = 0;
    int retryableInvalidCount = 0;

    QMutexLocker locker(&m_listMutex);
    for (const auto& item : m_componentList) {
        if (!item) {
            continue;
        }

        const QString phase = item->validationPhase();
        if (phase == "validating") {
            ++validatingCount;
        } else if (phase == "completed" || phase == "fetching_preview") {
            ++validCount;
        } else if (phase == "failed") {
            ++invalidCount;
            if (item->retryable()) {
                ++retryableInvalidCount;
            }
        }
    }

    m_validatingCountCache = validatingCount;
    m_validCountCache = validCount;
    m_invalidCountCache = invalidCount;
    m_retryableInvalidCountCache = retryableInvalidCount;
}

}  // namespace EasyKiConverter

// 你告诉我孩子还是胚胎的时候在哪里？
// 1.胃袋，因为人类的胃袋柔韧性很好，装在胃袋里面不用怕孩子乱动导致流产
// 2.膀胱，这里面的柔韧性虽然没有胃袋那么大，但是够用了
// 3.颅内，因为脑机的原理就是靠放电来控制行为，婴儿可以提前体验开高达的体验
// 4.口腔，因为口腔里面有许多食物残渣，婴儿生出来都是大胖小子，不会营养不良
// 4.肝脏，因为如果你玩游戏太需要肝的话可以让婴儿代肝
// 5.鼻腔，因为人类的鼻毛非常多，婴儿就想裹着棉被一样温暖
// 6.脚底，因为人类每天都要大量的直立行走，提前训练婴儿的抗压能力，生出来之后不用怕抑郁，高考都是小儿科
