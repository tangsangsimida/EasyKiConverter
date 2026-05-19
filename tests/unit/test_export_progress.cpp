#include "services/export/ExportProgress.h"

#include <QtTest/QtTest>

namespace EasyKiConverter {

class TestExportProgress : public QObject {
    Q_OBJECT

private slots:

    // === ExportOptions 测试 ===

    void exportOptionsModel3DFormatBitmask_data() {
        QTest::addColumn<int>("format");
        QTest::addColumn<bool>("wrl");
        QTest::addColumn<bool>("step");

        QTest::newRow("none") << static_cast<int>(ExportOptions::MODEL_3D_FORMAT_NONE) << false << false;
        QTest::newRow("wrl") << static_cast<int>(ExportOptions::MODEL_3D_FORMAT_WRL) << true << false;
        QTest::newRow("step") << static_cast<int>(ExportOptions::MODEL_3D_FORMAT_STEP) << false << true;
        QTest::newRow("both") << static_cast<int>(ExportOptions::MODEL_3D_FORMAT_BOTH) << true << true;
    }

    void exportOptionsModel3DFormatBitmask() {
        QFETCH(int, format);
        QFETCH(bool, wrl);
        QFETCH(bool, step);

        ExportOptions options;
        options.exportModel3DFormat = format;
        QCOMPARE(options.needsModel3DWrl(), wrl);
        QCOMPARE(options.needsModel3DStep(), step);
    }

    void exportOptionsNormalizePathMode_data() {
        QTest::addColumn<int>("input");
        QTest::addColumn<int>("expected");

        QTest::newRow("relative") << 0 << 0;
        QTest::newRow("absolute") << 1 << 1;
        QTest::newRow("out-of-range") << 2 << 0;
        QTest::newRow("negative") << -1 << 0;
    }

    void exportOptionsNormalizePathMode() {
        QFETCH(int, input);
        QFETCH(int, expected);
        QCOMPARE(ExportOptions::normalizePathMode(input), expected);
    }

    // === ExportItemStatus 测试 ===

    void itemStatusIsComplete_data() {
        QTest::addColumn<ExportItemStatus::Status>("status");
        QTest::addColumn<bool>("expected");

        QTest::newRow("Pending") << ExportItemStatus::Status::Pending << false;
        QTest::newRow("InProgress") << ExportItemStatus::Status::InProgress << false;
        QTest::newRow("Success") << ExportItemStatus::Status::Success << true;
        QTest::newRow("Failed") << ExportItemStatus::Status::Failed << true;
        QTest::newRow("Skipped") << ExportItemStatus::Status::Skipped << true;
    }

    void itemStatusIsComplete() {
        QFETCH(ExportItemStatus::Status, status);
        QFETCH(bool, expected);

        ExportItemStatus item;
        item.status = status;
        QCOMPARE(item.isComplete(), expected);
    }

    void itemStatusIsSuccess() {
        ExportItemStatus item;
        item.status = ExportItemStatus::Status::Success;
        QVERIFY(item.isSuccess());

        item.status = ExportItemStatus::Status::Failed;
        QVERIFY(!item.isSuccess());

        item.status = ExportItemStatus::Status::Pending;
        QVERIFY(!item.isSuccess());
    }

    void itemStatusDurationMs() {
        ExportItemStatus item;

        // 无效时间返回 0
        QCOMPARE(item.durationMs(), 0);

        // 有效时间返回正确差值
        item.startTime = QDateTime::fromString("2026-01-01T00:00:00", Qt::ISODate);
        item.endTime = QDateTime::fromString("2026-01-01T00:00:02", Qt::ISODate);
        QCOMPARE(item.durationMs(), 2000);

        // 只有 startTime 无效
        ExportItemStatus item2;
        item2.endTime = QDateTime::currentDateTime();
        QCOMPARE(item2.durationMs(), 0);
    }

    void itemStatusPercentage_data() {
        QTest::addColumn<qint64>("processed");
        QTest::addColumn<qint64>("total");
        QTest::addColumn<int>("expected");

        QTest::newRow("zero-total") << qint64(0) << qint64(0) << 0;
        QTest::newRow("half") << qint64(50) << qint64(100) << 50;
        QTest::newRow("complete") << qint64(100) << qint64(100) << 100;
        QTest::newRow("not-started") << qint64(0) << qint64(100) << 0;
    }

    void itemStatusPercentage() {
        QFETCH(qint64, processed);
        QFETCH(qint64, total);
        QFETCH(int, expected);

        ExportItemStatus item;
        item.bytesProcessed = processed;
        item.totalBytes = total;
        QCOMPARE(item.percentage(), expected);
    }

    // === ExportTypeProgress 测试 ===

    void typeProgressPercentage() {
        ExportTypeProgress progress;
        QCOMPARE(progress.percentage(), 0);

        progress.totalCount = 10;
        progress.completedCount = 3;
        QCOMPARE(progress.percentage(), 30);

        progress.completedCount = 10;
        QCOMPARE(progress.percentage(), 100);
    }

    void typeProgressIsComplete() {
        ExportTypeProgress progress;
        QVERIFY(progress.isComplete());  // 0 >= 0

        progress.totalCount = 5;
        QVERIFY(!progress.isComplete());

        progress.completedCount = 5;
        QVERIFY(progress.isComplete());

        progress.completedCount = 6;  // 超过也是完成
        QVERIFY(progress.isComplete());
    }

    // === PreloadProgress 测试 ===

