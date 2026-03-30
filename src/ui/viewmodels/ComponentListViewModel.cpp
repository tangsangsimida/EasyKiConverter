#include "ComponentListViewModel.h"

#include "ui/utils/ThumbnailGenerator.h"
#include "ui/viewmodels/ThumbnailUpdateManager.h"
#include "ui/viewmodels/ValidationStateManager.h"

#include <QClipboard>
#include <QDebug>
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
    connect(m_validationStateManager, &ValidationStateManager::validationCompleted, this, [this]() {
        fetchAllPreviewImages();
    });

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
                        qDebug() << "First preview image (index 0) set as thumbnail for component:" << componentId;
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
    m_componentIdIndex.insert(trimmedId);  // 维护索引
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

    // 重置两阶段控制变量
    m_validationStateManager->startValidation(newIds.count());

    // 一次性插入所有新元器件，只触发 1 次 UI 重建
    int first = m_componentList.count();
    int last = first + newIds.count() - 1;
    beginInsertRows(QModelIndex(), first, last);

    for (const QString& id : newIds) {
        auto item = new ComponentListItemData(id, this);
        item->setFetching(true);
        item->setValid(false);  // 验证开始时先设置为 false
        m_componentList.append(item);
        m_componentIdIndex.insert(id);
    }

    endInsertRows();
    emit componentCountChanged();
    emit filteredCountChanged();

    // 延迟分散发送网络请求，避免同时回调集中到达卡 UI
    // 增加延迟间隔到 150ms，减少同时触发的网络请求数量
    for (int i = 0; i < newIds.count(); ++i) {
        const QString& id = newIds[i];
        // 每个请求间隔 150ms，避免回调集中在同一帧
        QTimer::singleShot(i * 150, this, [this, id]() { m_service->fetchComponentData(id, false); });
        emit componentAdded(id, true, "Component added");
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

    // 使用 ComponentService 解析 BOM 文件
    QStringList componentIds = m_service->parseBomFile(localPath);

    if (componentIds.isEmpty()) {
        m_bomResult = "No valid component IDs found in BOM file";
        qWarning() << "No component IDs found in BOM file:" << localPath;
        emit bomResultChanged();
        return;
    }

    // 使用批量添加方法
    QStringList newIds;
    int skipped = 0;

    for (const QString& id : componentIds) {
        if (!componentExists(id)) {
            newIds.append(id);
        } else {
            skipped++;
        }
    }

    if (!newIds.isEmpty()) {
        addComponentsBatch(newIds);
    }

    QString resultMsg = QString("BOM file imported: %1 components added, %2 skipped").arg(newIds.count()).arg(skipped);
    m_bomResult = resultMsg;
    qDebug() << resultMsg;
    emit bomResultChanged();
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
    for (const auto& item : m_componentList) {
        if (item->componentId() == componentId) {
            return item;
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
    if (item) {
        QSharedPointer<ComponentData> dataPtr = QSharedPointer<ComponentData>::create(data);
        item->setComponentData(dataPtr);
        item->setFetching(false);
        item->setValid(true);
        item->setErrorMessage("");

        // 更新 hasInvalidComponents 状态
        updateHasInvalidComponents();
        emit filteredCountChanged();

        // 两阶段控制：验证完成后通知管理器
        m_validationStateManager->onComponentValidated(componentId);

        // 异步生成缩略图 - 使用 QPointer 防止悬垂指针
        QPointer<ComponentListItemData> safeItem = item;
        QThreadPool::globalInstance()->start(QRunnable::create([safeItem, dataPtr]() {
            QImage thumbnail = ThumbnailGenerator::generateThumbnail(dataPtr);
            if (safeItem) {  // 检查对象是否仍然有效
                QMetaObject::invokeMethod(safeItem, [safeItem, thumbnail]() {
                    if (safeItem) {  // 再次检查，确保在 invokeMethod 执行时仍然有效
                        safeItem->setThumbnail(thumbnail);
                    }
                });
            }
        }));

        // 尝试获取 LCSC 预览图覆盖生成的缩略图
        // 注意：现在由 fetchAllPreviewImages() 统一控制获取时机

        // 添加到待更新集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);
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

        // 根据错误类型设置不同的错误消息
        if (error.contains("Request timeout", Qt::CaseInsensitive) || error.contains("timeout", Qt::CaseInsensitive)) {
            item->setErrorMessage("验证超时（网络不稳定）");
        } else if (error.contains("No result", Qt::CaseInsensitive) || error.contains("404") ||
                   error.contains("not found", Qt::CaseInsensitive)) {
            item->setErrorMessage("元件不存在");
        } else {
            item->setErrorMessage(error);
        }

        // 更新 hasInvalidComponents 状态
        updateHasInvalidComponents();
        emit filteredCountChanged();

        // 两阶段控制：验证失败也要计数
        m_validationStateManager->onComponentFailed(componentId);

        // 添加到防抖集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);
    }
}

void ComponentListViewModel::handleLcscDataUpdated(const QString& componentId,
                                                   const QString& manufacturerPart,
                                                   const QString& datasheetUrl,
                                                   const QStringList& imageUrls) {
    qDebug() << "LCSC data updated for component:" << componentId
             << "Manufacturer Part:" << (manufacturerPart.isEmpty() ? "none" : manufacturerPart)
             << "Datasheet:" << (datasheetUrl.isEmpty() ? "none" : datasheetUrl) << "Images:" << imageUrls.size();

    auto item = findItemData(componentId);
    if (item) {
        // 更新 ComponentData
        if (item->componentData()) {
            auto data = item->componentData();
            if (!manufacturerPart.isEmpty()) {
                data->setManufacturerPart(manufacturerPart);
                qDebug() << "Manufacturer part updated in ComponentData:" << manufacturerPart;
            }
            if (!datasheetUrl.isEmpty()) {
                data->setDatasheet(datasheetUrl);

                // 检测并同步数据手册格式
                QString format = "pdf";
                if (datasheetUrl.toLower().contains(".html")) {
                    format = "html";
                }
                data->setDatasheetFormat(format);

                qDebug() << "Datasheet URL updated in ComponentData:" << datasheetUrl << "format:" << format;
            }
            if (!imageUrls.isEmpty()) {
                data->setPreviewImages(imageUrls);
                qDebug() << "Preview images updated in ComponentData:" << imageUrls.size() << "images";
            }
        }

        // 更新显示名称（使用制造商部件号）
        if (!manufacturerPart.isEmpty()) {
            item->setName(manufacturerPart);
            qDebug() << "Display name updated to manufacturer part:" << manufacturerPart;
        }

        // 添加到防抖集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);
    } else {
        qWarning() << "Component" << componentId << "not found in list, cannot update LCSC data";
    }
}

void ComponentListViewModel::handleDatasheetReady(const QString& componentId, const QByteArray& datasheetData) {
    qDebug() << "Datasheet ready for component:" << componentId << "size:" << datasheetData.size() << "bytes";

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

            qDebug() << "Datasheet data updated in ComponentData, size:" << datasheetData.size() << "bytes"
                     << "format:" << format;
        }
    } else {
        qWarning() << "Component" << componentId << "not found in list, cannot update datasheet data";
    }
}

