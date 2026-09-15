#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "models/ComponentData.h"
#include "services/export/ParallelExportService.h"
#include "tests/common/TestPaths.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;
using namespace EasyKiConverter::Test;

Q_DECLARE_METATYPE(QList<EasyKiConverter::ComponentData>)

class TestParallelExportCancellation : public QObject {
    Q_OBJECT

private slots:

    void initTestCase() {
        qRegisterMetaType<ExportOverallProgress>();
        qRegisterMetaType<ExportItemStatus>();
        qRegisterMetaType<QList<ComponentData>>();
    }

    void testFixtureDataCompletesExportPipeline() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        const QString libName = QStringLiteral("FixturePipeline");
        service.setOptions(makeOptions(tempDir.path(), libName));
        service.setOutputPath(tempDir.path());

        const QStringList componentIds = makeComponentIds(3);
        const QList<ComponentData> componentData = makeFixtureComponents(componentIds);

        service.startPreload(componentIds);

        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        const bool preloadInjected = QMetaObject::invokeMethod(
            &service, "onAllComponentDataCollected", Qt::DirectConnection, Q_ARG(QList<ComponentData>, componentData));
        QVERIFY(preloadInjected);
        QCOMPARE(preloadSpy.count(), 1);

        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy cancelledSpy(&service, &ParallelExportService::cancelled);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);

        service.startExport();

        QVERIFY2(completedSpy.wait(30000), "Parallel export should complete with fixture data");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), componentIds.size());
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(cancelledSpy.count(), 0);
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Completed);

        QString error;
        const QString symbolLibraryPath = tempDir.filePath(libName + QStringLiteral(".kicad_sym"));
        QVERIFY2(QFileInfo::exists(symbolLibraryPath), qPrintable(symbolLibraryPath));
        const QString symbolContent = TestPaths::readText(symbolLibraryPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(symbolContent.contains(QStringLiteral("(kicad_symbol_lib")));
        QVERIFY(symbolContent.contains(QStringLiteral("(symbol \"CANCEL_SYM_0\"")));
        QVERIFY(symbolContent.contains(QStringLiteral("\"FixturePipeline:CANCEL_FP_0\"")));

        const QString prettyDirPath = tempDir.filePath(libName + QStringLiteral(".pretty"));
        QVERIFY2(QDir(prettyDirPath).exists(), qPrintable(prettyDirPath));
        const QString footprintPath = QDir(prettyDirPath).filePath(QStringLiteral("CANCEL_FP_0.kicad_mod"));
        QVERIFY2(QFileInfo::exists(footprintPath), qPrintable(footprintPath));
        const QString footprintContent = TestPaths::readText(footprintPath, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(footprintContent.contains(QStringLiteral("(footprint easykiconverter:CANCEL_FP_0")));
        QVERIFY(footprintContent.contains(QStringLiteral("(pad 1 smd rect")));
    }

    void testMissingPreloadedDataFailsExportPipeline() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        const QString libName = QStringLiteral("MissingDataPipeline");
        service.setOptions(makeOptions(tempDir.path(), libName));
        service.setOutputPath(tempDir.path());

        const QStringList componentIds = makeComponentIds(2);
        const QList<ComponentData> invalidComponentData = makeInvalidComponents(componentIds);

        service.startPreload(componentIds);

        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        const bool preloadInjected = QMetaObject::invokeMethod(&service,
                                                               "onAllComponentDataCollected",
                                                               Qt::DirectConnection,
                                                               Q_ARG(QList<ComponentData>, invalidComponentData));
        QVERIFY(preloadInjected);
        QCOMPARE(preloadSpy.count(), 1);
        QCOMPARE(preloadSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(preloadSpy.at(0).at(1).toInt(), componentIds.size());
        QVERIFY(service.cachedData().isEmpty());

        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy cancelledSpy(&service, &ParallelExportService::cancelled);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);

        service.startExport();

        // Service state is set synchronously; always check first
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Failed);
        QVERIFY(!service.isRunning());
        QTest::qWait(100);
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(failedSpy.at(0).at(0).toString(), QStringLiteral("No exportable components after preload"));
        QCOMPARE(completedSpy.count(), 0);
        QCOMPARE(cancelledSpy.count(), 0);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Failed);
        QVERIFY(!service.isRunning());
        QVERIFY(!QFileInfo::exists(tempDir.filePath(libName + QStringLiteral(".kicad_sym"))));
        QVERIFY(!QDir(tempDir.filePath(libName + QStringLiteral(".pretty"))).exists());
    }

    void testItemStatusPreservesDiagnosticsAcrossUpdates() {
        ParallelExportService service;

        ExportItemStatus initialStatus;
        initialStatus.status = ExportItemStatus::Status::Success;
        initialStatus.diagnostics = {QStringLiteral("输入图元 UNKNOWN 未支持")};
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90001")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, initialStatus)));

        ExportItemStatus failedStatus;
        failedStatus.status = ExportItemStatus::Status::Failed;
        failedStatus.errorMessage = QStringLiteral("库写入失败");
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90001")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, failedStatus)));

        const ExportOverallProgress progress = service.getProgress();
        QVERIFY(progress.exportTypeProgress.contains(QStringLiteral("Symbol")));
        const ExportItemStatus finalStatus =
            progress.exportTypeProgress.value(QStringLiteral("Symbol")).itemStatus.value(QStringLiteral("C90001"));
        QCOMPARE(finalStatus.status, ExportItemStatus::Status::Failed);
        QCOMPARE(finalStatus.errorMessage, QStringLiteral("库写入失败"));
        QVERIFY(finalStatus.diagnostics.contains(QStringLiteral("输入图元 UNKNOWN 未支持")));
    }

    void testAltiumModel3DFailureIsNotOverwrittenByFootprintSuccess() {
        ParallelExportService service;
        ExportOptions options;
        options.targetFormat = TargetEdaFormat::Altium;
        options.exportModel3D = true;
        service.setOptions(options);

        ExportItemStatus modelFailure;
        modelFailure.status = ExportItemStatus::Status::Failed;
        modelFailure.errorMessage = QStringLiteral("STEP 3D model was not embedded in PcbLib");
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90003")),
                                          Q_ARG(QString, QStringLiteral("Model3D")),
                                          Q_ARG(ExportItemStatus, modelFailure)));

        ExportItemStatus footprintSuccess;
        footprintSuccess.status = ExportItemStatus::Status::Success;
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90003")),
                                          Q_ARG(QString, QStringLiteral("Footprint")),
                                          Q_ARG(ExportItemStatus, footprintSuccess)));

        const ExportTypeProgress modelProgress = service.getTypeProgress(QStringLiteral("Model3D"));
        const ExportItemStatus finalStatus = modelProgress.itemStatus.value(QStringLiteral("C90003"));
        QCOMPARE(finalStatus.status, ExportItemStatus::Status::Failed);
        QCOMPARE(finalStatus.errorMessage, modelFailure.errorMessage);
    }

    void testCancellationReportPreservesCollectedDiagnostics() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        ExportOptions options = makeOptions(tempDir.path(), QStringLiteral("CancelledReport"));
        options.debugMode = true;
        service.setOptions(options);
        service.setOutputPath(tempDir.path());

        ExportItemStatus status;
        status.status = ExportItemStatus::Status::Success;
        status.diagnostics = {QStringLiteral("输入图元 UNKNOWN 未支持")};
        QVERIFY(QMetaObject::invokeMethod(&service,
                                          "onExportItemStatusChanged",
                                          Qt::DirectConnection,
                                          Q_ARG(QString, QStringLiteral("C90002")),
                                          Q_ARG(QString, QStringLiteral("Symbol")),
                                          Q_ARG(ExportItemStatus, status)));

        service.cancelExport();
        QTest::qWait(100);

        const QString reportPath = tempDir.filePath(QStringLiteral("easykiconverter_export_detailed_report.md"));
        QVERIFY2(QFileInfo::exists(reportPath), qPrintable(reportPath));
        const QString report = TestPaths::readText(reportPath);
        QVERIFY(report.contains(QStringLiteral("export-cancelled")));
        QVERIFY(report.contains(QStringLiteral("输入图元 UNKNOWN 未支持")));
    }

    void testCancellationStopsRunningExportPipeline() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        service.setOptions(makeOptions(tempDir.path(), QStringLiteral("CancelledPipeline")));
        service.setOutputPath(tempDir.path());

        const QStringList componentIds = makeComponentIds(80);
        const QList<ComponentData> componentData = makeFixtureComponents(componentIds);
        QCOMPARE(componentData.size(), componentIds.size());

        service.startPreload(componentIds);

        QSignalSpy preloadSpy(&service, &ParallelExportService::preloadCompleted);
        const bool preloadInjected = QMetaObject::invokeMethod(
            &service, "onAllComponentDataCollected", Qt::DirectConnection, Q_ARG(QList<ComponentData>, componentData));
        QVERIFY(preloadInjected);
        QCOMPARE(preloadSpy.count(), 1);
        QCOMPARE(preloadSpy.at(0).at(0).toInt(), componentIds.size());
        QCOMPARE(preloadSpy.at(0).at(1).toInt(), 0);
        QCOMPARE(service.cachedData().size(), componentIds.size());

        QSignalSpy failedSpy(&service, &ParallelExportService::failed);
        QSignalSpy cancelledSpy(&service, &ParallelExportService::cancelled);
        QSignalSpy progressSpy(&service, &ParallelExportService::progressChanged);

        service.startExport();
        // 等待进度变化，确保导出已真正开始
        QVERIFY2(progressSpy.wait(5000), "Export did not start within 5s");
        QVERIFY(service.isRunning());

        service.cancelExport();

        // cancelExport() 无条件将状态转为 Cancelled 并发射 cancelled 信号
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(cancelledSpy.count(), 1);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Cancelled);
        QVERIFY(!service.isRunning());
    }

