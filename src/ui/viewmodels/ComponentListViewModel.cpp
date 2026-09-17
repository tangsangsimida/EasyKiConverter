#include "ComponentListViewModel.h"

#include "ComponentListDataCoordinator.h"
#include "ComponentValidationCoordinator.h"
#include "services/ConfigService.h"
#include "ui/viewmodels/ComponentValidationErrorPolicy.h"
#include "ui/viewmodels/ValidationStateManager.h"

#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QPointer>
#include <QStringList>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

// 创建元件列表视图模型并初始化异步更新资源。
/** @brief 创建元件列表视图模型并初始化异步更新资源。 */
ComponentListViewModel::ComponentListViewModel(ComponentService* service, QObject* parent)
    // 初始化 Qt 模型父对象和组件服务引用。
    : QAbstractListModel(parent), m_service(service) {
    /** @brief 创建验证状态管理器，统一维护列表验证进度和完成通知。 */
    m_validationStateManager = new ValidationStateManager(this);

    initializeTimers();
    initializeServiceConnections();
}

/** @brief 创建批处理、预览和延迟调度所需的定时器。 */
void ComponentListViewModel::initializeTimers() {
    // 缓存预览图使用固定时间窗批量更新，保证图片可以渐进显示。
    m_cachePreviewImageTimer = new QTimer(this);
    m_cachePreviewImageTimer->setSingleShot(true);
    m_cachePreviewImageTimer->setInterval(120);
    connect(m_cachePreviewImageTimer, &QTimer::timeout, this, &ComponentListViewModel::processCachePreviewImages);

    // 普通添加模式和 BOM 导入模式共用添加批处理定时器。
    m_batchAddTimer = new QTimer(this);
    m_batchAddTimer->setSingleShot(false);
    m_batchAddTimer->setInterval(50);
    connect(m_batchAddTimer, &QTimer::timeout, this, &ComponentListViewModel::processNextBatchAdd);

    // 聚合列表项属性通知，避免每个异步基础信息响应都刷新界面。
    m_batchUpdateTimer = new QTimer(this);
    m_batchUpdateTimer->setSingleShot(true);
    m_batchUpdateTimer->setInterval(100);
    connect(m_batchUpdateTimer, &QTimer::timeout, this, [this]() {
        const QList<QPointer<ComponentListItemData>> items = m_batchUpdateItems.take();
        for (const QPointer<ComponentListItemData>& item : items) {
            if (item) {
                emit item->dataChanged();
            }
        }
    });

    // 列表统计更新使用较短批处理窗口，减少批量操作时的界面抖动。
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

    // BOM 导入模式使用更长的窗口，集中处理累积的验证完成通知。
    m_bomImportUpdateTimer = new QTimer(this);
    m_bomImportUpdateTimer->setSingleShot(true);
    m_bomImportUpdateTimer->setInterval(300);
    connect(m_bomImportUpdateTimer, &QTimer::timeout, this, [this]() {
        m_bomImportMode = false;
        m_listUpdatePending = false;
        if (m_bomImportPendingUpdates > 0) {
            m_bomImportPendingUpdates = 0;
            scheduleListUpdate();
        }
    });

    // 延迟获取预览图，给验证完成信号和列表状态更新留出时间。
    m_delayedFetchPreviewTimer = new QTimer(this);
    m_delayedFetchPreviewTimer->setSingleShot(true);
    m_delayedFetchPreviewTimer->setInterval(100);
    connect(m_delayedFetchPreviewTimer, &QTimer::timeout, this, &ComponentListViewModel::delayedFetchPreviewImages);
}

/** @brief 连接验证完成、组件数据和预览图相关的异步服务信号。 */
void ComponentListViewModel::initializeServiceConnections() {
    // 所有元件验证完成后再请求预览图，避免验证和媒体请求同时争抢资源。
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

                m_previewUpdateBuffer.addIncremental(
                    componentId, imageIndex, QString::fromLatin1(byteArray.toBase64().constData()));
                if (!m_cachePreviewImageTimer->isActive()) {
                    m_cachePreviewImageTimer->start();
                }
            });
    connect(m_service,
            &ComponentService::previewImageFailed,
            this,
            [this](const QString& componentId, const QString& error) {
                // 预览图获取失败不影响验证状态，只记录日志并结束预览请求状态。
                auto item = findItemData(componentId);
                if (item && item->isValid() && item->validationPhase() == "fetching_preview") {
                    item->setValidationPhase("completed");
                    scheduleListUpdate();
                }
                markPreviewFetchCompleted(componentId);
            });
    connect(m_service,
            &ComponentService::previewImagesReady,
            this,
            [this](const QString& componentId, const QStringList& encodedImages) {
                // 收集到待处理列表，使用防抖避免频繁 UI 更新。
                m_previewUpdateBuffer.addComplete(componentId, encodedImages);
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

/** @brief 返回当前元件列表的行数。 */
int ComponentListViewModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid())
        return 0;
    QMutexLocker locker(&m_listMutex);
    return m_componentList.count();
}

