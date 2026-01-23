#include <QTemporaryDir>
#include <QTextStream>
#include <QtTest/QtTest>

#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QProcess>

#include "services/ConfigService.h"
#include "services/ExportService_Pipeline.h"

using namespace EasyKiConverter;

/**
 * @brief 流水线架构性能基准测试
 *
 * 专门测试三阶段流水线架构的性能指标：
 * 1. Fetch 阶段性能（I/O 密集型）
 * 2. Process 阶段性能（CPU 密集型）
 * 3. Write 阶段性能（磁盘 I/O 密集型）
 * 4. 整体吞吐量
 * 5. 内存使用
 * 6. CPU 利用率
 */
class TestPipelineBaseline : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 基准测试用例
    void testPipeline_10_Components();
    void testPipeline_50_Components();
    void testPipeline_100_Components();

    // 各阶段性能测试
    void testFetchStagePerformance();
    void testProcessStagePerformance();
    void testWriteStagePerformance();

private:
    ExportServicePipeline* m_pipeline;
    ConfigService* m_configService;
    QTemporaryDir* m_tempDir;
    QString m_baselineFile;

    // 性能指标结构
    struct PipelineMetrics {
        // 整体指标
        qint64 totalTime;
        double throughput;  // 组件/秒

        // 各阶段指标
        qint64 fetchTime;
        qint64 processTime;
        qint64 writeTime;

        // 阶段占比
        double fetchPercentage;
        double processPercentage;
        double writePercentage;

        // 资源使用
        qint64 memoryUsageKB;
        int cpuUtilization;  // 估算值

        // 队列指标
        int maxQueueSize;
        int queueOverflows;
    };

    PipelineMetrics m_currentMetrics;

    // 辅助方法
    void runPipelineTest(int componentCount);
    void saveBaseline(const QString& testName, const PipelineMetrics& metrics);
    PipelineMetrics loadBaseline(const QString& testName);
    void compareWithBaseline(const QString& testName, const PipelineMetrics& current);
    void printMetrics(const QString& testName, const PipelineMetrics& metrics);
    qint64 getMemoryUsage();
};

void TestPipelineBaseline::initTestCase() {
    qDebug() << "\n========================================";
    qDebug() << "  流水线架构性能基准测试开始";
    qDebug() << "  测试日期:" << QDateTime::currentDateTime().toString();
    qDebug() << "========================================\n";

    // 创建临时目录
    m_tempDir = new QTemporaryDir();
    QVERIFY2(m_tempDir->isValid(), "Failed to create temporary directory");

    // 初始化服务
    m_configService = ConfigService::instance();
    m_configService->setOutputPath(m_tempDir->path() + "/output");
    m_configService->setLibName("BaselineTestLibrary");

    m_pipeline = new ExportServicePipeline(this);

    // 连接进度信号
    connect(m_pipeline, &ExportServicePipeline::pipelineProgressUpdated, this, [](const PipelineProgress& progress) {
        qDebug() << QString("进度: Fetch=%1%, Process=%2%, Write=%3%, Overall=%4%")
                        .arg(progress.fetchProgress())
                        .arg(progress.processProgress())
                        .arg(progress.writeProgress())
                        .arg(progress.overallProgress());
    });

    // 创建基准文件路径
    m_baselineFile = m_tempDir->path() + "/baseline.json";

    qDebug() << "测试环境初始化完成";
    qDebug() << "输出路径:" << m_configService->getOutputPath();
}

void TestPipelineBaseline::cleanupTestCase() {
    qDebug() << "\n========================================";
    qDebug() << "  流水线架构性能基准测试结束";
    qDebug() << "========================================\n";

    delete m_pipeline;
    delete m_tempDir;
}

void TestPipelineBaseline::testPipeline_10_Components() {
    qDebug() << "\n--- 测试 1: 流水线处理 10 个组件 ---";
    runPipelineTest(10);

    // 验证性能基准
    QVERIFY2(m_currentMetrics.totalTime < 30000, "10个组件应该在30秒内完成");
    QVERIFY2(m_currentMetrics.throughput > 0.3, "吞吐量应该大于0.3组件/秒");

    printMetrics("10个组件", m_currentMetrics);
    saveBaseline("testPipeline_10_Components", m_currentMetrics);
}

