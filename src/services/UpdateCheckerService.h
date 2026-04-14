#ifndef UPDATECHECKERSERVICE_H
#define UPDATECHECKERSERVICE_H

#include "core/network/AsyncNetworkRequest.h"

#include <QObject>
#include <QString>

namespace EasyKiConverter {

class UpdateCheckerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool hasUpdate READ hasUpdate NOTIFY updateStateChanged)
    Q_PROPERTY(bool dismissed READ dismissed NOTIFY updateStateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString releaseName READ releaseName NOTIFY updateStateChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY updateStateChanged)
    Q_PROPERTY(QString error READ error NOTIFY updateStateChanged)

public:
    explicit UpdateCheckerService(QObject* parent = nullptr);
    ~UpdateCheckerService() override;

    bool checking() const {
        return m_checking;
    }

    bool hasUpdate() const {
        return m_hasUpdate;
    }

    bool dismissed() const {
        return m_dismissed;
    }

    QString currentVersion() const;
    QString latestVersion() const {
        return m_latestVersion;
    }
    QString releaseName() const {
        return m_releaseName;
    }
    QString releaseUrl() const {
        return m_releaseUrl;
    }
    QString error() const {
        return m_error;
    }

    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void dismissUpdate();
    Q_INVOKABLE void resetDismissed();

signals:
    void checkingChanged();
    void updateStateChanged();

private:
    static QString normalizeVersion(const QString& version);
    static bool isRemoteVersionNewer(const QString& currentVersion, const QString& latestVersion);
    void setChecking(bool checking);
    void applyUpdateInfo(const QString& latestVersion, const QString& releaseName, const QString& releaseUrl);
    void setError(const QString& error);

private:
    AsyncNetworkRequest* m_activeRequest{nullptr};
    bool m_checking{false};
    bool m_hasUpdate{false};
    bool m_dismissed{false};
    QString m_latestVersion;
    QString m_releaseName;
    QString m_releaseUrl;
    QString m_error;
};

}  // namespace EasyKiConverter

#endif  // UPDATECHECKERSERVICE_H
