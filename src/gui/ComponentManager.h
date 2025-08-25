#ifndef COMPONENTMANAGER_H
#define COMPONENTMANAGER_H

#include <QObject>
#include <QStringList>
#include <QListWidget>

class ComponentManager : public QObject
{
    Q_OBJECT

public:
    explicit ComponentManager(QObject *parent = nullptr);
    
    // 组件列表操作
    void addComponent(const QString& componentId);
    void removeComponent(const QString& componentId);
    void removeComponentAt(int index);
    void clearComponents();
    QStringList getComponents() const;
    int count() const;
    
    // 批量操作
    void addComponents(const QStringList& componentIds);
    void setComponents(const QStringList& componentIds);
    
    // 从文本解析组件（支持逗号分隔）
    void parseAndAddComponents(const QString& text);
    
signals:
    void componentsChanged();

private:
    QStringList components;
};

#endif // COMPONENTMANAGER_H