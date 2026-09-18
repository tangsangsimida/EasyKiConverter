#include "UpdateCheckerService.h"

#include "ConfigService.h"
#include "core/network/INetworkClient.h"
#include "core/network/NetworkClient.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUrl>
#include <QXmlStreamReader>

namespace EasyKiConverter {

namespace {

constexpr auto RELEASES_URL = "https://api.github.com/repos/tangsangsimida/EasyKiConverter/releases/latest";
constexpr auto RELEASES_ATOM_URL = "https://github.com/tangsangsimida/EasyKiConverter/releases.atom";

struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
    QStringList prerelease;
    bool valid = false;
};

// 解析并校验更新服务使用的 SemVer 字符串。
SemanticVersion parseVersion(const QString& value) {
    const QRegularExpression expression(
        QStringLiteral("^v?(\\d+)(?:\\.(\\d+))?(?:\\.(\\d+))?(?:-([0-9A-Za-z-]+(?:\\.[0-9A-Za-z-]+)*))?(?:\\+[0-9A-Za-"
                       "z-]+(?:\\.[0-9A-Za-z-]+)*)?$"));
    const QRegularExpressionMatch match = expression.match(value.trimmed());
    if (!match.hasMatch())
        return {};

    SemanticVersion result;
    result.major = match.captured(1).toInt();
    result.minor = match.captured(2).isEmpty() ? 0 : match.captured(2).toInt();
    result.patch = match.captured(3).isEmpty() ? 0 : match.captured(3).toInt();
    result.prerelease = match.captured(4).isEmpty() ? QStringList() : match.captured(4).split('.');
    result.valid = true;
    return result;
}

// 按 SemVer 规则比较两个已解析的版本。
int compareVersions(const SemanticVersion& left, const SemanticVersion& right) {
    if (left.major != right.major)
        return left.major < right.major ? -1 : 1;
    if (left.minor != right.minor)
        return left.minor < right.minor ? -1 : 1;
    if (left.patch != right.patch)
        return left.patch < right.patch ? -1 : 1;
    if (left.prerelease.isEmpty() && right.prerelease.isEmpty())
        return 0;
    if (left.prerelease.isEmpty())
        return 1;
    if (right.prerelease.isEmpty())
        return -1;

    const QRegularExpression numeric(QStringLiteral("^[0-9]+$"));
    const int count = qMax(left.prerelease.size(), right.prerelease.size());
    for (int index = 0; index < count; ++index) {
        if (index >= left.prerelease.size())
            return -1;
        if (index >= right.prerelease.size())
            return 1;
        const QString& leftPart = left.prerelease.at(index);
        const QString& rightPart = right.prerelease.at(index);
        const bool leftNumeric = numeric.match(leftPart).hasMatch();
        const bool rightNumeric = numeric.match(rightPart).hasMatch();
        if (leftNumeric && rightNumeric && leftPart.toLongLong() != rightPart.toLongLong())
            return leftPart.toLongLong() < rightPart.toLongLong() ? -1 : 1;
        if (leftNumeric != rightNumeric)
            return leftNumeric ? -1 : 1;
        if (leftPart != rightPart)
            return leftPart < rightPart ? -1 : 1;
    }
    return 0;
}

// 返回当前平台支持的资产架构名称候选。
QStringList assetArchitectureTokens() {
    QStringList preferred;
#if defined(Q_OS_WIN)
    preferred << (QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm"), Qt::CaseInsensitive)
                      ? QStringList{QStringLiteral("arm64")}
                      : QStringList{QStringLiteral("x64"), QStringLiteral("win64"), QStringLiteral("amd64")});
#elif defined(Q_OS_MACOS)
    preferred << (QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm"), Qt::CaseInsensitive)
                      ? QStringList{QStringLiteral("arm64")}
                      : QStringList{QStringLiteral("intel"), QStringLiteral("x64")});
#elif defined(Q_OS_LINUX)
    preferred << (QSysInfo::currentCpuArchitecture().contains(QStringLiteral("arm"), Qt::CaseInsensitive)
                      ? QStringList{QStringLiteral("aarch64"), QStringLiteral("arm64")}
                      : QStringList{QStringLiteral("x86_64"), QStringLiteral("amd64")});
#endif
    return preferred;
}

// 判断资产扩展名是否属于当前平台的可分发格式。
bool isPlatformAssetName(const QString& name) {
#if defined(Q_OS_WIN)
    return name.endsWith(QStringLiteral(".zip")) || name.endsWith(QStringLiteral(".msix"));
#elif defined(Q_OS_MACOS)
    return name.endsWith(QStringLiteral(".dmg"));
#elif defined(Q_OS_LINUX)
    return name.endsWith(QStringLiteral(".appimage"));
#else
    Q_UNUSED(name);
    return false;
#endif
}

// 从 Release 资产中选择当前平台和架构的下载地址。
QString chooseAsset(const QJsonArray& assets) {
    const QStringList preferred = assetArchitectureTokens();

    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString().toLower();
        const QString url = asset.value(QStringLiteral("browser_download_url")).toString();
        if (url.isEmpty() || !isPlatformAssetName(name))
            continue;
        for (const QString& token : preferred) {
            if (name.contains(token))
                return url;
        }
    }
    return QString();
}

