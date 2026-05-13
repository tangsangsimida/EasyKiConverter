#include "services/export/ExportWorkerHelpers.h"

#include "models/ComponentData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestExportWorkerHelpers : public QObject {
    Q_OBJECT

private slots:

    void defaultOutputDirAppendsSubdir() {
        const QString path = ExportWorkerHelpers::defaultOutputDir(QStringLiteral("symbols"));
        QVERIFY(path.contains(QStringLiteral("/export/symbols")));
    }

    void buildFilePathConcatenates() {
        const QString path = ExportWorkerHelpers::buildFilePath(
            QStringLiteral("C12345"), QStringLiteral("/tmp/out"), QStringLiteral(".kicad_sym"));
        QCOMPARE(path, QStringLiteral("/tmp/out/C12345.kicad_sym"));
    }

    void ensureOutputDirCreatesAndReturnsPath() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ExportOptions options;
        options.outputPath = tempDir.path();

        const QString result = ExportWorkerHelpers::ensureOutputDir(options, QStringLiteral("testdir"));
        QVERIFY(!result.isEmpty());
        QVERIFY(result.startsWith(tempDir.path()));
        QVERIFY(QDir(result).exists());
    }

    void ensureOutputDirFallsBackToDefaultWhenOutputPathEmpty() {
        ExportOptions options;  // outputPath empty
        const QString result = ExportWorkerHelpers::ensureOutputDir(options, QStringLiteral("sub"));
        QVERIFY(!result.isEmpty());
        QVERIFY(result.contains(QStringLiteral("/export/sub")));
    }

    void shouldSkipExistingReturnsTrueWhenFileExistsAndNotOverwriting() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString filePath = tempDir.filePath(QStringLiteral("existing.kicad_sym"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test");
        file.close();

        ExportOptions options;
        options.overwriteExistingFiles = false;
        QVERIFY(ExportWorkerHelpers::shouldSkipExisting(filePath, options));
    }

    void shouldNotSkipExistingWhenFileMissing() {
        ExportOptions options;
        options.overwriteExistingFiles = false;
        QVERIFY(!ExportWorkerHelpers::shouldSkipExisting(QStringLiteral("/nonexistent/file.kicad_sym"), options));
    }

    void shouldNotSkipExistingWhenOverwriteEnabled() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString filePath = tempDir.filePath(QStringLiteral("overwrite_me.kicad_sym"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test");
        file.close();

        ExportOptions options;
        options.overwriteExistingFiles = true;
        QVERIFY(!ExportWorkerHelpers::shouldSkipExisting(filePath, options));
    }

    void mergeComponentDataFillsEmptyTargetFromFallback() {
        auto fallback = QSharedPointer<ComponentData>::create();
        fallback->setLcscId(QStringLiteral("C12345"));
        fallback->setName(QStringLiteral("Test Resistor"));
        fallback->setPackage(QStringLiteral("0603"));
        fallback->setManufacturer(QStringLiteral("Test Corp"));
        fallback->setManufacturerPart(QStringLiteral("TEST-123"));
        fallback->setDatasheet(QStringLiteral("https://example.com/ds.pdf"));
        fallback->setDatasheetFormat(QStringLiteral("pdf"));
        fallback->setDatasheetData(QByteArrayLiteral("fake-pdf-data"));
        fallback->setPrefix(QStringLiteral("R"));

        auto symbol = QSharedPointer<SymbolData>::create();
        SymbolInfo symInfo;
        symInfo.name = QStringLiteral("RES_0603");
        symbol->setInfo(symInfo);
        fallback->setSymbolData(symbol);

        ComponentData target;
        target.setLcscId(QStringLiteral("C12345"));

        ExportWorkerHelpers::mergeComponentData(target, fallback);

        QCOMPARE(target.name(), QStringLiteral("Test Resistor"));
        QCOMPARE(target.package(), QStringLiteral("0603"));
        QCOMPARE(target.manufacturer(), QStringLiteral("Test Corp"));
        QCOMPARE(target.prefix(), QStringLiteral("R"));
        QCOMPARE(target.datasheet(), QStringLiteral("https://example.com/ds.pdf"));
        QCOMPARE(target.datasheetFormat(), QStringLiteral("pdf"));
        QCOMPARE(target.datasheetData(), QByteArrayLiteral("fake-pdf-data"));
        QVERIFY(target.symbolData());
        QCOMPARE(target.symbolData()->info().name, QStringLiteral("RES_0603"));
    }

    void mergeComponentDataPreservesExistingTargetValues() {
        auto fallback = QSharedPointer<ComponentData>::create();
        fallback->setLcscId(QStringLiteral("C12345"));
        fallback->setName(QStringLiteral("fallback name"));
        fallback->setPackage(QStringLiteral("fallback pkg"));

        ComponentData target;
        target.setLcscId(QStringLiteral("C12345"));
        target.setName(QStringLiteral("existing name"));

        ExportWorkerHelpers::mergeComponentData(target, fallback);

        QCOMPARE(target.name(), QStringLiteral("existing name"));  // Not overwritten
        QCOMPARE(target.package(), QStringLiteral("fallback pkg"));  // Was empty
    }

    void mergeComponentDataCopiesFullFallbackWhenTargetLcscIdEmpty() {
        auto fallback = QSharedPointer<ComponentData>::create();
        fallback->setLcscId(QStringLiteral("C99999"));
        fallback->setName(QStringLiteral("full name"));

        ComponentData target;  // lcscId is empty

        ExportWorkerHelpers::mergeComponentData(target, fallback);

        QCOMPARE(target.lcscId(), QStringLiteral("C99999"));
        QCOMPARE(target.name(), QStringLiteral("full name"));
    }

    void mergeComponentDataIsNoopWhenFallbackIsNull() {
        ComponentData target;
        target.setLcscId(QStringLiteral("C12345"));
        target.setName(QStringLiteral("original"));

        ExportWorkerHelpers::mergeComponentData(target, nullptr);

        QCOMPARE(target.lcscId(), QStringLiteral("C12345"));
        QCOMPARE(target.name(), QStringLiteral("original"));
    }

    void recomputeTypeProgressCountsZeroesAndRecalculates() {
        ExportTypeProgress progress;
        progress.totalCount = 3;

        ExportItemStatus s1;
        s1.status = ExportItemStatus::Status::Success;
        ExportItemStatus s2;
        s2.status = ExportItemStatus::Status::Failed;
        ExportItemStatus s3;
        s3.status = ExportItemStatus::Status::InProgress;

        progress.itemStatus[QStringLiteral("C1")] = s1;
        progress.itemStatus[QStringLiteral("C2")] = s2;
        progress.itemStatus[QStringLiteral("C3")] = s3;

        progress.successCount = 99;  // garbage
        progress.failedCount = 99;
        progress.completedCount = 99;

        ExportWorkerHelpers::recomputeTypeProgressCounts(progress);

        QCOMPARE(progress.successCount, 1);
        QCOMPARE(progress.failedCount, 1);
        QCOMPARE(progress.inProgressCount, 1);
        QCOMPARE(progress.completedCount, 2);  // Success + Failed
        QCOMPARE(progress.skippedCount, 0);
    }
};

QTEST_GUILESS_MAIN(TestExportWorkerHelpers)
#include "test_export_worker_helpers.moc"
