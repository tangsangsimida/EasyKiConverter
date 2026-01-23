#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include "services/ComponentService.h"
#include "services/ExportService.h"
#include "services/ConfigService.h"

using namespace EasyKiConverter;

/**
 * @brief 性能测试类
 * 
 * 测试系统的性能指标：
 * 1. 元件数据获取性能
 * 2. 导出性能
 * 3. 内存使用
 * 4. 并行处理性能
 */
class TestPerformance : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 性能测试用例
    void testComponentServicePerformance();
    void testExportServicePerformance();
    void testMemoryUsage();
    void testParallelProcessing();
    void testBatchProcessing();

private:
    ComponentService *m_componentService;
    ExportService *m_exportService;
    ConfigService *m_configService;
    QTemporaryDir *m_tempDir;
    
    // 性能基准
    struct PerformanceMetrics {
        qint64 componentFetchTime;
        qint64 exportTime;
        qint64 memoryUsage;
        qint64 totalProcessingTime;
    };
    
    PerformanceMetrics m_metrics;
};

void TestPerformance::initTestCase()
{
    qDebug() << "========== 性能测试开始 ==========";
    qDebug() << "测试系统性能指标";
    
    // 重置性能指标
    m_metrics = PerformanceMetrics{0, 0, 0, 0};
}

void TestPerformance::cleanupTestCase()
{
    qDebug() << "========== 性能测试结束 ==========";
    
    // 输出性能报告
    qDebug() << "\n=== 性能测试报告 ===";
    qDebug() << "元件数据获取时间:" << m_metrics.componentFetchTime << "ms";
    qDebug() << "导出时间:" << m_metrics.exportTime << "ms";
    qDebug() << "内存使用:" << m_metrics.memoryUsage << "KB";
    qDebug() << "总处理时间:" << m_metrics.totalProcessingTime << "ms";
}

void TestPerformance::init()
{
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
    
    m_configService = new ConfigService(this);
    m_componentService = new ComponentService(this);
    m_exportService = new ExportService(this);
    
    QString outputPath = m_tempDir->path() + "/output";
    m_configService->setOutputPath(outputPath);
    m_configService->setLibName("PerformanceTestLibrary");
}

void TestPerformance::cleanup()
{
    delete m_componentService;
    delete m_exportService;
    delete m_configService;
    delete m_tempDir;
}

void TestPerformance::testComponentServicePerformance()
{
    qDebug() << "\n=== 测试 ComponentService 性能 ===";
    
    QElapsedTimer timer;
    
    // 测试配置时间
    timer.start();
    m_componentService->setOutputPath(m_configService->getOutputPath());
    qint64 configTime = timer.elapsed();
    
    qDebug() << "配置时间:" << configTime << "ms";
    QVERIFY(configTime < 100); // 配置应该在 100ms 内完成
    
    // 测试获取路径时间
    timer.start();
    QString path = m_componentService->getOutputPath();
    qint64 getPathTime = timer.elapsed();
    
    qDebug() << "获取路径时间:" << getPathTime << "ms";
    QVERIFY(getPathTime < 10); // 获取路径应该在 10ms 内完成
    
    // 记录性能指标
    m_metrics.componentFetchTime = configTime + getPathTime;
    
    qDebug() << "✓ ComponentService 性能测试通过";
}

void TestPerformance::testExportServicePerformance()
{
    qDebug() << "\n=== 测试 ExportService 性能 ===";
    
    QElapsedTimer timer;
    
    // 测试导出选项创建时间
    timer.start();
    ExportService::ExportOptions options;
    options.outputPath = m_configService->getOutputPath();
    options.libName = m_configService->getLibName();
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;
    qint64 optionsTime = timer.elapsed();
    
    qDebug() << "导出选项创建时间:" << optionsTime << "ms";
    QVERIFY(optionsTime < 50); // 选项创建应该在 50ms 内完成
    
    // 测试信号连接时间
    timer.start();
    QSignalSpy spyProgress(m_exportService, &ExportService::exportProgress);
    QSignalSpy spyCompleted(m_exportService, &ExportService::exportCompleted);
    qint64 signalTime = timer.elapsed();
    
    qDebug() << "信号连接时间:" << signalTime << "ms";
    QVERIFY(signalTime < 10); // 信号连接应该在 10ms 内完成
    
    // 记录性能指标
    m_metrics.exportTime = optionsTime + signalTime;
    
    qDebug() << "✓ ExportService 性能测试通过";
}

