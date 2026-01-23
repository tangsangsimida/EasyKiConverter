#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QJsonObject>
#include "services/ComponentService.h"
#include "models/ComponentData.h"

using namespace EasyKiConverter;

class TestComponentService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // 测试用例
    void testCreation();
    void testFetchComponentData();
    void testSetOutputPath();
    void testSignals();

private:
    ComponentService *m_service;
};

void TestComponentService::initTestCase()
{
    qDebug() << "========== TestComponentService 开始 ==========";
}

void TestComponentService::cleanupTestCase()
{
    qDebug() << "========== TestComponentService 结束 ==========";
}

void TestComponentService::init()
{
    m_service = new ComponentService(this);
}

void TestComponentService::cleanup()
{
    delete m_service;
}

void TestComponentService::testCreation()
{
    QVERIFY(m_service != nullptr);
    qDebug() << "✓ ComponentService 创建成功";
}

void TestComponentService::testFetchComponentData()
{
    // 测试基本的元件数据获取接口
    QSignalSpy spy(m_service, &ComponentService::componentInfoReady);
    
    // 调用获取数据方法（注意：这需要网络连接，实际测试可能需要 mock）
    m_service->fetchComponentData("C12345", false);
    
    // 由于需要网络请求，这里只测试方法调用不会崩溃
    QVERIFY(m_service != nullptr);
    qDebug() << "✓ fetchComponentData 方法调用成功";
}

void TestComponentService::testSetOutputPath()
{
    QString testPath = "C:/test/output";
    m_service->setOutputPath(testPath);
    
    QCOMPARE(m_service->getOutputPath(), testPath);
    qDebug() << "✓ setOutputPath/getOutputPath 工作正常";
}

void TestComponentService::testSignals()
{
    // 测试信号连接
    QSignalSpy spyInfo(m_service, &ComponentService::componentInfoReady);
    QSignalSpy spyCad(m_service, &ComponentService::cadDataReady);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);
    
    QVERIFY(spyInfo.isValid());
    QVERIFY(spyCad.isValid());
    QVERIFY(spyError.isValid());
    
    qDebug() << "✓ 所有信号定义正确";
}

QTEST_MAIN(TestComponentService)
#include "test_component_service.moc"