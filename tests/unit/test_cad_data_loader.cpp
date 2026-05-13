#include "../common/MockNetworkClient.hpp"
#include "../common/TestPaths.hpp"
#include "services/CadDataLoader.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestCadDataLoader : public QObject {
    Q_OBJECT

private slots:

    void fetchAndParseCadDataSuccess() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C23186");
        mock.addJsonResponse(componentUrl(componentId),
                             responseWithResult(readCadFixture(QStringLiteral("cad_basic"))));

        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(result.success);
        QVERIFY(result.parsed.success);
        QCOMPARE(result.componentId, componentId);
        QCOMPARE(result.parsed.componentId, componentId);
        QCOMPARE(result.parsed.componentData.lcscId(), componentId);
        QCOMPARE(result.parsed.componentData.name(), QStringLiteral("0603WAF5101T5E"));
        QVERIFY(result.parsed.componentData.symbolData());
        QCOMPARE(result.parsed.componentData.symbolData()->info().name, QStringLiteral("0603WAF5101T5E"));
        QVERIFY(result.parsed.componentData.footprintData());
        QCOMPARE(result.parsed.componentData.footprintData()->info().name, QStringLiteral("R0603"));
        QVERIFY(result.parsed.componentData.model3DData());
        QVERIFY(!result.parsed.componentData.model3DData()->uuid().isEmpty());
        QVERIFY(result.errorMessage.isEmpty());
    }

    void fetchAndParseCadDataReportsSuccessFalse() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C_BAD");

        QJsonObject response;
        response.insert(QStringLiteral("success"), false);
        response.insert(QStringLiteral("code"), 500);
        response.insert(QStringLiteral("message"), QStringLiteral("temporary backend failure"));
        mock.addJsonResponse(componentUrl(componentId), response, 500);

        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.componentId, componentId);
        QCOMPARE(result.errorMessage, QStringLiteral("temporary backend failure"));
    }

    void fetchAndParseCadDataReports404AsMissingComponent() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C404");

        QJsonObject response;
        response.insert(QStringLiteral("success"), false);
        response.insert(QStringLiteral("code"), 404);
        response.insert(QStringLiteral("message"), QStringLiteral("not found"));
        mock.addJsonResponse(componentUrl(componentId), response, 404);

        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QStringLiteral("元器件不存在（404）"));
    }

    void fetchAndParseCadDataReportsMissingResult() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C_MISSING_RESULT");

        QJsonObject response;
        response.insert(QStringLiteral("success"), true);
        mock.addJsonResponse(componentUrl(componentId), response);

        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QStringLiteral("API response missing result field"));
    }

    void fetchAndParseCadDataReportsEmptyResult() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C_EMPTY_RESULT");

        QJsonObject response;
        response.insert(QStringLiteral("success"), true);
        response.insert(QStringLiteral("result"), QJsonObject{});
        mock.addJsonResponse(componentUrl(componentId), response);

        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QStringLiteral("Empty CAD data"));
    }

    void fetchAndParseCadDataReportsInvalidJson() {
        MockNetworkClient mock;
        const QString componentId = QStringLiteral("C_INVALID_JSON");
        mock.addResponse(componentUrl(componentId), QByteArrayLiteral("{"));

        const CadFetchTaskResult result = CadDataLoader::fetchAndParseCadData(componentId, &mock);

        QVERIFY(!result.success);
        QCOMPARE(result.errorMessage, QStringLiteral("Failed to parse CAD response JSON"));
    }

private:
    QString componentUrl(const QString& componentId) const {
        return QStringLiteral("https://easyeda.com/api/products/%1/components?version=6.5.51").arg(componentId);
    }

    QJsonObject readCadFixture(const QString& name) const {
        QString error;
        const QByteArray data =
            TestPaths::readBytes(TestPaths::fixturePath(QStringLiteral("easyeda/%1.json").arg(name)), &error);
        if (!error.isEmpty()) {
            qFatal("%s", qPrintable(error));
        }

        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            qFatal("Failed to parse CAD fixture");
        }
        return doc.object();
    }

    QJsonObject responseWithResult(const QJsonObject& result) const {
        QJsonObject response;
        response.insert(QStringLiteral("success"), true);
        response.insert(QStringLiteral("result"), result);
        return response;
    }
};

QTEST_GUILESS_MAIN(TestCadDataLoader)
#include "test_cad_data_loader.moc"
