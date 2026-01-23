#include "ComponentListViewModel.h"
#include <QDebug>
#include <QGuiApplication>
#include <QClipboard>
#include <QRegularExpression>

namespace EasyKiConverter
{

    ComponentListViewModel::ComponentListViewModel(ComponentService *service, QObject *parent)
        : QObject(parent), m_service(service), m_componentList(), m_bomFilePath(), m_bomResult()
    {
        // 连接 Service 信号
        connect(m_service, &ComponentService::componentInfoReady, this, &ComponentListViewModel::handleComponentInfoReady);
        connect(m_service, &ComponentService::cadDataReady, this, &ComponentListViewModel::handleCadDataReady);
        connect(m_service, &ComponentService::model3DReady, this, &ComponentListViewModel::handleModel3DReady);
        connect(m_service, &ComponentService::fetchError, this, &ComponentListViewModel::handleFetchError);
    }

    ComponentListViewModel::~ComponentListViewModel()
    {
    }

    void ComponentListViewModel::addComponent(const QString &componentId)
    {
        QString trimmedId = componentId.trimmed();

        if (trimmedId.isEmpty())
        {
            qWarning() << "Component ID is empty";
            return;
        }

        // 验证元件ID格式
        if (!validateComponentId(trimmedId))
        {
            qWarning() << "Invalid component ID format:" << trimmedId;

            // 尝试从文本中智能提取元件编号
            QStringList extractedIds = extractComponentIdFromText(trimmedId);
            if (extractedIds.isEmpty())
            {
                qWarning() << "Failed to extract component ID from text:" << trimmedId;
                emit componentAdded(trimmedId, false, "Invalid LCSC component ID format");
                return;
            }

            // 添加提取的元件编�?
            for (const QString &id : extractedIds)
            {
                if (!componentExists(id))
                {
                    m_componentList.append(id);
                    emit componentListChanged();
                    emit componentCountChanged();
                    emit componentAdded(id, true, "Component added");
                }
            }
            return;
        }

        // 检查元件是否已存在
        if (componentExists(trimmedId))
        {
            qWarning() << "Component already exists:" << trimmedId;
            emit componentAdded(trimmedId, false, "Component already exists");
            return;
        }

        m_componentList.append(trimmedId);
        emit componentListChanged();
        emit componentCountChanged();
        emit componentAdded(trimmedId, true, "Component added");
    }

    void ComponentListViewModel::removeComponent(int index)
    {
        if (index >= 0 && index < m_componentList.count())
        {
            QString removedId = m_componentList.takeAt(index);
            emit componentListChanged();
            emit componentCountChanged();
            emit componentRemoved(removedId);
        }
    }

    void ComponentListViewModel::clearComponentList()
    {
        if (!m_componentList.isEmpty())
        {
            m_componentList.clear();
            emit componentListChanged();
            emit componentCountChanged();
            emit listCleared();
        }
    }

    void ComponentListViewModel::pasteFromClipboard()
    {
        QClipboard *clipboard = QGuiApplication::clipboard();
        QString text = clipboard->text();

        if (text.isEmpty())
        {
            qWarning() << "Clipboard is empty";
            return;
        }

        QStringList extractedIds = extractComponentIdFromText(text);
        if (extractedIds.isEmpty())
        {
            qWarning() << "No valid component IDs found in clipboard";
            emit pasteCompleted(0, 0);
            return;
        }

        int added = 0;
        int skipped = 0;

        for (const QString &id : extractedIds)
        {
            if (!componentExists(id))
            {
                m_componentList.append(id);
                added++;
            }
            else
            {
                skipped++;
            }
        }

        if (added > 0)
        {
            emit componentListChanged();
            emit componentCountChanged();
        }

        emit pasteCompleted(added, skipped);
    }

    void ComponentListViewModel::selectBomFile(const QString &filePath)
    {
        qDebug() << "BOM file selected:" << filePath;

        if (m_bomFilePath != filePath)
        {
            m_bomFilePath = filePath;
            emit bomFilePathChanged();
        }

        // TODO: 实现 BOM 文件解析逻辑
        m_bomResult = "BOM file imported successfully";
        emit bomResultChanged();
    }

    void ComponentListViewModel::handleComponentDataFetched(const QString &componentId, const ComponentData &data)
    {
        qDebug() << "Component data fetched for:" << componentId;
        // TODO: 更新 UI 显示
    }

    void ComponentListViewModel::handleComponentDataFetchFailed(const QString &componentId, const QString &error)
    {
        qWarning() << "Component data fetch failed for:" << componentId << "Error:" << error;
        // TODO: 更新 UI 显示
    }

    void ComponentListViewModel::fetchComponentData(const QString &componentId, bool fetch3DModel)
    {
        qDebug() << "Fetching component data for:" << componentId;
        m_service->setOutputPath(m_outputPath);
        m_service->fetchComponentData(componentId, fetch3DModel);
    }

    void ComponentListViewModel::setOutputPath(const QString &path)
    {
        if (m_outputPath != path)
        {
            m_outputPath = path;
            emit outputPathChanged();
        }
    }

    bool ComponentListViewModel::validateComponentId(const QString &componentId) const
    {
        // LCSC 元件ID格式：以 'C' 开头，后面跟数�?
        QRegularExpression re("^C\\d+$");
        return re.match(componentId).hasMatch();
    }

    QStringList ComponentListViewModel::extractComponentIdFromText(const QString &text) const
    {
        QStringList extractedIds;

        // 匹配 LCSC 元件ID格式：以 'C' 开头，后面跟数�?
        QRegularExpression re("C\\d+");
        QRegularExpressionMatchIterator it = re.globalMatch(text);

        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            QString id = match.captured();
            if (!extractedIds.contains(id))
            {
                extractedIds.append(id);
            }
        }

        return extractedIds;
    }

    bool ComponentListViewModel::componentExists(const QString &componentId) const
    {
        return m_componentList.contains(componentId);
    }

    void ComponentListViewModel::handleComponentInfoReady(const QString &componentId, const ComponentData &data)
    {
        qDebug() << "Component info ready for:" << componentId;
        // 可以在这里更�?UI 显示
    }

    void ComponentListViewModel::handleCadDataReady(const QString &componentId, const ComponentData &data)
    {
        qDebug() << "CAD data ready for:" << componentId;
        // 可以在这里更�?UI 显示
    }

    void ComponentListViewModel::handleModel3DReady(const QString &uuid, const QString &filePath)
    {
        qDebug() << "3D model ready for UUID:" << uuid << "at:" << filePath;
        // 可以在这里更�?UI 显示
    }

    void ComponentListViewModel::handleFetchError(const QString &componentId, const QString &error)
    {
        qWarning() << "Fetch error for:" << componentId << "-" << error;
        // 可以在这里更�?UI 显示
    }

} // namespace EasyKiConverter
