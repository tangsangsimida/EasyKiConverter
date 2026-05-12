#include "services/BomParser.h"
#include "tests/common/TestPaths.hpp"
#include "utils/cli/FileReader.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

class TestBomParser : public QObject {
    Q_OBJECT

private slots:

    void validateIdAcceptsOnlyLcscComponentIds() {
        QVERIFY(BomParser::validateId(QStringLiteral("C1234")));
        QVERIFY(BomParser::validateId(QStringLiteral("c98765")));

        QVERIFY(!BomParser::validateId(QStringLiteral("C123")));
        QVERIFY(!BomParser::validateId(QStringLiteral("R12345")));
        QVERIFY(!BomParser::validateId(QStringLiteral("C0402")));
        QVERIFY(!BomParser::validateId(QStringLiteral("C0603")));
    }

    void parseCsvNormalizesDeduplicatesAndFiltersIds() {
        BomParser parser;
        const QString fixturePath = TestPaths::fixturePath(QStringLiteral("bom/mixed_components.csv"));

        const QStringList ids = parser.parse(fixturePath);

        QCOMPARE(ids, QStringList({QStringLiteral("C23186"), QStringLiteral("C23166"), QStringLiteral("C98765")}));
    }

    void fileReaderReadBomFileUsesBomParser() {
        QString error;
        const QString fixturePath = TestPaths::fixturePath(QStringLiteral("bom/mixed_components.csv"));

        const QStringList ids = FileReader::readBomFile(fixturePath, error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(ids, QStringList({QStringLiteral("C23186"), QStringLiteral("C23166"), QStringLiteral("C98765")}));
    }

    void fileReaderReadBomFileReportsNoValidIds() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString filePath = tempDir.filePath(QStringLiteral("invalid.csv"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        stream << "Designator,LCSC Part\n";
        stream << "R1,C0402\n";
        stream << "C1,C123\n";
        stream << "U1,NOT_AN_ID\n";
        file.close();

        QString error;
        const QStringList ids = FileReader::readBomFile(filePath, error);

        QVERIFY(ids.isEmpty());
        QVERIFY(error.contains(QStringLiteral("BOM 表中没有找到有效的元器件编号")));
    }
};

QTEST_GUILESS_MAIN(TestBomParser)
#include "test_bom_parser.moc"
