#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "models/FootprintData.h"
#include "models/SymbolData.h"
#include "tests/common/TestPaths.hpp"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestKiCadExporterGolden : public QObject {
    Q_OBJECT

private slots:

    void testSymbolLibraryMatchesGolden() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputPath = tempDir.filePath(QStringLiteral("GoldenLib.kicad_sym"));

        ExporterSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({makeSymbol()}, QStringLiteral("GoldenLib"), outputPath, false, false));

        QString error;
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(
            TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_symbol_library.kicad_sym"), &error),
            qPrintable(error));
    }

    void testFootprintMatchesGolden() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputPath = tempDir.filePath(QStringLiteral("GOLDEN_FOOTPRINT.kicad_mod"));

        ExporterFootprint exporter;
        QVERIFY(exporter.exportFootprint(makeFootprint(), outputPath));

        QString error;
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_footprint.kicad_mod"), &error),
                 qPrintable(error));
    }

private:
    SymbolData makeSymbol() const {
        SymbolData symbol;

        SymbolInfo info;
        info.name = QStringLiteral("GOLDEN_SYMBOL");
        info.prefix = QStringLiteral("U");
        info.package = QStringLiteral("GOLDEN_FOOTPRINT");
        info.lcscId = QStringLiteral("C12345");
        symbol.setInfo(info);
        symbol.setBbox({0, 0, 0, 0});

        return symbol;
    }

    FootprintData makeFootprint() const {
        FootprintData footprint;

        FootprintInfo info;
        info.name = QStringLiteral("GOLDEN_FOOTPRINT");
        footprint.setInfo(info);
        footprint.setBbox({0, 0, 0, 0});

        FootprintPad pad;
        pad.shape = QStringLiteral("rect");
        pad.centerX = 0;
        pad.centerY = 0;
        pad.width = 20;
        pad.height = 10;
        pad.layerId = 1;
        pad.number = QStringLiteral("1");
        pad.rotation = 0;
        footprint.addPad(pad);

        return footprint;
    }
};

QTEST_GUILESS_MAIN(TestKiCadExporterGolden)
#include "test_kicad_exporter_golden.moc"
