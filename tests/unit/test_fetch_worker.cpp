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
    QSharedPointer<ComponentExportStatus> runWorker(FetchWorker& worker) const {
        QSharedPointer<ComponentExportStatus> captured;
        QObject::connect(&worker,
                         &FetchWorker::fetchCompleted,
                         &worker,
                         [&captured](const QSharedPointer<ComponentExportStatus>& status) { captured = status; },
                         Qt::DirectConnection);
        worker.run();
        return captured;
    }

    QString componentInfoUrl(const QString& componentId) const {
        return QStringLiteral("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(componentId);
    }

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
