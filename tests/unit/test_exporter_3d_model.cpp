#include "core/kicad/Exporter3DModel.h"
#include "models/Model3DData.h"
#include "tests/common/TestPaths.hpp"

#include <QTemporaryDir>
#include <QTest>

#include <limits>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestExporter3DModel : public QObject {
    Q_OBJECT

private slots:

    // === calculateObjMinZ 测试 ===

    void calculateObjMinZEmptyReturnsMax() {
        QCOMPARE(Exporter3DModel::calculateObjMinZ(QByteArray()), std::numeric_limits<double>::max());
    }

    void calculateObjMinZNoVerticesReturnsMax() {
        const QByteArray obj("# comment\nmtllib test.mtl\nusemtl default\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), std::numeric_limits<double>::max());
    }

    void calculateObjMinZSingleVertex() {
        // OBJ 坐标单位是 0.01 inch，除以 2.54 转换为 mm
        // v 0 0 254 => Z = 254/2.54 = 100 mm
        const QByteArray obj("v 0 0 254\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), 100.0);
    }

    void calculateObjMinZMultipleVertices() {
        const QByteArray obj(
            "v 0 0 254\n"
            "v 10 20 127\n"
            "v -5 0 508\n");
        // min Z = min(100, 50, 200) = 50 mm
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), 50.0);
    }

    void calculateObjMinZNegativeZ() {
        const QByteArray obj("v 0 0 -254\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), -100.0);
    }

    void calculateObjMinZIgnoresNonVertexLines() {
        const QByteArray obj(
            "# comment\n"
            "vn 0 0 1\n"
            "vt 0 0\n"
            "v 0 0 254\n"
            "f 1 2 3\n");
        QCOMPARE(Exporter3DModel::calculateObjMinZ(obj), 100.0);
    }

    // === calculateWrlDisplayMinZ 测试 ===

    void calculateWrlDisplayMinZEmptyReturnsMax() {
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(QByteArray()), std::numeric_limits<double>::max());
    }

    void calculateWrlDisplayMinZNoPointsReturnsMax() {
        const QByteArray wrl("#VRML V2.0 utf8\nShape {}\n");
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrl), std::numeric_limits<double>::max());
    }

    void calculateWrlDisplayMinZSinglePoint() {
        // WRL 坐标 * 2.54 = mm（KiCad 按 1 WRL unit = 2.54 mm 解释）
        // point [0 0 0.5] => Z = 0.5 * 2.54 = 1.27 mm
        const QByteArray wrl(
            "#VRML V2.0 utf8\n"
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [0 0 0.5,]\n"
            "    }\n"
            "  }\n"
            "}\n");
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrl), 1.27);
    }

    void calculateWrlDisplayMinZMultiplePoints() {
        const QByteArray wrl(
            "#VRML V2.0 utf8\n"
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [\n"
            "        1.0 2.0 0.1,\n"
            "        3.0 4.0 0.5,\n"
            "        5.0 6.0 0.2,\n"
            "      ]\n"
            "    }\n"
            "  }\n"
            "}\n");
        // min Z = min(0.1, 0.5, 0.2) * 2.54 = 0.254
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrl), 0.254);
    }

    void calculateWrlDisplayMinZMultipleShapes() {
        const QByteArray wrl(
            "#VRML V2.0 utf8\n"
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [0 0 1.0,]\n"
            "    }\n"
            "  }\n"
            "}\n"
            "Shape {\n"
            "  geometry IndexedFaceSet {\n"
            "    coord Coordinate {\n"
            "      point [0 0 0.3,]\n"
            "    }\n"
            "  }\n"
            "}\n");
        // min Z across both shapes = 0.3 * 2.54 = 0.762
        QCOMPARE(Exporter3DModel::calculateWrlDisplayMinZ(wrl), 0.762);
    }

    // === convertToKiCadCoordinates 测试 ===

    void convertToKiCadCoordinatesRotatesX180() {
        Model3DData model;
        model.setRotation({0.0, 0.0, 0.0});

        Exporter3DModel exporter;
        exporter.convertToKiCadCoordinates(model);

        Model3DBase rot = model.rotation();
        QCOMPARE(rot.x, 180.0);
        QCOMPARE(rot.y, 0.0);
        QCOMPARE(rot.z, 0.0);
    }

    void convertToKiCadCoordinatesNegatesYZ() {
        Model3DData model;
        model.setRotation({0.0, 30.0, 45.0});

        Exporter3DModel exporter;
        exporter.convertToKiCadCoordinates(model);

        Model3DBase rot = model.rotation();
        QCOMPARE(rot.x, 180.0);
        QCOMPARE(rot.y, -30.0);
        QCOMPARE(rot.z, -45.0);
    }

    void convertToKiCadCoordinatesPreservesTranslation() {
        Model3DData model;
        model.setTranslation({1.5, -2.5, 3.0});
        model.setRotation({10.0, 20.0, 30.0});

        Exporter3DModel exporter;
        exporter.convertToKiCadCoordinates(model);

        Model3DBase trans = model.translation();
        QCOMPARE(trans.x, 1.5);
        QCOMPARE(trans.y, -2.5);
        QCOMPARE(trans.z, 3.0);
    }

    void convertToKiCadCoordinatesIdempotent() {
        // 两次转换应该恢复原始旋转（180+180=360，-(-y)=y，-(-z)=z）
        Model3DData model;
        model.setRotation({10.0, 20.0, 30.0});

        Exporter3DModel exporter;
        exporter.convertToKiCadCoordinates(model);
        exporter.convertToKiCadCoordinates(model);

        Model3DBase rot = model.rotation();
        QCOMPARE(rot.x, 370.0);  // 10 + 180 + 180
        QCOMPARE(rot.y, 20.0);  // -(-20) = 20
        QCOMPARE(rot.z, 30.0);  // -(-30) = 30
    }

    // === exportToStep 测试 ===

    void exportToStepWritesRawBytes() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        Model3DData model;
        const QByteArray stepData("ISO-10303-21;\nHEADER;\nENDSEC;\nENDISO-10303-21;\n");
        model.setStep(stepData);

        const QString savePath = tempDir.filePath(QStringLiteral("test.step"));

        Exporter3DModel exporter;
        QVERIFY(exporter.exportToStep(model, savePath));

        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), stepData);
    }

    void exportToStepEmptyDataCreatesEmptyFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        Model3DData model;
        const QString savePath = tempDir.filePath(QStringLiteral("empty.step"));

        Exporter3DModel exporter;
        QVERIFY(exporter.exportToStep(model, savePath));

        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.readAll().isEmpty());
    }

    // === exportToWrl 黄金文件测试 ===

    void exportToWrlSimpleTriangleMatchesGolden() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // 构造简单三角形 OBJ 数据（3 顶点 + 1 面 + 1 材质）
        Model3DData model;
        model.setName(QStringLiteral("TRIANGLE"));
        model.setTranslation({0.0, 0.0, 0.0});
        model.setRotation({0.0, 0.0, 0.0});
        model.setRawObj(
            QStringLiteral("# Simple triangle\n"
                           "mtllib triangle.mtl\n"
                           "usemtl copper\n"
                           "v 0 0 0\n"
                           "v 254 0 0\n"
                           "v 127 254 0\n"
                           "f 1 2 3\n"));

        const QString savePath = tempDir.filePath(QStringLiteral("TRIANGLE.wrl"));

        Exporter3DModel exporter;
        QVERIFY(exporter.exportToWrl(model, savePath));

        QString error;
        const QString actual = TestPaths::readText(savePath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY2(TestPaths::compareTextToGolden(actual, QStringLiteral("kicad/golden_3d_triangle.wrl"), &error),
                 qPrintable(error));
    }

    void exportToWrlContainsVrmlHeader() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        Model3DData model;
        model.setRawObj(QStringLiteral("v 0 0 0\nv 100 0 0\nv 50 100 0\nf 1 2 3\n"));

        const QString savePath = tempDir.filePath(QStringLiteral("header.wrl"));

        Exporter3DModel exporter;
        QVERIFY(exporter.exportToWrl(model, savePath));

        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(file.readAll());
        QVERIFY(content.startsWith(QStringLiteral("#VRML V2.0 utf8")));
    }

    void exportToWrlVerticesNormalizedToMm() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // OBJ 顶点 Z=254 => WRL 应为 254/2.54 - normalizeZ mm
        // 三个顶点 Z 都是 254，所以 normalizeZ = 254/2.54 = 100
        // WRL Z = 100 - 100 = 0
        Model3DData model;
        model.setRawObj(QStringLiteral("v 0 0 254\nv 254 0 254\nv 127 254 254\nf 1 2 3\n"));

        const QString savePath = tempDir.filePath(QStringLiteral("normalize.wrl"));

        Exporter3DModel exporter;
        QVERIFY(exporter.exportToWrl(model, savePath));

        QFile file(savePath);
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(file.readAll());

        // 所有 Z 坐标应为 0（归一化后）
        QVERIFY(content.contains(QStringLiteral("-50.0000 -50.0000 0.0000")));
        QVERIFY(content.contains(QStringLiteral("50.0000 -50.0000 0.0000")));
        QVERIFY(content.contains(QStringLiteral("0.0000 50.0000 0.0000")));
    }
};

QTEST_GUILESS_MAIN(TestExporter3DModel)
#include "test_exporter_3d_model.moc"
