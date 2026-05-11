#include "services/ConfigService.h"
#include "ui/viewmodels/ExportSettingsViewModel.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestExportSettingsViewModel : public QObject {
    Q_OBJECT

private slots:
    void init();
    void testLoadsCacheSettingsFromConfig();
    void testDiskCacheLimitClamped();

private:
    QTemporaryDir m_tempDir;
};

void TestExportSettingsViewModel::init() {
    QVERIFY(m_tempDir.isValid());

    ConfigService* config = ConfigService::instance();
    config->resetToDefaults();
    config->setCacheDir(QDir(m_tempDir.path()).filePath(QStringLiteral("configured-cache")));
    config->setDiskCacheLimitMB(4096);
}

void TestExportSettingsViewModel::testLoadsCacheSettingsFromConfig() {
    ExportSettingsViewModel viewModel(nullptr);

    QCOMPARE(viewModel.cacheDir(),
             QDir::cleanPath(QDir(m_tempDir.path()).filePath(QStringLiteral("configured-cache"))));
    QCOMPARE(viewModel.diskCacheLimitMB(), 4096);
}

void TestExportSettingsViewModel::testDiskCacheLimitClamped() {
    ExportSettingsViewModel viewModel(nullptr);
    QSignalSpy limitSpy(&viewModel, &ExportSettingsViewModel::diskCacheLimitMBChanged);

    viewModel.setDiskCacheLimitMB(0);
    QCOMPARE(viewModel.diskCacheLimitMB(), 1);
    QCOMPARE(ConfigService::instance()->getDiskCacheLimitMB(), 1);

    viewModel.setDiskCacheLimitMB(1048577);
    QCOMPARE(viewModel.diskCacheLimitMB(), 1048576);
    QCOMPARE(ConfigService::instance()->getDiskCacheLimitMB(), 1048576);
    QCOMPARE(limitSpy.count(), 2);
}

QTEST_GUILESS_MAIN(TestExportSettingsViewModel)
#include "test_export_settings_viewmodel.moc"
