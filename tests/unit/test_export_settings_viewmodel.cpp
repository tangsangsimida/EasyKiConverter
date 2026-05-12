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
    void testModel3DPathModeDefaultsAndPersists();
    void testNormalizePathMode();
    void testConfigServiceModel3DPathMode();

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

void TestExportSettingsViewModel::testModel3DPathModeDefaultsAndPersists() {
    ExportSettingsViewModel viewModel(nullptr);
    QSignalSpy pathModeSpy(&viewModel, &ExportSettingsViewModel::exportModel3DPathModeChanged);

    QCOMPARE(viewModel.exportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);

    viewModel.setExportModel3DPathMode(ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(viewModel.exportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(ConfigService::instance()->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_ABSOLUTE);

    viewModel.setExportModel3DPathMode(999);
    QCOMPARE(viewModel.exportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ConfigService::instance()->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(pathModeSpy.count(), 2);
}

void TestExportSettingsViewModel::testNormalizePathMode() {
    QCOMPARE(ExportOptions::normalizePathMode(0), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(1), ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(ExportOptions::normalizePathMode(999), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(-1), ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(ExportOptions::normalizePathMode(2), ExportOptions::MODEL_3D_PATH_RELATIVE);
}

void TestExportSettingsViewModel::testConfigServiceModel3DPathMode() {
    ConfigService* config = ConfigService::instance();

    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);

    config->setExportModel3DPathMode(ExportOptions::MODEL_3D_PATH_ABSOLUTE);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_ABSOLUTE);

    config->setExportModel3DPathMode(ExportOptions::MODEL_3D_PATH_RELATIVE);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);

    config->setExportModel3DPathMode(42);
    QCOMPARE(config->getExportModel3DPathMode(), ExportOptions::MODEL_3D_PATH_RELATIVE);
}

QTEST_GUILESS_MAIN(TestExportSettingsViewModel)
#include "test_export_settings_viewmodel.moc"
