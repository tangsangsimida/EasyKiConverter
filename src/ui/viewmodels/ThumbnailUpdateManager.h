#ifndef THUMBNAILUPDATEMANAGER_H
#define THUMBNAILUPDATEMANAGER_H

#include <QObject>
#include <QSet>
#include <QString>
#include <QTimer>

namespace EasyKiConverter {

class ThumbnailUpdateManager : public QObject {
    Q_OBJECT
public:
    explicit ThumbnailUpdateManager(QObject* parent = nullptr);
    ~ThumbnailUpdateManager() override;

    void scheduleUpdate(const QString& componentId);
    void scheduleUpdates(const QSet<QString>& componentIds);
    void clear();
    void setFlushCallback(std::function<void(const QSet<QString>&)> callback);
    bool hasPendingUpdates() const;
    int pendingCount() const;

private:
    void onTimerTimeout();

private:
    QTimer* m_timer;
    QSet<QString> m_pendingIndices;
    std::function<void(const QSet<QString>&)> m_flushCallback;
};

}  // namespace EasyKiConverter

#endif  // THUMBNAILUPDATEMANAGER_H