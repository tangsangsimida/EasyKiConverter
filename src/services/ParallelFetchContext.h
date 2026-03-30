#ifndef PARALLELFETCHCONTEXT_H
#define PARALLELFETCHCONTEXT_H

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

class ComponentData;

class ParallelFetchContext : public QObject {
    Q_OBJECT
public:
    explicit ParallelFetchContext(QObject* parent = nullptr);
    ~ParallelFetchContext() override;

    void start(int totalCount);
    void markCompleted(const QString& componentId, const ComponentData& data);
    void markFailed(const QString& componentId);

    bool isAllDone() const;
    int completedCount() const;
    int totalCount() const;
    QList<ComponentData> collectedData() const;

signals:
    void allCompleted(const QList<ComponentData>& data);

private:
    void checkCompletion();

private:
    QMap<QString, ComponentData> m_collectedData;
    int m_totalCount;
    int m_completedCount;
    bool m_isAllDone;
    mutable QMutex m_mutex;
};

}  // namespace EasyKiConverter

#endif  // PARALLELFETCHCONTEXT_H