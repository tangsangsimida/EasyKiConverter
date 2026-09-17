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
    // 创建临时 CSV 文件并返回路径；失败时返回空字符串
    QString createTempCsv(QTemporaryDir& tempDir, const QString& name, const QStringList& lines) {
        const QString filePath = tempDir.filePath(name);
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "Failed to create temp CSV:" << filePath;
            return {};
        }
        QTextStream stream(&file);
        for (const QString& line : lines) {
            stream << line << "\n";
        }
        return filePath;
    }

private slots:

    // 验证元件编号校验只接受有效的 LCSC 编号。
    void validateIdAcceptsOnlyLcscComponentIds() {
        QVERIFY(BomParser::validateId(QStringLiteral("C1234")));
        QVERIFY(BomParser::validateId(QStringLiteral("c13564")));

        QVERIFY(!BomParser::validateId(QStringLiteral("C123")));
        QVERIFY(!BomParser::validateId(QStringLiteral("R12345")));
        QVERIFY(!BomParser::validateId(QStringLiteral("C0402")));
        QVERIFY(!BomParser::validateId(QStringLiteral("C0603")));
    }

    // 验证文本提取会规范化大小写、去重并过滤排除编号。
    void extractIdsFromTextNormalizesDeduplicatesAndFiltersIds() {
        const QString text = QStringLiteral("C1234 c1234 C0402 R12345 C56789");

        QCOMPARE(BomParser::extractIdsFromText(text), QStringList({QStringLiteral("C1234"), QStringLiteral("C56789")}));
    }

    // 验证 CSV 解析会规范化、去重并过滤无效编号。
    void parseCsvNormalizesDeduplicatesAndFiltersIds() {
        BomParser parser;
        const QString fixturePath = TestPaths::fixturePath(QStringLiteral("bom/mixed_components.csv"));

        const QStringList ids = parser.parse(fixturePath);

        QCOMPARE(ids, QStringList({QStringLiteral("C23186"), QStringLiteral("C23166"), QStringLiteral("C13564")}));
    }

    // 验证 CSV 解析会跳过空单元格并读取带引号的编号。
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
        QVERIFY2(!filePath.isEmpty(), "Failed to create temp CSV");

        BomParser parser;
        QCOMPARE(parser.parse(filePath), QStringList({QStringLiteral("C21190"), QStringLiteral("C14663")}));
    }

    // 验证带引号逗号字段后面的编号仍能被正确提取。
    void parseCsvPreservesIdsAfterQuotedCommaFields() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString filePath = createTempCsv(tempDir,
                                               QStringLiteral("quoted-comma.csv"),
                                               {QStringLiteral("Designator,Comment,LCSC Part"),
                                                QStringLiteral("C1,\"resistor, 1%\",C21190"),
                                                QStringLiteral("C2,\"quoted \"\"note\"\"\",C14663")});
        QVERIFY2(!filePath.isEmpty(), "Failed to create temp CSV");

        BomParser parser;
        QCOMPARE(parser.parse(filePath), QStringList({QStringLiteral("C21190"), QStringLiteral("C14663")}));
    }

    // 验证跨行引号字段不会影响后续编号解析。
    void parseCsvPreservesIdsAfterMultilineQuotedFields() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString filePath = createTempCsv(tempDir,
                                               QStringLiteral("multiline.csv"),
                                               {QStringLiteral("Designator,Comment,LCSC Part"),
                                                QStringLiteral("C1,\"first line\nsecond line\",C21190"),
                                                QStringLiteral("C2,normal,C14663")});
        QVERIFY2(!filePath.isEmpty(), "Failed to create temp CSV");

        BomParser parser;
        QCOMPARE(parser.parse(filePath), QStringList({QStringLiteral("C21190"), QStringLiteral("C14663")}));
    }

    // 验证不支持格式和缺失文件均返回空结果。
    void parseUnsupportedOrMissingFilesReturnsEmptyList() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString unsupportedPath =
            createTempCsv(tempDir, QStringLiteral("bom.json"), {QStringLiteral("{\"lcsc\":\"C12345\"}")});
        QVERIFY2(!unsupportedPath.isEmpty(), "Failed to create temp JSON");

        BomParser parser;
        QVERIFY(parser.parse(unsupportedPath).isEmpty());
        QVERIFY(parser.parse(tempDir.filePath(QStringLiteral("missing.csv"))).isEmpty());
    }

    // 验证文件读取器通过 BOM 解析器读取 CSV 编号。
    void fileReaderReadBomFileUsesBomParser() {
        QString error;
        const QString fixturePath = TestPaths::fixturePath(QStringLiteral("bom/mixed_components.csv"));

        const QStringList ids = FileReader::readBomFile(fixturePath, error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(ids, QStringList({QStringLiteral("C23186"), QStringLiteral("C23166"), QStringLiteral("C13564")}));
    }

    // 验证文件读取器能报告没有有效编号的输入文件。
    void fileReaderReadBomFileReportsNoValidIds() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString filePath = createTempCsv(tempDir,
                                               QStringLiteral("invalid.csv"),
                                               {QStringLiteral("Designator,LCSC Part"),
                                                QStringLiteral("R1,C0402"),
                                                QStringLiteral("C1,C123"),
                                                QStringLiteral("U1,NOT_AN_ID")});
        QVERIFY2(!filePath.isEmpty(), "Failed to create temp CSV");

        QString error;
        const QStringList ids = FileReader::readBomFile(filePath, error);

        QVERIFY(ids.isEmpty());
        QVERIFY(error.contains(QStringLiteral("BOM 表中没有找到有效的元器件编号")));
    }

    // 验证文件读取器能报告不存在的输入文件。
    void fileReaderReadBomFileReportsMissingInput() {
        QString error;
        const QStringList ids = FileReader::readBomFile(QStringLiteral("/nonexistent/bom.csv"), error);

        QVERIFY(ids.isEmpty());
        QVERIFY(error.contains(QStringLiteral("输入文件不存在")));
    }

    // === XLSX 测试 ===

    // 验证 XLSX 解析结果只包含有效的 LCSC 编号。
    void parseXlsxFiltersComponentIds() {
        const QString xlsxPath = TestPaths::fixturePath(QStringLiteral("bom/testbom.xlsx"));

        BomParser parser;
        const QStringList ids = parser.parse(xlsxPath);

        // 验证解析出有效的 LCSC 编号
        QVERIFY(!ids.isEmpty());
        for (const QString& id : ids) {
            QVERIFY2(BomParser::validateId(id), qPrintable(QStringLiteral("Invalid LCSC ID in XLSX: %1").arg(id)));
        }
    }

    // 验证 XLSX 解析结果已完成编号去重。
    void parseXlsxDeduplicatesIds() {
        const QString xlsxPath = TestPaths::fixturePath(QStringLiteral("bom/testbom.xlsx"));

        BomParser parser;
        const QStringList ids = parser.parse(xlsxPath);

        // 验证去重
        QSet<QString> uniqueIds(ids.begin(), ids.end());
        QCOMPARE(ids.size(), uniqueIds.size());
    }

    // 验证文件读取器能够读取 XLSX BOM 文件。
    void fileReaderReadBomFileXlsx() {
        const QString xlsxPath = TestPaths::fixturePath(QStringLiteral("bom/testbom.xlsx"));

        QString error;
        const QStringList ids = FileReader::readBomFile(xlsxPath, error);

        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!ids.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestBomParser)
#include "test_bom_parser.moc"