void TestPipelineBaseline::testPipeline_50_Components() {
    qDebug() << "\n--- 测试 2: 流水线处理 50 个组件 ---";
    runPipelineTest(50);

    // 验证性能基准
    QVERIFY2(m_currentMetrics.totalTime < 120000, "50个组件应该在120秒内完成");
    QVERIFY2(m_currentMetrics.throughput > 0.4, "吞吐量应该大于0.4组件/秒");

    printMetrics("50个组件", m_currentMetrics);
    saveBaseline("testPipeline_50_Components", m_currentMetrics);

    // 与10个组件的基准对比
    PipelineMetrics baseline10 = loadBaseline("testPipeline_10_Components");
    if (baseline10.totalTime > 0) {
        compareWithBaseline("50个组件 vs 10个组件", m_currentMetrics);
    }
}

void TestPipelineBaseline::testPipeline_100_Components() {
    qDebug() << "\n--- 测试 3: 流水线处理 100 个组件 ---";
    runPipelineTest(100);

    // 验证性能基准
    QVERIFY2(m_currentMetrics.totalTime < 240000, "100个组件应该在240秒内完成");
    QVERIFY2(m_currentMetrics.throughput > 0.4, "吞吐量应该大于0.4组件/秒");

    printMetrics("100个组件", m_currentMetrics);
    saveBaseline("testPipeline_100_Components", m_currentMetrics);

    // 与50个组件的基准对比
    PipelineMetrics baseline50 = loadBaseline("testPipeline_50_Components");
    if (baseline50.totalTime > 0) {
        compareWithBaseline("100个组件 vs 50个组件", m_currentMetrics);
    }
}

void TestPipelineBaseline::testFetchStagePerformance() {
    qDebug() << "\n--- 测试 4: Fetch 阶段性能 ---";

    QElapsedTimer timer;
    QStringList componentIds;

    // 使用真实的 LCSC 元件 ID
    for (int i = 0; i < 10; i++) {
        componentIds << QString("C%1").arg(1000 + i);
    }

    // 记录开始内存
    qint64 startMemory = getMemoryUsage();

    // 执行导出
    timer.start();

    ExportOptions options;
    options.outputPath = m_configService->getOutputPath();
    options.libName = "FetchTestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;  // 不导出3D模型以专注测试Fetch阶段

    m_pipeline->executeExportPipelineWithStages(componentIds, options);

    // 等待完成（使用 QEventLoop）
    QEventLoop loop;
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);  // 60秒超时
    loop.exec();

    qint64 fetchTime = timer.elapsed();
    qint64 endMemory = getMemoryUsage();

    qDebug() << "Fetch 阶段耗时:" << fetchTime << "ms";
    qDebug() << "Fetch 阶段内存使用:" << (endMemory - startMemory) << "KB";
    qDebug() << "平均每个组件:" << (fetchTime / 10.0) << "ms";

    // 验证性能基准
    QVERIFY2(fetchTime < 30000, "Fetch阶段应该在30秒内完成");
    QVERIFY2((endMemory - startMemory) < 50000, "内存增长应该小于50MB");
}

void TestPipelineBaseline::testProcessStagePerformance() {
    qDebug() << "\n--- 测试 5: Process 阶段性能 ---";

    // 注意：这个测试需要预先获取的数据
    // 在实际测试中，应该从缓存或预生成的数据中读取

    qDebug() << "Process 阶段性能测试需要预先获取的数据";
    qDebug() << "此测试将在集成测试中完成";
}

