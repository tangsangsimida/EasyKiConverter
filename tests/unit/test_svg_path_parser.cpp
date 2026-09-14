#include "core/ir/GeometryNormalizer.h"
#include "core/utils/SvgPathParser.h"

#include <QTest>

using namespace EasyKiConverter;

class TestSvgPathParser : public QObject {
    Q_OBJECT

private slots:

    void parsesMoveLineHorizontalVerticalAndClose() {
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 0 0 H 10 V 5 L 0 5 Z"));

        QCOMPARE(points.size(), 5);
        QCOMPARE(points.at(0), QPointF(0, 0));
        QCOMPARE(points.at(1), QPointF(10, 0));
        QCOMPARE(points.at(2), QPointF(10, 5));
        QCOMPARE(points.at(3), QPointF(0, 5));
        QCOMPARE(points.at(4), QPointF(0, 0));
    }

    void parsesRelativeCommands() {
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 10 10 l 5 0 h 5 v 5"));

        QCOMPARE(points.size(), 4);
        QCOMPARE(points.at(0), QPointF(10, 10));
        QCOMPARE(points.at(1), QPointF(15, 10));
        QCOMPARE(points.at(2), QPointF(20, 10));
        QCOMPARE(points.at(3), QPointF(20, 15));
    }

    void parsesRepeatedParameterGroups() {
        const QList<QPointF> linePoints = SvgPathParser::parsePath(QStringLiteral("M 0 0 L 10 0 10 10 0 10 Z"));
        QCOMPARE(linePoints.size(), 5);
        QCOMPARE(linePoints.last(), QPointF(0, 0));

        const QList<QPointF> curvePoints = SvgPathParser::parsePath(QStringLiteral("M 0 0 Q 10 20 20 0 30 -20 40 0"));
        QVERIFY(curvePoints.size() > 25);
        QCOMPARE(curvePoints.at(8), QPointF(10, 10));
        QCOMPARE(curvePoints.last(), QPointF(40, 0));

        const QList<QPointF> arcPoints =
            SvgPathParser::parsePath(QStringLiteral("M 0 0 A 10 10 0 0 1 10 10 10 10 0 0 1 20 0"));
        QVERIFY(arcPoints.size() > 60);
        QCOMPARE(arcPoints.last(), QPointF(20, 0));
    }

    void smoothCurvesPreserveReflectedControlPoints() {
        const QList<QPointF> cubicPoints =
            SvgPathParser::parsePath(QStringLiteral("M 0 0 C 0 10 10 10 10 0 S 20 -10 20 0"));
        QCOMPARE(cubicPoints.at(24), QPointF(15, -7.5));

        const QList<QPointF> quadraticPoints = SvgPathParser::parsePath(QStringLiteral("M 0 0 Q 10 20 20 0 T 40 0"));
        QCOMPARE(quadraticPoints.at(24), QPointF(30, -10));
    }

    void cubicBezierProducesPolylineIncludingEndpoint() {
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 0 0 C 0 10 10 10 10 0"));

        QVERIFY(points.size() > 3);
        QCOMPARE(points.first(), QPointF(0, 0));
        QCOMPARE(points.last(), QPointF(10, 0));
    }

    void arcProducesIntermediatePointsAndEndpoint() {
        const QList<QPointF> points = SvgPathParser::parsePath(QStringLiteral("M 0 0 A 10 10 0 0 1 10 10"));

        QVERIFY(points.size() > 3);
        QCOMPARE(points.first(), QPointF(0, 0));
        QCOMPARE(points.last(), QPointF(10, 10));
    }

    void normalizedArcUsesOnlyGeometryCoordinates() {
        const QList<QPointF> points = IR::GeometryNormalizer::parseSimpleSvgPath(
            QStringLiteral("M 0 0 A 10 10 0 0 1 10 10 A 10 10 0 0 1 20 0 Z"));

        QVERIFY(points.size() > 10);
        for (const QPointF& point : points) {
            QVERIFY2(qAbs(point.x()) < 10.0, "arc parser produced an implausibly large X coordinate");
            QVERIFY2(qAbs(point.y()) < 10.0, "arc parser produced an implausibly large Y coordinate");
        }
        QCOMPARE(points.first(), points.last());
    }

