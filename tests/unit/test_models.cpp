#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/ir/Model3DDataConverter.h"
#include "core/ir/SymbolDataConverter.h"
#include "core/kicad/Exporter3DModel.h"
#include "models/FootprintData.h"
#include "models/FootprintDataSerializer.h"
#include "models/SymbolData.h"
#include "models/SymbolDataSerializer.h"

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

#include <limits>

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
        original.addGraphicOrder({QStringLiteral("P"), 0});

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
        QCOMPARE(restored.graphicOrder().size(), 1);
        QCOMPARE(restored.graphicOrder().first().type, QStringLiteral("P"));
        QCOMPARE(restored.graphicOrder().first().index, 0);
    }

    void testSymbolValidationReportsAllGeometryIssues() {
        SymbolData symbol;
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{1.0, 1.0, 10.0, 10.0});

        SymbolRectangle rectangle;
        rectangle.width = 0.0;
        rectangle.height = 2.0;
        rectangle.rx = -1.0;
        rectangle.ry = 0.0;
        symbol.addRectangle(rectangle);

        SymbolPath path;
        symbol.addPath(path);
        SymbolText text;
        symbol.addText(text);
        SymbolImage image;
        image.width = -1.0;
        image.height = 2.0;
        image.data = QByteArrayLiteral("image-data");
        symbol.addImage(image);

        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.size() >= 4);
        QVERIFY(errors.contains(QStringLiteral("Rectangle 0 has a non-positive size")));
        QVERIFY(errors.contains(QStringLiteral("Rectangle 0 has a negative or non-finite corner radius")));
        QVERIFY(errors.contains(QStringLiteral("Path 0 has no commands")));
        QVERIFY(errors.contains(QStringLiteral("Text 0 is empty")));
        QVERIFY(errors.contains(QStringLiteral("Image 0 has invalid bounds or rotation")));
        QCOMPARE(symbol.validate(), errors.first());
    }

    void testSymbolValidationChecksMultipartGeometry() {
        SymbolData symbol;
        SymbolInfo info;
        info.name = QStringLiteral("MULTIPART_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{1.0, 1.0, 10.0, 10.0});

        SymbolPart part;
        part.unitNumber = 1;
        SymbolPath invalidPath;
        part.paths.append(invalidPath);
        SymbolImage invalidImage;
        invalidImage.width = 1.0;
        invalidImage.height = -2.0;
        invalidImage.data = QByteArrayLiteral("image-data");
        part.images.append(invalidImage);
        symbol.addPart(part);

        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.contains(QStringLiteral("Part 0 Path 0 has no commands")));
        QVERIFY(errors.contains(QStringLiteral("Part 0 Image 0 has invalid bounds or rotation")));
    }

    void testSymbolValidationChecksPinGeometry() {
        SymbolData symbol;
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_PIN_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 10.0});

        SymbolPin pin;
        pin.settings.spicePinNumber = QStringLiteral("1");
        pin.settings.posX = std::numeric_limits<double>::quiet_NaN();
        symbol.addPin(pin);

        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.contains(QStringLiteral("Pin 0 has a non-finite position")));
        QVERIFY(!symbol.isValid());
    }

    void testCommonPartRoundTripAndIrMapping() {
        SymbolData symbol;
        SymbolInfo info;
        info.name = QStringLiteral("COMMON_PART_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 20.0, 20.0});

        SymbolPart commonPart;
        commonPart.unitNumber = 0;
        commonPart.commonToAllParts = true;
        SymbolPin commonPin;
        commonPin.settings.spicePinNumber = QStringLiteral("VCC");
        commonPin.settings.posX = 0.0;
        commonPin.settings.posY = 0.0;
        commonPart.pins.append(commonPin);
        commonPart.graphicOrder.append({QStringLiteral("P"), 0});

        SymbolPart visiblePart;
        visiblePart.unitNumber = 1;
        SymbolPin visiblePin;
        visiblePin.settings.spicePinNumber = QStringLiteral("IN");
        visiblePin.settings.posX = 10.0;
        visiblePin.settings.posY = 0.0;
        visiblePart.pins.append(visiblePin);
        SymbolText visibleText;
        visibleText.text = QStringLiteral("LABEL");
        visibleText.anchor = QStringLiteral("end");
        visibleText.textSize = 8.0;
        visiblePart.texts.append(visibleText);
        SymbolRectangle visibleRectangle;
        visibleRectangle.posX = 0.0;
        visibleRectangle.posY = 0.0;
        visibleRectangle.rx = 0.0;
        visibleRectangle.ry = 0.0;
        visibleRectangle.width = 4.0;
        visibleRectangle.height = 2.0;
        visiblePart.rectangles.append(visibleRectangle);
        visiblePart.graphicOrder.append({QStringLiteral("P"), 0});
        visiblePart.graphicOrder.append({QStringLiteral("R"), 0});
        visiblePart.graphicOrder.append({QStringLiteral("T"), 0});

        symbol.addPart(commonPart);
        symbol.addPart(visiblePart);

        SymbolData restored;
        QVERIFY(restored.fromJson(symbol.toJson()));
        QCOMPARE(restored.parts().size(), 2);
        QVERIFY(restored.parts().first().commonToAllParts);
        QCOMPARE(restored.parts().at(1).rectangles.size(), 1);
        QCOMPARE(restored.parts().at(1).graphicOrder.size(), 3);
        QCOMPARE(restored.parts().at(1).graphicOrder.at(1).type, QStringLiteral("R"));
        QCOMPARE(restored.parts().at(1).graphicOrder.at(2).type, QStringLiteral("T"));

        const IR::SymbolComponentIR ir = IR::toSymbolIR(restored);
        QCOMPARE(ir.partCount, 1);
        QCOMPARE(ir.pins.size(), 2);
        QVERIFY(ir.pins.at(0).commonToAllParts);
        QCOMPARE(ir.pins.at(0).partIndex, -1);
        QVERIFY(!ir.pins.at(1).commonToAllParts);
        QCOMPARE(ir.pins.at(1).partIndex, 0);
        QCOMPARE(ir.texts.size(), 1);
        QCOMPARE(ir.texts.first().anchor, QStringLiteral("end"));
        QCOMPARE(ir.graphicOrder.at(0).partIndex, -1);
        QCOMPARE(ir.graphicOrder.at(1).partIndex, 0);
    }

    void testSymbolValidationChecksTextGeometry() {
        SymbolData symbol;
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_TEXT_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 10.0});

        SymbolText text;
        text.posX = std::numeric_limits<double>::quiet_NaN();
        text.text = QStringLiteral("LABEL");
        text.visible = true;
        text.textSize = 0.0;
        text.anchor = QStringLiteral("baseline");
        symbol.addText(text);

        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.contains(QStringLiteral("Text 0 has a non-finite position or rotation")));
        QVERIFY(errors.contains(QStringLiteral("Text 0 has a non-positive font size")));
        QVERIFY(errors.contains(QStringLiteral("Text 0 has an unsupported anchor baseline")));
        QVERIFY(!symbol.isValid());
    }

    void testSymbolValidationChecksPathCommands() {
        SymbolData symbol;
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_PATH_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 10.0});

        SymbolPath exponentPath;
        exponentPath.paths = QStringLiteral("M 0 0 L 1e-3 2e-3");
        symbol.addPath(exponentPath);

        SymbolPath unsupportedPath;
        unsupportedPath.paths = QStringLiteral("M 0 0 X 10 10");
        symbol.addPath(unsupportedPath);

        SymbolPath moveOnlyPath;
        moveOnlyPath.paths = QStringLiteral("M 2 2");
        symbol.addPath(moveOnlyPath);

        SymbolPath noMovePath;
        noMovePath.paths = QStringLiteral("L 4 4");
        symbol.addPath(noMovePath);

        SymbolPath incompletePath;
        incompletePath.paths = QStringLiteral("M 0 0 L 1");
        symbol.addPath(incompletePath);

        SymbolPath invalidTokenPath;
        invalidTokenPath.paths = QStringLiteral("M 0 0 L 1 1 @");
        symbol.addPath(invalidTokenPath);

        SymbolPath repeatedMovePath;
        repeatedMovePath.paths = QStringLiteral("M 0 0 1 1");
        symbol.addPath(repeatedMovePath);

        const QStringList errors = symbol.validationErrors();
        QVERIFY(!errors.contains(QStringLiteral("Path 0 has invalid command parameters")));
        QVERIFY(errors.contains(QStringLiteral("Path 1 contains an unsupported command")));
        QVERIFY(errors.contains(QStringLiteral("Path 2 has no drawable commands")));
        QVERIFY(errors.contains(QStringLiteral("Path 3 has no initial move command")));
        QVERIFY(errors.contains(QStringLiteral("Path 4 has invalid command parameters")));
        QVERIFY(errors.contains(QStringLiteral("Path 5 has invalid command parameters")));
        QVERIFY(!errors.contains(QStringLiteral("Path 6 has no drawable commands")));
        QVERIFY(!errors.contains(QStringLiteral("Path 6 has invalid command parameters")));
    }

    void testSymbolValidationChecksGraphicOrderReferences() {
        SymbolData symbol;
        SymbolInfo info;
        info.name = QStringLiteral("INVALID_ORDER_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 10.0});

        SymbolRectangle rectangle;
        rectangle.width = 1.0;
        rectangle.height = 1.0;
        symbol.addRectangle(rectangle);
        symbol.addGraphicOrder({QStringLiteral("R"), 0});
        symbol.addGraphicOrder({QStringLiteral("R"), 0});
        symbol.addGraphicOrder({QStringLiteral("PT"), 0});
        symbol.addGraphicOrder({QStringLiteral("UNKNOWN"), 0});

        const QStringList errors = symbol.validationErrors();
        QVERIFY(errors.contains(QStringLiteral("Graphic order 1 duplicates R index 0")));
        QVERIFY(errors.contains(QStringLiteral("Graphic order 2 has out-of-range PT index 0")));
        QVERIFY(errors.contains(QStringLiteral("Graphic order 3 has unknown type UNKNOWN")));
    }

    void testSymbolValidationAcceptsOriginBasedBoundingBox() {
        SymbolData symbol;
        SymbolInfo info;
        info.name = QStringLiteral("ORIGIN_SYMBOL");
        symbol.setInfo(info);
        symbol.setBbox(SymbolBBox{0.0, 0.0, 10.0, 5.0});

        QVERIFY(symbol.validationErrors().isEmpty());
        QVERIFY(symbol.isValid());
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
        auto irModel = IR::toModel3DIR(modelData);
        QVERIFY(exporter.exportToStep(irModel, stepPath));

        QFile stepFile(stepPath);
        QVERIFY(stepFile.open(QIODevice::ReadOnly));
        const QString content = QString::fromLatin1(stepFile.readAll());

        QVERIFY(content.contains(QStringLiteral("#1=CARTESIAN_POINT('',(10.,20.,5.));")));
        QVERIFY(content.contains(QStringLiteral("#2=CARTESIAN_POINT('',(14.,24.,7.));")));
        QVERIFY(content.contains(QStringLiteral("#3=CARTESIAN_POINT('',(0.,0.,0.));")));
        QVERIFY(content.contains(QStringLiteral("#6=DIRECTION('',(0.,0.,1.));")));
    }

    void testObjMinZMatchesExportedWrlNormalization() {
        const QByteArray positiveObj(
            "v 0 0 2.54\n"
            "v 1 1 5.08\n"
            "f 1 2 1\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(positiveObj), 1.0);

        const QByteArray negativeObj(
            "v 0 0 -2.54\n"
            "v 1 1 2.54\n"
            "f 1 2 1\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(negativeObj), -1.0);

        QCOMPARE(Exporter3DModel::calculateObjMinZ(QByteArray("f 1 2 3\n")), std::numeric_limits<double>::max());

        const QByteArray wrlData(
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [\n"
            "        0 0 -0.5,\n"
            "        0 0 1.0,\n"
            "      ]\n"
            "    }\n"
            "  }\n"
            "}\n");
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrlData), -1.27);
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