/** @brief 返回指定行和角色对应的元件数据。 */
QVariant ComponentListViewModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_componentList.count())
        return QVariant();

    QMutexLocker locker(&m_listMutex);
    if (role == ItemDataRole) {
        return QVariant::fromValue(m_componentList.at(index.row()));
    }

    return QVariant();
}

/** @brief 返回供 QML 使用的模型角色名称。 */
QHash<int, QByteArray> ComponentListViewModel::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[ItemDataRole] = "itemData";
    return roles;
}

/** @brief 添加单个元件并启动其数据验证。 */
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

/** @brief 按列表位置删除元件并取消相关请求。 */
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

    // 取消该元器件的网络请求
    if (m_service) {
        m_service->cancelRequestForComponent(removedId);
    }

    // 检查元器件是否已经在飞行中（已 dispatch）
    const bool wasInFlight = m_validationQueue.isInFlight(removedId);

    // 从验证队列和飞行中列表中移除
    m_validationQueue.remove(removedId);

    // 检查是否正在获取中（锁外操作，因为 m_pendingValidationCount 有自己的逻辑）
    if (item->isFetching()) {
        m_pendingValidationCount--;
        // 只有当元器件已经在飞行中（已 dispatch）时才减少 m_validationPendingCount
        // 因为 m_validationPendingCount 只在 dispatch 时增加
        if (wasInFlight) {
            m_validationPendingCount--;
        }
        // 通知 ValidationStateManager 取消验证
        m_validationStateManager->cancelValidation(1);
    }

    beginRemoveRows(QModelIndex(), index, index);
    delete item;
    endRemoveRows();

    rebuildComponentIdIndex();
    updateHasInvalidComponents();

    scheduleListUpdate();
    emit componentRemoved(removedId);
}

/** @brief 按元件编号删除元件。 */
void ComponentListViewModel::removeComponentById(const QString& componentId) {
    int indexToRemove = -1;
    {
        QMutexLocker locker(&m_listMutex);
        indexToRemove = m_componentIdIndex.indexOf(componentId);
    }
    if (indexToRemove >= 0) {
        removeComponent(indexToRemove);
    }
}

