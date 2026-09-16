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

    // 验证符号 ZIP 至少包含头部、引脚和矩形等基本 ASCII 记录。
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

    // 验证封装库同时写出 Padstack 与 Cell 两类目标文件。
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

    // 验证通孔焊盘会建立可被 Padstack 引用的钻孔定义。
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

    // 验证圆弧、文本、填充区域和独立孔都能写入，并且诊断信息说明转换策略。
    void unsupportedFootprintElementsAreReported() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        IR::FootprintComponentIR footprint;
        footprint.name = QStringLiteral("DIAGNOSTIC_FOOTPRINT");
        IR::FootprintTextIR text;
        text.text = QStringLiteral("MARK");
        text.position = QPointF(1.0, 2.0);
        text.fontSize = 1.0;
        footprint.texts.append(text);
        IR::FootprintArcIR arc;
        arc.center = QPointF(0.0, 0.0);
        arc.radius = 1.0;
        arc.startAngle = 0.0;
        arc.endAngle = 90.0;
        footprint.arcs.append(arc);
        IR::FootprintRegionIR region;
        region.vertices = {QPointF(-1.0, -1.0), QPointF(1.0, -1.0), QPointF(1.0, 1.0)};
        footprint.regions.append(region);
        IR::FootprintHoleIR hole;
        hole.center = QPointF(3.0, 4.0);
        hole.radius = 0.5;
        footprint.holes.append(hole);

        const QString outputPath = tempDir.filePath(QStringLiteral("diagnostics.zip"));
        ExporterXpeditionFootprint exporter;
        QVERIFY(exporter.exportFootprintLibrary({footprint}, QStringLiteral("Library"), outputPath));
        const QStringList diagnostics = exporter.diagnostics();
        const QString diagnosticText = diagnostics.join(QStringLiteral("\n"));
        QVERIFY(diagnosticText.contains(QStringLiteral("折线近似")));
        QVERIFY(diagnosticText.contains(QStringLiteral("已写入")));

        QFile output(outputPath);
        QVERIFY(output.open(QIODevice::ReadOnly));
        const QByteArray data = output.readAll();
        QVERIFY(data.contains("..TEXT \"MARK\""));
        QVERIFY(data.contains("POLYLINE_SHAPE"));
        QVERIFY(data.contains("MH1"));
    }
};

QTEST_GUILESS_MAIN(TestXpeditionExporter)

#include "test_xpedition_exporter.moc"
// 使用最小但完整的符号数据，覆盖引脚位置和主体图元写入。
// 这些元素分别覆盖近似转换、文本转义、填充区域闭合和独立孔建模。
