#include "core/kicad/FootprintGraphicsGenerator.h"
#include "core/kicad/SymbolGraphicsGenerator.h"

#include <QTest>

using namespace EasyKiConverter;

class TestKiCadGraphicsGenerators : public QObject {
    Q_OBJECT

private slots:

    void generatePadHandlesSmdRect() {
        FootprintPad pad{};
        pad.shape = QStringLiteral("rect");
        pad.centerX = 10;
        pad.centerY = -5;
        pad.width = 20;
        pad.height = 10;
        pad.layerId = 1;
        pad.number = QStringLiteral("1");
        pad.rotation = 270;

        const QString output = FootprintGraphicsGenerator().generatePad(pad, 0, 0);

        QVERIFY(output.contains(QStringLiteral("(pad 1 smd rect")));
        QVERIFY(output.contains(QStringLiteral("(at 2.54 -1.27 -90.00)")));
        QVERIFY(output.contains(QStringLiteral("(size 5.08 2.54)")));
        QVERIFY(output.contains(QStringLiteral("(layers F.Cu F.Paste F.Mask)")));
    }

    void generatePadHandlesThroughHoleOvalDrill() {
        FootprintPad pad{};
        pad.shape = QStringLiteral("oval");
        pad.centerX = 0;
        pad.centerY = 0;
        pad.width = 12;
        pad.height = 20;
        pad.layerId = 11;
        pad.number = QStringLiteral("P(2)");
        pad.holeRadius = 2;
        pad.holeLength = 8;

        const QString output = FootprintGraphicsGenerator().generatePad(pad, 0, 0);

        QVERIFY(output.contains(QStringLiteral("(pad 2 thru_hole oval")));
        QVERIFY(output.contains(QStringLiteral("(layers *.Cu *.Mask)")));
        QVERIFY(output.contains(QStringLiteral("(drill oval 1.00 2.03)")));
    }

    void generateCustomPadCreatesPrimitivePolygon() {
        FootprintPad pad{};
        pad.shape = QStringLiteral("polygon");
        pad.centerX = 10;
        pad.centerY = 0;
        pad.width = 4;
        pad.height = 4;
        pad.layerId = 1;
        pad.number = QStringLiteral("3");
        pad.points = QStringLiteral("8 -2 12 -2 12 2 8 2");

        const QString output = FootprintGraphicsGenerator().generatePad(pad, 0, 0);

        QVERIFY(output.contains(QStringLiteral("(pad 3 smd custom")));
        QVERIFY(output.contains(QStringLiteral("(primitives")));
        QVERIFY(output.contains(QStringLiteral("(gr_poly")));
        QVERIFY(output.contains(QStringLiteral("(xy -0.51 -0.51)")));
    }

    void generatePinMapsTypesAndDynamicStyles() {
        SymbolGraphicsGenerator generator;
        SymbolBBox bbox{0, 0, 0, 0};

        SymbolPin pin{};
        pin.settings.posX = 10;
        pin.settings.posY = -10;
        pin.settings.rotation = 90;
        pin.settings.spicePinNumber = QStringLiteral(" 1 ");
        pin.settings.type = PinType::Bidirectional;
        pin.pinPath.path = QStringLiteral("M 0 0 h -20");
        pin.name.text = QStringLiteral(" IN A ");
        pin.dot.isDisplayed = true;
        pin.clock.isDisplayed = true;

        const QString output = generator.generatePin(pin, bbox);

        QVERIFY(output.contains(QStringLiteral("(pin bidirectional inverted_clock")));
        QVERIFY(output.contains(QStringLiteral("(at 2.54 2.54 270)")));
        QVERIFY(output.contains(QStringLiteral("(length 5.08)")));
        QVERIFY(output.contains(QStringLiteral("(name \"INA\"")));
        QVERIFY(output.contains(QStringLiteral("(number \"1\"")));
    }

    void generatePolylineAndPathReturnKiCadPolylines() {
        SymbolGraphicsGenerator generator;
        generator.setCurrentBBox({0, 0, 0, 0});

        SymbolPolyline polyline{};
        polyline.points = QStringLiteral("0 0 10 0 10 10");
        polyline.strokeWidth = 0;
        polyline.fillColor = true;

        const QString polylineOutput = generator.generatePolyline(polyline);
        QVERIFY(polylineOutput.contains(QStringLiteral("(polyline")));
        QVERIFY(polylineOutput.contains(QStringLiteral("(xy 0.00 0.00) (xy 2.54 0.00) (xy 2.54 -2.54)")));
        QVERIFY(polylineOutput.contains(QStringLiteral("(fill (type background))")));

        SymbolPath path{};
        path.paths = QStringLiteral("M 0 0 L 10 0 L 10 10 Z");
        path.strokeWidth = 0;
        path.fillColor = false;

        const QString pathOutput = generator.generatePath(path);
        QVERIFY(pathOutput.contains(QStringLiteral("(polyline")));
        QVERIFY(pathOutput.contains(QStringLiteral("(fill (type none))")));
        QVERIFY(pathOutput.contains(QStringLiteral("(xy 0.00 0.00)")));
    }
};

QTEST_GUILESS_MAIN(TestKiCadGraphicsGenerators)
#include "test_kicad_graphics_generators.moc"