    void normalizedQuadraticProducesEndpoint() {
        const QList<QPointF> points = IR::GeometryNormalizer::parseSimpleSvgPath(QStringLiteral("M 0 0 Q 10 20 20 0"));

        QVERIFY(points.size() > 3);
        QCOMPARE(points.first(), QPointF(0, 0));
        QCOMPARE(points.last(), QPointF(5.08, 0));
        QVERIFY(points.at(points.size() / 2).y() > 2.0);
    }

    void smoothCurvesProduceValidEndpoints() {
        const QList<QPointF> points =
            SvgPathParser::parsePath(QStringLiteral("M 0 0 C 0 10 10 10 10 0 S 20 -10 20 0 Q 30 10 40 0 T 60 0"));

        QVERIFY(points.size() > 20);
        QCOMPARE(points.first(), QPointF(0, 0));
        QCOMPARE(points.last(), QPointF(60, 0));
    }

    void preservesNativeLineAndCubicSegments() {
        const QList<SvgPathSegment> segments =
            SvgPathParser::parseSegments(QStringLiteral("M 0 0 C 0 10 10 10 10 0 L 20 0 Q 25 10 30 0 Z"));

        QCOMPARE(segments.size(), 4);
        QCOMPARE(segments.at(0).type, SvgPathSegment::Type::CubicBezier);
        QCOMPARE(segments.at(0).start, QPointF(0, 0));
        QCOMPARE(segments.at(0).control1, QPointF(0, 10));
        QCOMPARE(segments.at(0).control2, QPointF(10, 10));
        QCOMPARE(segments.at(0).end, QPointF(10, 0));
        QCOMPARE(segments.at(1).type, SvgPathSegment::Type::Line);
        QCOMPARE(segments.at(1).end, QPointF(20, 0));
        QCOMPARE(segments.at(2).type, SvgPathSegment::Type::CubicBezier);
        QVERIFY(qAbs(segments.at(2).control1.x() - 23.3333333333) < 1e-9);
        QVERIFY(qAbs(segments.at(2).control1.y() - 6.6666666667) < 1e-9);
        QVERIFY(qAbs(segments.at(2).control2.x() - 26.6666666667) < 1e-9);
        QVERIFY(qAbs(segments.at(2).control2.y() - 6.6666666667) < 1e-9);
        QCOMPARE(segments.at(2).end, QPointF(30, 0));
        QCOMPARE(segments.at(3).type, SvgPathSegment::Type::Line);
        QCOMPARE(segments.at(3).end, QPointF(0, 0));
    }

    void preservesCircularArcSegmentsAndFallsBackForEllipses() {
        const QList<SvgPathSegment> circular =
            SvgPathParser::parseSegments(QStringLiteral("M 0 0 A 10 10 0 0 1 10 10"));
        QCOMPARE(circular.size(), 1);
        QCOMPARE(circular.first().type, SvgPathSegment::Type::CircularArc);
        QCOMPARE(circular.first().start, QPointF(0, 0));
        QCOMPARE(circular.first().end, QPointF(10, 10));
        QVERIFY(circular.first().arcMid.x() > 6.0);
        QVERIFY(circular.first().arcMid.y() < 5.0);

        const QList<SvgPathSegment> elliptical =
            SvgPathParser::parseSegments(QStringLiteral("M 0 0 A 10 5 0 0 1 10 10"));
        QVERIFY(elliptical.size() > 2);
        for (const SvgPathSegment& segment : elliptical)
            QCOMPARE(segment.type, SvgPathSegment::Type::Line);
    }

    void invalidOrEmptyPathsReturnNoPoints() {
        QVERIFY(SvgPathParser::parsePath(QString()).isEmpty());
        QVERIFY(SvgPathParser::parsePath(QStringLiteral("Q 1 2")).isEmpty());
        QVERIFY(SvgPathParser::parseSegments(QString()).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSvgPathParser)
#include "test_svg_path_parser.moc"
