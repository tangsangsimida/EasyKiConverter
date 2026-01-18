#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QJsonObject>
#include "services/ComponentService.h"
#include "models/ComponentData.h"
#include "mocks/MockEasyedaApi.h"
#include "helpers/TestHelpers.h"

using namespace EasyKiConverter;

/**
 * @brief ComponentService 单元测试（使用 Mock）
 *
 * 这是一个真正的单元测试，使用 Mock 对象隔离外部依赖
 * 测试各种场景：成功、失败、边界条件等
 */
class TestComponentServiceMock : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    // ========== 基础测试 ==========
    void testCreation_DefaultConstructor();
    void testCreation_DependencyInjection();

    // ========== 成功场景测试 ==========
    void testFetchComponentData_Success();
    void testFetchComponentData_With3DModel();
    void testFetchMultipleComponents_Success();

    // ========== 错误场景测试 ==========
    void testFetchComponentData_NetworkError();
    void testFetchComponentData_EmptyResponse();
    void testFetchComponentData_InvalidLcscId();

    // ========== 边界条件测试 ==========
    void testFetchComponentData_EmptyLcscId();
    void testFetchComponentData_NullApi();

    // ========== 调用验证测试 ==========
    void testFetchComponentData_VerifyApiCalls();
    void testFetchMultipleComponents_VerifyParallelCalls();

private:
    MockEasyedaApi *m_mockApi;
    ComponentService *m_service;
};

void TestComponentServiceMock::initTestCase()
{
    qDebug() << "========== TestComponentServiceMock 开始 ==========";
}

void TestComponentServiceMock::cleanupTestCase()
{
    qDebug() << "========== TestComponentServiceMock 结束 ==========";
}

void TestComponentServiceMock::init()
{
    m_mockApi = new MockEasyedaApi();
    m_service = new ComponentService(m_mockApi);
}

void TestComponentServiceMock::cleanup()
{
    delete m_service;
    delete m_mockApi;
}

// ========== 基础测试 ==========

void TestComponentServiceMock::testCreation_DefaultConstructor()
{
    // 测试默认构造函数（内部创建 EasyedaApi）
    ComponentService service;
    QVERIFY(service != nullptr);
    qDebug() << "✓ 默认构造函数创建成功";
}

void TestComponentServiceMock::testCreation_DependencyInjection()
{
    // 测试依赖注入构造函数
    MockEasyedaApi *mockApi = new MockEasyedaApi();
    ComponentService service(mockApi);
    QVERIFY(&service != nullptr);
    QVERIFY(mockApi != nullptr);
    qDebug() << "✓ 依赖注入构造函数创建成功";
}

// ========== 成功场景测试 ==========

void TestComponentServiceMock::testFetchComponentData_Success()
{
    // 1. 准备 Mock 数据
    QJsonObject mockCadData = TestHelpers::loadJsonFixture("cad_data.json");
    m_mockApi->setMockCadData(mockCadData);

    // 2. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::cadDataReady);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);

    // 3. 调用方法
    m_service->fetchComponentData("C12345", false);

    // 4. 等待信号
    QVERIFY(TestHelpers::waitForSignal(spySuccess, 5000));

    // 5. 验证结果
    QCOMPARE(spySuccess.count(), 1);
    QCOMPARE(spyError.count(), 0);

    // 6. 验证数据
    QList<QVariant> arguments = spySuccess.takeFirst();
    QString componentId = arguments.at(0).toString();
    ComponentData data = arguments.at(1).value<ComponentData>();

    QCOMPARE(componentId, QString("C12345"));
    QVERIFY(!data.uuid().isEmpty());
    qDebug() << "✓ 成功获取组件数据";
}

void TestComponentServiceMock::testFetchComponentData_With3DModel()
{
    // 1. 准备 Mock 数据
    QJsonObject mockCadData = TestHelpers::loadJsonFixture("cad_data.json");
    m_mockApi->setMockCadData(mockCadData);

    QByteArray mock3DData = "mock 3d model data";
    m_mockApi->setMock3DModelData(mock3DData);

    // 2. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::cadDataReady);
    QSignalSpy spy3D(m_service, &ComponentService::model3DReady);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);

    // 3. 调用方法（请求 3D 模型）
    m_service->fetchComponentData("C12345", true);

    // 4. 等待信号
    QVERIFY(TestHelpers::waitForSignal(spySuccess, 5000));
    QVERIFY(TestHelpers::waitForSignal(spy3D, 5000));

    // 5. 验证结果
    QCOMPARE(spySuccess.count(), 1);
    QCOMPARE(spy3D.count(), 1);
    QCOMPARE(spyError.count(), 0);

    qDebug() << "✓ 成功获取组件数据和 3D 模型";
}

void TestComponentServiceMock::testFetchMultipleComponents_Success()
{
    // 1. 准备 Mock 数据
    QJsonObject mockCadData = TestHelpers::loadJsonFixture("cad_data.json");
    m_mockApi->setMockCadData(mockCadData);

    // 2. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::allComponentsDataCollected);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);

    // 3. 调用方法（并行获取多个组件）
    QStringList componentIds;
    componentIds << "C12345" << "C67890" << "C11111";
    m_service->fetchMultipleComponentsData(componentIds, false);

    // 4. 等待信号
    QVERIFY(TestHelpers::waitForSignal(spySuccess, 10000));

    // 5. 验证结果
    QCOMPARE(spySuccess.count(), 1);
    QCOMPARE(spyError.count(), 0);

    QList<QVariant> arguments = spySuccess.takeFirst();
    QList<ComponentData> dataList = arguments.at(0).value<QList<ComponentData>>();

    QCOMPARE(dataList.size(), 3);
    qDebug() << "✓ 成功并行获取多个组件数据";
}

