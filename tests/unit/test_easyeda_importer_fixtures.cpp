#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "tests/common/TestPaths.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestEasyedaImporterFixtures : public QObject {
    Q_OBJECT

private slots:

    void testSymbolFixtureImportsMetadataAndGeometry() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/symbol_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaSymbolImporter importer;
        const QSharedPointer<SymbolData> symbol = importer.importSymbolData(fixture);

        QVERIFY(symbol);
        QCOMPARE(symbol->info().name, QStringLiteral("FIXTURE_SYMBOL"));
        QCOMPARE(symbol->info().prefix, QStringLiteral("U"));
        QCOMPARE(symbol->info().package, QStringLiteral("FIXTURE_FOOTPRINT"));
        QCOMPARE(symbol->info().manufacturer, QStringLiteral("Fixture Inc"));
        QCOMPARE(symbol->info().lcscId, QStringLiteral("C12345"));
        QCOMPARE(symbol->info().datasheet, QStringLiteral("https://example.test/datasheet.pdf"));
        QCOMPARE(symbol->bbox().width, 120.0);
        QCOMPARE(symbol->bbox().height, 80.0);

        QCOMPARE(symbol->rectangles().size(), 1);
        QCOMPARE(symbol->rectangles().first().width, 60.0);
        QCOMPARE(symbol->rectangles().first().height, 40.0);

        QCOMPARE(symbol->pins().size(), 1);
        const SymbolPin pin = symbol->pins().first();
        QCOMPARE(pin.settings.spicePinNumber, QStringLiteral("1"));
        QCOMPARE(pin.settings.posX, 10.0);
        QCOMPARE(pin.settings.posY, 20.0);
        QCOMPARE(pin.settings.rotation, 0);
        QCOMPARE(pin.settings.type, PinType::Input);
        QCOMPARE(pin.name.text, QStringLiteral("VCC"));
        QCOMPARE(pin.pinPath.path, QStringLiteral("M 10 20 h 20"));
    }

    void testFootprintFixtureImportsMetadataAndGeometry() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/footprint_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaFootprintImporter importer;
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(fixture);

        QVERIFY(footprint);
        QCOMPARE(footprint->info().name, QStringLiteral("FIXTURE_FOOTPRINT"));
        QCOMPARE(footprint->info().type, QStringLiteral("smd"));
        QCOMPARE(footprint->info().model3DName, QStringLiteral("FIXTURE_MODEL"));
        QCOMPARE(footprint->bbox().width, 100.0);
        QCOMPARE(footprint->bbox().height, 50.0);

        QCOMPARE(footprint->pads().size(), 1);
        const FootprintPad pad = footprint->pads().first();
        QCOMPARE(pad.shape, QStringLiteral("rect"));
        QCOMPARE(pad.centerX, 10.0);
        QCOMPARE(pad.centerY, 20.0);
        QCOMPARE(pad.width, 30.0);
        QCOMPARE(pad.height, 40.0);
        QCOMPARE(pad.layerId, 1);
        QCOMPARE(pad.number, QStringLiteral("1"));
        QCOMPARE(pad.rotation, 90.0);

        QCOMPARE(footprint->tracks().size(), 1);
        QCOMPARE(footprint->tracks().first().points, QStringLiteral("0 0 100 50"));
        QCOMPARE(footprint->rectangles().size(), 1);
        QCOMPARE(footprint->rectangles().first().strokeWidth, 1.0);
        QCOMPARE(footprint->layers().size(), 2);
        QCOMPARE(footprint->objectVisibilities().size(), 2);
    }

    void testRealCadFixtureWith3DImportsSymbolAndFootprint() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/cad_basic.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaSymbolImporter symbolImporter;
        const QSharedPointer<SymbolData> symbol = symbolImporter.importSymbolData(fixture);

        QVERIFY(symbol);
        QCOMPARE(symbol->info().name, QStringLiteral("0603WAF5101T5E"));
        QCOMPARE(symbol->info().package, QStringLiteral("R0603"));
        QCOMPARE(symbol->info().manufacturer, QStringLiteral("UNI-ROYAL(厚声)"));
        QCOMPARE(symbol->info().lcscId, QStringLiteral("C23186"));
        QVERIFY(symbol->pins().size() >= 2);

        EasyedaFootprintImporter footprintImporter;
        const QSharedPointer<FootprintData> footprint = footprintImporter.importFootprintData(fixture);

        QVERIFY(footprint);
        QCOMPARE(footprint->info().name, QStringLiteral("R0603"));
        QCOMPARE(footprint->info().model3DName, QStringLiteral("R0603"));
        QCOMPARE(footprint->info().uuid3d, QStringLiteral("e06b23482b894f1ebafac2e1696a59f5"));
        QCOMPARE(footprint->model3D().uuid(), QStringLiteral("6bd5cd867e9542ebae21caaf5d2d4c4d"));
        QCOMPARE(footprint->model3D().name(), QStringLiteral("R0603"));
        QVERIFY(footprint->pads().size() >= 2);
        QVERIFY(footprint->outlines().size() >= 1);
    }

    void testRealCadFixtureWithoutUuid3DStillImportsOutlineModel() {
        QString error;
        const QJsonObject fixture = loadFixtureObject(QStringLiteral("easyeda/cad_no_3d.json"), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const QJsonObject packageHead =
            fixture.value(QStringLiteral("packageDetail"))
                .toObject()
                .value(QStringLiteral("dataStr"))
                .toObject()
                .value(QStringLiteral("head"))
                .toObject();
        QVERIFY(!packageHead.contains(QStringLiteral("uuid_3d")));

        EasyedaFootprintImporter importer;
        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(fixture);

        QVERIFY(footprint);
        QCOMPARE(footprint->info().name, QStringLiteral("C0603"));
        QVERIFY(footprint->info().uuid3d.isEmpty());
        QCOMPARE(footprint->model3D().uuid(), QStringLiteral("ac9b32e974bc448eab36b1293f859dcb"));
        QCOMPARE(footprint->model3D().name(), QStringLiteral("C0603_L1.6-W0.8-H0.8"));
        QVERIFY(footprint->pads().size() >= 2);
        QVERIFY(footprint->outlines().size() >= 1);
    }

    void testRealModel3DFixtureIsObjData() {
        QString error;
        const QByteArray modelData = TestPaths::readBytes(
            TestPaths::fixturePath(QStringLiteral("easyeda/model3d_r0603.obj")), &error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(modelData.size() > 1024);
        QVERIFY(modelData.contains("v "));
        QVERIFY(modelData.contains("newmtl"));
    }

private:
    QJsonObject loadFixtureObject(const QString& relativePath, QString* errorMessage) const {
        const QByteArray bytes = TestPaths::readBytes(TestPaths::fixturePath(relativePath), errorMessage);
        if (errorMessage != nullptr && !errorMessage->isEmpty()) {
            return {};
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            if (errorMessage != nullptr) {
                *errorMessage = parseError.errorString();
            }
            return {};
        }
        if (!document.isObject()) {
            if (errorMessage != nullptr) {
                *errorMessage = QStringLiteral("Fixture root must be a JSON object");
            }
            return {};
        }
        return document.object();
    }
};

QTEST_GUILESS_MAIN(TestEasyedaImporterFixtures)
#include "test_easyeda_importer_fixtures.moc"
