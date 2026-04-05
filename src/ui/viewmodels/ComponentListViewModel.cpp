#include "ComponentListViewModel.h"

#include "ui/utils/ThumbnailGenerator.h"
#include "ui/viewmodels/ThumbnailUpdateManager.h"
#include "ui/viewmodels/ValidationStateManager.h"

#include <QClipboard>
#include <QDebug>
#include <QFile>
#include <QGuiApplication>
#include <QPointer>
#include <QRunnable>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

ComponentListViewModel::ComponentListViewModel(ComponentService* service, QObject* parent)
    : QAbstractListModel(parent), m_service(service), m_componentList(), m_bomFilePath(), m_bomResult() {
    m_thumbnailUpdateManager = new ThumbnailUpdateManager(this);
    m_thumbnailUpdateManager->setFlushCallback([this](const QSet<QString>& componentIds) {
        for (const QString& componentId : componentIds) {
            auto item = findItemData(componentId);
            if (item) {
                int index = m_componentList.indexOf(item);
                if (index >= 0) {
                    emit dataChanged(createIndex(index, 0), createIndex(index, 0));
                }
            }
        }
    });

    m_validationStateManager = new ValidationStateManager(this);
    // 注意：不再需要 validationCompleted 信号来触发预览图获取
    // 预览图获取现在由队列自动处理

    // 连接 Service 信号
    connect(m_service, &ComponentService::componentInfoReady, this, &ComponentListViewModel::handleComponentInfoReady);
    connect(m_service, &ComponentService::cadDataReady, this, &ComponentListViewModel::handleCadDataReady);
    connect(m_service, &ComponentService::model3DReady, this, &ComponentListViewModel::handleModel3DReady);
    connect(m_service,
            &ComponentService::previewImageReady,
            this,
            [this](const QString& componentId, const QImage& image, int imageIndex) {
                auto item = findItemData(componentId);
                if (item) {
                    // 按索引顺序插入预览图，确保显示顺序正确
                    item->insertPreviewImage(image, imageIndex);
                    // 第一张预览图始终覆盖缩略图显示
                    if (imageIndex == 0) {
                        item->setThumbnail(image);
                    }
                    // 滚动时不触发 UI 更新，避免卡顿
                    if (!m_isScrolling) {
                        m_thumbnailUpdateManager->scheduleUpdate(componentId);
                    }
                }
            });
    connect(m_service,
            &ComponentService::previewImageDataReady,
            this,
            &ComponentListViewModel::handlePreviewImageDataReady);
    connect(m_service, &ComponentService::allImagesReady, this, &ComponentListViewModel::handleAllImagesReady);
    connect(m_service, &ComponentService::lcscDataUpdated, this, &ComponentListViewModel::handleLcscDataUpdated);
    connect(m_service, &ComponentService::datasheetReady, this, &ComponentListViewModel::handleDatasheetReady);
    connect(m_service, &ComponentService::fetchError, this, &ComponentListViewModel::handleFetchError);
    connect(m_service, &ComponentService::previewImageFailed, this, &ComponentListViewModel::handlePreviewImageFailed);
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
    item->setValid(false);  // 验证开始时先设置为 false
    m_componentList.append(item);
    m_componentIdIndex.insert(trimmedId, m_componentList.count() - 1);  // 维护索引（O(1)查找）
    endInsertRows();

    // 触发获取数据
    m_service->fetchComponentData(trimmedId, false);

    // 两阶段控制：单个添加也使用两阶段
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
        m_componentIdIndex.remove(removedId);  // 维护索引

        // 两阶段控制：更新状态
        m_validatedComponentIds.removeAll(removedId);
        // 只有当元器件还在验证中（isFetching=true）时才减少计数
        if (item->isFetching()) {
            m_pendingValidationCount--;
        }

        delete item;
        endRemoveRows();

        // 重建索引（因为删除后后续项的索引会改变）
        rebuildComponentIdIndex();

        // 更新 hasInvalidComponents 状态
        updateHasInvalidComponents();

        // 如果删除后验证队列为空，尝试开始预览图获取
        if (m_pendingValidationCount <= 0 && !m_validatedComponentIds.isEmpty()) {
            m_previewFetchEnabled = true;
            fetchAllPreviewImages();
        }

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
        // 清空服务层的缓存，包括 LCSC 图片服务的缓存
        if (m_service) {
            m_service->clearCache();
        }

        beginResetModel();
        qDeleteAll(m_componentList);
        m_componentList.clear();
        m_componentIdIndex.clear();  // 清空索引
        endResetModel();

        // 两阶段控制：重置状态
        m_validationStateManager->reset();

        // 更新 hasInvalidComponents 状态
        updateHasInvalidComponents();

        emit componentCountChanged();
        emit filteredCountChanged();
        emit listCleared();
    }
}