void ComponentListViewModel::handlePreviewImageDataReady(const QString& componentId,
                                                         const QByteArray& imageData,
                                                         int imageIndex) {
    qDebug() << "Preview image data ready for component:" << componentId << "index:" << imageIndex
             << "size:" << imageData.size() << "bytes";

    auto item = findItemData(componentId);
    if (item && item->componentData()) {
        // 保存数据到 ComponentListViewModel 的 ComponentData 中（用于导出）
        int currentCount = item->componentData()->previewImageData().size();
        item->componentData()->addPreviewImageData(imageData, imageIndex);
        int newCount = item->componentData()->previewImageData().size();
        qDebug() << "Preview image data saved to ComponentData, index:" << imageIndex << "count:" << currentCount
                 << "->" << newCount;
    }
}

void ComponentListViewModel::handlePreviewImageFailed(const QString& componentId, const QString& error) {
    qWarning() << "Preview image fetch failed for component:" << componentId << "error:" << error;

    auto item = findItemData(componentId);
    if (item) {
        // 设置 isFetching 为 false
        item->setFetching(false);

        // 不清除已生成的缩略图，保持显示默认缩略图
        qWarning() << "Preview image failed for component:" << componentId << "keeping default thumbnail";

        // 添加到待更新集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);
    }
}

void ComponentListViewModel::handleAllImagesReady(const QString& componentId, const QList<QByteArray>& imageDataList) {
    qDebug() << "All images ready for component:" << componentId << "count:" << imageDataList.size();

    auto item = findItemData(componentId);
    if (item && item->componentData()) {
        // 设置 isFetching 为 false
        item->setFetching(false);

        // 更新所有图片数据到 ComponentListViewModel 的 ComponentData 中
        item->componentData()->setPreviewImageData(imageDataList);
        qDebug() << "All image data updated in ComponentListViewModel for component:" << componentId
                 << "count:" << imageDataList.size();

        // 打印每张图片的大小
        for (int i = 0; i < imageDataList.size(); ++i) {
            qDebug() << "  Image" << i << "size:" << imageDataList[i].size() << "bytes";
        }

        // 添加到待更新集合
        m_thumbnailUpdateManager->scheduleUpdate(componentId);
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