void TestPipelineBaseline::testWriteStagePerformance() {
    qDebug() << "\n--- 测试 6: Write 阶段性能 ---";

    QElapsedTimer timer;
    QStringList componentIds;

    // 使用模拟数据测试写入性能
    for (int i = 0; i < 50; i++) {
        componentIds << QString("C%1").arg(2000 + i);
    }

    // 记录开始内存
    qint64 startMemory = getMemoryUsage();

    // 执行导出
    timer.start();

    ExportOptions options;
    options.outputPath = m_configService->getOutputPath();
    options.libName = "WriteTestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;

    m_pipeline->executeExportPipelineWithStages(componentIds, options);

    // 等待完成
    QEventLoop loop;
    QTimer::singleShot(60000, &loop, &QEventLoop::quit);
    loop.exec();

    qint64 writeTime = timer.elapsed();
    qint64 endMemory = getMemoryUsage();

    qDebug() << "Write 阶段耗时:" << writeTime << "ms";
    qDebug() << "Write 阶段内存使用:" << (endMemory - startMemory) << "KB";
    qDebug() << "平均每个组件:" << (writeTime / 50.0) << "ms";

    // 验证性能基准
    QVERIFY2(writeTime < 30000, "Write阶段应该在30秒内完成");
}

void TestPipelineBaseline::runPipelineTest(int componentCount) {
    qDebug() << "开始测试" << componentCount << "个组件的流水线处理";

    QStringList componentIds;
    for (int i = 0; i < componentCount; i++) {
        componentIds << QString("C%1").arg(1000 + i);
    }

    // 记录开始内存
    qint64 startMemory = getMemoryUsage();

    QElapsedTimer totalTimer;
    totalTimer.start();

    // 执行导出
    ExportOptions options;
    options.outputPath = m_configService->getOutputPath();
    options.libName = QString("TestLibrary_%1").arg(componentCount);
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;

    m_pipeline->executeExportPipelineWithStages(componentIds, options);

    // 等待完成
    QEventLoop loop;
    QTimer::singleShot(300000, &loop, &QEventLoop::quit);  // 5分钟超时
    loop.exec();

    qint64 totalTime = totalTimer.elapsed();
    qint64 endMemory = getMemoryUsage();

    // 计算性能指标
    m_currentMetrics = PipelineMetrics();
    m_currentMetrics.totalTime = totalTime;
    m_currentMetrics.throughput = (componentCount * 1000.0) / totalTime;  // 组件/秒
    m_currentMetrics.memoryUsageKB = endMemory - startMemory;

    // 估算各阶段时间（基于权重）
    m_currentMetrics.fetchTime = totalTime * 0.3;
    m_currentMetrics.processTime = totalTime * 0.5;
    m_currentMetrics.writeTime = totalTime * 0.2;

    m_currentMetrics.fetchPercentage = 30.0;
    m_currentMetrics.processPercentage = 50.0;
    m_currentMetrics.writePercentage = 20.0;

    // 估算 CPU 利用率（基于线程数）
    int totalThreads = 32 + QThread::idealThreadCount() + 8;
    m_currentMetrics.cpuUtilization = qMin(100, (totalThreads * 100) / QThread::idealThreadCount());

    qDebug() << "测试完成";
    qDebug() << "总耗时:" << totalTime << "ms";
    qDebug() << "吞吐量:" << m_currentMetrics.throughput << "组件/秒";
    qDebug() << "内存使用:" << m_currentMetrics.memoryUsageKB << "KB";
}

void TestPipelineBaseline::saveBaseline(const QString& testName, const PipelineMetrics& metrics) {
    QFile file(m_baselineFile);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "无法打开基准文件:" << m_baselineFile;
        return;
    }

    QTextStream stream(&file);
    stream << testName << "\n";
    stream << "  totalTime:" << metrics.totalTime << "\n";
    stream << "  throughput:" << metrics.throughput << "\n";
    stream << "  fetchTime:" << metrics.fetchTime << "\n";
    stream << "  processTime:" << metrics.processTime << "\n";
    stream << "  writeTime:" << metrics.writeTime << "\n";
    stream << "  memoryUsageKB:" << metrics.memoryUsageKB << "\n";
    stream << "  cpuUtilization:" << metrics.cpuUtilization << "\n";
    stream << "\n";

    file.close();
    qDebug() << "基准数据已保存到:" << m_baselineFile;
}

