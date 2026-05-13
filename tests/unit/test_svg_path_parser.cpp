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

    void invalidOrEmptyPathsReturnNoPoints() {
        QVERIFY(SvgPathParser::parsePath(QString()).isEmpty());
        QVERIFY(SvgPathParser::parsePath(QStringLiteral("Q 1 2")).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestSvgPathParser)
#include "test_svg_path_parser.moc"
