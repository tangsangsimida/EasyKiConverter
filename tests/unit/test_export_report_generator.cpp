#include "core/network/NetworkClient.h"
#include "services/export/ExportProgress.h"
#include "services/export/ExportReportGenerator.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace EasyKiConverter;

class TestExportReportGenerator : public QObject {
    Q_OBJECT

private slots:

    void cleanupTestCase() {
        NetworkClient::destroyInstance();
    }

    void debugModeDisabledDoesNotWriteReport() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ExportOptions options = makeOptions(tempDir.path());
        options.debugMode = false;

        ExportReportGenerator::writeDetailedReport(QStringLiteral("export-completed"), options, makeProgress());

        QVERIFY(!QFile::exists(reportPath(tempDir.path())));
    }

    void emptyOutputPathDoesNotWriteReport() {
        ExportOptions options = makeOptions(QString());
        options.debugMode = true;

        ExportReportGenerator::writeDetailedReport(QStringLiteral("export-completed"), options, makeProgress());

        QVERIFY(options.outputPath.isEmpty());
    }

    void detailedReportCreatesOutputDirectoryAndWritesProgressSnapshot() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString outputPath = QDir(tempDir.path()).filePath(QStringLiteral("nested/reports"));
        ExportOptions options = makeOptions(outputPath);
        const ExportOverallProgress progress = makeProgress();

        ExportReportGenerator::writeDetailedReport(QStringLiteral("export-failed-test"), options, progress);

        const QString content = readText(reportPath(outputPath));
        QVERIFY(content.contains(QStringLiteral("# EasyKiConverter Export Detailed Report")));
        QVERIFY(content.contains(QStringLiteral("- Reason: export-failed-test")));
        QVERIFY(content.contains(QStringLiteral("- Weak network support: enabled")));
        QVERIFY(content.contains(QStringLiteral("- Output path: %1").arg(outputPath)));
        QVERIFY(content.contains(QStringLiteral("- Total components: 3")));

        QVERIFY(content.contains(QStringLiteral("## Export Options")));
        QVERIFY(content.contains(QStringLiteral("- Library name: ReportLib")));
        QVERIFY(content.contains(QStringLiteral("- Export symbol: yes")));
        QVERIFY(content.contains(QStringLiteral("- Export footprint: yes")));
        QVERIFY(content.contains(QStringLiteral("- Export 3D model: no")));
        QVERIFY(content.contains(QStringLiteral("- Export preview images: yes")));
        QVERIFY(content.contains(QStringLiteral("- Export datasheet: no")));
        QVERIFY(content.contains(QStringLiteral("- Overwrite existing files: yes")));
        QVERIFY(content.contains(QStringLiteral("- Update mode: yes")));
        QVERIFY(content.contains(QStringLiteral("- Debug mode: yes")));

        QVERIFY(content.contains(QStringLiteral("## Preload")));
        QVERIFY(content.contains(QStringLiteral("- Completed: 2/3")));
        QVERIFY(content.contains(QStringLiteral("- Success: 2")));
        QVERIFY(content.contains(QStringLiteral("- Failed: 1")));
        QVERIFY(content.contains(QStringLiteral("- In progress: 0")));
        QVERIFY(content.contains(QStringLiteral("### Preload Failures")));
        QVERIFY(content.contains(QStringLiteral("- `C404`: Not found")));

        QVERIFY(content.contains(QStringLiteral("### Symbol")));
        QVERIFY(content.contains(QStringLiteral("- Completed: 3/3")));
        QVERIFY(content.contains(QStringLiteral("- Skipped: 1")));
        QVERIFY(content.contains(QStringLiteral("### Footprint")));
        QVERIFY(content.contains(QStringLiteral("- Completed: 2/3")));

        QVERIFY(content.contains(QStringLiteral("## Weak Network Diagnostics")));
        QVERIFY(content.contains(QStringLiteral("NetworkRuntimeStats total{")));
    }

private:
    static ExportOptions makeOptions(const QString& outputPath) {
        ExportOptions options;
        options.outputPath = outputPath;
        options.libName = QStringLiteral("ReportLib");
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = false;
        options.exportPreviewImages = true;
        options.exportDatasheet = false;
        options.overwriteExistingFiles = true;
        options.updateMode = true;
        options.weakNetworkSupport = true;
        options.debugMode = true;
        return options;
    }

    static ExportOverallProgress makeProgress() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Failed;
        progress.totalComponents = 3;
        progress.startTime = QDateTime::fromString(QStringLiteral("2026-05-12T10:00:00"), Qt::ISODate);
        progress.endTime = QDateTime::fromString(QStringLiteral("2026-05-12T10:01:00"), Qt::ISODate);

        progress.preloadProgress.totalCount = 3;
        progress.preloadProgress.completedCount = 2;
        progress.preloadProgress.successCount = 2;
        progress.preloadProgress.failedCount = 1;
        progress.preloadProgress.inProgressCount = 0;
        progress.preloadProgress.failedComponents.insert(QStringLiteral("C404"), QStringLiteral("Not found"));

        ExportTypeProgress symbolProgress;
        symbolProgress.typeName = QStringLiteral("Symbol");
        symbolProgress.totalCount = 3;
        symbolProgress.completedCount = 3;
        symbolProgress.successCount = 2;
        symbolProgress.failedCount = 0;
        symbolProgress.skippedCount = 1;
        symbolProgress.inProgressCount = 0;
        progress.exportTypeProgress.insert(QStringLiteral("Symbol"), symbolProgress);

        ExportTypeProgress footprintProgress;
        footprintProgress.typeName = QStringLiteral("Footprint");
        footprintProgress.totalCount = 3;
        footprintProgress.completedCount = 2;
        footprintProgress.successCount = 1;
        footprintProgress.failedCount = 1;
        footprintProgress.skippedCount = 0;
        footprintProgress.inProgressCount = 1;
        progress.exportTypeProgress.insert(QStringLiteral("Footprint"), footprintProgress);

        return progress;
    }

    static QString reportPath(const QString& outputPath) {
        return QDir(outputPath).filePath(QStringLiteral("easykiconverter_export_detailed_report.md"));
    }

    static QString readText(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTest::qFail(qPrintable(QStringLiteral("Unable to open file '%1': %2").arg(path, file.errorString())),
                         __FILE__,
                         __LINE__);
            return {};
        }

        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        return in.readAll();
    }
};

QTEST_GUILESS_MAIN(TestExportReportGenerator)
#include "test_export_report_generator.moc"