/** @brief 清空元件列表及其缓存、验证状态。 */
void ComponentListViewModel::clearComponentList() {
    clearAttentionHints();
    m_bomImportComplete = false;
    m_listUpdatePending = false;
    m_bomImportMode = false;
    m_bomImportPendingUpdates = 0;
    m_pendingPreviewFetchIds.clear();
    m_previewUpdateBuffer.clear();
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

/** @brief 根据列表顺序重建元件编号索引。 */
void ComponentListViewModel::rebuildComponentIdIndex() {
    QMutexLocker locker(&m_listMutex);
    m_componentIdIndex.rebuild(m_componentList);
}

/** @brief 收集并批量调度元件编号。 */
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

/** @brief 处理一批待添加元件并更新验证队列。 */
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
                m_validationQueue.hasInFlight()) {
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

/** @brief 启动验证队列中的并发元件请求。 */
void ComponentListViewModel::startValidationQueue() {
    ComponentValidationCoordinator::start(*this);
}

/** @brief 从验证队列中调度下一个元件请求。 */
void ComponentListViewModel::processNextValidation() {
    ComponentValidationCoordinator::processNext(*this);
}

/** @brief 处理单个元件验证完成并推进验证队列。 */
void ComponentListViewModel::onValidationComplete(const QString& componentId) {
    ComponentValidationCoordinator::complete(*this, componentId);
}

/** @brief 从系统剪贴板提取并批量添加元件编号。 */
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

/** @brief 将当前全部元件编号复制到系统剪贴板。 */
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

/** @brief 异步解析用户选择的 BOM 文件并添加元件。 */
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

/** @brief 请求指定元件的数据，可选择是否获取三维模型。 */
void ComponentListViewModel::fetchComponentData(const QString& componentId, bool fetch3DModel) {
    qDebug() << "Fetching component data for:" << componentId;
    m_service->setOutputPath(m_outputPath);
    m_service->fetchComponentData(componentId, fetch3DModel);
}

/** @brief 设置导出输出目录并通知界面。 */
void ComponentListViewModel::setOutputPath(const QString& path) {
    if (m_outputPath != path) {
        m_outputPath = path;
        emit outputPathChanged();
    }
}

/** @brief 委托服务验证元件编号格式。 */
bool ComponentListViewModel::validateComponentId(const QString& componentId) const {
    return m_service->validateComponentId(componentId);
}

/** @brief 从文本中提取服务支持的元件编号。 */
QStringList ComponentListViewModel::extractComponentIdFromText(const QString& text) const {
    return m_service->extractComponentIdFromText(text);
}

/** @brief 判断元件编号是否已存在于当前列表。 */
bool ComponentListViewModel::componentExists(const QString& componentId) const {
    QMutexLocker locker(&m_listMutex);
    return m_componentIdIndex.contains(componentId);
}

/** @brief 根据元件编号查找对应的列表项。 */
ComponentListItemData* ComponentListViewModel::findItemData(const QString& componentId) const {
    QMutexLocker locker(&m_listMutex);
    const int index = m_componentIdIndex.indexOf(componentId);
    if (index >= 0 && index < m_componentList.count()) {
        return m_componentList.at(index);
    }
    return nullptr;
}

/** @brief 将异步返回的基础信息合并到列表项。 */
void ComponentListViewModel::handleComponentInfoReady(const QString& componentId, const ComponentData& data) {
    ComponentListDataCoordinator::handleComponentInfo(*this, componentId, data);
}

/** @brief 接收 CAD 数据并完成元件验证状态更新。 */
void ComponentListViewModel::handleCadDataReady(const QString& componentId, const ComponentData& data) {
    ComponentListDataCoordinator::handleCadData(*this, componentId, data);
}

/** @brief 记录三维模型准备完成事件。 */
void ComponentListViewModel::handleModel3DReady(const QString& uuid, const QString& filePath) {
    qDebug() << "3D model ready for UUID:" << uuid << "at:" << filePath;
}

/** @brief 根据获取错误更新元件验证或预览状态。 */
void ComponentListViewModel::handleFetchError(const QString& componentId, const QString& error) {
    ComponentListDataCoordinator::handleFetchError(*this, componentId, error);
}

/** @brief 将 LCSC 返回的制造商、数据手册和图片信息合并到列表项。 */
void ComponentListViewModel::handleLcscDataUpdated(const QString& componentId,
                                                   const QString& manufacturerPart,
                                                   const QString& datasheetUrl,
                                                   const QStringList& imageUrls) {
    ComponentListDataCoordinator::handleLcscData(*this, componentId, manufacturerPart, datasheetUrl, imageUrls);
}

/** @brief 保存异步返回的数据手册内容并推断其格式。 */
void ComponentListViewModel::handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData) {
    ComponentListDataCoordinator::handleDatasheet(*this, componentId, datasheetData);
}

/** @brief 批量应用缓存中的预览图编码结果。 */
void ComponentListViewModel::processCachePreviewImages() {
    // 获取并清空待处理映射（需要锁保护）
    const ComponentListPreviewUpdateBuffer::Snapshot snapshot = m_previewUpdateBuffer.take();
    const auto& pending = snapshot.completeImages;
    const auto& incrementalPending = snapshot.incrementalImages;

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

/** @brief 重新请求指定列表项的基础信息。 */
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

/** @brief 重新请求所有可重试的失败元件。 */
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

/** @brief 返回当前列表中的全部元件编号。 */
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

/** @brief 返回指定元件已验证的预加载数据。 */
QSharedPointer<ComponentData> ComponentListViewModel::getPreloadedData(const QString& componentId) const {
    auto item = findItemData(componentId);
    if (item && item->isValid() && item->componentData()) {
        return item->componentData();
    }
    return nullptr;
}

/** @brief 返回全部已验证元件的预加载数据。 */
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

/** @brief 将指定文本写入系统剪贴板。 */
void ComponentListViewModel::copyToClipboard(const QString& text) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);
    qDebug() << "Copied to clipboard:" << text;
}