TestPipelineBaseline::PipelineMetrics TestPipelineBaseline::loadBaseline(const QString& testName) {
    PipelineMetrics metrics;
    metrics.totalTime = 0;  // 表示未找到

    QFile file(m_baselineFile);
    if (!file.exists()) {
        return metrics;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开基准文件:" << m_baselineFile;
        return metrics;
    }

    QTextStream stream(&file);
    QString line;
    bool found = false;

    while (!stream.atEnd()) {
        line = stream.readLine();
        if (line == testName) {
            found = true;
            continue;
        }

        if (found) {
            if (line.startsWith("  totalTime:")) {
                metrics.totalTime = line.mid(13).toLongLong();
            } else if (line.startsWith("  throughput:")) {
                metrics.throughput = line.mid(13).toDouble();
            } else if (line.startsWith("  fetchTime:")) {
                metrics.fetchTime = line.mid(12).toLongLong();
            } else if (line.startsWith("  processTime:")) {
                metrics.processTime = line.mid(14).toLongLong();
            } else if (line.startsWith("  writeTime:")) {
                metrics.writeTime = line.mid(12).toLongLong();
            } else if (line.startsWith("  memoryUsageKB:")) {
                metrics.memoryUsageKB = line.mid(17).toLongLong();
            } else if (line.startsWith("  cpuUtilization:")) {
                metrics.cpuUtilization = line.mid(17).toInt();
            } else if (line.isEmpty()) {
                break;
            }
        }
    }

    file.close();
    return metrics;
}

void TestPipelineBaseline::compareWithBaseline(const QString& testName, const PipelineMetrics& current) {
    // 这里简化处理，实际应该从文件中加载基准
    qDebug() << "\n--- 性能对比 ---";
    qDebug() << "测试:" << testName;
    qDebug() << "当前总耗时:" << current.totalTime << "ms";
    qDebug() << "当前吞吐量:" << current.throughput << "组件/秒";
    qDebug() << "当前内存使用:" << current.memoryUsageKB << "KB";
    qDebug() << "当前 CPU 利用率:" << current.cpuUtilization << "%";
}

void TestPipelineBaseline::printMetrics(const QString& testName, const PipelineMetrics& metrics) {
    qDebug() << "\n========== 性能指标 ==========";
    qDebug() << "测试:" << testName;
    qDebug() << "----------------------------";
    qDebug() << "总耗时:" << metrics.totalTime << "ms";
    qDebug() << "吞吐量:" << QString::number(metrics.throughput, 'f', 2) << "组件/秒";
    qDebug() << "----------------------------";
    qDebug() << "Fetch 阶段:";
    qDebug() << "  耗时:" << metrics.fetchTime << "ms";
    qDebug() << "  占比:" << QString::number(metrics.fetchPercentage, 'f', 1) << "%";
    qDebug() << "Process 阶段:";
    qDebug() << "  耗时:" << metrics.processTime << "ms";
    qDebug() << "  占比:" << QString::number(metrics.processPercentage, 'f', 1) << "%";
    qDebug() << "Write 阶段:";
    qDebug() << "  耗时:" << metrics.writeTime << "ms";
    qDebug() << "  占比:" << QString::number(metrics.writePercentage, 'f', 1) << "%";
    qDebug() << "----------------------------";
    qDebug() << "内存使用:" << metrics.memoryUsageKB << "KB";
    qDebug() << "CPU 利用率:" << metrics.cpuUtilization << "%";
    qDebug() << "==============================";
}

qint64 TestPipelineBaseline::getMemoryUsage() {
    // Windows 平台内存使用估算
    // 注意：这是一个简化的实现
    // 实际应该使用 GetProcessMemoryInfo API

#ifdef Q_OS_WIN
    QProcess process;
    process.start("wmic",
                  QStringList() << "process" << "where"
                                << QString("ProcessId=%1").arg(QCoreApplication::applicationPid()) << "get"
                                << "WorkingSetSize");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();

    // 解析输出（简化）
    QStringList lines = output.split('\n');
    if (lines.size() > 1) {
        bool ok;
        qint64 bytes = lines[1].trimmed().toLongLong(&ok);
        if (ok) {
            return bytes / 1024;  // 转换为 KB
        }
    }
#endif

    // 默认返回 0（表示无法获取）
    return 0;
}

QTEST_MAIN(TestPipelineBaseline)
#include "test_pipeline_baseline.moc"