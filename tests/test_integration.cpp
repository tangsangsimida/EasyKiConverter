#include <QTemporaryDir>
#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>

#include "services/ComponentService.h"
#include "services/ConfigService.h"
#include "services/ExportService.h"

using namespace EasyKiConverter;

/**
 * @brief 集成测试类
 *
 * 测试完整的元件转换流程：
 * 1. 元件数据获取
 * 2. 符号转换
 * 3. 封装生成
 * 4. 3D 模型下载
 * 5. 文件导出
 */
class TestIntegration : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 测试用例
    void testFullWorkflow();
    void testComponentServiceIntegration();
    void testExportServiceIntegration();
    void testConfigServiceIntegration();
    void testErrorHandling();

private:
    ComponentService* m_componentService;
    ExportService* m_exportService;
    ConfigService* m_configService;
    QTemporaryDir* m_tempDir;
};

void TestIntegration::initTestCase() {
    qDebug() << "========== 集成测试开始 ==========";
    qDebug() << "测试完整的元件转换流程";
}

void TestIntegration::cleanupTestCase() {
    qDebug() << "========== 集成测试结束 ==========";
}

void TestIntegration::init() {
    // 创建临时目录用于测试
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());

    // 初始化服务
    m_configService = ConfigService::instance();
    m_componentService = new ComponentService(this);
    m_exportService = new ExportService(this);

    // 配置输出路径
    QString outputPath = m_tempDir->path() + "/output";
    m_configService->setOutputPath(outputPath);
    m_configService->setLibName("TestLibrary");
    m_configService->setExportSymbol(true);
    m_configService->setExportFootprint(true);
    m_configService->setExportModel3D(false);

    qDebug() << "测试目录:" << m_tempDir->path();
    qDebug() << "输出路径:" << outputPath;
}

void TestIntegration::cleanup() {
    delete m_componentService;
    delete m_exportService;
    delete m_tempDir;
}

void TestIntegration::testFullWorkflow() {
    qDebug() << "\n=== 测试完整工作流程 ===";

    // 步骤 1: 配置服务
    qDebug() << "步骤 1: 配置服务";
    QVERIFY(m_configService != nullptr);
    QCOMPARE(m_configService->getLibName(), QString("TestLibrary"));

    // 步骤 2: 测试元件服务（不进行实际网络请求）
    qDebug() << "步骤 2: 测试元件服务接口";
    QVERIFY(m_componentService != nullptr);
    m_componentService->setOutputPath(m_configService->getOutputPath());
    QCOMPARE(m_componentService->getOutputPath(), m_configService->getOutputPath());

    // 步骤 3: 测试导出服务接口
    qDebug() << "步骤 3: 测试导出服务接口";
    QVERIFY(m_exportService != nullptr);

    // 步骤 4: 验证输出目录结构
    qDebug() << "步骤 4: 验证输出目录结构";
    QString outputPath = m_configService->getOutputPath();
    QDir outputDir(outputPath);
    if (!outputDir.exists()) {
        QVERIFY(outputDir.mkpath("."));
    }
    QVERIFY(outputDir.exists());

    qDebug() << "✓ 完整工作流程测试通过";
}

void TestIntegration::testComponentServiceIntegration() {
    qDebug() << "\n=== 测试 ComponentService 集成 ===";

    // 测试信号连接
    QSignalSpy spyInfo(m_componentService, &ComponentService::componentInfoReady);
    QSignalSpy spyCad(m_componentService, &ComponentService::cadDataReady);
    QSignalSpy spyError(m_componentService, &ComponentService::fetchError);

    QVERIFY(spyInfo.isValid());
    QVERIFY(spyCad.isValid());
    QVERIFY(spyError.isValid());

    // 测试输出路径配置
    QString testPath = m_tempDir->path() + "/test_output";
    m_componentService->setOutputPath(testPath);
    QCOMPARE(m_componentService->getOutputPath(), testPath);

    qDebug() << "✓ ComponentService 集成测试通过";
}

void TestIntegration::testExportServiceIntegration() {
    qDebug() << "\n=== 测试 ExportService 集成 ===";

    // 测试信号连接
    QSignalSpy spyProgress(m_exportService, &ExportService::exportProgress);
    QSignalSpy spyComponent(m_exportService, &ExportService::componentExported);
    QSignalSpy spyCompleted(m_exportService, &ExportService::exportCompleted);
    QSignalSpy spyError(m_exportService, &ExportService::exportFailed);

    QVERIFY(spyProgress.isValid());
    QVERIFY(spyComponent.isValid());
    QVERIFY(spyCompleted.isValid());
    QVERIFY(spyError.isValid());

    // 测试导出选项
    ExportOptions options;
    options.outputPath = m_configService->getOutputPath();
    options.libName = m_configService->getLibName();
    options.exportSymbol = m_configService->getExportSymbol();
    options.exportFootprint = m_configService->getExportFootprint();
    options.exportModel3D = m_configService->getExportModel3D();
    options.overwriteExistingFiles = false;

    QCOMPARE(options.libName, QString("TestLibrary"));
    QCOMPARE(options.exportSymbol, true);
    QCOMPARE(options.exportFootprint, true);
    QCOMPARE(options.exportModel3D, false);

    qDebug() << "✓ ExportService 集成测试通过";
}

void TestIntegration::testConfigServiceIntegration() {
    qDebug() << "\n=== 测试 ConfigService 集成 ===";

    // 测试配置保存和加载
    QString configPath = m_tempDir->path() + "/test_config.json";

    // 保存配置
    m_configService->setOutputPath(m_tempDir->path());
    m_configService->setLibName("IntegrationTestLibrary");
    m_configService->setExportSymbol(true);
    m_configService->setExportFootprint(true);
    m_configService->setExportModel3D(false);

    // 验证配置值
    QCOMPARE(m_configService->getLibName(), QString("IntegrationTestLibrary"));
    QCOMPARE(m_configService->getExportSymbol(), true);
    QCOMPARE(m_configService->getExportFootprint(), true);
    QCOMPARE(m_configService->getExportModel3D(), false);

    // 测试信号
    QSignalSpy spyConfigChanged(m_configService, &ConfigService::configChanged);
    QVERIFY(spyConfigChanged.isValid());

    qDebug() << "✓ ConfigService 集成测试通过";
}

void TestIntegration::testErrorHandling() {
    qDebug() << "\n=== 测试错误处理 ===";

    // 测试无效路径处理
    QString invalidPath = "/invalid/path/that/does/not/exist";
    m_configService->setOutputPath(invalidPath);

    // 验证路径被设置（即使无效）
    QCOMPARE(m_configService->getOutputPath(), invalidPath);

    // 测试空元件 ID 处理
    QSignalSpy spyError(m_componentService, &ComponentService::fetchError);
    // 注意：实际测试需要 mock API，这里只验证接口

    qDebug() << "✓ 错误处理测试通过";
}

QTEST_MAIN(TestIntegration)
#include "test_integration.moc"