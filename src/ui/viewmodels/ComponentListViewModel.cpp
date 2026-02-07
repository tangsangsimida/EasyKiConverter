#include "ComponentListViewModel.h"

#include "ui/utils/ThumbnailGenerator.h"

#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QRunnable>
#include <QThreadPool>
#include <QUrl>
#include <QtConcurrent>

namespace EasyKiConverter {

ComponentListViewModel::ComponentListViewModel(ComponentService* service, QObject* parent)
    : QObject(parent), m_service(service), m_componentList(), m_bomFilePath(), m_bomResult() {
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

QQmlListProperty<ComponentListItemData> ComponentListViewModel::componentList() {
    return QQmlListProperty<ComponentListItemData>(this,
                                                   &m_componentList,
                                                   &ComponentListViewModel::appendComponent,
                                                   &ComponentListViewModel::componentCount,
                                                   &ComponentListViewModel::componentAt,
                                                   &ComponentListViewModel::clearComponents);
}

void ComponentListViewModel::appendComponent(QQmlListProperty<ComponentListItemData>* list, ComponentListItemData* p) {
    ComponentListViewModel* viewModel = qobject_cast<ComponentListViewModel*>(list->object);
    if (viewModel && p) {
        viewModel->m_componentList.append(p);
        emit viewModel->componentListChanged();
        emit viewModel->componentCountChanged();
    }
}

qsizetype ComponentListViewModel::componentCount(QQmlListProperty<ComponentListItemData>* list) {
    ComponentListViewModel* viewModel = qobject_cast<ComponentListViewModel*>(list->object);
    return viewModel ? viewModel->m_componentList.count() : 0;
}

ComponentListItemData* ComponentListViewModel::componentAt(QQmlListProperty<ComponentListItemData>* list, qsizetype i) {
    ComponentListViewModel* viewModel = qobject_cast<ComponentListViewModel*>(list->object);
    return viewModel ? viewModel->m_componentList.at(i) : nullptr;
}

void ComponentListViewModel::clearComponents(QQmlListProperty<ComponentListItemData>* list) {
    ComponentListViewModel* viewModel = qobject_cast<ComponentListViewModel*>(list->object);
    if (viewModel) {
        viewModel->clearComponentList();
    }
}

void ComponentListViewModel::addComponent(const QString& componentId) {
    QString trimmedId = componentId.trimmed();

    if (trimmedId.isEmpty()) {
        qWarning() << "Component ID is empty";
        return;
    }

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

        // 添加提取的元件编号
        for (const QString& id : extractedIds) {
            if (!componentExists(id)) {
                auto item = new ComponentListItemData(id, this);
                item->setFetching(true);  // 开始添加时标记为正在获取
                m_componentList.append(item);

                // 触发获取数据
                m_service->fetchComponentData(id, false);  // 验证时不获取 3D 模型以加快速度

                emit componentListChanged();
                emit componentCountChanged();
                emit componentAdded(id, true, "Component added");
            }
        }
        return;
    }

    // 检查元件是否已存在
    if (componentExists(trimmedId)) {
        qWarning() << "Component already exists:" << trimmedId;
        emit componentAdded(trimmedId, false, "Component already exists");
        return;
    }

    auto item = new ComponentListItemData(trimmedId, this);
    item->setFetching(true);
    m_componentList.append(item);

    // 触发获取数据
    m_service->fetchComponentData(trimmedId, false);

    emit componentListChanged();
    emit componentCountChanged();
    emit componentAdded(trimmedId, true, "Component added");
}

void ComponentListViewModel::removeComponent(int index) {
    if (index >= 0 && index < m_componentList.count()) {
        auto item = m_componentList.takeAt(index);
        QString removedId = item->componentId();
        delete item;
        emit componentListChanged();
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
        qDeleteAll(m_componentList);
        m_componentList.clear();
        emit componentListChanged();
        emit componentCountChanged();
        emit listCleared();
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

    int added = 0;
    int skipped = 0;

    for (const QString& id : extractedIds) {
        if (!componentExists(id)) {
            addComponent(id);  // 使用 addComponent 处理添加和验证逻辑
            added++;
        } else {
            skipped++;
        }
    }

    // addComponent 已发出变更信号
    emit pasteCompleted(added, skipped);
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

    // 添加提取的元件编号到列表
    int added = 0;
    int skipped = 0;

    for (const QString& id : componentIds) {
        if (!componentExists(id)) {
            addComponent(id);  // 使用 addComponent 触发验证
            added++;
        } else {
            skipped++;
        }
    }

    QString resultMsg = QString("BOM file imported: %1 components added, %2 skipped").arg(added).arg(skipped);
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
    // LCSC 元件ID格式：以 'C' 开头，后面跟至少4位数字
    QRegularExpression re("^C\\d{4,}$");
    return re.match(componentId).hasMatch();
}

QStringList ComponentListViewModel::extractComponentIdFromText(const QString& text) const {
    QStringList extractedIds;

    // 匹配 LCSC 元件ID格式：以 'C' 开头，后面跟至少4位数字
    QRegularExpression re("C\\d{4,}");
    QRegularExpressionMatchIterator it = re.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString id = match.captured();
        if (!extractedIds.contains(id)) {
            extractedIds.append(id);
        }
    }

    return extractedIds;
}

bool ComponentListViewModel::componentExists(const QString& componentId) const {
    for (const auto& item : m_componentList) {
        if (item->componentId() == componentId) {
            return true;
        }
    }
    return false;
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

        // 异步生成缩略图
        QThreadPool::globalInstance()->start(QRunnable::create([item, dataPtr]() {
            QImage thumbnail = ThumbnailGenerator::generateThumbnail(dataPtr);
            QMetaObject::invokeMethod(item, [item, thumbnail]() { item->setThumbnail(thumbnail); });
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

QSharedPointer<ComponentData> ComponentListViewModel::getPreloadedData(const QString& componentId) const {
    auto item = findItemData(componentId);
    if (item && item->isValid() && item->componentData()) {
        return item->componentData();
    }
    return nullptr;
}

}  // namespace EasyKiConverter