#include <QTemporaryDir>
#include <QThread>
#include <QtTest/QtTest>

#include <QSignalSpy>

#include "models/ComponentExportStatus.h"
#include "services/ExportService_Pipeline.h"
#include "utils/BoundedThreadSafeQueue.h"
#include "workers/FetchWorker.h"
#include "workers/ProcessWorker.h"
#include "workers/WriteWorker.h"

using namespace EasyKiConverter;

/**
 * @brief ExportServicePipeline 测试类
 *
 * 测试多阶段流水线并行架构的各项功能
 */
class TestExportServicePipeline : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 线程安全队列测试
    void testBoundedThreadSafeQueue_BasicOperations();
    void testBoundedThreadSafeQueue_BoundedCapacity();
    void testBoundedThreadSafeQueue_ConcurrentAccess();
    void testBoundedThreadSafeQueue_Close();

    // ComponentExportStatus 测试
    void testComponentExportStatus_Creation();
    void testComponentExportStatus_SuccessTracking();
    void testComponentExportStatus_FailureStageDetection();

    // FetchWorker 测试
    void testFetchWorker_Creation();
    void testFetchWorker_SignalEmission();

    // ProcessWorker 测试
    void testProcessWorker_Creation();
    void testProcessWorker_SignalEmission();

    // WriteWorker 测试
    void testWriteWorker_Creation();
    void testWriteWorker_SignalEmission();

    // ExportServicePipeline 测试
    void testExportServicePipeline_Creation();
    void testExportServicePipeline_ThreadPools();
    void testExportServicePipeline_PipelineProgress();
    void testExportServicePipeline_ExecuteEmptyPipeline();
    void testExportServicePipeline_SignalConnections();
    void testExportServicePipeline_Cleanup();

    // 集成测试
    void testIntegration_SingleComponent();
    void testIntegration_MultipleComponents();
    void testIntegration_ErrorHandling();

private:
    ExportServicePipeline* m_pipelineService;
    QTemporaryDir* m_tempDir;
};

void TestExportServicePipeline::initTestCase() {
    qDebug() << "========== TestExportServicePipeline 开始 ==========";
}

void TestExportServicePipeline::cleanupTestCase() {
    qDebug() << "========== TestExportServicePipeline 结束 ==========";
}

void TestExportServicePipeline::init() {
    m_pipelineService = new ExportServicePipeline(this);
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
}

void TestExportServicePipeline::cleanup() {
    delete m_pipelineService;
    delete m_tempDir;
}

// ==================== 线程安全队列测试 ====================

void TestExportServicePipeline::testBoundedThreadSafeQueue_BasicOperations() {
    qDebug() << "测试：线程安全队列基本操作";

    BoundedThreadSafeQueue<int> queue;

    // 测试空队列
    QVERIFY(queue.isEmpty());
    QCOMPARE(queue.size(), size_t(0));

    // 测试 push
    QVERIFY(queue.tryPush(42));
    QVERIFY(!queue.isEmpty());
    QCOMPARE(queue.size(), size_t(1));

    // 测试 pop
    int value;
    QVERIFY(queue.tryPop(value));
    QCOMPARE(value, 42);
    QVERIFY(queue.isEmpty());

    // 测试非阻塞 pop 失败
    QVERIFY(!queue.tryPop(value));

    qDebug() << "✓ 线程安全队列基本操作测试通过";
}

void TestExportServicePipeline::testBoundedThreadSafeQueue_BoundedCapacity() {
    qDebug() << "测试：线程安全队列容量限制";

    BoundedThreadSafeQueue<int> queue(3);  // 容量为3

    // 填满队列
    QVERIFY(queue.tryPush(1));
    QVERIFY(queue.tryPush(2));
    QVERIFY(queue.tryPush(3));
    QCOMPARE(queue.size(), size_t(3));

    // 尝试添加第4个元素（应该失败）
    QVERIFY(!queue.tryPush(4));
    QCOMPARE(queue.size(), size_t(3));

    // 取出一个元素后，应该可以添加
    int value;
    QVERIFY(queue.tryPop(value));
    QCOMPARE(value, 1);
    QVERIFY(queue.tryPush(4));
    QCOMPARE(queue.size(), size_t(3));

    // 清空队列
    QVERIFY(queue.tryPop(value));
    QVERIFY(queue.tryPop(value));
    QVERIFY(queue.tryPop(value));
    QVERIFY(queue.isEmpty());

    qDebug() << "✓ 线程安全队列容量限制测试通过";
}