    void preloadProgressPercentage() {
        PreloadProgress progress;
        QCOMPARE(progress.percentage(), 0);

        progress.totalCount = 4;
        progress.completedCount = 1;
        QCOMPARE(progress.percentage(), 25);
    }

    void preloadProgressIsComplete() {
        PreloadProgress progress;
        QVERIFY(progress.isComplete());  // 0 >= 0

        progress.totalCount = 3;
        QVERIFY(!progress.isComplete());

        progress.completedCount = 3;
        QVERIFY(progress.isComplete());
    }

    // === ExportOverallProgress 测试 ===

    void overallProgressIdleReturnsZero() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Idle;
        QCOMPARE(progress.overallPercentage(), 0);
    }

    void overallProgressCompletedReturns100() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Completed;
        QCOMPARE(progress.overallPercentage(), 100);
    }

    void overallProgressCancelledFailedReturnsZero() {
        ExportOverallProgress progress;

        progress.currentStage = ExportOverallProgress::Stage::Cancelled;
        QCOMPARE(progress.overallPercentage(), 0);

        progress.currentStage = ExportOverallProgress::Stage::Failed;
        QCOMPARE(progress.overallPercentage(), 0);
    }

    void overallProgressPreloadingReturnsPreloadPercentage() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Preloading;
        progress.preloadProgress.totalCount = 10;
        progress.preloadProgress.completedCount = 4;
        QCOMPARE(progress.overallPercentage(), 40);
    }

    void overallProgressExportingAggregatesTypes() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;

        ExportTypeProgress symbol;
        symbol.totalCount = 10;
        symbol.completedCount = 5;
        progress.exportTypeProgress[QStringLiteral("Symbol")] = symbol;

        ExportTypeProgress footprint;
        footprint.totalCount = 10;
        footprint.completedCount = 10;
        progress.exportTypeProgress[QStringLiteral("Footprint")] = footprint;

        // total: 20, completed: 15 => 75%
        QCOMPARE(progress.overallPercentage(), 75);
    }

    void overallProgressExportingEmptyReturnsZero() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;
        QCOMPARE(progress.overallPercentage(), 0);
    }

    void overallProgressIsComplete_data() {
        QTest::addColumn<ExportOverallProgress::Stage>("stage");
        QTest::addColumn<bool>("expected");

        QTest::newRow("Idle") << ExportOverallProgress::Stage::Idle << false;
        QTest::newRow("Completed") << ExportOverallProgress::Stage::Completed << true;
        QTest::newRow("Cancelled") << ExportOverallProgress::Stage::Cancelled << true;
        QTest::newRow("Failed") << ExportOverallProgress::Stage::Failed << true;
    }

    void overallProgressIsComplete() {
        QFETCH(ExportOverallProgress::Stage, stage);
        QFETCH(bool, expected);

        ExportOverallProgress progress;
        progress.currentStage = stage;
        QCOMPARE(progress.isComplete(), expected);
    }

    void overallProgressPreloadingCompleteWhenEmpty() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Preloading;
        // 空的 preloadProgress（totalCount=0）视为完成
        QVERIFY(progress.isComplete());
    }

    void overallProgressPreloadingNotCompleteWhenPending() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Preloading;
        progress.preloadProgress.totalCount = 5;
        progress.preloadProgress.completedCount = 3;
        QVERIFY(!progress.isComplete());
    }

    void overallProgressExportingCompleteWhenEmpty() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;
        // 空的 exportTypeProgress 视为完成
        QVERIFY(progress.isComplete());
    }

    void overallProgressIsCompleteWhenAllTypesDone() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;

        ExportTypeProgress type1;
        type1.totalCount = 5;
        type1.completedCount = 5;
        progress.exportTypeProgress[QStringLiteral("A")] = type1;

        ExportTypeProgress type2;
        type2.totalCount = 3;
        type2.completedCount = 3;
        progress.exportTypeProgress[QStringLiteral("B")] = type2;

        QVERIFY(progress.isComplete());
    }

    void overallProgressIsNotCompleteWhenAnyTypePending() {
        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;

        ExportTypeProgress type1;
        type1.totalCount = 5;
        type1.completedCount = 5;
        progress.exportTypeProgress[QStringLiteral("A")] = type1;

        ExportTypeProgress type2;
        type2.totalCount = 3;
        type2.completedCount = 2;
        progress.exportTypeProgress[QStringLiteral("B")] = type2;

        QVERIFY(!progress.isComplete());
    }

    void overallProgressTotalSuccessAndFailedCounts() {
        ExportOverallProgress progress;

        ExportTypeProgress type1;
        type1.successCount = 8;
        type1.failedCount = 2;
        progress.exportTypeProgress[QStringLiteral("Symbol")] = type1;

        ExportTypeProgress type2;
        type2.successCount = 5;
        type2.failedCount = 0;
        progress.exportTypeProgress[QStringLiteral("Footprint")] = type2;

        QCOMPARE(progress.totalSuccessCount(), 13);
        QCOMPARE(progress.totalFailedCount(), 2);
    }

    void overallProgressEmptyTypesReturnsZeroCounts() {
        ExportOverallProgress progress;
        QCOMPARE(progress.totalSuccessCount(), 0);
        QCOMPARE(progress.totalFailedCount(), 0);
    }
};

}  // namespace EasyKiConverter

QTEST_GUILESS_MAIN(EasyKiConverter::TestExportProgress)
#include "test_export_progress.moc"
