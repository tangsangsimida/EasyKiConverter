#include "../common/MockNetworkClient.hpp"
#include "services/ConfigService.h"
#include "services/UpdateCheckerService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;
using EasyKiConverter::Test::MockNetworkClient;

namespace {

constexpr auto RELEASES_URL = "https://api.github.com/repos/tangsangsimida/EasyKiConverter/releases/latest";

// 构造统一的稳定或预发布 Release 模拟响应。
QJsonObject release(const QString& tag, bool draft = false, bool prerelease = false) {
    return QJsonObject{
        {QStringLiteral("tag_name"), tag},
        {QStringLiteral("name"), QStringLiteral("EasyKiConverter %1").arg(tag)},
        {QStringLiteral("html_url"),
         QStringLiteral("https://github.com/tangsangsimida/EasyKiConverter/releases/tag/%1").arg(tag)},
        {QStringLiteral("draft"), draft},
        {QStringLiteral("prerelease"), prerelease},
        {QStringLiteral("assets"),
         QJsonArray{QJsonObject{
             {QStringLiteral("name"), QStringLiteral("EasyKiConverter-x86_64.AppImage")},
             {QStringLiteral("browser_download_url"),
              QStringLiteral("https://example.com/EasyKiConverter-linux.AppImage")},
         }}},
    };
}

}  // namespace

class TestUpdateCheckerService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void init();
    void cleanup();
    void semVerComparisonSupportsReleaseCandidates();
    void invalidVersionDoesNotReportUpdate();
    void invalidReleaseIsRejected();
    void draftAndPrereleaseAreRejected();
    void startupAutoCheckIsDisabledByDefault();
    void dismissAndIgnorePersistSeparately();
    void newerReleaseClearsIgnoredVersion();
    void automaticCheckHonorsIntervalAndManualCheckBypassesIt();
    void networkFailureFallsBackToCachedRelease();
    void rateLimitFailureExposesFriendlyState();
    void unmatchedAssetFallsBackToReleasePage();

private:
    void prepareConfig();

    QTemporaryDir m_configDirectory;
    QString m_configPath;
};

// 初始化测试使用的应用版本和临时配置文件路径。
void TestUpdateCheckerService::initTestCase() {
    QCoreApplication::setApplicationVersion(QStringLiteral("3.1.12"));
    QVERIFY(m_configDirectory.isValid());
    m_configPath = QDir(m_configDirectory.path()).filePath(QStringLiteral("config.json"));
}

// 为每个用例恢复隔离的默认配置。
void TestUpdateCheckerService::init() {
    prepareConfig();
}

// 清理单例配置，避免影响其他测试用例。
void TestUpdateCheckerService::cleanup() {
    ConfigService::instance()->resetToDefaults();
}

// 将 ConfigService 重定向到本测试的临时文件。
void TestUpdateCheckerService::prepareConfig() {
    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
    QVERIFY(config->saveConfig(m_configPath));
    QVERIFY(config->loadConfig(m_configPath));
}

// 验证稳定版、候选版和构建元数据的比较规则。
void TestUpdateCheckerService::semVerComparisonSupportsReleaseCandidates() {
    QVERIFY(UpdateCheckerService::isRemoteVersionNewer(QStringLiteral("3.1.12"), QStringLiteral("v3.1.13-beta.1")));
    QVERIFY(UpdateCheckerService::isRemoteVersionNewer(QStringLiteral("3.1.13-beta.1"), QStringLiteral("3.1.13-rc.1")));
    QVERIFY(UpdateCheckerService::isRemoteVersionNewer(QStringLiteral("3.1.13-rc.1"), QStringLiteral("3.1.13")));
    QVERIFY(!UpdateCheckerService::isRemoteVersionNewer(QStringLiteral("3.1.13+build.1"),
                                                        QStringLiteral("3.1.13+build.2")));
    QCOMPARE(UpdateCheckerService::normalizeVersion(QStringLiteral("v3.1.13+build.123")),
             QStringLiteral("3.1.13+build.123"));
}

// 验证非法版本不会被误判为更新。
void TestUpdateCheckerService::invalidVersionDoesNotReportUpdate() {
    QVERIFY(UpdateCheckerService::normalizeVersion(QStringLiteral("not-a-version")).isEmpty());
    QVERIFY(!UpdateCheckerService::isRemoteVersionNewer(QStringLiteral("3.1.12"), QStringLiteral("release-latest")));
}

// 验证新配置默认关闭启动时自动检查，但不影响手动检查入口。
void TestUpdateCheckerService::startupAutoCheckIsDisabledByDefault() {
    QCOMPARE(ConfigService::instance()->getUpdateAutoCheck(), false);
}

// 验证缺少必需字段的 Release 会进入失败状态。
void TestUpdateCheckerService::invalidReleaseIsRejected() {
    MockNetworkClient network;
    QJsonObject invalid = release(QStringLiteral("3.1.13"));
    invalid.remove(QStringLiteral("html_url"));
    network.addJsonResponse(QString::fromLatin1(RELEASES_URL), invalid);

    UpdateCheckerService service(network);
    service.checkForUpdates();
    QTRY_COMPARE(service.status(), UpdateCheckerService::Status::Failed);
    QVERIFY(!service.error().isEmpty());
}

