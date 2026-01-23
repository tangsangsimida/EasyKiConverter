#include <QtTest/QtTest>

#include <QSignalSpy>

#include "core/easyeda/EasyedaApi.h"
#include "core/easyeda/EasyedaImporter.h"
#include "services/ComponentDataCollector.h"

using namespace EasyKiConverter;

class TestComponentDataCollector : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 测试用例
    void testCreation();
    void testStateMachine();
    void testStart();
    void testCancel();
    void testSignals();

private:
    ComponentDataCollector* m_collector;
    EasyedaApi* m_api;
    EasyedaImporter* m_importer;
};

void TestComponentDataCollector::initTestCase() {
    qDebug() << "========== TestComponentDataCollector 开始 ==========";
}

void TestComponentDataCollector::cleanupTestCase() {
    qDebug() << "========== TestComponentDataCollector 结束 ==========";
}

void TestComponentDataCollector::init() {
    m_api = new EasyedaApi(this);
    m_importer = new EasyedaImporter(this);
    m_collector = new ComponentDataCollector("C12345", m_api, m_importer, this);
}

void TestComponentDataCollector::cleanup() {
    delete m_collector;
    delete m_importer;
    delete m_api;
}

void TestComponentDataCollector::testCreation() {
    QVERIFY(m_collector != nullptr);
    QCOMPARE(m_collector->componentId(), QString("C12345"));
    QCOMPARE(m_collector->state(), ComponentDataCollector::Idle);
    qDebug() << "✓ ComponentDataCollector 创建成功，初始状态正确";
}

void TestComponentDataCollector::testStateMachine() {
    // 测试状态机状态
    QCOMPARE(m_collector->state(), ComponentDataCollector::Idle);

    // 设置 3D 模型导出选项
    m_collector->setExport3DModel(true);

    qDebug() << "✓ 状态机初始状态正确";
}

void TestComponentDataCollector::testStart() {
    // 测试启动数据收集
    QSignalSpy spyStarted(m_collector, &ComponentDataCollector::started);
    QSignalSpy spyStateChanged(m_collector, &ComponentDataCollector::stateChanged);

    m_collector->start();

    // 验证状态变为 FetchingCadData
    QCOMPARE(m_collector->state(), ComponentDataCollector::FetchingCadData);

    qDebug() << "✓ start() 方法工作正常，状态转换正确";
}

void TestComponentDataCollector::testCancel() {
    // 测试取消数据收集
    m_collector->start();
    m_collector->cancel();

    // 验证状态变为 Failed 或 Idle
    QVERIFY(m_collector->state() == ComponentDataCollector::Failed ||
            m_collector->state() == ComponentDataCollector::Idle);

    qDebug() << "✓ cancel() 方法工作正常";
}

void TestComponentDataCollector::testSignals() {
    // 测试所有信号
    QSignalSpy spyStarted(m_collector, &ComponentDataCollector::started);
    QSignalSpy spyCompleted(m_collector, &ComponentDataCollector::completed);
    QSignalSpy spyFailed(m_collector, &ComponentDataCollector::failed);
    QSignalSpy spyStateChanged(m_collector, &ComponentDataCollector::stateChanged);
    QSignalSpy spyDataCollected(m_collector, &ComponentDataCollector::dataCollected);
    QSignalSpy spyError(m_collector, &ComponentDataCollector::errorOccurred);

    QVERIFY(spyStarted.isValid());
    QVERIFY(spyCompleted.isValid());
    QVERIFY(spyFailed.isValid());
    QVERIFY(spyStateChanged.isValid());
    QVERIFY(spyDataCollected.isValid());
    QVERIFY(spyError.isValid());

    qDebug() << "✓ 所有信号定义正确";
}

QTEST_MAIN(TestComponentDataCollector)
#include "test_component_data_collector.moc"