// 将 GitHub Releases Atom 响应转换为更新服务使用的最小 Release 对象。
QJsonObject parseAtomRelease(const QByteArray& data) {
    QXmlStreamReader reader(data);
    QString title;
    QString releaseUrl;
    bool inEntry = false;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == QStringLiteral("entry")) {
            inEntry = true;
            title.clear();
            releaseUrl.clear();
            continue;
        }
        if (inEntry && reader.isEndElement() && reader.name() == QStringLiteral("entry")) {
            const QRegularExpression tagExpression(
                QStringLiteral("/tag/(v?[0-9]+\\.[0-9]+\\.[0-9]+(?:-[0-9A-Za-z.-]+)?)$"));
            const QRegularExpressionMatch match = tagExpression.match(QUrl(releaseUrl).path());
            if (match.hasMatch() && !title.isEmpty() && !releaseUrl.isEmpty()) {
                const QString tag = match.captured(1);
                return QJsonObject{{QStringLiteral("tag_name"), tag},
                                   {QStringLiteral("name"), title},
                                   {QStringLiteral("html_url"), releaseUrl},
                                   {QStringLiteral("draft"), false},
                                   {QStringLiteral("prerelease"), tag.contains(QLatin1Char('-'))},
                                   {QStringLiteral("assets"), QJsonArray{}}};
            }
            inEntry = false;
            continue;
        }
        if (!inEntry || !reader.isStartElement())
            continue;
        if (reader.name() == QStringLiteral("title")) {
            title = reader.readElementText().trimmed();
        } else if (reader.name() == QStringLiteral("link")) {
            const auto attributes = reader.attributes();
            if (attributes.value(QStringLiteral("rel")) == QStringLiteral("alternate"))
                releaseUrl = attributes.value(QStringLiteral("href")).toString();
        }
    }
    return {};
}

}  // namespace

// 创建更新服务并载入最近一次有效的本地缓存。
UpdateCheckerService::UpdateCheckerService(QObject* parent)
    // 使用项目默认网络客户端保持原有构造方式兼容。
    : QObject(parent), m_networkClient(&NetworkClient::instance()) {
    loadCachedRelease();
}

// 创建更新服务并载入最近一次有效的本地缓存。
UpdateCheckerService::UpdateCheckerService(INetworkClient& networkClient, QObject* parent)
    // 初始化网络客户端并保持可测试的依赖注入边界。
    : QObject(parent), m_networkClient(&networkClient) {
    loadCachedRelease();
}