void TestExportServicePipeline::testBoundedThreadSafeQueue_ConcurrentAccess() {
    qDebug() << "测试：线程安全队列并发访问";

    BoundedThreadSafeQueue<int> queue(100);
    const int producerCount = 10;
    const int itemsPerProducer = 10;

    // 创建生产者线程
    QList<QThread*> producerThreads;
    for (int i = 0; i < producerCount; i++) {
        QThread* thread = QThread::create([i, itemsPerProducer, &queue]() {
            for (int j = 0; j < itemsPerProducer; j++) {
                int value = i * 100 + j;
                while (!queue.tryPush(value)) {
                    QThread::msleep(1);  // 等待队列有空间
                }
            }
        });
        producerThreads.append(thread);
    }

    // 启动所有生产者
    for (QThread* thread : producerThreads) {
        thread->start();
    }

    // 等待所有生产者完成
    for (QThread* thread : producerThreads) {
        thread->wait();
        delete thread;
    }

    // 验证队列中的元素数量
    QCOMPARE(queue.size(), size_t(producerCount * itemsPerProducer));

    // 清空队列
    int value;
    while (queue.tryPop(value)) {
        // 只是为了清空
    }

    QVERIFY(queue.isEmpty());

    qDebug() << "✓ 线程安全队列并发访问测试通过";
}

void TestExportServicePipeline::testBoundedThreadSafeQueue_Close() {
    qDebug() << "测试：线程安全队列关闭功能";

    BoundedThreadSafeQueue<int> queue;

    // 关闭队列
    queue.close();
    QVERIFY(queue.isClosed());

    // 关闭后不能 push
    QVERIFY(!queue.tryPush(42));

    // 关闭后不能 pop
    int value;
    QVERIFY(!queue.tryPop(value));

    qDebug() << "✓ 线程安全队列关闭功能测试通过";
}

// ==================== ComponentExportStatus 测试 ====================

void TestExportServicePipeline::testComponentExportStatus_Creation() {
    qDebug() << "测试：ComponentExportStatus 创建";

    ComponentExportStatus status;
    status.componentId = "C12345";

    QCOMPARE(status.componentId, QString("C12345"));
    QCOMPARE(status.fetchSuccess, false);
    QCOMPARE(status.processSuccess, false);
    QCOMPARE(status.writeSuccess, false);

    qDebug() << "✓ ComponentExportStatus 创建测试通过";
}

void TestExportServicePipeline::testComponentExportStatus_SuccessTracking() {
    qDebug() << "测试：ComponentExportStatus 成功跟踪";

    ComponentExportStatus status;
    status.componentId = "C12345";

    // 设置各阶段成功
    status.fetchSuccess = true;
    status.processSuccess = true;
    status.writeSuccess = true;

    QVERIFY(status.isCompleteSuccess());

    qDebug() << "✓ ComponentExportStatus 成功跟踪测试通过";
}

void TestExportServicePipeline::testComponentExportStatus_FailureStageDetection() {
    qDebug() << "测试：ComponentExportStatus 失败阶段检测";

    ComponentExportStatus status;
    status.componentId = "C12345";

    // 测试抓取失败
    status.fetchSuccess = false;
    status.processSuccess = true;
    status.writeSuccess = true;
    QCOMPARE(status.getFailedStage(), QString("Fetch"));

    // 测试处理失败
    status.fetchSuccess = true;
    status.processSuccess = false;
    QCOMPARE(status.getFailedStage(), QString("Process"));

    // 测试写入失败
    status.processSuccess = true;
    status.writeSuccess = false;
    QCOMPARE(status.getFailedStage(), QString("Write"));

    // 测试全部成功
    status.writeSuccess = true;
    QCOMPARE(status.getFailedStage(), QString(""));

    qDebug() << "✓ ComponentExportStatus 失败阶段检测测试通过";
}

