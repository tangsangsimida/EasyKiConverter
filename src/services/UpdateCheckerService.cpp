#include "UpdateCheckerService.h"

#include "core/network/NetworkClient.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace EasyKiConverter {

namespace {

constexpr auto RELEASES_URL = "https://api.github.com/repos/tangsangsimida/EasyKiConverter/releases/latest";

QStringList splitVersion(const QString& version) {
    return version.split('.', Qt::SkipEmptyParts);
}

}  // namespace

UpdateCheckerService::UpdateCheckerService(QObject* parent) : QObject(parent) {}

UpdateCheckerService::~UpdateCheckerService() {
    if (m_activeRequest) {
        m_activeRequest->cancel();
        m_activeRequest->deleteLater();
        m_activeRequest = nullptr;
    }
}

QString UpdateCheckerService::currentVersion() const {
    return normalizeVersion(QCoreApplication::applicationVersion());
}

void UpdateCheckerService::checkForUpdates() {
    if (m_checking) {
        return;
    }

    if (m_activeRequest) {
        m_activeRequest->cancel();
        m_activeRequest->deleteLater();
        m_activeRequest = nullptr;
    }

    setError(QString());
    setChecking(true);

    RetryPolicy policy;
    policy.maxRetries = 2;
    policy.baseTimeoutMs = 15000;

    m_activeRequest = NetworkClient::instance().getAsync(QUrl(QString::fromLatin1(RELEASES_URL)), policy);
    connect(m_activeRequest, &AsyncNetworkRequest::finished, this, [this](const NetworkResult& result) {
        if (m_activeRequest) {
            m_activeRequest->deleteLater();
            m_activeRequest = nullptr;
        }

        setChecking(false);

        if (result.wasCancelled) {
            return;
        }

        if (!result.success) {
            setError(result.error.isEmpty() ? QStringLiteral("Update check failed") : result.error);
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(result.data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            setError(QStringLiteral("Invalid release response"));
            return;
        }

        const QJsonObject object = doc.object();
        const QString latestVersion = normalizeVersion(object.value(QStringLiteral("tag_name")).toString());
        const QString releaseName = object.value(QStringLiteral("name")).toString();
        const QString releaseUrl = object.value(QStringLiteral("html_url")).toString();

        if (latestVersion.isEmpty() || releaseUrl.isEmpty()) {
            setError(QStringLiteral("Incomplete release metadata"));
            return;
        }

        applyUpdateInfo(latestVersion, releaseName, releaseUrl);
    });
}

void UpdateCheckerService::dismissUpdate() {
    if (!m_dismissed) {
        m_dismissed = true;
        emit updateStateChanged();
    }
}

void UpdateCheckerService::resetDismissed() {
    if (m_dismissed) {
        m_dismissed = false;
        emit updateStateChanged();
    }
}

QString UpdateCheckerService::normalizeVersion(const QString& version) {
    QString normalized = version.trimmed();
    normalized.remove(QRegularExpression(QStringLiteral("^[^0-9]*")));
    const QRegularExpression semverExpr(QStringLiteral("(\\d+)(?:\\.(\\d+))?(?:\\.(\\d+))?"));
    const QRegularExpressionMatch match = semverExpr.match(normalized);
    if (!match.hasMatch()) {
        return QString();
    }

    const QString major = match.captured(1);
    const QString minor = match.captured(2).isEmpty() ? QStringLiteral("0") : match.captured(2);
    const QString patch = match.captured(3).isEmpty() ? QStringLiteral("0") : match.captured(3);
    return QStringLiteral("%1.%2.%3").arg(major, minor, patch);
}

bool UpdateCheckerService::isRemoteVersionNewer(const QString& currentVersion, const QString& latestVersion) {
    const QStringList currentParts = splitVersion(currentVersion);
    const QStringList latestParts = splitVersion(latestVersion);
    const int maxParts = qMax(currentParts.size(), latestParts.size());

    for (int i = 0; i < maxParts; ++i) {
        const int currentPart = i < currentParts.size() ? currentParts.at(i).toInt() : 0;
        const int latestPart = i < latestParts.size() ? latestParts.at(i).toInt() : 0;
        if (latestPart > currentPart) {
            return true;
        }
        if (latestPart < currentPart) {
            return false;
        }
    }

    return false;
}

void UpdateCheckerService::setChecking(bool checking) {
    if (m_checking != checking) {
        m_checking = checking;
        emit checkingChanged();
    }
}

void UpdateCheckerService::applyUpdateInfo(const QString& latestVersion,
                                           const QString& releaseName,
                                           const QString& releaseUrl) {
    const bool oldHasUpdate = m_hasUpdate;
    const QString oldVersion = m_latestVersion;
    const QString oldReleaseName = m_releaseName;
    const QString oldReleaseUrl = m_releaseUrl;
    const bool oldDismissed = m_dismissed;

    m_latestVersion = latestVersion;
    m_releaseName = releaseName;
    m_releaseUrl = releaseUrl;
    m_hasUpdate = isRemoteVersionNewer(currentVersion(), latestVersion);

    if (!m_hasUpdate || oldVersion != latestVersion) {
        m_dismissed = false;
    }

    if (oldHasUpdate != m_hasUpdate || oldVersion != m_latestVersion || oldReleaseName != m_releaseName ||
        oldReleaseUrl != m_releaseUrl || oldDismissed != m_dismissed) {
        emit updateStateChanged();
    }
}

void UpdateCheckerService::setError(const QString& error) {
    if (m_error != error) {
        m_error = error;
        emit updateStateChanged();
    }
}

}  // namespace EasyKiConverter