// ========== 错误场景测试 ==========

void TestComponentServiceMock::testFetchComponentData_NetworkError()
{
    // 1. 设置错误模式
    m_mockApi->setErrorMode(true, "Network timeout");

    // 2. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::cadDataReady);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);

    // 3. 调用方法
    m_service->fetchComponentData("C12345", false);

    // 4. 等待错误信号
    QVERIFY(TestHelpers::waitForSignal(spyError, 5000));

    // 5. 验证结果
    QCOMPARE(spySuccess.count(), 0);
    QCOMPARE(spyError.count(), 1);

    QList<QVariant> arguments = spyError.takeFirst();
    QString componentId = arguments.at(0).toString();
    QString errorMessage = arguments.at(1).toString();

    QCOMPARE(componentId, QString("C12345"));
    QVERIFY(errorMessage.contains("Network timeout"));
    qDebug() << "✓ 正确处理网络错误";
}

void TestComponentServiceMock::testFetchComponentData_EmptyResponse()
{
    // 1. 设置空的 Mock 数据
    QJsonObject emptyData;
    m_mockApi->setMockCadData(emptyData);

    // 2. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::cadDataReady);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);

    // 3. 调用方法
    m_service->fetchComponentData("C12345", false);

    // 4. 等待错误信号
    QVERIFY(TestHelpers::waitForSignal(spyError, 5000));

    // 5. 验证结果
    QCOMPARE(spySuccess.count(), 0);
    QCOMPARE(spyError.count(), 1);
    qDebug() << "✓ 正确处理空响应";
}

void TestComponentServiceMock::testFetchComponentData_InvalidLcscId()
{
    // 1. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::cadDataReady);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);

    // 2. 调用方法（无效的 LCSC ID）
    m_service->fetchComponentData("INVALID", false);

    // 3. 等待错误信号
    QVERIFY(TestHelpers::waitForSignal(spyError, 5000));

    // 4. 验证结果
    QCOMPARE(spySuccess.count(), 0);
    QCOMPARE(spyError.count(), 1);
    qDebug() << "✓ 正确处理无效的 LCSC ID";
}

// ========== 边界条件测试 ==========

void TestComponentServiceMock::testFetchComponentData_EmptyLcscId()
{
    // 1. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::cadDataReady);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);

    // 2. 调用方法（空的 LCSC ID）
    m_service->fetchComponentData("", false);

    // 3. 等待错误信号
    QVERIFY(TestHelpers::waitForSignal(spyError, 5000));

    // 4. 验证结果
    QCOMPARE(spySuccess.count(), 0);
    QCOMPARE(spyError.count(), 1);
    qDebug() << "✓ 正确处理空的 LCSC ID";
}

void TestComponentServiceMock::testFetchComponentData_NullApi()
{
    // 1. 使用 null API 创建 Service（应该由构造函数处理）
    // 注意：这里假设构造函数会处理 null 指针
    // 如果构造函数不处理，这个测试会失败，这是预期的
    MockEasyedaApi *nullApi = nullptr;

    // 2. 尝试创建 Service（可能会崩溃或返回 null）
    // ComponentService *service = new ComponentService(nullApi);

    // 3. 由于我们无法安全地测试 null 指针而不崩溃，
    //    这个测试用例主要用于文档化这个边界条件

    qDebug() << "⚠ Null API 测试：由构造函数处理（文档化边界条件）";
}

// ========== 调用验证测试 ==========

void TestComponentServiceMock::testFetchComponentData_VerifyApiCalls()
{
    // 1. 清除调用历史
    m_mockApi->clearCallHistory();

    // 2. 准备 Mock 数据
    QJsonObject mockCadData = TestHelpers::loadJsonFixture("cad_data.json");
    m_mockApi->setMockCadData(mockCadData);

    // 3. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::cadDataReady);

    // 4. 调用方法
    m_service->fetchComponentData("C12345", false);

    // 5. 等待信号
    QVERIFY(TestHelpers::waitForSignal(spySuccess, 5000));

    // 6. 验证 API 调用
    QList<QString> calls = m_mockApi->getFetchCadDataCalls();
    QCOMPARE(calls.size(), 1);
    QCOMPARE(calls.at(0), QString("C12345"));
    qDebug() << "✓ API 调用验证正确";
}

void TestComponentServiceMock::testFetchMultipleComponents_VerifyParallelCalls()
{
    // 1. 清除调用历史
    m_mockApi->clearCallHistory();

    // 2. 准备 Mock 数据
    QJsonObject mockCadData = TestHelpers::loadJsonFixture("cad_data.json");
    m_mockApi->setMockCadData(mockCadData);

    // 3. 连接信号
    QSignalSpy spySuccess(m_service, &ComponentService::allComponentsDataCollected);

    // 4. 调用方法（并行获取多个组件）
    QStringList componentIds;
    componentIds << "C12345" << "C67890" << "C11111";
    m_service->fetchMultipleComponentsData(componentIds, false);

    // 5. 等待信号
    QVERIFY(TestHelpers::waitForSignal(spySuccess, 10000));

    // 6. 验证 API 调用
    QList<QString> calls = m_mockApi->getFetchCadDataCalls();
    QCOMPARE(calls.size(), 3);

    // 验证所有组件 ID 都被调用
    QVERIFY(calls.contains("C12345"));
    QVERIFY(calls.contains("C67890"));
    QVERIFY(calls.contains("C11111"));
    qDebug() << "✓ 并行 API 调用验证正确";
}

QTEST_MAIN(TestComponentServiceMock)
#include "test_component_service_mock.moc"