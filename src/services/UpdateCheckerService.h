#ifndef UPDATECHECKERSERVICE_H
#define UPDATECHECKERSERVICE_H

#include "core/network/AsyncNetworkRequest.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

namespace EasyKiConverter {

class INetworkClient;

/**
 * @brief 管理 GitHub Release 更新检查、缓存和用户提醒策略。
 *
 * 网络请求统一通过 INetworkClient 注入，默认使用项目的 NetworkClient 单例；
 * 测试可以注入 MockNetworkClient，避免访问真实 GitHub 服务。
 */
class UpdateCheckerService : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
    Q_PROPERTY(bool hasUpdate READ hasUpdate NOTIFY updateStateChanged)
    Q_PROPERTY(bool dismissed READ dismissed NOTIFY updateStateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QString releaseName READ releaseName NOTIFY updateStateChanged)
    Q_PROPERTY(QString releaseUrl READ releaseUrl NOTIFY updateStateChanged)
    Q_PROPERTY(QString latestReleasePageUrl READ latestReleasePageUrl CONSTANT)
    Q_PROPERTY(QString assetUrl READ assetUrl NOTIFY updateStateChanged)
    Q_PROPERTY(QString error READ error NOTIFY updateStateChanged)
    Q_PROPERTY(bool rateLimited READ rateLimited NOTIFY updateStateChanged)
    Q_PROPERTY(Status status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(bool autoCheckEnabled READ autoCheckEnabled WRITE setAutoCheckEnabled NOTIFY autoCheckEnabledChanged)
    Q_PROPERTY(qint64 lastCheckTime READ lastCheckTime NOTIFY metadataChanged)
    Q_PROPERTY(qint64 lastSuccessfulCheckTime READ lastSuccessfulCheckTime NOTIFY metadataChanged)

public:
    /** @brief 更新检查状态。 */
    enum class Status {
        NotChecked,
        Checking,
        UpdateAvailable,
        UpToDate,
        Failed,
        Ignored,
    };
    Q_ENUM(Status)

    /** @brief 创建使用项目默认网络客户端的更新服务。 */
    explicit UpdateCheckerService(QObject* parent = nullptr);

    /** @brief 创建可注入测试网络客户端的更新服务。 */
    explicit UpdateCheckerService(INetworkClient& networkClient, QObject* parent = nullptr);
    ~UpdateCheckerService() override;

    /** @brief 返回当前是否正在检查。 */
    bool checking() const {
        return m_checking;
    }

    /** @brief 返回是否存在高于当前版本的 Release。 */
    bool hasUpdate() const {
        return m_hasUpdate;
    }

    /** @brief 返回当前 Release 是否已被关闭提醒。 */
    bool dismissed() const {
        return m_dismissed;
    }

    /** @brief 返回应用当前版本。 */
    QString currentVersion() const;

    /** @brief 返回最新 Release 版本。 */
    QString latestVersion() const {
        return m_latestVersion;
    }

    /** @brief 返回最新 Release 名称。 */
    QString releaseName() const {
        return m_releaseName;
    }

    /** @brief 返回 Release 页面地址。 */
    QString releaseUrl() const {
        return m_releaseUrl;
    }

    /** @brief 返回官方最新 Release 页面地址，供检查失败时人工访问。 */
    QString latestReleasePageUrl() const {
        return QStringLiteral("https://github.com/EasyKiconverter/EasyKiConverter/releases/latest");
    }

    /** @brief 返回匹配平台的资产地址。 */
    QString assetUrl() const {
        return m_assetUrl;
    }

    /** @brief 返回最近一次检查错误。 */
    QString error() const {
        return m_error;
    }

    /** @brief 返回最近一次检查是否被 GitHub 访问限制。 */
    bool rateLimited() const {
        return m_rateLimited;
    }

    /** @brief 返回当前检查状态。 */
    Status status() const {
        return m_status;
    }

    /** @brief 返回供 QML 使用的状态键。 */
    QString statusText() const;
    /** @brief 返回是否启用启动时自动检查。 */
    bool autoCheckEnabled() const;
    /** @brief 返回最近一次检查时间。 */
    qint64 lastCheckTime() const;
    /** @brief 返回最近一次成功检查时间。 */
    qint64 lastSuccessfulCheckTime() const;

    /** @brief 检查更新；force 为 true 时绕过自动检查间隔。 */
    Q_INVOKABLE void checkForUpdates(bool force = true);
    /** @brief 稍后提醒，重启后仍保持当前版本的提醒状态。 */
    Q_INVOKABLE void dismissUpdate();
    /** @brief 忽略当前 Release，直到出现更高版本。 */
    Q_INVOKABLE void ignoreUpdate();
    /** @brief 清除当前内存中的关闭状态。 */
    Q_INVOKABLE void resetDismissed();
    /** @brief 设置是否启用启动时自动检查。 */
    Q_INVOKABLE void setAutoCheckEnabled(bool enabled);
    /** @brief 手动读取最近一次缓存的 Release 信息。 */
    Q_INVOKABLE void loadCachedRelease();

    /** @brief 规范化并校验 SemVer 字符串。 */
    static QString normalizeVersion(const QString& version);
    /** @brief 按 SemVer 规则判断远程版本是否高于当前版本。 */
    static bool isRemoteVersionNewer(const QString& currentVersion, const QString& latestVersion);

signals:
    void checkingChanged();
    void updateStateChanged();
    void statusChanged();
    void autoCheckEnabledChanged();
    void metadataChanged();

private:
    void setChecking(bool checking);
    void setStatus(Status status);
    void setError(const QString& error);
    bool applyRelease(const QJsonObject& release, bool fromCache);
    bool validateRelease(const QJsonObject& release, QString* reason) const;
    void applyUpdateInfo(const QString& latestVersion,
                         const QString& releaseName,
                         const QString& releaseUrl,
                         const QString& assetUrl);

    INetworkClient* m_networkClient{nullptr};
    AsyncNetworkRequest* m_activeRequest{nullptr};
    bool m_checking{false};
    bool m_hasUpdate{false};
    bool m_dismissed{false};
    QString m_latestVersion;
    QString m_releaseName;
    QString m_releaseUrl;
    QString m_assetUrl;
    QString m_error;
    bool m_rateLimited{false};
    Status m_status{Status::NotChecked};
};

}  // namespace EasyKiConverter

#endif  // UPDATECHECKERSERVICE_H
