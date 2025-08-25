#include "ComponentManager.h"
#include <QString>

ComponentManager::ComponentManager(QObject *parent)
    : QObject(parent)
{
}

void ComponentManager::addComponent(const QString& componentId)
{
    QString trimmedId = componentId.trimmed();
    if (!trimmedId.isEmpty() && !components.contains(trimmedId)) {
        components.append(trimmedId);
        emit componentsChanged();
    }
}

void ComponentManager::removeComponent(const QString& componentId)
{
    components.removeAll(componentId);
    emit componentsChanged();
}

void ComponentManager::removeComponentAt(int index)
{
    if (index >= 0 && index < components.size()) {
        components.removeAt(index);
        emit componentsChanged();
    }
}

void ComponentManager::clearComponents()
{
    components.clear();
    emit componentsChanged();
}

QStringList ComponentManager::getComponents() const
{
    return components;
}

int ComponentManager::count() const
{
    return components.size();
}

void ComponentManager::addComponents(const QStringList& componentIds)
{
    bool changed = false;
    for (const QString& id : componentIds) {
        QString trimmedId = id.trimmed();
        if (!trimmedId.isEmpty() && !components.contains(trimmedId)) {
            components.append(trimmedId);
            changed = true;
        }
    }
    
    if (changed) {
        emit componentsChanged();
    }
}

void ComponentManager::setComponents(const QStringList& componentIds)
{
    components.clear();
    addComponents(componentIds);
}

void ComponentManager::parseAndAddComponents(const QString& text)
{
    // 修复Qt版本兼容性问题
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QStringList ids = text.split(",", Qt::SkipEmptyParts);
#else
    QStringList ids = text.split(",", QString::SkipEmptyParts);
#endif
    
    addComponents(ids);
}