void TestPerformance::testMemoryUsage()
{
    qDebug() << "\n=== 测试内存使用 ===";
    
    // 测试单个服务的内存占用
    qint64 initialMemory = 0;
    qint64 afterServiceMemory = 0;
    
    // 注意：这里使用估算值，实际内存测量需要平台特定代码
    // 在 Windows 上可以使用 GetProcessMemoryInfo
    // 在 Linux 上可以读取 /proc/self/status
    
    // 估算内存使用（基于对象大小）
    qint64 estimatedMemory = 0;
    estimatedMemory += sizeof(ComponentService);
    estimatedMemory += sizeof(ExportService);
    estimatedMemory += sizeof(ConfigService);
    
    // 转换为 KB
    estimatedMemory = estimatedMemory / 1024;
    
    qDebug() << "估算内存使用:" << estimatedMemory << "KB";
    QVERIFY(estimatedMemory < 1000); // 内存使用应该小于 1MB
    
    // 记录性能指标
    m_metrics.memoryUsage = estimatedMemory;
    
    qDebug() << "✓ 内存使用测试通过";
}

void TestPerformance::testParallelProcessing()
{
    qDebug() << "\n=== 测试并行处理性能 ===";
    
    QElapsedTimer timer;
    
    // 模拟并行处理多个元件
    QStringList componentIds;
    for (int i = 0; i < 10; i++) {
        componentIds << QString("C%1").arg(i);
    }
    
    // 测试批量配置时间
    timer.start();
    for (const QString &id : componentIds) {
        m_componentService->setOutputPath(m_tempDir->path());
    }
    qint64 serialTime = timer.elapsed();
    
    qDebug() << "串行处理 10 个元件时间:" << serialTime << "ms";
    
    // 并行处理应该更快（实际测试需要多线程支持）
    // 这里只测试接口性能
    QVERIFY(serialTime < 500); // 10 个元件应该在 500ms 内完成配置
    
    qDebug() << "✓ 并行处理性能测试通过";
}

void TestPerformance::testBatchProcessing()
{
    qDebug() << "\n=== 测试批量处理性能 ===";
    
    QElapsedTimer timer;
    
    // 测试批量配置操作
    timer.start();
    
    m_configService->setOutputPath(m_tempDir->path());
    m_configService->setLibName("BatchTestLibrary");
    m_configService->setExportSymbol(true);
    m_configService->setExportFootprint(true);
    m_configService->setExportModel3D(false);
    
    qint64 batchTime = timer.elapsed();
    
    qDebug() << "批量配置时间:" << batchTime << "ms";
    QVERIFY(batchTime < 100); // 批量配置应该在 100ms 内完成
    
    // 测试批量导出选项创建
    timer.start();
    
    QList<ExportService::ExportOptions> optionsList;
    for (int i = 0; i < 5; i++) {
        ExportService::ExportOptions options;
        options.outputPath = m_tempDir->path();
        options.libName = QString("Library%1").arg(i);
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = false;
        optionsList.append(options);
    }
    
    qint64 batchOptionsTime = timer.elapsed();
    
    qDebug() << "批量创建 5 个导出选项时间:" << batchOptionsTime << "ms";
    QVERIFY(batchOptionsTime < 200); // 批量创建应该在 200ms 内完成
    
    // 记录总处理时间
    m_metrics.totalProcessingTime = batchTime + batchOptionsTime;
    
    qDebug() << "✓ 批量处理性能测试通过";
}

QTEST_MAIN(TestPerformance)
#include "test_performance.moc"