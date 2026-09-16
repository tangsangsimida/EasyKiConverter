#include "core/ir/FootprintIR.h"
#include "core/ir/SymbolIR.h"
#include "core/xpedition/ExporterXpeditionFootprint.h"
#include "core/xpedition/ExporterXpeditionSymbol.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestXpeditionExporter : public QObject {
    Q_OBJECT

private slots:

    void symbolLibraryContainsAsciiEntry() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::SymbolComponentIR symbol;
        symbol.name = QStringLiteral("TEST_SYMBOL");
        symbol.designatorPrefix = QStringLiteral("U");
        IR::SymbolPinIR pin;
        pin.name = QStringLiteral("IN");
        pin.designator = QStringLiteral("1");
        pin.position = QPointF(2.54, 0);
        pin.namePosition = QPointF(1.27, 0);
        pin.numberPosition = QPointF(1.27, -1.27);
        pin.length = 2.54;
        pin.direction = IR::PinDirection::Right;
        symbol.pins.append(pin);
        symbol.rectangles.append({-2.54, -1.27, 2.54, 1.27});

        const QString outputPath = tempDir.filePath(QStringLiteral("symbols.zip"));
        ExporterXpeditionSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary({symbol}, QStringLiteral("Library"), outputPath, false, false));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.startsWith("PK\x03\x04"));
        QVERIFY(data.contains("TEST_SYMBOL.1"));
        QVERIFY(data.contains("V 50"));
        QVERIFY(data.contains("P 1"));
        QVERIFY(data.contains("l 2"));
    }

    void footprintLibraryContainsPadAndCellEntries() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("TEST_FOOTPRINT");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.position = QPointF(0, 0);
        pad.size = QSizeF(1.27, 1.27);
        pad.shape = IR::PadShape::Rect;
        footprint.pads.append(pad);

        const QString outputPath = tempDir.filePath(QStringLiteral("footprints.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.startsWith("PK\x03\x04"));
        QVERIFY(data.contains("TEST_FOOTPRINT_Pads.hkp"));
        QVERIFY(data.contains("TEST_FOOTPRINT_Cell.hkp"));
        QVERIFY(data.contains(".PADSTACK"));
        QVERIFY(data.contains("..PIN \"1\""));
    }

    void throughHolePadUsesReferencedHoleDefinition() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("TH_FOOTPRINT");
        IR::FootprintPadIR pad;
        pad.number = QStringLiteral("1");
        pad.size = QSizeF(2.0, 2.0);
        pad.holeSize = 1.0;
        pad.padType = IR::PadType::ThroughHole;
        pad.shape = IR::PadShape::Ellipse;
        footprint.pads.append(pad);

        const QString outputPath = tempDir.filePath(QStringLiteral("through-hole.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("...HOLE_NAME \"HOLE_39.3701\""));
        QVERIFY(data.contains(".Hole \"HOLE_39.3701\""));
    }

    void unsupportedFootprintElementsAreReported() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("DIAGNOSTIC_FOOTPRINT");
        footprint.texts.append(IR::FootprintTextIR{});
        footprint.arcs.append(IR::FootprintArcIR{});
        footprint.regions.append(IR::FootprintRegionIR{});
        footprint.holes.append(IR::FootprintHoleIR{});

        const QString outputPath = tempDir.filePath(QStringLiteral("diagnostics.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));
        const QStringList diagnostics = exporter.diagnostics();
        QVERIFY(diagnostics.join(QStringLiteral("\n")).contains(QStringLiteral("未写入")));
    }
};

QTEST_GUILESS_MAIN(TestXpeditionExporter)

#include "test_xpedition_exporter.moc"
