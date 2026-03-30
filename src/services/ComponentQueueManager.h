#ifndef COMPONENTQUEUEMANAGER_H
#define COMPONENTQUEUEMANAGER_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>

namespace EasyKiConverter {

class ComponentQueueManager : public QObject {
    Q_OBJECT
public:
    explicit ComponentQueueManager(int maxConcurrent = 5, QObject* parent = nullptr);
    ~ComponentQueueManager() override;

    void start(const QStringList& componentIds);
    void stop();
    void checkAndProcessNext();
    void reset();

    int activeRequestCount() const;
    bool isRunning() const;
    int maxConcurrentRequests() const;

    static const int QUEUE_CHECK_INTERVAL_MS = 500;
    static const int TOTAL_TIMEOUT_MS = 300000;

signals:
    void requestReady(const QString& componentId);
    void queueEmpty();
    void timeout();
    void progress(int completed, int total);

private slots:
    void onCheckTimeout();
    void onTotalTimeout();

private:
    QStringList m_requestQueue;
    QSet<QString> m_pendingComponents;
    int m_maxConcurrentRequests;
    QTimer* m_checkTimer;
    QTimer* m_totalTimer;
    bool m_isRunning;
};

}  // namespace EasyKiConverter

#endif  // COMPONENTQUEUEMANAGER_H
