#include "services/export/ExportProgress.h"
#include "services/export/ParallelExportService.h"
#include "ui/viewmodels/ExportProgressViewModel.h"

#include <QMetaType>
#include <QtTest/QtTest>

namespace EasyKiConverter {

class TestExportProgressViewModel : public QObject {
    Q_OBJECT

private slots:

    void initTestCase() {
        qRegisterMetaType<ExportOverallProgress>();
        qRegisterMetaType<ExportItemStatus>();
    }

    void stageProgressRespectsPipelineOrdering() {
        ParallelExportService service;
        ExportProgressViewModel viewModel(&service, nullptr, nullptr);

        viewModel.startExport({"C1", "C2", "C3", "C4"},
                              "/tmp/easykiconverter-test",
                              "testlib",
                              true,
                              true,
                              true,
                              0,
                              true,
                              false,
                              false,
                              false,
                              false);

        ExportOverallProgress progress;
        progress.currentStage = ExportOverallProgress::Stage::Exporting;
        progress.totalComponents = 4;

        ExportTypeProgress preview;
        preview.typeName = QStringLiteral("PreviewImages");
        preview.totalCount = 4;
        preview.completedCount = 2;
        progress.exportTypeProgress.insert(preview.typeName, preview);

        ExportTypeProgress symbol;
        symbol.typeName = QStringLiteral("Symbol");
        symbol.totalCount = 4;
        symbol.completedCount = 4;
        progress.exportTypeProgress.insert(symbol.typeName, symbol);

        ExportTypeProgress footprint;
        footprint.typeName = QStringLiteral("Footprint");
        footprint.totalCount = 4;
        footprint.completedCount = 4;
        progress.exportTypeProgress.insert(footprint.typeName, footprint);

        ExportTypeProgress model3D;
        model3D.typeName = QStringLiteral("Model3D");
        model3D.totalCount = 4;
        model3D.completedCount = 4;
        progress.exportTypeProgress.insert(model3D.typeName, model3D);

        const bool invoked = QMetaObject::invokeMethod(
            &viewModel, "handleProgressChanged", Qt::DirectConnection, Q_ARG(ExportOverallProgress, progress));
        QVERIFY(invoked);

        QCOMPARE(viewModel.fetchProgress(), 75);
        QCOMPARE(viewModel.processProgress(), 75);
        QCOMPARE(viewModel.writeProgress(), 75);
        QCOMPARE(viewModel.progress(), 75);
    }

    void itemStatusesAggregateToComponentAndTypeCounts() {
        ParallelExportService service;
        ExportProgressViewModel viewModel(&service, nullptr, nullptr);

        viewModel.startExport({"C1", "C2"},
                              "/tmp/easykiconverter-test",
                              "testlib",
                              true,
                              true,
                              true,
                              0,
                              false,
                              false,
                              false,
                              false,
                              false);

        auto makeStatus = [](ExportItemStatus::Status state, const QString& error = QString()) {
            ExportItemStatus status;
            status.status = state;
            status.errorMessage = error;
            return status;
        };

        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C1")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C1")),
                                          Q_ARG(QString, QStringLiteral("Footprint")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C1")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));

        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C2")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));
        QVERIFY(QMetaObject::invokeMethod(
            &viewModel,
            "handleItemStatusChanged",
            Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("C2")),
            Q_ARG(QString, QStringLiteral("Footprint")),
            Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Failed, QStringLiteral("broken")))));
        QVERIFY(QMetaObject::invokeMethod(&viewModel,
                                          "handleItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C2")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, makeStatus(ExportItemStatus::Status::Success))));

        QVERIFY(QMetaObject::invokeMethod(&viewModel, "flushPendingUpdates", Qt::DirectConnection));

        QCOMPARE(viewModel.successCount(), 1);
        QCOMPARE(viewModel.failureCount(), 1);
        QCOMPARE(viewModel.symbolSuccessCount(), 2);
        QCOMPARE(viewModel.footprintSuccessCount(), 1);
        QCOMPARE(viewModel.model3DSuccessCount(), 2);
        QCOMPARE(viewModel.filteredPendingCount(), 0);

        const QVariantList results = viewModel.resultsList();
        QCOMPARE(results.size(), 2);

        const QVariantMap first = results.at(0).toMap();
        QCOMPARE(first.value("status").toString(), QStringLiteral("success"));

        const QVariantMap second = results.at(1).toMap();
        QCOMPARE(second.value("status").toString(), QStringLiteral("failed"));
        QCOMPARE(second.value("footprintStatus").toString(), QStringLiteral("failed"));
        QCOMPARE(second.value("error").toString(), QStringLiteral("broken"));
    }
};

}  // namespace EasyKiConverter

QTEST_GUILESS_MAIN(EasyKiConverter::TestExportProgressViewModel)
#include "test_export_progress_viewmodel.moc"
