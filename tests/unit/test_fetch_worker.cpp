#include "tests/common/MockNetworkClient.hpp"
#include "tests/common/TestPaths.hpp"
#include "workers/FetchWorker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestFetchWorker : public QObject {
    Q_OBJECT

private slots:

    // 验证注入的网络客户端能够获取并保存组件信息。
    void runFetchesComponentInfoWithInjectedNetworkClient() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C23186");
        const QString url = componentInfoUrl(componentId);

        const QJsonObject response = successfulComponentResponse();
        const QByteArray expectedRaw = QJsonDocument(response).toJson(QJsonDocument::Compact);
        mock.addJsonResponse(url, response);

        FetchWorker worker(componentId, false, false, QString(), &mock);
        QSharedPointer<ComponentExportStatus> status = runWorker(worker);

        QVERIFY(status);
        QVERIFY(status->fetchSuccess);
        QCOMPARE(status->fetchMessage, QStringLiteral("Fetch completed successfully"));
        QCOMPARE(status->componentId, componentId);
        QCOMPARE(status->componentInfoRaw, expectedRaw);
        QCOMPARE(status->cadDataRaw, expectedRaw);
        QCOMPARE(status->cinfoJsonRaw, expectedRaw);
        QCOMPARE(status->cadJsonRaw, expectedRaw);
        QCOMPARE(status->networkDiagnostics.size(), 1);
        QCOMPARE(status->networkDiagnostics.first().url, url);
        QCOMPARE(status->networkDiagnostics.first().statusCode, 200);
    }

    // 验证 API 错误响应能够进入获取失败状态。
    void runReportsApiErrorResponse() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C404");
        const QString url = componentInfoUrl(componentId);

        QJsonObject response;
        response.insert(QStringLiteral("success"), false);
        response.insert(QStringLiteral("message"), QStringLiteral("component missing"));
        mock.addJsonResponse(url, response, 404);

        FetchWorker worker(componentId, false, false, QString(), &mock);
        QSharedPointer<ComponentExportStatus> status = runWorker(worker);

        QVERIFY(status);
        QVERIFY(!status->fetchSuccess);
        QCOMPARE(status->fetchMessage, QStringLiteral("API returned error: component missing"));
        QCOMPARE(status->networkDiagnostics.size(), 1);
        QCOMPARE(status->networkDiagnostics.first().statusCode, 404);
    }

    // 验证 403 访问拒绝会传递到导出状态且不会被标记为限流。
    void runPropagatesForbiddenDiagnostics() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C403");
        const QString url = componentInfoUrl(componentId);
        mock.addErrorResponse(url, QStringLiteral("Access denied"), 403);

        FetchWorker worker(componentId, false, false, QString(), &mock);
        QSharedPointer<ComponentExportStatus> status = runWorker(worker);

        QVERIFY(status);
        QVERIFY(!status->fetchSuccess);
        QCOMPARE(status->networkDiagnostics.size(), 1);
        const auto& diagnostic = status->networkDiagnostics.first();
        QCOMPARE(diagnostic.statusCode, 403);
        QCOMPARE(diagnostic.errorString, QStringLiteral("Access denied"));
        QVERIFY(!diagnostic.wasRateLimited);
        QVERIFY(!diagnostic.hasRateLimitHint);
    }

    // 验证仅获取三维模型时会记录两个模型请求。
    void runFetchesExisting3DModelOnly() {
        MockNetworkClient mock;
        const QString uuid = QStringLiteral("uuid-123");
        const QString objUrl = QStringLiteral("https://modules.easyeda.com/3dmodel/%1").arg(uuid);
        const QString stepUrl = QStringLiteral("https://modules.easyeda.com/qAxj6KHrDKw4blvCG8QJPs7Y/%1").arg(uuid);

        mock.addResponse(objUrl, QByteArrayLiteral("obj-data"));
        mock.addResponse(stepUrl, QByteArrayLiteral("step-data"));

        FetchWorker worker(QStringLiteral("C3D"), true, true, uuid, &mock);
        QSharedPointer<ComponentExportStatus> status = runWorker(worker);

        QVERIFY(status);
        QVERIFY(status->fetchSuccess);
        QVERIFY(status->need3DModel);
        QVERIFY(status->fetch3DOnly);
        QCOMPARE(status->model3DObjRaw, QByteArrayLiteral("obj-data"));
        QCOMPARE(status->model3DStepRaw, QByteArrayLiteral("step-data"));
        QCOMPARE(status->networkDiagnostics.size(), 2);
        QCOMPARE(status->networkDiagnostics.at(0).url, objUrl);
        QCOMPARE(status->networkDiagnostics.at(1).url, stepUrl);
    }

    // 验证任务开始前取消会发布取消状态。
    void abortBeforeRunEmitsCancelledStatus() {
        MockNetworkClient mock;
        FetchWorker worker(QStringLiteral("C_CANCEL"), false, false, QString(), &mock);

        worker.abort();
        QSharedPointer<ComponentExportStatus> status = runWorker(worker);

        QVERIFY(status);
        QVERIFY(!status->fetchSuccess);
        QVERIFY(status->isCancelled);
        QCOMPARE(status->fetchMessage, QStringLiteral("Export cancelled"));
        QVERIFY(status->networkDiagnostics.isEmpty());
    }

private:
    // 同步执行 Worker 并捕获完成信号中的状态对象。
    QSharedPointer<ComponentExportStatus> runWorker(FetchWorker& worker) const {
        QSharedPointer<ComponentExportStatus> captured;
        QObject::connect(
            &worker,
            &FetchWorker::fetchCompleted,
            &worker,
            [&captured](const QSharedPointer<ComponentExportStatus>& status) { captured = status; },
            Qt::DirectConnection);
        worker.run();
        return captured;
    }

    // 构造组件信息请求地址。
    QString componentInfoUrl(const QString& componentId) const {
        return QStringLiteral("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(componentId);
    }

    // 从项目夹具加载可用于测试的成功响应。
    QJsonObject successfulComponentResponse() const {
        QString error;
        const QByteArray fixture =
            TestPaths::readBytes(TestPaths::fixturePath(QStringLiteral("easyeda/cad_basic.json")), &error);
        if (!error.isEmpty()) {
            qFatal("%s", qPrintable(error));
        }

        QJsonParseError parseError;
        const QJsonDocument fixtureDoc = QJsonDocument::fromJson(fixture, &parseError);
        if (parseError.error != QJsonParseError::NoError || !fixtureDoc.isObject()) {
            qFatal("Failed to parse cad_basic.json fixture");
        }

        QJsonObject response;
        response.insert(QStringLiteral("success"), true);
        response.insert(QStringLiteral("result"), fixtureDoc.object());
        return response;
    }
};

QTEST_GUILESS_MAIN(TestFetchWorker)
#include "test_fetch_worker.moc"
