#include "../common/MockNetworkClient.hpp"
#include "core/easyeda/EasyedaApi.h"
#include "services/CadDataLoader.h"

#include <QSignalSpy>
#include <QtTest>

#include <memory>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

// 场景1: 不存在元器件返回 {"success":false,"code":404} 时进入 failed
//
// 测试策略：由于 CadDataLoader::fetchAndParseCadData() 是 static 方法，直接调用
// NetworkClient::instance()，无法在不修改生产代码的情况下注入 mock。
//
// 我们通过 EasyedaApi 间接验证部分行为：
// EasyedaApi::handleCadDataResponse 会检查 result 是否存在（这是 CadDataLoader
// 业务检查的一部分）。完整的 success=false / code=404 检查在 CadDataLoader 中，
// 需要将 fetchAndParseCadData 改为接受 INetworkClient* 参数才能测试。
class TestCadDataLoader404Error : public QObject {
    Q_OBJECT

private slots:

    void init() {
        m_mockClient = std::make_unique<MockNetworkClient>();
        m_api = std::make_unique<EasyedaApi>(m_mockClient.get());
    }

    // 验证：result 字段缺失时 EasyedaApi 触发 fetchError（CadDataLoader 业务检查的一部分）
    void testMissingResult_TriggersFetchError() {
        const QString lcscId = QStringLiteral("C2041");
        const QString expectedUrl = QStringLiteral("https://easyeda.com/api/products/C2041/components?version=6.5.51");

        // API 返回 success=true 但缺少 result 字段
        QJsonObject mockResponse;
        mockResponse.insert(QStringLiteral("success"), true);
        // 故意没有 result 字段
        m_mockClient->addJsonResponse(expectedUrl, mockResponse);

        QSignalSpy spy(m_api.get(), qOverload<const QString&, const QString&>(&EasyedaApi::fetchError));
        m_api->fetchCadData(lcscId);
        QVERIFY2(spy.wait(1000), "fetchError 信号应该在 1s 内触发");
        QCOMPARE(spy.count(), 1);

        const QString errorMsg = spy.at(0).at(1).toString();
        QVERIFY2(errorMsg.contains(QStringLiteral("No result")),
                 qPrintable(QStringLiteral("错误信息应包含 'No result'，实际: ") + errorMsg));
    }

private:
    std::unique_ptr<MockNetworkClient> m_mockClient;
    std::unique_ptr<EasyedaApi> m_api;
};

QTEST_GUILESS_MAIN(TestCadDataLoader404Error)
#include "test_cad_data_loader.moc"