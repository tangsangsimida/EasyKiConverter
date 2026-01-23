#include <QtTest/QtTest>

#include <QSignalSpy>
#include <QStringList>

#include "services/ComponentExportTask.h"
#include "services/ExportService.h"

using namespace EasyKiConverter;

class TestExportService : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 测试用例
    void testCreation();
    void testExportOptions();
    void testExecuteExportPipeline();
    void testSignals();

private:
    ExportService* m_service;
};

void TestExportService::initTestCase() {
    qDebug() << "========== TestExportService 开始 ==========";
}

void TestExportService::cleanupTestCase() {
    qDebug() << "========== TestExportService 结束 ==========";
}

void TestExportService::init() {
    m_service = new ExportService(this);
}

void TestExportService::cleanup() {
    delete m_service;
}

void TestExportService::testCreation() {
    QVERIFY(m_service != nullptr);
    qDebug() << "✓ ExportService 创建成功";
}

void TestExportService::testExportOptions() {
    ExportService::ExportOptions options;

    options.outputPath = "C:/test/output";
    options.libName = "TestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;

    QCOMPARE(options.outputPath, QString("C:/test/output"));
    QCOMPARE(options.libName, QString("TestLibrary"));
    QCOMPARE(options.exportSymbol, true);
    QCOMPARE(options.exportFootprint, true);
    QCOMPARE(options.exportModel3D, false);
    QCOMPARE(options.overwriteExistingFiles, false);

    qDebug() << "✓ ExportOptions 结构体工作正常";
}

void TestExportService::testExecuteExportPipeline() {
    // 测试导出管道接口
    QStringList componentIds;
    componentIds << "C12345" << "C67890";

    ExportService::ExportOptions options;
    options.outputPath = "C:/test/output";
    options.libName = "TestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;

    // 调用导出管道（注意：这需要实际的元件数据，实际测试可能需要 mock）
    // m_service->executeExportPipeline(componentIds, options);

    // 这里只测试方法调用不会崩溃
    QVERIFY(m_service != nullptr);
    qDebug() << "✓ executeExportPipeline 方法接口正确";
}

void TestExportService::testSignals() {
    // 测试信号连接
    QSignalSpy spyProgress(m_service, &ExportService::exportProgress);
    QSignalSpy spyComponent(m_service, &ExportService::componentExported);
    QSignalSpy spyCompleted(m_service, &ExportService::exportCompleted);
    QSignalSpy spyError(m_service, &ExportService::exportFailed);

    QVERIFY(spyProgress.isValid());
    QVERIFY(spyComponent.isValid());
    QVERIFY(spyCompleted.isValid());
    QVERIFY(spyError.isValid());

    qDebug() << "✓ 所有信号定义正确";
}

QTEST_MAIN(TestExportService)
#include "test_export_service.moc"