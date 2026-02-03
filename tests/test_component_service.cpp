#include "models/ComponentData.h"
#include "services/ComponentService.h"

#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTextStream>
#include <QtTest/QtTest>

using namespace EasyKiConverter;

class TestComponentService : public QObject {
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
    void testValidateComponentId();
    void testExtractComponentIdFromText();
    void testParseBomFile();

private:
    ComponentService* m_service;
};

void TestComponentService::initTestCase() {
    qDebug() << "========== TestComponentService 开始 ==========";
}

void TestComponentService::cleanupTestCase() {
    qDebug() << "========== TestComponentService 结束 ==========";
}

void TestComponentService::init() {
    m_service = new ComponentService(this);
}

void TestComponentService::cleanup() {
    delete m_service;
}

void TestComponentService::testCreation() {
    QVERIFY(m_service != nullptr);
    qDebug() << "✓ ComponentService 创建成功";
}

void TestComponentService::testFetchComponentData() {
    // 在 CI 环境中跳过网络请求测试
    if (qEnvironmentVariableIsSet("CI")) {
        QSKIP("Skipping network test in CI environment to avoid flaky failures");
    }

    // 测试基本的元件数据获取接口
    QSignalSpy spy(m_service, &ComponentService::componentInfoReady);

    // 调用获取数据方法（注意：这需要网络连接，实际测试可能需要 mock）
    m_service->fetchComponentData("C12345", false);

    // 由于需要网络请求，这里只测试方法调用不会崩溃
    QVERIFY(m_service != nullptr);
    qDebug() << "✓ fetchComponentData 方法调用成功";
}

void TestComponentService::testSetOutputPath() {
    QString testPath = "C:/test/output";
    m_service->setOutputPath(testPath);

    QCOMPARE(m_service->getOutputPath(), testPath);
    qDebug() << "✓ setOutputPath/getOutputPath 工作正常";
}

void TestComponentService::testSignals() {
    // 测试信号连接
    QSignalSpy spyInfo(m_service, &ComponentService::componentInfoReady);
    QSignalSpy spyCad(m_service, &ComponentService::cadDataReady);
    QSignalSpy spyError(m_service, &ComponentService::fetchError);

    QVERIFY(spyInfo.isValid());
    QVERIFY(spyCad.isValid());
    QVERIFY(spyError.isValid());

    qDebug() << "✓ 所有信号定义正确";
}

void TestComponentService::testValidateComponentId() {
    // 测试有效的 LCSC 元件 ID
    QVERIFY(m_service->validateComponentId("C1234"));
    QVERIFY(m_service->validateComponentId("C12345"));
    QVERIFY(m_service->validateComponentId("C1234567890"));

    // 测试无效的元件 ID
    QVERIFY(!m_service->validateComponentId("1234"));     // 缺少 C 前缀
    QVERIFY(!m_service->validateComponentId("C123"));     // 数字不足4位
    QVERIFY(!m_service->validateComponentId("D1234"));    // 错误的前缀
    QVERIFY(!m_service->validateComponentId("C123A"));    // 包含字母
    QVERIFY(!m_service->validateComponentId(""));         // 空字符串
    QVERIFY(!m_service->validateComponentId("C123 45"));  // 包含空格

    qDebug() << "✓ validateComponentId 工作正常";
}

void TestComponentService::testExtractComponentIdFromText() {
    // 测试从文本中提取元件 ID
    QString text1 = "C1234 C5678 C9012";
    QStringList ids1 = m_service->extractComponentIdFromText(text1);
    QCOMPARE(ids1.size(), 3);
    QVERIFY(ids1.contains("C1234"));
    QVERIFY(ids1.contains("C5678"));
    QVERIFY(ids1.contains("C9012"));

    // 测试从混合文本中提取
    QString text2 = "元件列表: C12345, C67890, 另一个 C11111";
    QStringList ids2 = m_service->extractComponentIdFromText(text2);
    QCOMPARE(ids2.size(), 3);
    QVERIFY(ids2.contains("C12345"));
    QVERIFY(ids2.contains("C67890"));
    QVERIFY(ids2.contains("C11111"));

    // 测试重复元件 ID 去重
    QString text3 = "C1234 C1234 C5678";
    QStringList ids3 = m_service->extractComponentIdFromText(text3);
    QCOMPARE(ids3.size(), 2);
    QVERIFY(ids3.contains("C1234"));
    QVERIFY(ids3.contains("C5678"));

    // 测试空文本
    QString text4 = "";
    QStringList ids4 = m_service->extractComponentIdFromText(text4);
    QCOMPARE(ids4.size(), 0);

    // 测试无匹配文本
    QString text5 = "没有元件编号的文本";
    QStringList ids5 = m_service->extractComponentIdFromText(text5);
    QCOMPARE(ids5.size(), 0);

    qDebug() << "✓ extractComponentIdFromText 工作正常";
}

void TestComponentService::testParseBomFile() {
    // 创建测试 CSV 文件
    QString tempCsvPath = QDir::tempPath() + "/test_bom.csv";
    QFile tempCsv(tempCsvPath);

    QVERIFY(tempCsv.open(QIODevice::WriteOnly | QIODevice::Text));

    QTextStream out(&tempCsv);
    out.setEncoding(QStringConverter::Utf8);
    out << "元件编号,数量,描述\n";
    out << "C1234,10,电阻\n";
    out << "C5678,5,电容\n";
    out << "C9012,20,电感\n";
    out << "D1234,3,二极管\n";   // 无效的元件编号
    out << "C345,2,无效编号\n";  // 数字不足4位
    tempCsv.close();

    // 解析 CSV 文件
    QStringList componentIds = m_service->parseBomFile(tempCsvPath);

    // 验证结果
    QCOMPARE(componentIds.size(), 3);
    QVERIFY(componentIds.contains("C1234"));
    QVERIFY(componentIds.contains("C5678"));
    QVERIFY(componentIds.contains("C9012"));
    QVERIFY(!componentIds.contains("D1234"));
    QVERIFY(!componentIds.contains("C345"));

    qDebug() << "✓ parseBomFile 工作正常";

    // 清理临时文件
    QFile::remove(tempCsvPath);
}

QTEST_MAIN(TestComponentService)
#include "test_component_service.moc"