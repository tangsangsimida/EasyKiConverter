#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/FootprintData.h"
#include "models/FootprintDataSerializer.h"
#include "models/SymbolData.h"
#include "models/SymbolDataSerializer.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
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

    void testModel3DAbsoluteOriginImport() {
        EasyedaFootprintImporter importer;
        const QJsonObject cadData = makeCadDataWithModelOrigin(QStringLiteral("3998.803,2982.7698"));

        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(cadData);

        QVERIFY(footprint);
        QCOMPARE(footprint->model3D().translation().x, 3998.803);
        QCOMPARE(footprint->model3D().translation().y, 2982.7698);
    }

    void testModel3DRelativeOriginImport() {
        EasyedaFootprintImporter importer;
        const QJsonObject cadData = makeCadDataWithModelOrigin(QStringLiteral("1.5,-2"));

        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(cadData);

        QVERIFY(footprint);
        QCOMPARE(footprint->model3D().translation().x, 4000.3);
        QCOMPARE(footprint->model3D().translation().y, 2980.35);
    }

    void testModel3DUnrelatedOriginFallsBackToFootprintCenter() {
        EasyedaFootprintImporter importer;
        const QJsonObject cadData = makeCadDataWithModelOrigin(QStringLiteral("400,300"));

        const QSharedPointer<FootprintData> footprint = importer.importFootprintData(cadData);

        QVERIFY(footprint);
        QCOMPARE(footprint->model3D().translation().x, 3998.8);
        QCOMPARE(footprint->model3D().translation().y, 2982.35);
    }

    void testStepExportPreservesStepAssemblyStructure() {
        Model3DData modelData;
        modelData.setStep(
            QByteArray("ISO-10303-21;\n"
                       "DATA;\n"
                       "#1=CARTESIAN_POINT('',(10.,20.,5.));\n"
                       "#2=CARTESIAN_POINT('',(14.,24.,7.));\n"
                       "#3=CARTESIAN_POINT('',(0.,0.,0.));\n"
                       "#4=VERTEX_POINT('',#1);\n"
                       "#5=VERTEX_POINT('',#2);\n"
                       "#6=DIRECTION('',(0.,0.,1.));\n"
                       "ENDSEC;\n"
                       "END-ISO-10303-21;\n"));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString stepPath = tempDir.filePath(QStringLiteral("model.step"));

        Exporter3DModel exporter;
        QVERIFY(exporter.exportToStep(modelData, stepPath));

        QFile stepFile(stepPath);
        QVERIFY(stepFile.open(QIODevice::ReadOnly));
        const QString content = QString::fromLatin1(stepFile.readAll());

        QVERIFY(content.contains(QStringLiteral("#1=CARTESIAN_POINT('',(10.,20.,5.));")));
        QVERIFY(content.contains(QStringLiteral("#2=CARTESIAN_POINT('',(14.,24.,7.));")));
        QVERIFY(content.contains(QStringLiteral("#3=CARTESIAN_POINT('',(0.,0.,0.));")));
        QVERIFY(content.contains(QStringLiteral("#6=DIRECTION('',(0.,0.,1.));")));
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

private:
    QJsonObject makeCadDataWithModelOrigin(const QString& origin) const {
        QJsonObject cadData;
        cadData.insert(QStringLiteral("SMT"), true);

        QJsonObject cPara;
        cPara.insert(QStringLiteral("package"), QStringLiteral("TEST_FOOTPRINT"));
        cPara.insert(QStringLiteral("3DModel"), QStringLiteral("TEST_MODEL"));

        QJsonObject head;
        head.insert(QStringLiteral("c_para"), cPara);

        QJsonObject bbox;
        bbox.insert(QStringLiteral("x"), 3977.3);
        bbox.insert(QStringLiteral("y"), 2971.0);
        bbox.insert(QStringLiteral("width"), 43.0);
        bbox.insert(QStringLiteral("height"), 22.7);

        QJsonObject attrs;
        attrs.insert(QStringLiteral("c_etype"), QStringLiteral("outline3D"));
        attrs.insert(QStringLiteral("uuid"), QStringLiteral("model-uuid"));
        attrs.insert(QStringLiteral("title"), QStringLiteral("TEST_MODEL"));
        attrs.insert(QStringLiteral("c_origin"), origin);
        attrs.insert(QStringLiteral("c_rotation"), QStringLiteral("0,0,0"));
        attrs.insert(QStringLiteral("z"), QStringLiteral("0"));

        QJsonObject svgNode;
        svgNode.insert(QStringLiteral("attrs"), attrs);

        QJsonArray shapes;
        shapes.append(QStringLiteral("SVGNODE~") +
                      QString::fromUtf8(QJsonDocument(svgNode).toJson(QJsonDocument::Compact)));

        QJsonObject dataStr;
        dataStr.insert(QStringLiteral("head"), head);
        dataStr.insert(QStringLiteral("BBox"), bbox);
        dataStr.insert(QStringLiteral("shape"), shapes);

        QJsonObject packageDetail;
        packageDetail.insert(QStringLiteral("dataStr"), dataStr);
        cadData.insert(QStringLiteral("packageDetail"), packageDetail);

        return cadData;
    }
};

QTEST_GUILESS_MAIN(TestModels)
#include "test_models.moc"
