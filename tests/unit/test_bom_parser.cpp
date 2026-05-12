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

private:
    // 创建临时 CSV 文件并返回路径，避免重复 QTemporaryDir + QFile 样板代码
    QString createTempCsv(QTemporaryDir& tempDir, const QString& name, const QStringList& lines) {
        const QString filePath = tempDir.filePath(name);
        QFile file(filePath);
        Q_ASSERT(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        for (const QString& line : lines) {
            stream << line << "\n";
        }
        return filePath;
    }

private slots:

    void validateIdAcceptsOnlyLcscComponentIds() {
        QVERIFY(BomParser::validateId(QStringLiteral("C1234")));
        QVERIFY(BomParser::validateId(QStringLiteral("c13564")));

        QVERIFY(!BomParser::validateId(QStringLiteral("C123")));
        QVERIFY(!BomParser::validateId(QStringLiteral("R12345")));
        QVERIFY(!BomParser::validateId(QStringLiteral("C0402")));
        QVERIFY(!BomParser::validateId(QStringLiteral("C0603")));
    }

    void parseCsvNormalizesDeduplicatesAndFiltersIds() {
        BomParser parser;
        const QString fixturePath = TestPaths::fixturePath(QStringLiteral("bom/mixed_components.csv"));

        const QStringList ids = parser.parse(fixturePath);

        QCOMPARE(ids, QStringList({QStringLiteral("C23186"), QStringLiteral("C23166"), QStringLiteral("C13564")}));
    }

    void parseCsvSkipsEmptyCellsAndReadsQuotedIds() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString filePath = createTempCsv(tempDir,
                                               QStringLiteral("quoted.csv"),
                                               {QStringLiteral("Designator,LCSC Part,Comment"),
                                                QStringLiteral("R1,,empty cell"),
                                                QStringLiteral("C1,\"c21190\",quoted lowercase"),
                                                QStringLiteral("U1,C21190,duplicate canonical"),
                                                QStringLiteral("U2,\"C14663\",quoted canonical")});

        BomParser parser;
        QCOMPARE(parser.parse(filePath), QStringList({QStringLiteral("C21190"), QStringLiteral("C14663")}));
    }

    void parseUnsupportedOrMissingFilesReturnsEmptyList() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString unsupportedPath =
            createTempCsv(tempDir, QStringLiteral("bom.json"), {QStringLiteral("{\"lcsc\":\"C12345\"}")});

        BomParser parser;
        QVERIFY(parser.parse(unsupportedPath).isEmpty());
        QVERIFY(parser.parse(tempDir.filePath(QStringLiteral("missing.csv"))).isEmpty());
    }

    void fileReaderReadBomFileUsesBomParser() {
        QString error;
        const QString fixturePath = TestPaths::fixturePath(QStringLiteral("bom/mixed_components.csv"));

        const QStringList ids = FileReader::readBomFile(fixturePath, error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(ids, QStringList({QStringLiteral("C23186"), QStringLiteral("C23166"), QStringLiteral("C13564")}));
    }

    void fileReaderReadBomFileReportsNoValidIds() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString filePath = createTempCsv(tempDir,
                                               QStringLiteral("invalid.csv"),
                                               {QStringLiteral("Designator,LCSC Part"),
                                                QStringLiteral("R1,C0402"),
                                                QStringLiteral("C1,C123"),
                                                QStringLiteral("U1,NOT_AN_ID")});

        QString error;
        const QStringList ids = FileReader::readBomFile(filePath, error);

        QVERIFY(ids.isEmpty());
        QVERIFY(error.contains(QStringLiteral("BOM 表中没有找到有效的元器件编号")));
    }

    void fileReaderReadBomFileReportsMissingInput() {
        QString error;
        const QStringList ids = FileReader::readBomFile(QStringLiteral("/nonexistent/bom.csv"), error);

        QVERIFY(ids.isEmpty());
        QVERIFY(error.contains(QStringLiteral("输入文件不存在")));
    }
};

QTEST_GUILESS_MAIN(TestBomParser)
#include "test_bom_parser.moc"
