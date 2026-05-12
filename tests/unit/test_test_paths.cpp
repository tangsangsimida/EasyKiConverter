#include "tests/common/TestPaths.hpp"

#include <QFileInfo>
#include <QTest>

using namespace EasyKiConverter::Test;

class TestTestPaths : public QObject {
    Q_OBJECT

private slots:

    void testTestsRootExists() {
        QVERIFY2(QFileInfo::exists(TestPaths::testsRoot()), qPrintable(TestPaths::testsRoot()));
    }

    void testFixtureAndGoldenPaths() {
        QCOMPARE(TestPaths::fixturePath(QStringLiteral("easyeda/.gitkeep")),
                 QDir::cleanPath(TestPaths::testsRoot() + QStringLiteral("/fixtures/easyeda/.gitkeep")));
        QCOMPARE(TestPaths::goldenPath(QStringLiteral("kicad/.gitkeep")),
                 QDir::cleanPath(TestPaths::testsRoot() + QStringLiteral("/golden/kicad/.gitkeep")));
    }

    void testCompareTextToGolden() {
        QString error;
        const QString expected = TestPaths::readText(TestPaths::goldenPath(QStringLiteral("README.md")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        QVERIFY2(TestPaths::compareTextToGolden(expected, QStringLiteral("README.md"), &error), qPrintable(error));
        QVERIFY2(
            !TestPaths::compareTextToGolden(expected + QStringLiteral("\nextra"), QStringLiteral("README.md"), &error),
            "Mismatch should be reported for modified content");
        QVERIFY2(error.contains(QStringLiteral("Golden file mismatch")), qPrintable(error));
    }
};

QTEST_GUILESS_MAIN(TestTestPaths)
#include "test_test_paths.moc"