void ComponentListViewModel::rebuildComponentIdIndex() {
    // 重建 ID 到索引的映射（删除操作后需要调用）
    m_componentIdIndex.clear();
    for (int i = 0; i < m_componentList.count(); ++i) {
        QString id = m_componentList.at(i)->componentId();
        m_componentIdIndex.insert(id, i);
    }
}

void ComponentListViewModel::addComponentsBatch(const QStringList& componentIds) {
    // 收集需要添加的新 ID（去重 + 验证）
    QStringList newIds;
    for (const QString& rawId : componentIds) {
        QString id = rawId.trimmed().toUpper();
        if (id.isEmpty())
            continue;

        // 验证格式
        if (!validateComponentId(id)) {
            // 尝试提取
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

    // 第一阶段：快速添加所有元器件到列表，不触发验证
    // 这样可以立即显示元器件列表，UI保持流畅
    int pendingCount = newIds.count();

    beginInsertRows(QModelIndex(), m_componentList.count(), m_componentList.count() + pendingCount - 1);
    for (const QString& id : newIds) {
        auto item = new ComponentListItemData(id, this);
        item->setFetching(true);
        item->setValid(false);
        m_componentList.append(item);
        m_componentIdIndex.insert(id, m_componentList.count() - 1);
    }
    endInsertRows();

    emit componentCountChanged();
    emit filteredCountChanged();

    // 重置验证状态管理器
    m_validationStateManager->startValidation(pendingCount);

    // 第二阶段：开始验证队列处理
    startValidationQueue();
}

void ComponentListViewModel::startValidationQueue() {
    // 验证队列：使用 5 个并发 worker
    // 只验证 isFetching=true 的组件（新添加的或需要重试的）
    const int CONCURRENT_WORKERS = 5;

    // 初始化验证队列：只包含需要验证的组件
    m_validationQueue.clear();
    for (auto item : m_componentList) {
        if (item->isFetching()) {
            m_validationQueue.append(item->componentId());
        }
    }
    m_validationPendingCount = 0;
    m_validationCompletedCount = 0;
    m_validationTotalCount = m_validationQueue.count();

    // 如果没有待验证的组件，直接返回
    if (m_validationQueue.isEmpty()) {
        return;
    }

    // 启动第一批验证（5个并发）
    int initialCount = qMin(CONCURRENT_WORKERS, m_validationQueue.count());
    for (int i = 0; i < initialCount; ++i) {
        QString componentId = m_validationQueue.takeFirst();
        m_service->fetchComponentData(componentId, false);
        m_validationPendingCount++;
    }
}

void ComponentListViewModel::processNextValidation() {
    // 如果队列为空，不处理
    if (m_validationQueue.isEmpty()) {
        return;
    }

    // 取出下一个进行验证
    QString componentId = m_validationQueue.takeFirst();
    m_service->fetchComponentData(componentId, false);
}

void ComponentListViewModel::onValidationComplete(const QString& componentId) {
    Q_UNUSED(componentId);
    m_validationCompletedCount++;

    // 只有当 pending > 0 时才递减（防止负数）
    if (m_validationPendingCount > 0) {
        m_validationPendingCount--;
    }

    qDebug() << "onValidationComplete:" << componentId
              << "pending:" << m_validationPendingCount
              << "completed:" << m_validationCompletedCount
              << "queue size:" << m_validationQueue.size();

    // 如果队列非空，继续处理下一个
    if (!m_validationQueue.isEmpty()) {
        processNextValidation();
    } else if (m_validationPendingCount <= 0) {
        // 队列为空且没有待处理的验证，启动预览图队列
        qDebug() << "All validation complete, starting preview image queue";
        startPreviewImageQueue();
    }
}

void ComponentListViewModel::startPreviewImageQueue() {
    // 预览图队列：使用 5 个并发 worker
    const int CONCURRENT_WORKERS = 5;

    // 初始化预览图队列（只包含验证通过的元器件）
    m_previewQueue.clear();
    for (auto item : m_componentList) {
        if (item->isValid()) {
            m_previewQueue.append(item->componentId());
        }
    }
    m_previewPendingCount = 0;
    m_previewCompletedCount = 0;
    m_previewTotalCount = m_previewQueue.count();

    if (m_previewQueue.isEmpty()) {
        return;
    }

    // 启动第一批预览图获取（5个并发）
    int initialCount = qMin(CONCURRENT_WORKERS, m_previewQueue.count());
    for (int i = 0; i < initialCount; ++i) {
        QString componentId = m_previewQueue.takeFirst();
        m_service->fetchLcscPreviewImage(componentId);
        m_previewPendingCount++;
    }
}

void ComponentListViewModel::processNextPreviewImage() {
    if (m_previewQueue.isEmpty()) {
        return;
    }

    QString componentId = m_previewQueue.takeFirst();
    m_service->fetchLcscPreviewImage(componentId);
}

void ComponentListViewModel::onPreviewImageComplete(const QString& componentId) {
    Q_UNUSED(componentId);
    m_previewPendingCount--;
    m_previewCompletedCount++;

    if (!m_previewQueue.isEmpty()) {
        processNextPreviewImage();
    } else if (m_previewPendingCount == 0) {
        // 所有预览图获取完成
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

    // 统计新增和跳过数量
    int skipped = 0;
    QStringList newIds;
    for (const QString& id : extractedIds) {
        if (componentExists(id)) {
            skipped++;
        } else {
            newIds.append(id);
        }
    }

    // 使用批量添加
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

    // 收集所有元器件编号
    QStringList componentIds;
    for (const auto* item : m_componentList) {
        if (item) {
            componentIds.append(item->componentId());
        }
    }

    // 用换行符连接所有编号
    QString textToCopy = componentIds.join("\n");

    // 复制到剪贴板
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

    // 将 URL 转换为本地文件路径
    QString localPath = filePath;
    if (filePath.startsWith("file:///")) {
        localPath = QUrl(filePath).toLocalFile();
        qDebug() << "Converted URL to local path:" << localPath;
    }

    // 显示正在解析的状态，避免阻塞 UI
    m_bomResult = "Parsing BOM file...";
    emit bomResultChanged();

    // 使用 QtConcurrent 在后台线程解析 BOM 文件，避免阻塞 UI
    QFuture<QStringList> future = QtConcurrent::run([this, localPath]() { return m_service->parseBomFile(localPath); });

    // 监视未来结果，完成后回到主线程处理
    QFutureWatcher<QStringList>* watcher = new QFutureWatcher<QStringList>(this);
    QPointer<ComponentListViewModel> self(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [self, watcher, localPath]() {
        if (!self) {
            return;  // ViewModel 已销毁，不执行任何操作
        }
        QStringList componentIds = watcher->result();
        watcher->deleteLater();

        if (componentIds.isEmpty()) {
            self->m_bomResult = "No valid component IDs found in BOM file";
            qWarning() << "No component IDs found in BOM file:" << localPath;
            emit self->bomResultChanged();
            return;
        }

        // 使用批量添加方法
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
    // 委托给 Service 层处理业务逻辑
    return m_service->validateComponentId(componentId);
}

QStringList ComponentListViewModel::extractComponentIdFromText(const QString& text) const {
    // 委托给 Service 层处理业务逻辑
    return m_service->extractComponentIdFromText(text);
}

bool ComponentListViewModel::componentExists(const QString& componentId) const {
    return m_componentIdIndex.contains(componentId);
}

ComponentListItemData* ComponentListViewModel::findItemData(const QString& componentId) const {
    // 使用 QHash 查找索引，实现 O(1) 查找
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
        item->setName(data.name());
        item->setPackage(data.package());
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

        // 注意：设置 item->setValid(true) 不会产生新的 invalid 组件
        // 所以不需要调用 updateHasInvalidComponents() 和 emit filteredCountChanged()
        // 这些信号只在删除或失败时才有必要

        // 两阶段控制：验证完成后通知管理器
        m_validationStateManager->onComponentValidated(componentId);

        // 立即生成缩略图（基于符号/封装数据），而不是设置为空
        // 后续预览图到达后会覆盖此缩略图
        if (item->componentData()) {
            QImage thumb = ThumbnailGenerator::generateThumbnail(item->componentData());
            if (!thumb.isNull()) {
                item->setThumbnail(thumb);
            } else {
                // 生成失败时设置验证成功占位符
                item->setThumbnail(ThumbnailGenerator::generatePlaceholderThumbnail(componentId));
            }
        } else {
            // 没有数据时设置验证成功占位符
            item->setThumbnail(ThumbnailGenerator::generatePlaceholderThumbnail(componentId));
        }

        // 添加到待更新集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);

        // 通知验证队列处理下一个
        onValidationComplete(componentId);
    }
}

void ComponentListViewModel::handleModel3DReady(const QString& uuid, const QString& filePath) {
    qDebug() << "3D model ready for UUID:" << uuid << "at:" << filePath;
    // 3D 模型下载通常比较慢，验证阶段不强制要求显示，但如果来了也可以更新
}

void ComponentListViewModel::handleFetchError(const QString& componentId, const QString& error) {
    qWarning() << "Fetch error for:" << componentId << "-" << error;

    auto item = findItemData(componentId);

    if (item) {
        item->setFetching(false);

        // 标记为无效
        item->setValid(false);

        // 根据错误类型设置不同的错误消息和缩略图
        if (error.contains("Request timeout", Qt::CaseInsensitive) || error.contains("timeout", Qt::CaseInsensitive)) {
            item->setErrorMessage("验证超时（网络不稳定）");
            // 超时时保持原缩略图，让用户可以重试
        } else if (error.contains("No result", Qt::CaseInsensitive) || error.contains("404") ||
                   error.contains("not found", Qt::CaseInsensitive)) {
            item->setErrorMessage("元件不存在");
            // 设置错误占位缩略图（红色圆圈 X）
            item->setThumbnail(ThumbnailGenerator::generateErrorPlaceholderThumbnail(componentId));
        } else {
            item->setErrorMessage(error);
            // 设置错误占位缩略图
            item->setThumbnail(ThumbnailGenerator::generateErrorPlaceholderThumbnail(componentId));
        }

        // 更新 hasInvalidComponents 状态
        updateHasInvalidComponents();
        emit filteredCountChanged();

        // 两阶段控制：验证失败也要计数
        m_validationStateManager->onComponentFailed(componentId);

        // 添加到防抖集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);

        // 通知验证队列处理下一个（验证失败也要继续队列）
        onValidationComplete(componentId);
    }
}

void ComponentListViewModel::handleLcscDataUpdated(const QString& componentId,
                                                   const QString& manufacturerPart,
                                                   const QString& datasheetUrl,
                                                   const QStringList& imageUrls) {
    auto item = findItemData(componentId);
    if (item) {
        // 更新 ComponentData
        if (item->componentData()) {
            auto data = item->componentData();
            if (!manufacturerPart.isEmpty()) {
                data->setManufacturerPart(manufacturerPart);
            }
            if (!datasheetUrl.isEmpty()) {
                data->setDatasheet(datasheetUrl);

                // 检测并同步数据手册格式
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

        // 更新显示名称（使用制造商部件号）
        if (!manufacturerPart.isEmpty()) {
            item->setName(manufacturerPart);
        }

        // 添加到防抖集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);
    } else {
        qWarning() << "Component" << componentId << "not found in list, cannot update LCSC data";
    }
}

void ComponentListViewModel::handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData) {
    auto item = findItemData(componentId);
    if (item) {
        // 更新 ComponentData
        if (item->componentData()) {
            auto data = item->componentData();
            data->setDatasheetData(datasheetData);

            // 确保格式信息正确（从 ComponentService 的 ComponentData 同步）
            // 如果 ComponentService 的 ComponentData 有格式信息，优先使用它
            QString format = data->datasheetFormat();
            if (format.isEmpty()) {
                // 如果没有格式信息，基于内容检测
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

void ComponentListViewModel::handlePreviewImageDataReady(const QString& componentId,
                                                         const QByteArray& imageData,
                                                         int imageIndex) {
    auto item = findItemData(componentId);
    if (item && item->componentData()) {
        // 保存数据到 ComponentListViewModel 的 ComponentData 中（用于导出）
        item->componentData()->addPreviewImageData(imageData, imageIndex);
    }
}

void ComponentListViewModel::handlePreviewImageFailed(const QString& componentId, const QString& error) {
    qWarning() << "Preview image fetch failed for component:" << componentId << "error:" << error;

    auto item = findItemData(componentId);
    if (item) {
        // 设置 isFetching 为 false
        item->setFetching(false);

        // 预览图加载失败但元器件验证成功，显示成功占位符
        // 元器件存在，只是没有LCSC预览图
        item->setThumbnail(ThumbnailGenerator::generatePlaceholderThumbnail(componentId));

        // 添加到待更新集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);

        // 通知预览图队列处理下一个
        onPreviewImageComplete(componentId);
    }
}

void ComponentListViewModel::handleAllImagesReady(const QString& componentId, const QStringList& imagePaths) {
    auto item = findItemData(componentId);
    if (item && item->componentData()) {
        // 设置 isFetching 为 false
        item->setFetching(false);

        // 只有当有有效图片时才更新图片数据
        if (!imagePaths.isEmpty()) {
            // 异步加载所有图片数据到 ComponentData 中
            QFutureWatcher<QList<QByteArray>>* watcher = new QFutureWatcher<QList<QByteArray>>(this);
            connect(watcher, &QFutureWatcher<QList<QByteArray>>::finished, this, [this, watcher, item, componentId]() {
                QList<QByteArray> imageDataList = watcher->result();
                watcher->deleteLater();
                if (!imageDataList.isEmpty()) {
                    item->componentData()->setPreviewImageData(imageDataList);
                }
                // 添加到待更新集合
                m_thumbnailUpdateManager->scheduleUpdate(componentId);

                // 通知预览图队列处理下一个
                onPreviewImageComplete(componentId);
            });

            // 使用 QtConcurrent::run 在后台等待所有文件读取完成
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
        } else {
            // 没有图片，设置验证成功占位符
            item->setThumbnail(ThumbnailGenerator::generatePlaceholderThumbnail(componentId));
            // 添加到待更新集合
            m_thumbnailUpdateManager->scheduleUpdate(componentId);

            // 通知预览图队列处理下一个
            onPreviewImageComplete(componentId);
        }
    }
}

void ComponentListViewModel::refreshComponentInfo(int index) {
    if (index >= 0 && index < m_componentList.count()) {
        auto item = m_componentList.at(index);
        item->setFetching(true);
        item->setValid(false);  // 验证开始时先设置为 false
        item->setErrorMessage("");
        m_service->fetchComponentData(item->componentId(), false);

        // 两阶段控制
        m_validationStateManager->startValidation(1);
    }
}

void ComponentListViewModel::retryAllInvalidComponents() {
    int retryCount = 0;
    for (int i = 0; i < m_componentList.count(); ++i) {
        auto item = m_componentList.at(i);
        if (item && !item->isValid() && !item->isFetching()) {
            item->setFetching(true);
            item->setValid(false);  // 验证开始时先设置为 false
            item->setErrorMessage("");
            m_service->fetchComponentData(item->componentId(), false);
            retryCount++;
        }
    }

    // 两阶段控制
    if (retryCount > 0) {
        m_validationStateManager->startValidation(retryCount);
    }

    // 通知 QML 列表需要更新过滤状态
    emit filteredCountChanged();
}

void ComponentListViewModel::retryPreviewImage(const QString& componentId) {
    qDebug() << "Retry preview image for component:" << componentId;

    auto item = findItemData(componentId);
    if (item) {
        // 设置 isFetching 为 true
        item->setFetching(true);

        // 重新获取预览图
        m_service->fetchLcscPreviewImage(componentId);
    } else {
        qWarning() << "Component not found for preview image retry:" << componentId;
    }
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

void ComponentListViewModel::fetchAllPreviewImages() {
    QStringList validatedIds = m_validationStateManager->validatedComponentIds();
    if (validatedIds.isEmpty()) {
        qDebug() << "No validated components to fetch preview images for";
        return;
    }

    qDebug() << "Fetching preview images for" << validatedIds.count() << "validated components";

    // 使用批量获取方法获取所有验证通过元器件的预览图
    m_service->fetchBatchPreviewImages(validatedIds);
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
