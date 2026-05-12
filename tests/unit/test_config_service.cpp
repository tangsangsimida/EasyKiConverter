#include "services/ConfigService.h"

#include <QDir>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestConfigService : public QObject {
    Q_OBJECT

private slots:
    void testCacheSettingsPersistAcrossLoad();
    void testDiskCacheLimitClamped();
    void testExportModePersistAcrossLoad();
};

void TestConfigService::testCacheSettingsPersistAcrossLoad() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("config.json"));
    const QString cacheDir = QDir(tempDir.path()).filePath(QStringLiteral("custom-cache"));

    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
    config->setCacheDir(cacheDir);
    config->setDiskCacheLimitMB(2048);
    QVERIFY(config->saveConfig(configPath));

    config->resetToDefaults();
    QVERIFY(config->loadConfig(configPath));

    QCOMPARE(config->getCacheDir(), QDir::cleanPath(cacheDir));
    QCOMPARE(config->getDiskCacheLimitMB(), 2048);
}

void TestConfigService::testDiskCacheLimitClamped() {
    ConfigService* config = ConfigService::instance();

    config->setDiskCacheLimitMB(0);
    QCOMPARE(config->getDiskCacheLimitMB(), 1);

    config->setDiskCacheLimitMB(1048577);
    QCOMPARE(config->getDiskCacheLimitMB(), 1048576);
}

void TestConfigService::testExportModePersistAcrossLoad() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    const QString configPath = QDir(tempDir.path()).filePath(QStringLiteral("config.json"));

    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
    QCOMPARE(config->getExportMode(), 0);

    config->setExportMode(1);
    QCOMPARE(config->getExportMode(), 1);
    QVERIFY(config->saveConfig(configPath));

    config->resetToDefaults();
    QCOMPARE(config->getExportMode(), 0);

    QVERIFY(config->loadConfig(configPath));
    QCOMPARE(config->getExportMode(), 1);
}

QTEST_GUILESS_MAIN(TestConfigService)
#include "test_config_service.moc"
