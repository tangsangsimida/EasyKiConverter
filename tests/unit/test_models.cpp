#include "models/FootprintData.h"
#include "models/FootprintDataSerializer.h"
#include "models/SymbolData.h"
#include "models/SymbolDataSerializer.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

using namespace EasyKiConverter;

class TestModels : public QObject {
    Q_OBJECT

private slots:

    void initTestCase() {}

    void cleanupTestCase() {}

    void testSymbolDataRoundTrip() {
        SymbolData original;

        // 设置基本信息
        SymbolInfo info;
        info.name = "Test Component";
        info.lcscId = "C12345";
        info.prefix = "U";
        original.setInfo(info);

        // 设置边界
        SymbolBBox bbox = {0, 0, 100, 100};
        original.setBbox(bbox);

        // 添加一个引脚
        SymbolPin pin;
        pin.settings.spicePinNumber = "1";
        pin.settings.posX = 10.0;
        pin.settings.posY = 20.0;
        pin.name.text = "VCC";
        original.addPin(pin);

        // 序列化
        QJsonObject json = original.toJson();

        // 反序列化
        SymbolData restored;
        bool ok = restored.fromJson(json);

        QVERIFY(ok);
        QCOMPARE(restored.info().lcscId, original.info().lcscId);
        QCOMPARE(restored.info().name, original.info().name);
        QCOMPARE(restored.pins().size(), original.pins().size());
        QCOMPARE(restored.pins().at(0).settings.spicePinNumber, original.pins().at(0).settings.spicePinNumber);
        QCOMPARE(restored.pins().at(0).name.text, original.pins().at(0).name.text);
    }

    void testFootprintDataRoundTrip() {
        FootprintData original;

        // 设置基本信息
        FootprintInfo info;
        info.name = "SOT-23";
        info.uuid = "uuid-123";
        original.setInfo(info);

        // 添加一个焊盘
        FootprintPad pad;
        pad.number = "1";
        pad.centerX = 1.0;
        pad.centerY = 2.0;
        pad.width = 0.5;
        pad.height = 0.8;
        pad.shape = "RECT";
        original.addPad(pad);

        // 序列化
        QJsonObject json = original.toJson();

        // 反序列化
        FootprintData restored;
        bool ok = restored.fromJson(json);

        QVERIFY(ok);
        QCOMPARE(restored.info().name, original.info().name);
        QCOMPARE(restored.pads().size(), original.pads().size());
        QCOMPARE(restored.pads().at(0).number, original.pads().at(0).number);
        QCOMPARE(restored.pads().at(0).shape, original.pads().at(0).shape);
    }

    void testSymbolGeometrySerialization() {
        SymbolData original;

        // 添加矩形
        SymbolRectangle rect;
        rect.posX = 10;
        rect.posY = 10;
        rect.width = 50;
        rect.height = 30;
        rect.strokeWidth = 1.0;
        original.addRectangle(rect);

        // 添加圆
        SymbolCircle circle;
        circle.centerX = 100;
        circle.centerY = 100;
        circle.radius = 20;
        original.addCircle(circle);

        QJsonObject json = original.toJson();

        SymbolData restored;
        restored.fromJson(json);

        QCOMPARE(restored.rectangles().size(), 1);
        QCOMPARE(restored.rectangles().at(0).width, 50.0);
        QCOMPARE(restored.circles().size(), 1);
        QCOMPARE(restored.circles().at(0).radius, 20.0);
    }
};

QTEST_GUILESS_MAIN(TestModels)
#include "test_models.moc"