// 验证 Draft 和 Pre-release 默认不会被接受。
void TestUpdateCheckerService::draftAndPrereleaseAreRejected() {
    for (const QJsonObject& candidate :
         {release(QStringLiteral("3.1.13"), true), release(QStringLiteral("3.1.13-beta.1"), false, true)}) {
        MockNetworkClient network;
        network.addJsonResponse(QString::fromLatin1(RELEASES_URL), candidate);
        UpdateCheckerService service(network);
        service.checkForUpdates();
        QTRY_COMPARE(service.status(), UpdateCheckerService::Status::Failed);
    }
}

// 验证稍后提醒和忽略版本使用独立的持久化字段。
void TestUpdateCheckerService::dismissAndIgnorePersistSeparately() {
    MockNetworkClient network;
    network.addJsonResponse(QString::fromLatin1(RELEASES_URL), release(QStringLiteral("3.1.13")));

    UpdateCheckerService service(network);
    service.checkForUpdates();
    QTRY_VERIFY(service.hasUpdate());
    service.dismissUpdate();
    QCOMPARE(ConfigService::instance()->getUpdateRemindedVersion(), QStringLiteral("3.1.13"));
    QCOMPARE(ConfigService::instance()->getUpdateIgnoredVersion(), QString());

    UpdateCheckerService restored(network);
    QVERIFY(restored.dismissed());
    restored.resetDismissed();
    restored.ignoreUpdate();
    QCOMPARE(ConfigService::instance()->getUpdateIgnoredVersion(), QStringLiteral("3.1.13"));
    QCOMPARE(ConfigService::instance()->getUpdateRemindedVersion(), QString());
}

// 验证出现更高版本后旧忽略记录会被清除。
void TestUpdateCheckerService::newerReleaseClearsIgnoredVersion() {
    ConfigService::instance()->setUpdateIgnoredVersion(QStringLiteral("3.1.13"));
    MockNetworkClient network;
    network.addJsonResponse(QString::fromLatin1(RELEASES_URL), release(QStringLiteral("3.1.14")));

    UpdateCheckerService service(network);
    service.checkForUpdates();
    QTRY_VERIFY(service.hasUpdate());
    QCOMPARE(ConfigService::instance()->getUpdateIgnoredVersion(), QString());
}

// 验证自动检查遵守间隔而手动检查可以强制执行。
void TestUpdateCheckerService::automaticCheckHonorsIntervalAndManualCheckBypassesIt() {
    ConfigService* config = ConfigService::instance();
    config->setUpdateAutoCheck(true);
    config->setUpdateLastCheckTime(QDateTime::currentSecsSinceEpoch());

    MockNetworkClient network;
    network.addJsonResponse(QString::fromLatin1(RELEASES_URL), release(QStringLiteral("3.1.13")));
    UpdateCheckerService service(network);
    service.checkForUpdates(false);
    QCOMPARE(service.status(), UpdateCheckerService::Status::NotChecked);
    service.checkForUpdates(true);
    QTRY_COMPARE(service.status(), UpdateCheckerService::Status::UpdateAvailable);
}

// 验证网络失败时仍保留有效的本地 Release 缓存。
void TestUpdateCheckerService::networkFailureFallsBackToCachedRelease() {
    ConfigService* config = ConfigService::instance();
    config->setUpdateCachedRelease(release(QStringLiteral("3.1.13")));

    MockNetworkClient network;
    network.addErrorResponse(QString::fromLatin1(RELEASES_URL), QStringLiteral("network unavailable"));
    UpdateCheckerService service(network);
    service.checkForUpdates();
    QTRY_COMPARE(service.status(), UpdateCheckerService::Status::Failed);
    QVERIFY(service.hasUpdate());
    QCOMPARE(service.latestVersion(), QStringLiteral("3.1.13"));
    QCOMPARE(service.assetUrl(), QStringLiteral("https://example.com/EasyKiConverter-linux.AppImage"));
}

// 验证 GitHub 限流响应会暴露专用状态，界面无需展示原始请求地址。
void TestUpdateCheckerService::rateLimitFailureExposesFriendlyState() {
    MockNetworkClient network;
    network.addErrorResponse(QString::fromLatin1(RELEASES_URL), QStringLiteral("HTTP 403"), 403);

    UpdateCheckerService service(network);
    service.checkForUpdates();
    QTRY_COMPARE(service.status(), UpdateCheckerService::Status::Failed);
    QVERIFY(service.rateLimited());
    QVERIFY(service.error().contains(QStringLiteral("HTTP 403")));
}

// 验证当前平台没有匹配资产时不会误打开其他平台的下载包。
void TestUpdateCheckerService::unmatchedAssetFallsBackToReleasePage() {
    QJsonObject candidate = release(QStringLiteral("3.1.13"));
    candidate[QStringLiteral("assets")] = QJsonArray{QJsonObject{
        {QStringLiteral("name"), QStringLiteral("EasyKiConverter-win64.zip")},
        {QStringLiteral("browser_download_url"), QStringLiteral("https://example.com/windows.zip")},
    }};

    MockNetworkClient network;
    network.addJsonResponse(QString::fromLatin1(RELEASES_URL), candidate);
    UpdateCheckerService service(network);
    service.checkForUpdates();
    QTRY_COMPARE(service.status(), UpdateCheckerService::Status::UpdateAvailable);
    QVERIFY(service.assetUrl().isEmpty());
    QVERIFY(!service.releaseUrl().isEmpty());
}

QTEST_GUILESS_MAIN(TestUpdateCheckerService)
#include "test_update_checker_service.moc"
