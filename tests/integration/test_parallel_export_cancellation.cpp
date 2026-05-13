#include "core/easyeda/EasyedaFootprintImporter.h"
#include "core/easyeda/EasyedaSymbolImporter.h"
#include "models/ComponentData.h"
#include "services/export/ParallelExportService.h"
#include "tests/common/TestPaths.hpp"

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

    void testCancellationStopsRunningExportPipeline() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ParallelExportService service;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("CancelledPipeline");
        options.exportSymbol = true;
        options.exportFootprint = true;
        options.exportModel3D = false;
        options.exportPreviewImages = false;
        options.exportDatasheet = false;
        options.overwriteExistingFiles = true;
        service.setOptions(options);
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

        QSignalSpy cancelledSpy(&service, &ParallelExportService::cancelled);
        QSignalSpy completedSpy(&service, &ParallelExportService::completed);
        QSignalSpy failedSpy(&service, &ParallelExportService::failed);

        service.startExport();
        service.cancelExport();

        QCOMPARE(cancelledSpy.count(), 1);
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Cancelled);
        QVERIFY(!service.isRunning());

        QTest::qWait(100);
        QCOMPARE(completedSpy.count(), 0);
        QCOMPARE(service.getProgress().currentStage, ExportOverallProgress::Stage::Cancelled);
    }

private:
    static QStringList makeComponentIds(int count) {
        QStringList ids;
        ids.reserve(count);
        for (int i = 0; i < count; ++i) {
            ids.append(QStringLiteral("C9%1").arg(10000 + i));
        }
        return ids;
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