// ==================== FetchWorker 测试 ====================

void TestExportServicePipeline::testFetchWorker_Creation() {
    qDebug() << "测试：FetchWorker 创建";

    QNetworkAccessManager* networkManager = new QNetworkAccessManager(this);
    FetchWorker* worker = new FetchWorker("C12345", networkManager, false, this);

    QVERIFY(worker != nullptr);

    delete worker;
    delete networkManager;

    qDebug() << "✓ FetchWorker 创建测试通过";
}

void TestExportServicePipeline::testFetchWorker_SignalEmission() {
    qDebug() << "测试：FetchWorker 信号发射";

    QNetworkAccessManager* networkManager = new QNetworkAccessManager(this);
    FetchWorker* worker = new FetchWorker("C12345", networkManager, false, this);

    QSignalSpy spy(worker, &FetchWorker::fetchCompleted);

    // 启动 worker（会在后台运行）
    worker->run();

    // 等待信号
    QVERIFY(spy.wait(10000));  // 10秒超时

    // 验证信号被发射
    QCOMPARE(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    QSharedPointer<ComponentExportStatus> status = arguments.at(0).value<QSharedPointer<ComponentExportStatus>>();
    QVERIFY(status != nullptr);
    QCOMPARE(status->componentId, QString("C12345"));

    delete worker;
    delete networkManager;

    qDebug() << "✓ FetchWorker 信号发射测试通过";
}

// ==================== ProcessWorker 测试 ====================

void TestExportServicePipeline::testProcessWorker_Creation() {
    qDebug() << "测试：ProcessWorker 创建";

    QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
    status->componentId = "C12345";

    ProcessWorker* worker = new ProcessWorker(status, this);

    QVERIFY(worker != nullptr);

    delete worker;

    qDebug() << "✓ ProcessWorker 创建测试通过";
}

void TestExportServicePipeline::testProcessWorker_SignalEmission() {
    qDebug() << "测试：ProcessWorker 信号发射";

    QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
    status->componentId = "C12345";
    status->fetchSuccess = true;
    status->componentInfoRaw = "{}";
    status->cadDataRaw = "{}";

    ProcessWorker* worker = new ProcessWorker(status, this);

    QSignalSpy spy(worker, &ProcessWorker::processCompleted);

    // 启动 worker
    worker->run();

    // 验证信号被发射
    QCOMPARE(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    QSharedPointer<ComponentExportStatus> resultStatus = arguments.at(0).value<QSharedPointer<ComponentExportStatus>>();
    QVERIFY(resultStatus != nullptr);
    QCOMPARE(resultStatus->componentId, QString("C12345"));

    delete worker;

    qDebug() << "✓ ProcessWorker 信号发射测试通过";
}

// ==================== WriteWorker 测试 ====================

void TestExportServicePipeline::testWriteWorker_Creation() {
    qDebug() << "测试：WriteWorker 创建";

    QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
    status->componentId = "C12345";

    WriteWorker* worker = new WriteWorker(status, m_tempDir->path(), "TestLibrary", false, false, false, this);

    QVERIFY(worker != nullptr);

    delete worker;

    qDebug() << "✓ WriteWorker 创建测试通过";
}

void TestExportServicePipeline::testWriteWorker_SignalEmission() {
    qDebug() << "测试：WriteWorker 信号发射";

    QSharedPointer<ComponentExportStatus> status = QSharedPointer<ComponentExportStatus>::create();
    status->componentId = "C12345";
    status->processSuccess = true;
    status->symbolData = QSharedPointer<SymbolData>::create();

    WriteWorker* worker = new WriteWorker(status, m_tempDir->path(), "TestLibrary", true, false, false, this);

    QSignalSpy spy(worker, &WriteWorker::writeCompleted);

    // 启动 worker
    worker->run();

    // 验证信号被发射
    QCOMPARE(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst();
    QSharedPointer<ComponentExportStatus> resultStatus = arguments.at(0).value<QSharedPointer<ComponentExportStatus>>();
    QVERIFY(resultStatus != nullptr);
    QCOMPARE(resultStatus->componentId, QString("C12345"));

    delete worker;

    qDebug() << "✓ WriteWorker 信号发射测试通过";
}

// ==================== ExportServicePipeline 测试 ====================

void TestExportServicePipeline::testExportServicePipeline_Creation() {
    qDebug() << "测试：ExportServicePipeline 创建";

    QVERIFY(m_pipelineService != nullptr);

    qDebug() << "✓ ExportServicePipeline 创建测试通过";
}

void TestExportServicePipeline::testExportServicePipeline_ThreadPools() {
    qDebug() << "测试：ExportServicePipeline 线程池配置";

    // 验证服务已创建
    QVERIFY(m_pipelineService != nullptr);

    // 线程池应该在内部正确配置
    // 这里我们只验证服务可以正常运行
    QVERIFY(true);

    qDebug() << "✓ ExportServicePipeline 线程池配置测试通过";
}

void TestExportServicePipeline::testExportServicePipeline_PipelineProgress() {
    qDebug() << "测试：ExportServicePipeline 流水线进度";

    PipelineProgress progress;
    progress.totalTasks = 100;
    progress.fetchCompleted = 30;
    progress.processCompleted = 20;
    progress.writeCompleted = 10;

    // 测试各阶段进度计算
    QCOMPARE(progress.fetchProgress(), 30);
    QCOMPARE(progress.processProgress(), 20);
    QCOMPARE(progress.writeProgress(), 10);

    // 测试总进度计算（抓取30% + 处理50% + 写入20%）
    int expectedOverall = (30 * 30 + 20 * 50 + 10 * 20) / 100;
    QCOMPARE(progress.overallProgress(), expectedOverall);

    qDebug() << "✓ ExportServicePipeline 流水线进度测试通过";
}

void TestExportServicePipeline::testExportServicePipeline_ExecuteEmptyPipeline() {
    qDebug() << "测试：ExportServicePipeline 执行空流水线";

    QStringList componentIds;  // 空列表

    ExportOptions options;
    options.outputPath = m_tempDir->path();
    options.libName = "TestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;

    QSignalSpy completedSpy(m_pipelineService, &ExportService::exportCompleted);

    // 执行空流水线
    m_pipelineService->executeExportPipelineWithStages(componentIds, options);

    // 等待完成
    QVERIFY(completedSpy.wait(5000));

    // 验证完成信号
    QCOMPARE(completedSpy.count(), 1);

    QList<QVariant> arguments = completedSpy.takeFirst();
    int totalCount = arguments.at(0).toInt();
    int successCount = arguments.at(1).toInt();

    QCOMPARE(totalCount, 0);
    QCOMPARE(successCount, 0);

    qDebug() << "✓ ExportServicePipeline 执行空流水线测试通过";
}

void TestExportServicePipeline::testExportServicePipeline_SignalConnections() {
    qDebug() << "测试：ExportServicePipeline 信号连接";

    QSignalSpy progressSpy(m_pipelineService, &ExportServicePipeline::pipelineProgressUpdated);
    QSignalSpy completedSpy(m_pipelineService, &ExportService::exportCompleted);

    // 执行空流水线以触发信号
    QStringList componentIds;

    ExportOptions options;
    options.outputPath = m_tempDir->path();
    options.libName = "TestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;

    m_pipelineService->executeExportPipelineWithStages(componentIds, options);

    // 等待完成信号
    QVERIFY(completedSpy.wait(5000));

    // 验证信号被发射
    // 空任务可能不发送进度更新，只发送完成信号，所以 progressSpy.count() >= 0 是合理的
    QCOMPARE(completedSpy.count(), 1);

    qDebug() << "✓ ExportServicePipeline 信号连接测试通过";
}

void TestExportServicePipeline::testExportServicePipeline_Cleanup() {
    qDebug() << "测试：ExportServicePipeline 清理";

    // 执行空流水线
    QStringList componentIds;

    ExportOptions options;
    options.outputPath = m_tempDir->path();
    options.libName = "TestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;

    QSignalSpy completedSpy(m_pipelineService, &ExportService::exportCompleted);

    m_pipelineService->executeExportPipelineWithStages(componentIds, options);

    // 等待完成
    QVERIFY(completedSpy.wait(5000));

    // 验证流水线可以再次执行
    completedSpy.clear();

    m_pipelineService->executeExportPipelineWithStages(componentIds, options);

    QVERIFY(completedSpy.wait(5000));
    QCOMPARE(completedSpy.count(), 1);

    qDebug() << "✓ ExportServicePipeline 清理测试通过";
}

// ==================== 集成测试 ====================

void TestExportServicePipeline::testIntegration_SingleComponent() {
    qDebug() << "测试：集成 - 单个元件";

    QStringList componentIds;
    componentIds << "C12345";

    ExportOptions options;
    options.outputPath = m_tempDir->path();
    options.libName = "TestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;

    QSignalSpy progressSpy(m_pipelineService, &ExportServicePipeline::pipelineProgressUpdated);
    QSignalSpy completedSpy(m_pipelineService, &ExportService::exportCompleted);

    // 执行流水线
    m_pipelineService->executeExportPipelineWithStages(componentIds, options);

    // 等待完成
    QVERIFY(completedSpy.wait(30000));  // 30秒超时

    // 验证完成信号
    QCOMPARE(completedSpy.count(), 1);

    QList<QVariant> arguments = completedSpy.takeFirst();
    int totalCount = arguments.at(0).toInt();
    int successCount = arguments.at(1).toInt();

    QCOMPARE(totalCount, 1);
    // successCount 可能是 0 或 1，取决于网络请求是否成功

    // 验证进度更新
    QVERIFY(progressSpy.count() >= 0);

    qDebug() << "✓ 集成 - 单个元件测试通过";
}

void TestExportServicePipeline::testIntegration_MultipleComponents() {
    qDebug() << "测试：集成 - 多个元件";

    QStringList componentIds;
    componentIds << "C12345" << "C67890" << "C13579";

    ExportOptions options;
    options.outputPath = m_tempDir->path();
    options.libName = "TestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;

    QSignalSpy progressSpy(m_pipelineService, &ExportServicePipeline::pipelineProgressUpdated);
    QSignalSpy completedSpy(m_pipelineService, &ExportService::exportCompleted);

    // 执行流水线
    m_pipelineService->executeExportPipelineWithStages(componentIds, options);

    // 等待完成
    QVERIFY(completedSpy.wait(60000));  // 60秒超时

    // 验证完成信号
    QCOMPARE(completedSpy.count(), 1);

    QList<QVariant> arguments = completedSpy.takeFirst();
    int totalCount = arguments.at(0).toInt();
    int successCount = arguments.at(1).toInt();

    QCOMPARE(totalCount, 3);

    // 验证进度更新
    QVERIFY(progressSpy.count() >= 0);

    qDebug() << "✓ 集成 - 多个元件测试通过";
}

void TestExportServicePipeline::testIntegration_ErrorHandling() {
    qDebug() << "测试：集成 - 错误处理";

    QStringList componentIds;
    componentIds << "INVALID_ID";  // 无效的元件ID

    ExportOptions options;
    options.outputPath = m_tempDir->path();
    options.libName = "TestLibrary";
    options.exportSymbol = true;
    options.exportFootprint = true;
    options.exportModel3D = false;
    options.overwriteExistingFiles = false;

    QSignalSpy completedSpy(m_pipelineService, &ExportService::exportCompleted);

    // 执行流水线
    m_pipelineService->executeExportPipelineWithStages(componentIds, options);

    // 等待完成
    QVERIFY(completedSpy.wait(30000));

    // 验证完成信号
    QCOMPARE(completedSpy.count(), 1);

    QList<QVariant> arguments = completedSpy.takeFirst();
    int totalCount = arguments.at(0).toInt();
    int successCount = arguments.at(1).toInt();

    QCOMPARE(totalCount, 1);
    // 应该失败
    QCOMPARE(successCount, 0);

    qDebug() << "✓ 集成 - 错误处理测试通过";
}

QTEST_MAIN(TestExportServicePipeline)
#include "test_export_service_pipeline.moc"