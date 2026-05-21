#include "core/network/INetworkClient.h"
#include "core/network/NetworkClient.h"
#include "core/utils/GzipUtils.h"
#include "tests/common/MockNetworkClient.hpp"

#include <QSignalSpy>
#include <QtTest>

#include <memory>
#include <zlib.h>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

/**
 * @brief NetworkClient 接口和 MockNetworkClient 行为测试
 *
 * 覆盖：重试策略、超时配置、429退避、并发限制、单例销毁/重建
 */
class TestNetworkClient : public QObject {
    Q_OBJECT

private slots:

    void init() {
        // 每个测试前销毁单例，确保隔离
        NetworkClient::destroyInstance();
        m_mockClient = std::make_unique<MockNetworkClient>();
    }

    void cleanup() {
        m_mockClient.reset();
        NetworkClient::destroyInstance();
    }

    // === MockNetworkClient 基本功能 ===

    void testMock_Get_Success() {
        const QUrl url(QStringLiteral("https://api.example.com/data"));
        QJsonObject mockData;
        mockData.insert(QStringLiteral("key"), QStringLiteral("value"));
        m_mockClient->addJsonResponse(url.toString(), mockData);

        NetworkResult result = m_mockClient->get(url);

        QVERIFY(result.success);
        QCOMPARE(result.statusCode, 200);
        QVERIFY(!result.data.isEmpty());

        QJsonDocument doc = QJsonDocument::fromJson(result.data);
        QCOMPARE(doc.object().value(QStringLiteral("key")).toString(), QStringLiteral("value"));
    }

    void testMock_Get_Error() {
        const QUrl url(QStringLiteral("https://api.example.com/fail"));
        m_mockClient->addErrorResponse(url.toString(), QStringLiteral("Connection refused"), 0);

        NetworkResult result = m_mockClient->get(url);

        QVERIFY(!result.success);
        QCOMPARE(result.error, QStringLiteral("Connection refused"));
        QCOMPARE(result.statusCode, 0);
    }

    void testMock_Get_WithResourceType() {
        const QUrl url(QStringLiteral("https://api.example.com/component"));
        QJsonObject mockData;
        mockData.insert(QStringLiteral("lcscId"), QStringLiteral("C12345"));
        m_mockClient->addJsonResponse(url.toString(), mockData);

        NetworkResult result = m_mockClient->get(url, ResourceType::ComponentInfo);

        QVERIFY(result.success);
        QCOMPARE(result.diagnostic.resourceType, ResourceType::ComponentInfo);
        QCOMPARE(result.diagnostic.profileName, QStringLiteral("ComponentInfo"));
    }

    void testMock_Post_DefaultBehavior() {
        const QUrl url(QStringLiteral("https://api.example.com/submit"));

        // MockNetworkClient 默认 POST 返回空结果（success=false）
        NetworkResult result = m_mockClient->post(url, QByteArray("{\"test\":true}"));

        // 没有配置 POST 响应时，返回默认值
        QVERIFY(!result.success);
        QCOMPARE(result.statusCode, 0);
    }

    void testMock_AsyncGet_SignalEmitted() {
        const QUrl url(QStringLiteral("https://api.example.com/async"));
        QJsonObject mockData;
        mockData.insert(QStringLiteral("async"), true);
        m_mockClient->addJsonResponse(url.toString(), mockData);

        AsyncNetworkRequest* request = m_mockClient->getAsync(url, ResourceType::ComponentInfo);
        QSignalSpy spy(request, &AsyncNetworkRequest::finished);

        // 等待延迟完成信号
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);

        NetworkResult result = spy.at(0).at(0).value<NetworkResult>();
        QVERIFY(result.success);