// 释放正在进行的请求，避免服务销毁后继续回调。
UpdateCheckerService::~UpdateCheckerService() {
    if (m_activeRequest) {
        m_activeRequest->cancel();
        m_activeRequest->deleteLater();
        m_activeRequest = nullptr;
    }
}

// 返回由应用元数据提供并经过规范化的当前版本。
QString UpdateCheckerService::currentVersion() const {
    return normalizeVersion(QCoreApplication::applicationVersion());
}

// 返回供 QML 稳定判断的状态键。
QString UpdateCheckerService::statusText() const {
    // 将内部枚举映射为不会随语言变化的 QML 状态键。
    switch (m_status) {
        case Status::Checking:
            return QStringLiteral("checking");
        case Status::UpdateAvailable:
            return QStringLiteral("update_available");
        case Status::UpToDate:
            return QStringLiteral("up_to_date");
        case Status::Failed:
            return QStringLiteral("failed");
        case Status::Ignored:
            return QStringLiteral("ignored");
        case Status::NotChecked:
        default:
            return QStringLiteral("not_checked");
    }
}

// 读取启动自动检查开关。
bool UpdateCheckerService::autoCheckEnabled() const {
    return ConfigService::instance()->getUpdateAutoCheck();
}

// 读取最近一次检查开始时间。
qint64 UpdateCheckerService::lastCheckTime() const {
    return ConfigService::instance()->getUpdateLastCheckTime();
}

// 读取最近一次成功检查时间。
qint64 UpdateCheckerService::lastSuccessfulCheckTime() const {
    return ConfigService::instance()->getUpdateLastSuccessfulCheckTime();
}

// 按自动检查策略发起一次不会重复并发的 Release 请求。
void UpdateCheckerService::checkForUpdates(bool force) {
    if (m_checking)
        return;

    ConfigService* config = ConfigService::instance();
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    const qint64 interval = static_cast<qint64>(config->getUpdateCheckIntervalHours()) * 3600;
    if (!force && !config->getUpdateAutoCheck())
        return;
    if (!force && config->getUpdateLastCheckTime() > 0 && now - config->getUpdateLastCheckTime() < interval)
        return;

    config->setUpdateLastCheckTime(now);
    emit metadataChanged();
    setError(QString());
    setStatus(Status::Checking);
    setChecking(true);

    const RetryPolicy policy =
        RetryPolicy::fromProfile(RequestProfiles::updateCheck(), config->getWeakNetworkSupport());
    startReleaseRequest(QUrl(QString::fromLatin1(RELEASES_URL)), policy, true);
}

// 发起更新源请求，并在 API 限流时保留 Atom 回退机会。
void UpdateCheckerService::startReleaseRequest(const QUrl& url, const RetryPolicy& policy, bool allowAtomFallback) {
    m_activeRequest = m_networkClient->getAsync(url, ResourceType::UpdateCheck, policy);
    connect(m_activeRequest,
            &AsyncNetworkRequest::finished,
            this,
            [this, policy, allowAtomFallback](const NetworkResult& result) {
                handleReleaseResponse(result, policy, allowAtomFallback);
            });
}

