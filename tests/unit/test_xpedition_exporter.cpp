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
};

QTEST_GUILESS_MAIN(TestXpeditionExporter)

#include "test_xpedition_exporter.moc"
