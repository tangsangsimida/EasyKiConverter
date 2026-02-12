#include "ComponentListViewModel.h"

#include "ui/utils/ThumbnailGenerator.h"

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
    // 连接 Service 信号
    connect(m_service, &ComponentService::componentInfoReady, this, &ComponentListViewModel::handleComponentInfoReady);
    connect(m_service, &ComponentService::cadDataReady, this, &ComponentListViewModel::handleCadDataReady);
    connect(m_service, &ComponentService::model3DReady, this, &ComponentListViewModel::handleModel3DReady);
    connect(
        m_service, &ComponentService::previewImageReady, this, [this](const QString& componentId, const QImage& image) {
            auto item = findItemData(componentId);
            if (item) {
                item->setThumbnail(image);
            }
        });
    connect(m_service, &ComponentService::fetchError, this, &ComponentListViewModel::handleFetchError);
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

        // 添加提取的元件编号 - 使用批量添加
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
    m_componentList.append(item);
    m_componentIdIndex.insert(trimmedId);  // 维护索引
    endInsertRows();

    // 触发获取数据
    m_service->fetchComponentData(trimmedId, false);

    emit componentCountChanged();
    emit componentAdded(trimmedId, true, "Component added");
}

void ComponentListViewModel::removeComponent(int index) {
    if (index >= 0 && index < m_componentList.count()) {
        beginRemoveRows(QModelIndex(), index, index);
        auto item = m_componentList.takeAt(index);
        QString removedId = item->componentId();
        m_componentIdIndex.remove(removedId);  // 维护索引
        delete item;
        endRemoveRows();

        emit componentCountChanged();
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
        beginResetModel();
        qDeleteAll(m_componentList);
        m_componentList.clear();
        m_componentIdIndex.clear();  // 清空索引
        endResetModel();

        emit componentCountChanged();
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

    // 一次性插入所有新元器件，只触发 1 次 UI 重建
    int first = m_componentList.count();
    int last = first + newIds.count() - 1;
    beginInsertRows(QModelIndex(), first, last);

    for (const QString& id : newIds) {
        auto item = new ComponentListItemData(id, this);
        item->setFetching(true);
        m_componentList.append(item);
        m_componentIdIndex.insert(id);
    }

    endInsertRows();
    emit componentCountChanged();

    // 延迟分散发送网络请求，避免同时回调集中到达卡 UI
    for (int i = 0; i < newIds.count(); ++i) {
        const QString& id = newIds[i];
        // 每个请求间隔 50ms，避免回调集中在同一帧
        QTimer::singleShot(i * 50, this, [this, id]() { m_service->fetchComponentData(id, false); });
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
        m_service->fetchLcscPreviewImage(componentId);
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
        // 如果是严重错误（如404），标记为无效
        if (error.contains("404") || error.contains("not found", Qt::CaseInsensitive)) {
            item->setValid(false);
            item->setErrorMessage("Not Found");
        } else {
            item->setErrorMessage(error);
        }
    }
}

void ComponentListViewModel::refreshComponentInfo(int index) {
    if (index >= 0 && index < m_componentList.count()) {
        auto item = m_componentList.at(index);
        item->setFetching(true);
        item->setErrorMessage("");
        m_service->fetchComponentData(item->componentId(), false);
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

}  // namespace EasyKiConverter