// 处理 JSON API 或 Atom 回退响应，并统一更新检查状态。
void UpdateCheckerService::handleReleaseResponse(const NetworkResult& result,
                                                 const RetryPolicy& policy,
                                                 bool allowAtomFallback) {
    if (m_activeRequest) {
        m_activeRequest->deleteLater();
        m_activeRequest = nullptr;
    }
    if (result.wasCancelled) {
        setChecking(false);
        m_rateLimited = false;
        setError(QStringLiteral("Update check was cancelled"));
        setStatus(Status::NotChecked);
        return;
    }

    if (!result.success) {
        const bool rateLimited = result.statusCode == 403 || result.statusCode == 429 ||
                                 result.diagnostic.errorType == NetworkErrorType::Forbidden ||
                                 result.diagnostic.errorType == NetworkErrorType::RateLimited ||
                                 result.diagnostic.wasRateLimited;
        if (allowAtomFallback && rateLimited) {
            m_rateLimited = true;
            startReleaseRequest(QUrl(QString::fromLatin1(RELEASES_ATOM_URL)), policy, false);
            return;
        }
        setChecking(false);
        loadCachedRelease();
        m_rateLimited = m_rateLimited || rateLimited;
        setError(result.error.isEmpty() ? QStringLiteral("Update check failed") : result.error);
        setStatus(Status::Failed);
        return;
    }

    setChecking(false);
    m_rateLimited = false;
    const QJsonObject release =
        allowAtomFallback ? QJsonDocument::fromJson(result.data).object() : parseAtomRelease(result.data);
    if (release.isEmpty() || !applyRelease(release, false)) {
        setError(QStringLiteral("Invalid release response"));
        setStatus(Status::Failed);
    }
}

// 持久化当前版本的稍后提醒选择。
void UpdateCheckerService::dismissUpdate() {
    if (!m_hasUpdate)
        return;
    ConfigService::instance()->setUpdateRemindedVersion(m_latestVersion);
    m_dismissed = true;
    setStatus(Status::Ignored);
    emit updateStateChanged();
}

// 持久化忽略当前版本的选择，并清除稍后提醒标记。
void UpdateCheckerService::ignoreUpdate() {
    if (m_latestVersion.isEmpty())
        return;
    ConfigService::instance()->setUpdateIgnoredVersion(m_latestVersion);
    dismissUpdate();
    ConfigService::instance()->setUpdateRemindedVersion(QString());
}

// 清除当前运行实例中的提示关闭状态。
void UpdateCheckerService::resetDismissed() {
    if (!m_dismissed)
        return;
    m_dismissed = false;
    setStatus(m_hasUpdate ? Status::UpdateAvailable : Status::UpToDate);
    emit updateStateChanged();
}

// 保存启动自动检查开关并通知界面。
void UpdateCheckerService::setAutoCheckEnabled(bool enabled) {
    if (autoCheckEnabled() == enabled)
        return;
    ConfigService::instance()->setUpdateAutoCheck(enabled);
    emit autoCheckEnabledChanged();
}

// 从 ConfigService 读取并应用最近一次有效 Release 缓存。
void UpdateCheckerService::loadCachedRelease() {
    const QJsonObject cached = ConfigService::instance()->getUpdateCachedRelease();
    if (!cached.isEmpty())
        applyRelease(cached, true);
}

// 将带 v 前缀的版本规范化为统一的 SemVer 表示。
QString UpdateCheckerService::normalizeVersion(const QString& version) {
    const SemanticVersion parsed = parseVersion(version);
    if (!parsed.valid)
        return QString();
    QString normalized = QStringLiteral("%1.%2.%3").arg(parsed.major).arg(parsed.minor).arg(parsed.patch);
    if (!parsed.prerelease.isEmpty())
        normalized += QStringLiteral("-") + parsed.prerelease.join('.');
    const int buildMarker = version.indexOf('+');
    if (buildMarker >= 0)
        normalized += version.mid(buildMarker).trimmed();
    return normalized;
}

// 判断远程版本是否严格高于当前版本。
bool UpdateCheckerService::isRemoteVersionNewer(const QString& current, const QString& latest) {
    const SemanticVersion currentVersion = parseVersion(current);
    const SemanticVersion latestVersion = parseVersion(latest);
    return currentVersion.valid && latestVersion.valid && compareVersions(currentVersion, latestVersion) < 0;
}

