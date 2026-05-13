#include "core/kicad/ExporterFootprint.h"
#include "core/kicad/ExporterSymbol.h"
#include "models/FootprintData.h"
#include "models/Model3DData.h"
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

    void testFootprintWith3DMatchesGolden() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputPath = tempDir.filePath(QStringLiteral("FOOTPRINT_WITH_3D.kicad_mod"));

        ExporterFootprint exporter;
        QVERIFY(exporter.exportFootprint(makeFootprintWith3D(),
                                         outputPath,
                                         QStringLiteral("../GoldenLib.3dmodels/FOOTPRINT_WITH_3D.wrl"),
                                         QStringLiteral("../GoldenLib.3dmodels/FOOTPRINT_WITH_3D.step")));

        QString error;
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(
                     actual, QStringLiteral("kicad/golden_footprint_with_3d.kicad_mod"), &error),
                 qPrintable(error));
    }

    void testMultiComponentLibraryMatchesGolden() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputPath = tempDir.filePath(QStringLiteral("MultiLib.kicad_sym"));

        ExporterSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary(
            {makeCapacitorSymbol(), makeInductorSymbol()}, QStringLiteral("MultiLib"), outputPath, false, false));

        QString error;
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(
                     actual, QStringLiteral("kicad/golden_multi_component_lib.kicad_sym"), &error),
                 qPrintable(error));
    }

    void testSymbolWithSpecialCharsMatchesGolden() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString outputPath = tempDir.filePath(QStringLiteral("SpecialLib.kicad_sym"));

        ExporterSymbol exporter;
        QVERIFY(exporter.exportSymbolLibrary(
            {makeResistorSymbol()}, QStringLiteral("SpecialLib"), outputPath, false, false));

        QString error;
        const QString actual = TestPaths::readText(outputPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(
                     actual, QStringLiteral("kicad/golden_symbol_with_special_chars.kicad_sym"), &error),
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

        FootprintPad pad{};
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

    FootprintData makeFootprintWith3D() const {
        FootprintData footprint;

        FootprintInfo info;
        info.name = QStringLiteral("FOOTPRINT_WITH_3D");
        info.description = QStringLiteral("Footprint with 3D model references");
        info.type = QStringLiteral("smd");
        footprint.setInfo(info);
        footprint.setBbox({0, 0, 0, 0});

        FootprintPad pad1{};
        pad1.shape = QStringLiteral("rect");
        pad1.centerX = -6;
        pad1.centerY = 0;
        pad1.width = 5;
        pad1.height = 7;
        pad1.layerId = 1;
        pad1.number = QStringLiteral("1");
        pad1.rotation = 0;
        footprint.addPad(pad1);

        FootprintPad pad2{};
        pad2.shape = QStringLiteral("rect");
        pad2.centerX = 6;
        pad2.centerY = 0;
        pad2.width = 5;
        pad2.height = 7;
        pad2.layerId = 1;
        pad2.number = QStringLiteral("2");
        pad2.rotation = 0;
        footprint.addPad(pad2);

        Model3DData model3D;
        model3D.setName(QStringLiteral("FOOTPRINT_WITH_3D"));
        model3D.setTranslation({0.0, -0.1, 0.0});
        footprint.setModel3D(model3D);

        return footprint;
    }

    SymbolData makeCapacitorSymbol() const {
        SymbolData symbol;

        SymbolInfo info;
        info.name = QStringLiteral("CAPACITOR_0402");
        info.prefix = QStringLiteral("C");
        info.package = QStringLiteral("0402_C");
        info.lcscId = QStringLiteral("C23186");
        symbol.setInfo(info);
        symbol.setBbox({-6, -6, 12, 12});

        SymbolPolyline pl1;
        pl1.points = QStringLiteral("-6 -6 6 -6");
        pl1.strokeWidth = 0;
        pl1.fillColor = false;
        pl1.isLocked = true;
        symbol.addPolyline(pl1);

        SymbolPolyline pl2;
        pl2.points = QStringLiteral("-6 6 6 6");
        pl2.strokeWidth = 0;
        pl2.fillColor = false;
        pl2.isLocked = true;
        symbol.addPolyline(pl2);

        SymbolPin pin1{};
        pin1.settings.posX = 0;
        pin1.settings.posY = 20;
        pin1.settings.rotation = 90;
        pin1.settings.spicePinNumber = QStringLiteral("1");
        pin1.settings.type = PinType::Unspecified;
        pin1.pinPath.path = QStringLiteral("M 0 20 h -14");
        pin1.name.text = QStringLiteral("~");
        symbol.addPin(pin1);

        SymbolPin pin2{};
        pin2.settings.posX = 0;
        pin2.settings.posY = -20;
        pin2.settings.rotation = 270;
        pin2.settings.spicePinNumber = QStringLiteral("2");
        pin2.settings.type = PinType::Unspecified;
        pin2.pinPath.path = QStringLiteral("M 0 -20 h 14");
        pin2.name.text = QStringLiteral("~");
        symbol.addPin(pin2);

        return symbol;
    }

    SymbolData makeInductorSymbol() const {
        SymbolData symbol;

        SymbolInfo info;
        info.name = QStringLiteral("INDUCTOR_0603");
        info.prefix = QStringLiteral("L");
        info.package = QStringLiteral("0603_L");
        info.lcscId = QStringLiteral("C34567");
        symbol.setInfo(info);
        symbol.setBbox({0, -5, 20, 10});

        SymbolArc arc1;
        arc1.path.append(QPointF(0, 0));
        arc1.path.append(QPointF(5, -5));
        arc1.path.append(QPointF(10, 0));
        arc1.strokeWidth = 0;
        arc1.fillColor = false;
        arc1.isLocked = true;
        symbol.addArc(arc1);

        SymbolArc arc2;
        arc2.path.append(QPointF(10, 0));
        arc2.path.append(QPointF(15, -5));
        arc2.path.append(QPointF(20, 0));
        arc2.strokeWidth = 0;
        arc2.fillColor = false;
        arc2.isLocked = true;
        symbol.addArc(arc2);

        SymbolPin pin1{};
        pin1.settings.posX = -10;
        pin1.settings.posY = 0;
        pin1.settings.rotation = 0;
        pin1.settings.spicePinNumber = QStringLiteral("1");
        pin1.settings.type = PinType::Unspecified;
        pin1.pinPath.path = QStringLiteral("M -10 0 h -10");
        pin1.name.text = QStringLiteral("~");
        symbol.addPin(pin1);

        SymbolPin pin2{};
        pin2.settings.posX = 30;
        pin2.settings.posY = 0;
        pin2.settings.rotation = 180;
        pin2.settings.spicePinNumber = QStringLiteral("2");
        pin2.settings.type = PinType::Unspecified;
        pin2.pinPath.path = QStringLiteral("M 30 0 h 10");
        pin2.name.text = QStringLiteral("~");
        symbol.addPin(pin2);

        return symbol;
    }

    SymbolData makeResistorSymbol() const {
        SymbolData symbol;

        SymbolInfo info;
        info.name = QStringLiteral("RESISTOR_0402");
        info.prefix = QStringLiteral("R");
        info.package = QStringLiteral("0603_R");
        info.lcscId = QStringLiteral("C12345");
        info.description = QStringLiteral("10K \"thin film\"\n±1%");
        symbol.setInfo(info);
        symbol.setBbox({-5, -10, 10, 30});

        SymbolPolyline pl1;
        pl1.points = QStringLiteral("0 20 0 10");
        pl1.strokeWidth = 0;
        pl1.fillColor = false;
        pl1.isLocked = true;
        symbol.addPolyline(pl1);

        SymbolPolyline pl2;
        pl2.points = QStringLiteral("0 -10 0 -20");
        pl2.strokeWidth = 0;
        pl2.fillColor = false;
        pl2.isLocked = true;
        symbol.addPolyline(pl2);

        SymbolRectangle rect;
        rect.posX = -5;
        rect.posY = 10;
        rect.width = 10;
        rect.height = -20;
        rect.strokeWidth = 0;
        rect.fillColor = QString();
        rect.isLocked = true;
        symbol.addRectangle(rect);

        SymbolPin pin1{};
        pin1.settings.posX = 0;
        pin1.settings.posY = 30;
        pin1.settings.rotation = 90;
        pin1.settings.spicePinNumber = QStringLiteral("1");
        pin1.settings.type = PinType::Unspecified;
        pin1.pinPath.path = QStringLiteral("M 0 30 h -10");
        pin1.name.text = QStringLiteral("~");
        symbol.addPin(pin1);

        SymbolPin pin2{};
        pin2.settings.posX = 0;
        pin2.settings.posY = -30;
        pin2.settings.rotation = 270;
        pin2.settings.spicePinNumber = QStringLiteral("2");
        pin2.settings.type = PinType::Unspecified;
        pin2.pinPath.path = QStringLiteral("M 0 -30 h 10");
        pin2.name.text = QStringLiteral("~");
        symbol.addPin(pin2);

        return symbol;
    }
};

QTEST_GUILESS_MAIN(TestKiCadExporterGolden)
#include "test_kicad_exporter_golden.moc"
