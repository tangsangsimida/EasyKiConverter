#include "core/easyeda/EasyedaApi.h"
#include "tests/common/MockNetworkAdapter.hpp"

#include <QSignalSpy>
#include <QtTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestEasyedaApi : public QObject {
    Q_OBJECT

private slots:

    void init() {
        m_mockAdapter = new MockNetworkAdapter();
        m_api = new EasyedaApi(m_mockAdapter);
    }

    void cleanup() {
        delete m_api;
    }

    void testFetchComponentInfo_Success() {
        QString lcscId = "C12345";
        QString expectedUrl = "https://easyeda.com/api/products/C12345/components?version=6.5.51";

        QJsonObject mockResponse;
        mockResponse.insert("success", true);
        mockResponse.insert("lcscId", lcscId);

        m_mockAdapter->addJsonResponse(expectedUrl, mockResponse);

        QSignalSpy spy(m_api, &EasyedaApi::componentInfoFetched);
        m_api->fetchComponentInfo(lcscId);

        // 等待异步信号触发（由于是 Mock 请求且使用了 singleShot(0)，等待 1000ms 绰绰有余）
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m_mockAdapter->lastUrl(), expectedUrl);

        QJsonObject result = spy.at(0).at(0).toJsonObject();
        QCOMPARE(result.value("lcscId").toString(), lcscId);
    }

    void testFetchComponentInfo_InvalidId() {
        QSignalSpy spy(m_api, qOverload<const QString&>(&EasyedaApi::fetchError));
        m_api->fetchComponentInfo("INVALID");

        // 无效 ID 的报错通常是同步的，但稳妥起见我们也可以在报错分支走一遍
        QCOMPARE(spy.count(), 1);
        QString errorMsg = spy.at(0).at(0).toString();
        QVERIFY(errorMsg.contains("Invalid LCSC ID format"));
    }

private:
    MockNetworkAdapter* m_mockAdapter;
    EasyedaApi* m_api;
};

QTEST_GUILESS_MAIN(TestEasyedaApi)
#include "test_easyeda_api.moc"