        request->deleteLater();
    }

    // === RetryPolicy 配置验证 ===

    void testRetryPolicy_DefaultValues() {
        RetryPolicy policy;

        QCOMPARE(policy.maxRetries, 3);
        QCOMPARE(policy.baseTimeoutMs, 30000);
        QCOMPARE(policy.jitterFactor, 0.2);
        QVERIFY(policy.retryableStatusCodes.contains(429));
        QVERIFY(policy.retryableStatusCodes.contains(500));
        QVERIFY(policy.retryableStatusCodes.contains(502));
        QVERIFY(policy.retryableStatusCodes.contains(503));
        QVERIFY(policy.retryableStatusCodes.contains(504));
    }

    void testRetryPolicy_FromProfile_ComponentInfo() {
        RequestProfile profile = RequestProfiles::componentInfo();
        RetryPolicy policy = RetryPolicy::fromProfile(profile);

        QCOMPARE(policy.maxRetries, profile.maxRetries);
        QCOMPARE(policy.baseTimeoutMs, qMax(profile.connectTimeoutMs, profile.readTimeoutMs));
        QCOMPARE(policy.jitterFactor, profile.jitterFactor);
        QCOMPARE(policy.delays.size(), profile.maxRetries);
    }

    void testRetryPolicy_FromProfile_WeakNetwork() {
        RequestProfile profile = RequestProfiles::componentInfo();
        RetryPolicy policy = RetryPolicy::fromProfile(profile, true);

        QCOMPARE(policy.maxRetries, profile.weakNetworkMaxRetries);
        QCOMPARE(policy.baseTimeoutMs,
                 qMax(profile.connectTimeoutMs, profile.readTimeoutMs) * profile.weakNetworkTimeoutMultiplier);
        QCOMPARE(policy.delays.size(), profile.weakNetworkMaxRetries);
    }

    void testRetryPolicy_BackoffDelays() {
        RequestProfile profile = RequestProfiles::componentInfo();
        RetryPolicy policy = RetryPolicy::fromProfile(profile);

        // 验证指数退避：1000, 2000, 4000
        QCOMPARE(policy.delays.at(0), 1000);
        QCOMPARE(policy.delays.at(1), 2000);
        QCOMPARE(policy.delays.at(2), 4000);
    }

    // === RequestProfile 配置验证 ===

    void testRequestProfiles_ComponentInfo() {
        RequestProfile profile = RequestProfiles::componentInfo();

        QCOMPARE(profile.type, ResourceType::ComponentInfo);
        QCOMPARE(profile.name, QStringLiteral("ComponentInfo"));
        QCOMPARE(profile.connectTimeoutMs, 15000);
        QCOMPARE(profile.readTimeoutMs, 30000);
        QCOMPARE(profile.maxRetries, 3);
        QCOMPARE(profile.maxConcurrent, 10);
        QVERIFY(profile.cacheable);
        QVERIFY(profile.allowCancellation);
    }

    void testRequestProfiles_CadData() {
        RequestProfile profile = RequestProfiles::cadData();

        QCOMPARE(profile.type, ResourceType::CadData);
        QCOMPARE(profile.connectTimeoutMs, 15000);
        QCOMPARE(profile.readTimeoutMs, 45000);
        QCOMPARE(profile.maxConcurrent, 10);
    }

    void testRequestProfiles_PreviewImage() {
        RequestProfile profile = RequestProfiles::previewImage();

        QCOMPARE(profile.type, ResourceType::PreviewImage);
        QCOMPARE(profile.connectTimeoutMs, 20000);
        QCOMPARE(profile.readTimeoutMs, 40000);
        QCOMPARE(profile.maxRetries, 2);
        QCOMPARE(profile.maxConcurrent, 5);
    }

    void testRequestProfiles_Model3D() {
        RequestProfile profileObj = RequestProfiles::model3DObj();
        RequestProfile profileStep = RequestProfiles::model3DStep();

        QCOMPARE(profileObj.type, ResourceType::Model3DObj);
        QCOMPARE(profileStep.type, ResourceType::Model3DStep);
        QCOMPARE(profileObj.maxConcurrent, 3);
        QCOMPARE(profileStep.maxConcurrent, 3);
    }

    void testRequestProfiles_UpdateCheck() {
        RequestProfile profile = RequestProfiles::updateCheck();

        QCOMPARE(profile.type, ResourceType::UpdateCheck);
        QCOMPARE(profile.connectTimeoutMs, 10000);
        QCOMPARE(profile.readTimeoutMs, 15000);
        QCOMPARE(profile.maxConcurrent, 2);
        QVERIFY(!profile.cacheable);
    }

    void testRequestProfiles_FromType() {
        QCOMPARE(RequestProfiles::fromType(ResourceType::ComponentInfo).type, ResourceType::ComponentInfo);
        QCOMPARE(RequestProfiles::fromType(ResourceType::CadData).type, ResourceType::CadData);
        QCOMPARE(RequestProfiles::fromType(ResourceType::PreviewImage).type, ResourceType::PreviewImage);
        QCOMPARE(RequestProfiles::fromType(ResourceType::Datasheet).type, ResourceType::Datasheet);
        QCOMPARE(RequestProfiles::fromType(ResourceType::Model3DObj).type, ResourceType::Model3DObj);
        QCOMPARE(RequestProfiles::fromType(ResourceType::Model3DStep).type, ResourceType::Model3DStep);
        QCOMPARE(RequestProfiles::fromType(ResourceType::WorkerRequest).type, ResourceType::WorkerRequest);
        QCOMPARE(RequestProfiles::fromType(ResourceType::UpdateCheck).type, ResourceType::UpdateCheck);
    }

    // === NetworkResult 结构验证 ===

    void testNetworkResult_DefaultValues() {
        NetworkResult result;

        QVERIFY(result.data.isEmpty());
        QCOMPARE(result.statusCode, 0);
        QVERIFY(result.error.isEmpty());
        QCOMPARE(result.retryCount, 0);
        QCOMPARE(result.elapsedMs, 0);
        QVERIFY(!result.success);
        QVERIFY(!result.wasCancelled);
    }

    void testNetworkResult_SuccessWithData() {
        NetworkResult result;
        result.success = true;
        result.statusCode = 200;
        result.data = QByteArray("{\"test\":true}");

        QVERIFY(result.success);
        QCOMPARE(result.statusCode, 200);
        QVERIFY(!result.data.isEmpty());
    }

    // === NetworkDiagnostic 验证 ===

    void testNetworkDiagnostic_ErrorTypeToString() {
        QCOMPARE(NetworkDiagnostic::errorTypeToString(NetworkErrorType::None), QStringLiteral("None"));
        QCOMPARE(NetworkDiagnostic::errorTypeToString(NetworkErrorType::Timeout), QStringLiteral("Timeout"));
        QCOMPARE(NetworkDiagnostic::errorTypeToString(NetworkErrorType::ConnectionRefused),
                 QStringLiteral("ConnectionRefused"));
        QCOMPARE(NetworkDiagnostic::errorTypeToString(NetworkErrorType::RateLimited), QStringLiteral("RateLimited"));
        QCOMPARE(NetworkDiagnostic::errorTypeToString(NetworkErrorType::ServerError), QStringLiteral("ServerError"));
        QCOMPARE(NetworkDiagnostic::errorTypeToString(NetworkErrorType::NotFound), QStringLiteral("NotFound"));
        QCOMPARE(NetworkDiagnostic::errorTypeToString(NetworkErrorType::Canceled), QStringLiteral("Canceled"));
    }

    // === 单例销毁/重建 ===

    void testSingleton_DestroyAndRecreate() {
        // 第一次获取实例
        NetworkClient& instance1 = NetworkClient::instance();
        Q_UNUSED(instance1);

        // 销毁实例
        NetworkClient::destroyInstance();

        // 重新获取实例
        NetworkClient& instance2 = NetworkClient::instance();
        Q_UNUSED(instance2);

        // 验证可以正常销毁
        NetworkClient::destroyInstance();
    }

    // === 并发限制配置验证 ===

    void testConcurrencyLimits_AllTypes() {
        // ComponentInfo: 10
        QCOMPARE(RequestProfiles::componentInfo().maxConcurrent, 10);
        // CadData: 10
        QCOMPARE(RequestProfiles::cadData().maxConcurrent, 10);
        // PreviewImage: 5
        QCOMPARE(RequestProfiles::previewImage().maxConcurrent, 5);
        // ProductSearch: 5
        QCOMPARE(RequestProfiles::productSearch().maxConcurrent, 5);
        // Datasheet: 3
        QCOMPARE(RequestProfiles::datasheet().maxConcurrent, 3);
        // Model3DObj: 3
        QCOMPARE(RequestProfiles::model3DObj().maxConcurrent, 3);
        // Model3DStep: 3
        QCOMPARE(RequestProfiles::model3DStep().maxConcurrent, 3);
        // UpdateCheck: 2
        QCOMPARE(RequestProfiles::updateCheck().maxConcurrent, 2);
        // WorkerRequest: 5
        QCOMPARE(RequestProfiles::workerRequest().maxConcurrent, 5);
    }

    // === 弱网模式配置验证 ===

    void testWeakNetwork_Configuration() {
        RequestProfile profile = RequestProfiles::componentInfo();

        // 弱网模式下更多重试
        QCOMPARE(profile.weakNetworkMaxRetries, 5);
        // 弱网模式下双倍超时
        QCOMPARE(profile.weakNetworkTimeoutMultiplier, 2);
    }

    void testWeakNetwork_AllProfilesHaveWeakNetworkSettings() {
        auto profiles = {RequestProfiles::componentInfo(),
                         RequestProfiles::cadData(),
                         RequestProfiles::previewImage(),
                         RequestProfiles::productSearch(),
                         RequestProfiles::datasheet(),
                         RequestProfiles::model3DObj(),
                         RequestProfiles::model3DStep(),
                         RequestProfiles::workerRequest(),
                         RequestProfiles::updateCheck()};

        for (const auto& profile : profiles) {
            QVERIFY2(profile.weakNetworkMaxRetries > 0,
                     qPrintable(QStringLiteral("%1 should have weakNetworkMaxRetries > 0").arg(profile.name)));
            QVERIFY2(profile.weakNetworkTimeoutMultiplier >= 2,
                     qPrintable(QStringLiteral("%1 should have weakNetworkTimeoutMultiplier >= 2").arg(profile.name)));
        }
    }

    // === GzipUtils 测试 ===

    void testGzipUtils_IsGzipped_ValidMagicBytes() {
        // Gzip magic number: 0x1f 0x8b
        QByteArray data;
        data.append(static_cast<char>(0x1f));
        data.append(static_cast<char>(0x8b));
        data.append("rest of data");
        QVERIFY(GzipUtils::isGzipped(data));
    }

    void testGzipUtils_IsGzipped_NotGzip() {
        QByteArray data("{\"json\": true}");
        QVERIFY(!GzipUtils::isGzipped(data));
    }

    void testGzipUtils_IsGzipped_TooShort() {
        QVERIFY(!GzipUtils::isGzipped(QByteArray()));
        QVERIFY(!GzipUtils::isGzipped(QByteArray("x")));
    }

    void testGzipUtils_Decompress_TooSmall() {
        // 小于 18 字节的数据直接返回失败
        QByteArray smallData("short");
        GzipUtils::DecompressResult result = GzipUtils::decompress(smallData);
        QVERIFY(!result.success);
    }

    void testGzipUtils_Decompress_NotGzip_ReturnsAsIs() {
        // 非 gzip 数据 >= 18 字节，返回原始数据 + success=true
        QByteArray original = QByteArray("this is plain text data longer than 18 bytes");
        GzipUtils::DecompressResult result = GzipUtils::decompress(original);
        QVERIFY(result.success);
        QCOMPARE(result.data, original);
    }

    void testGzipUtils_Decompress_ValidGzip() {
        // 用 zlib 压缩一段数据，然后验证解压
        const QByteArray original("Hello, EasyKiConverter gzip test!");
        QByteArray compressed = compressGzip(original);

        QVERIFY(GzipUtils::isGzipped(compressed));

        GzipUtils::DecompressResult result = GzipUtils::decompress(compressed);
        QVERIFY(result.success);
        QCOMPARE(result.data, original);
    }

    void testGzipUtils_Decompress_EmptyGzip() {
        // 最小合法 gzip 流（18 字节：header + empty deflate block）
        // 构造一个空 gzip 流
        QByteArray emptyGzip = compressGzip(QByteArray());
        if (emptyGzip.size() >= 18) {
            GzipUtils::DecompressResult result = GzipUtils::decompress(emptyGzip);
            QVERIFY(result.success);
            QVERIFY(result.data.isEmpty());
        }
    }

    // === AsyncNetworkRequest 测试 ===

    void testAsyncRequest_CreateFinished_IsImmediatelyDone() {
        NetworkResult expected;
        expected.success = true;
        expected.statusCode = 200;
        expected.data = QByteArray("test data");

        AsyncNetworkRequest* request = AsyncNetworkRequest::createFinished(expected);

        // isFinished() 和 result() 应立即可用（同步设置状态）
        QVERIFY(request->isFinished());
        QVERIFY(!request->isCancelled());
        QCOMPARE(request->result().data, QByteArray("test data"));
        QCOMPARE(request->result().statusCode, 200);

        // finished 信号通过 QTimer::singleShot(0) 延迟发射
        // 先 connect 再等待事件循环处理
        QSignalSpy spy(request, &AsyncNetworkRequest::finished);
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);

        request->deleteLater();
    }

    void testAsyncRequest_CompleteWithResultDelayed_EmitsSignal() {
        const QUrl url(QStringLiteral("https://api.example.com/delayed"));
        m_mockClient->addResponse(url.toString(), QByteArray("delayed data"), 200);

        AsyncNetworkRequest* request = m_mockClient->getAsync(url, ResourceType::Unknown);
        QSignalSpy spy(request, &AsyncNetworkRequest::finished);

        QVERIFY(spy.wait(1000));
        QVERIFY(request->isFinished());

        NetworkResult result = spy.at(0).at(0).value<NetworkResult>();
        QVERIFY(result.success);
        QCOMPARE(result.data, QByteArray("delayed data"));

        request->deleteLater();
    }

    void testAsyncRequest_Cancel_SetsCancelledState() {
        // cancel() 在请求未完成时应设置取消状态
        // 使用 createFinished 的 cancelledResult 来验证取消语义
        NetworkResult cancelled;
        cancelled.success = false;
        cancelled.wasCancelled = true;
        cancelled.error = QStringLiteral("Cancelled");

        // 先 connect 再创建，确保能收到延迟信号
        AsyncNetworkRequest* request = AsyncNetworkRequest::createFinished(cancelled);
        QSignalSpy spy(request, &AsyncNetworkRequest::finished);

        // isFinished() 和 result() 应立即可用（同步设置状态）
        QVERIFY(request->isFinished());
        QVERIFY(request->isCancelled());
        QVERIFY(request->result().wasCancelled);
        QVERIFY(!request->result().success);

        // finished 信号通过 QTimer::singleShot(0) 延迟发射
        QVERIFY(spy.wait(1000));
        QCOMPARE(spy.count(), 1);

        request->deleteLater();
    }

    void testSyncGet_RejectedByWhitelist_ReturnsQuickly() {
        // 同步 get() 被白名单拒绝时应立即返回失败，不应死锁
        QElapsedTimer timer;
        timer.start();

        NetworkResult result =
            NetworkClient::instance().get(QUrl("https://evil.example/x"), ResourceType::PreviewImage);

        qint64 elapsed = timer.elapsed();

        QVERIFY(!result.success);
        QVERIFY(result.error.contains("not allowed") || result.error.contains("not in any known allowlist"));
        QVERIFY(elapsed < 2000);  // 应在 2 秒内返回，而非超时等待
    }

    void testAsyncGet_RejectedByWhitelist_EmitsFinished() {
        // 异步 getAsync() 被白名单拒绝时应发出 finished 信号
        AsyncNetworkRequest* request =
            NetworkClient::instance().getAsync(QUrl("https://evil.example/x"), ResourceType::PreviewImage);

        QVERIFY(request != nullptr);

        // isFinished() 应立即可用（同步设置状态）
        QVERIFY(request->isFinished());
        QVERIFY(!request->result().success);
        QVERIFY(request->result().error.contains("not allowed") ||
                request->result().error.contains("not in any known allowlist"));

        // finished 信号通过 QTimer::singleShot(0) 延迟发射
        QSignalSpy spy(request, &AsyncNetworkRequest::finished);
        QVERIFY(spy.wait(3000));
        QCOMPARE(spy.count(), 1);

        request->deleteLater();
    }

    void testAsyncRequest_Cancel_ViaDestructor() {
        // 验证析构时 cancel 逻辑不会崩溃
        // 通过 MockNetworkClient 创建请求，不保存引用
        const QUrl url(QStringLiteral("https://api.example.com/destructor"));
        m_mockClient->addResponse(url.toString(), QByteArray("data"), 200);

        AsyncNetworkRequest* request = m_mockClient->getAsync(url, ResourceType::Unknown);
        QSignalSpy spy(request, &AsyncNetworkRequest::finished);

        // 等待完成
        QVERIFY(spy.wait(1000));
        QVERIFY(request->isFinished());

        // 删除已完成的请求不应崩溃
        delete request;
    }

private:
    std::unique_ptr<MockNetworkClient> m_mockClient;

    static QByteArray compressGzip(const QByteArray& data) {
        QByteArray compressed;
        z_stream stream;
        memset(&stream, 0, sizeof(stream));

        if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
            return compressed;
        }

        stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(data.constData()));
        stream.avail_in = data.size();

        char buffer[4096];
        int ret;
        do {
            stream.avail_out = sizeof(buffer);
            stream.next_out = reinterpret_cast<Bytef*>(buffer);
            ret = deflate(&stream, Z_FINISH);
            if (ret == Z_OK || ret == Z_STREAM_END) {
                compressed.append(buffer, sizeof(buffer) - stream.avail_out);
            }
        } while (ret == Z_OK);

        deflateEnd(&stream);
        return compressed;
    }
};

QTEST_GUILESS_MAIN(TestNetworkClient)
#include "test_network_client.moc"