private:
    static ExportOptions makeOptions(const QString& outputPath, const QString& libName) {
        ExportOptions options;
        options.outputPath = outputPath;
        options.libName = libName;
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = false;
        options.exportPreviewImages = false;
        options.exportDatasheet = false;
        options.overwriteExistingFiles = true;
        return options;
    }

    static QStringList makeComponentIds(int count) {
        QStringList ids;
        ids.reserve(count);
        for (int i = 0; i < count; ++i) {
            ids.append(QStringLiteral("C9%1").arg(10000 + i));
        }
        return ids;
    }

    static QList<ComponentData> makeInvalidComponents(const QStringList& componentIds) {
        QList<ComponentData> components;
        components.reserve(componentIds.size());

        for (const QString& componentId : componentIds) {
            ComponentData component;
            component.setLcscId(componentId);
            component.setName(QStringLiteral("Invalid Fixture %1").arg(componentId));
            components.append(component);
        }

        return components;
    }

    static QList<ComponentData> makeFixtureComponents(const QStringList& componentIds) {
        QString error;
        const QJsonObject symbolFixture =
            TestPaths::readJsonObject(TestPaths::fixturePath(QStringLiteral("easyeda/symbol_basic.json")), &error);
        if (!error.isEmpty()) {
            qFatal("Unable to read symbol fixture: %s", qPrintable(error));
        }

        const QJsonObject footprintFixture =
            TestPaths::readJsonObject(TestPaths::fixturePath(QStringLiteral("easyeda/footprint_basic.json")), &error);
        if (!error.isEmpty()) {
            qFatal("Unable to read footprint fixture: %s", qPrintable(error));
        }

        EasyedaSymbolImporter symbolImporter;
        EasyedaFootprintImporter footprintImporter;
        const QSharedPointer<SymbolData> baseSymbol = symbolImporter.importSymbolData(symbolFixture);
        const QSharedPointer<FootprintData> baseFootprint = footprintImporter.importFootprintData(footprintFixture);
        if (!baseSymbol || !baseFootprint) {
            qFatal("Unable to import EasyEDA fixtures");
        }

        QList<ComponentData> components;
        components.reserve(componentIds.size());

        for (int i = 0; i < componentIds.size(); ++i) {
            const QString& componentId = componentIds.at(i);
            const QString suffix = QString::number(i);

            auto symbol = QSharedPointer<SymbolData>::create(*baseSymbol);
            SymbolInfo symbolInfo = symbol->info();
            symbolInfo.name = QStringLiteral("CANCEL_SYM_%1").arg(suffix);
            symbolInfo.package = QStringLiteral("CANCEL_FP_%1").arg(suffix);
            symbolInfo.lcscId = componentId;
            symbol->setInfo(symbolInfo);

            auto footprint = QSharedPointer<FootprintData>::create(*baseFootprint);
            FootprintInfo footprintInfo = footprint->info();
            footprintInfo.name = QStringLiteral("CANCEL_FP_%1").arg(suffix);
            footprint->setInfo(footprintInfo);

            ComponentData component;
            component.setLcscId(componentId);
            component.setName(QStringLiteral("Cancellation Fixture %1").arg(suffix));
            component.setSymbolData(symbol);
            component.setFootprintData(footprint);
            components.append(component);
        }

        return components;
    }
};

QTEST_GUILESS_MAIN(TestParallelExportCancellation)
#include "test_parallel_export_cancellation.moc"
