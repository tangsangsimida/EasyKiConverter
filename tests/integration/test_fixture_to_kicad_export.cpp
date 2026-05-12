#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "tests/common/TestPaths.hpp"

#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestFixtureToKiCadExport : public QObject {
    Q_OBJECT

private slots:

    void testFixtureDataExportsKiCadFiles() {
        QString error;
        const QJsonObject symbolFixture =
            TestPaths::readJsonObject(TestPaths::fixturePath(QStringLiteral("easyeda/symbol_basic.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        const QJsonObject footprintFixture =
            TestPaths::readJsonObject(TestPaths::fixturePath(QStringLiteral("easyeda/footprint_basic.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        EasyedaSymbolImporter symbolImporter;
        EasyedaFootprintImporter footprintImporter;
        const QSharedPointer<SymbolData> symbol = symbolImporter.importSymbolData(symbolFixture);
        const QSharedPointer<FootprintData> footprint = footprintImporter.importFootprintData(footprintFixture);
        QVERIFY(symbol);
        QVERIFY(footprint);

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString symbolLibraryPath = tempDir.filePath(QStringLiteral("FixtureLib.kicad_sym"));
        const QString footprintPath = tempDir.filePath(QStringLiteral("FIXTURE_FOOTPRINT.kicad_mod"));

        ExporterSymbol symbolExporter;
        QVERIFY(symbolExporter.exportSymbolLibrary(
            {*symbol}, QStringLiteral("FixtureLib"), symbolLibraryPath, false, false));

        ExporterFootprint footprintExporter;
        QVERIFY(footprintExporter.exportFootprint(*footprint, footprintPath));

        QVERIFY2(QFileInfo::exists(symbolLibraryPath), qPrintable(symbolLibraryPath));
        QVERIFY2(QFileInfo::exists(footprintPath), qPrintable(footprintPath));

        const QString symbolContent = TestPaths::readText(symbolLibraryPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(symbolContent.contains(QStringLiteral("(kicad_symbol_lib")));
        QVERIFY(symbolContent.contains(QStringLiteral("(symbol \"FIXTURE_SYMBOL\"")));
        QVERIFY(symbolContent.contains(QStringLiteral("\"Footprint\"")));
        QVERIFY(symbolContent.contains(QStringLiteral("\"FixtureLib:FIXTURE_FOOTPRINT\"")));
        QVERIFY(symbolContent.contains(QStringLiteral("(pin input line")));

        const QString footprintContent = TestPaths::readText(footprintPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(footprintContent.contains(QStringLiteral("(footprint easykiconverter:FIXTURE_FOOTPRINT")));
        QVERIFY(footprintContent.contains(QStringLiteral("(pad 1 smd rect")));
        QVERIFY(footprintContent.contains(QStringLiteral("(fp_line")));
    }
};

QTEST_GUILESS_MAIN(TestFixtureToKiCadExport)
#include "test_fixture_to_kicad_export.moc"