/** @brief 保留旧接口并提示调用方使用批量预览图接口。 */
void ComponentListViewModel::fetchAllPreviewImages() {
    qDebug() << "fetchAllPreviewImages() is deprecated - use fetchPreviewImages(componentIds) instead";
}

/** @brief 为指定的有效元件批量获取预览图。 */
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

    for (const QString& componentId : std::as_const(validIds)) {
        auto item = findItemData(componentId);
        if (item && item->isValid() && item->previewImageCount() == 0) {
            item->setValidationPhase("fetching_preview");
        }
    }
    scheduleListUpdate();

    // 使用批量获取预览图接口，避免为每个组件创建单独的定时器
    // LcscImageService 会自动处理缓存加载和队列管理
    m_service->fetchBatchPreviewImages(validIds);
}

/** @brief 处理旧版延迟预览图入口，目前由验证完成信号替代。 */
void ComponentListViewModel::delayedFetchPreviewImages() {
    // 预览图获取现在由 ValidationStateManager::validationCompleted 信号触发
    // 此函数不再需要，使用信号机制避免了计数器溢出的问题
    qDebug() << "delayedFetchPreviewImages called but ignored - using signal-based trigger instead";
}

/** @brief 根据状态跟踪器刷新是否存在可重试失败项。 */
void ComponentListViewModel::updateHasInvalidComponents() {
    const bool hasInvalid = m_stateTracker.hasRetryableInvalidComponents();
    if (hasInvalid != m_hasInvalidComponents) {
        m_hasInvalidComponents = hasInvalid;
        emit hasInvalidComponentsChanged();
    }
}

/** @brief 设置列表过滤模式并通知 QML。 */
void ComponentListViewModel::setFilterMode(const QString& mode) {
    if (m_stateTracker.setFilterMode(mode)) {
        emit filterModeChanged();
        emit filteredCountChanged();
    }
}

/** @brief 更新列表滚动状态。 */
void ComponentListViewModel::setScrolling(bool scrolling) {
    if (m_isScrolling != scrolling) {
        m_isScrolling = scrolling;
        emit isScrollingChanged();
    }
}

/** @brief 标记一个元件的预览图请求完成。 */
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

/** @brief 清除界面上的验证和预览图提示。 */
void ComponentListViewModel::dismissAttentionHints() {
    clearAttentionHints();
}

/** @brief 同步更新列表项和服务缓存中的元件描述。 */
void ComponentListViewModel::updateComponentDescription(const QString& componentId, const QString& description) {
    ComponentListItemData* item = findItemData(componentId);
    if (!item) {
        return;
    }

    item->setDescription(description);
    if (m_service) {
        m_service->updateComponentDescription(componentId, description);
    }
}

/** @brief 清空验证完成和预览图完成提示状态。 */
void ComponentListViewModel::clearAttentionHints() {
    const bool changed = m_validationReadyHint || m_previewReadyHint || !m_pendingPreviewFetchIds.isEmpty();
    m_validationReadyHint = false;
    m_previewReadyHint = false;
    m_pendingPreviewFetchIds.clear();
    if (changed) {
        emit attentionStateChanged();
    }
}

/** @brief 更新元件预览图和数据手册的导出状态。 */
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

/** @brief 返回当前过滤模式下的元件数量。 */
int ComponentListViewModel::filteredCount() const {
    return m_stateTracker.filteredCount(componentCount());
}

/** @brief 返回正在验证的元件数量。 */
int ComponentListViewModel::validatingCount() const {
    return m_stateTracker.validatingCount();
}

/** @brief 返回验证成功的元件数量。 */
int ComponentListViewModel::validCount() const {
    return m_stateTracker.validCount();
}

/** @brief 返回验证失败的元件数量。 */
int ComponentListViewModel::invalidCount() const {
    return m_stateTracker.invalidCount();
}

/** @brief 以防抖方式安排列表状态刷新。 */
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

/** @brief 在列表锁保护下重新计算验证统计。 */
void ComponentListViewModel::recomputeStateCounters() {
    QMutexLocker locker(&m_listMutex);
    m_stateTracker.recompute(m_componentList);
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