// 校验 GitHub Release 的稳定性、版本和下载元数据。
bool UpdateCheckerService::validateRelease(const QJsonObject& release, QString* reason) const {
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QString name = release.value(QStringLiteral("name")).toString();
    const QUrl url(release.value(QStringLiteral("html_url")).toString());
    if (release.value(QStringLiteral("draft")).toBool(false) ||
        release.value(QStringLiteral("prerelease")).toBool(false)) {
        if (reason)
            *reason = QStringLiteral("Release is not a stable published release");
        return false;
    }
    if (normalizeVersion(tag).isEmpty() || name.isEmpty() || !url.isValid() ||
        url.scheme() != QStringLiteral("https")) {
        if (reason)
            *reason = QStringLiteral("Incomplete release metadata");
        return false;
    }
    if (!release.value(QStringLiteral("assets")).isArray()) {
        if (reason)
            *reason = QStringLiteral("Release assets are missing");
        return false;
    }
    return true;
}

// 应用远程或缓存 Release，并同步持久化检查结果。
bool UpdateCheckerService::applyRelease(const QJsonObject& release, bool fromCache) {
    QString reason;
    if (!validateRelease(release, &reason)) {
        setError(reason);
        setStatus(Status::Failed);
        return false;
    }
    const QString version = normalizeVersion(release.value(QStringLiteral("tag_name")).toString());
    ConfigService* config = ConfigService::instance();
    const QString ignored = config->getUpdateIgnoredVersion();
    const QString reminded = config->getUpdateRemindedVersion();
    config->beginBatchUpdate();
    if (!fromCache) {
        config->setUpdateCachedRelease(release);
        config->setUpdateLastSuccessfulCheckTime(QDateTime::currentSecsSinceEpoch());
    }
    if (!ignored.isEmpty() && isRemoteVersionNewer(ignored, version))
        config->setUpdateIgnoredVersion(QString());
    if (!reminded.isEmpty() && isRemoteVersionNewer(reminded, version))
        config->setUpdateRemindedVersion(QString());
    config->endBatchUpdate();

    applyUpdateInfo(version,
                    release.value(QStringLiteral("name")).toString(),
                    release.value(QStringLiteral("html_url")).toString(),
                    chooseAsset(release.value(QStringLiteral("assets")).toArray()));
    setError(QString());
    setStatus(m_hasUpdate ? (m_dismissed ? Status::Ignored : Status::UpdateAvailable) : Status::UpToDate);
    emit metadataChanged();
    return true;
}

// 更新检查中标志并发出对应通知。
void UpdateCheckerService::setChecking(bool checking) {
    if (m_checking == checking)
        return;
    m_checking = checking;
    emit checkingChanged();
}

// 更新状态枚举并通知 QML。
void UpdateCheckerService::setStatus(Status status) {
    if (m_status == status)
        return;
    m_status = status;
    emit statusChanged();
}

// 将 Release 元数据映射到公开属性和用户提醒策略。
void UpdateCheckerService::applyUpdateInfo(const QString& version,
                                           const QString& name,
                                           const QString& url,
                                           const QString& asset) {
    const QString ignoredVersion = ConfigService::instance()->getUpdateIgnoredVersion();
    const QString remindedVersion = ConfigService::instance()->getUpdateRemindedVersion();
    const bool oldState = m_hasUpdate;
    const QString oldVersion = m_latestVersion;
    const bool oldDismissed = m_dismissed;
    m_latestVersion = version;
    m_releaseName = name;
    m_releaseUrl = url;
    m_assetUrl = asset;
    m_hasUpdate = isRemoteVersionNewer(currentVersion(), version);
    m_dismissed = m_hasUpdate && ((!ignoredVersion.isEmpty() && ignoredVersion == version) ||
                                  (!remindedVersion.isEmpty() && remindedVersion == version));
    if (oldState != m_hasUpdate || oldVersion != m_latestVersion || oldDismissed != m_dismissed)
        emit updateStateChanged();
}

// 保存诊断信息并触发状态刷新。
void UpdateCheckerService::setError(const QString& error) {
    if (m_error == error)
        return;
    m_error = error;
    emit updateStateChanged();
}

}  // namespace EasyKiConverter
