#include "models/ComponentData.h"
#include "services/export/ExportTypeStage.h"
#include "services/export/FootprintExportStage.h"
#include "services/export/SymbolExportStage.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest/QtTest>

namespace EasyKiConverter {

class ImmediateSuccessStage final : public ExportTypeStage {
public:
    ImmediateSuccessStage() : ExportTypeStage("ImmediateSuccess", 1, nullptr) {}

protected:
    QObject* createWorker() override {
        return new QObject();
    }

    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override {
        Q_UNUSED(data);
        completeItemProgress(worker, componentId, true);
        delete worker;
    }
};

class DeferredStage final : public ExportTypeStage {
public:
    DeferredStage() : ExportTypeStage("Deferred", 1, nullptr) {}

protected:
    QObject* createWorker() override {
        return new QObject();
    }

    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override {
        Q_UNUSED(data);
        QTimer::singleShot(0, worker, [this, worker, componentId]() {
            completeItemProgress(worker, componentId, false, QStringLiteral("Cancelled"));
            worker->deleteLater();
        });
    }
};

class FailStage final : public ExportTypeStage {
public:
    FailStage() : ExportTypeStage("Fail", 1, nullptr) {}

protected:
    QObject* createWorker() override {
        return new QObject();
    }

    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override {
        Q_UNUSED(data);
        completeItemProgress(worker, componentId, false, QStringLiteral("Export failed: %1").arg(componentId));
        delete worker;
    }
};

class ConcurrentStage final : public ExportTypeStage {
public:
    explicit ConcurrentStage(int maxConcurrent) : ExportTypeStage("Concurrent", maxConcurrent, nullptr) {}

    int maxStartedWorkers() const {
        return m_maxStartedWorkers;
    }

protected:
    QObject* createWorker() override {
        return new QObject();
    }

    void startWorker(QObject* worker, const QString& componentId, const QSharedPointer<ComponentData>& data) override {
        Q_UNUSED(data);
        Q_UNUSED(componentId);
        m_startedWorkers++;
        m_maxStartedWorkers = qMax(m_maxStartedWorkers, m_startedWorkers);
        // 使用 deferred 完成，以便观察并发限制
        QTimer::singleShot(50, worker, [this, worker, componentId]() {
            m_startedWorkers--;
            completeItemProgress(worker, componentId, true);
            worker->deleteLater();
        });
    }

private:
    int m_startedWorkers = 0;
    int m_maxStartedWorkers = 0;
};

class TestExportTypeStage : public QObject {
    Q_OBJECT

private slots:

    void emitsItemStatusForEveryComponentIncludingLast() {
        ImmediateSuccessStage stage;
        QSignalSpy itemSpy(&stage, &ExportTypeStage::itemStatusChanged);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QStringList ids = {"C1001", "C1002"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);

        // Each component emits 2 signals: InProgress (from startNextWorker) and Success (from completeItemProgress)
        QCOMPARE(itemSpy.count(), ids.size() * 2);
        QCOMPARE(completedSpy.count(), 1);

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, ids.size());
        QCOMPARE(progress.completedCount, ids.size());
        QCOMPARE(progress.successCount, ids.size());
        QCOMPARE(progress.failedCount, 0);
    }

    void completedSignalSeesStageNotRunning() {
        ImmediateSuccessStage stage;
        bool wasRunningWhenCompleted = true;
        connect(&stage, &ExportTypeStage::completed, &stage, [&stage, &wasRunningWhenCompleted]() {
            wasRunningWhenCompleted = stage.isRunning();
        });

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C1001")] = QSharedPointer<ComponentData>::create();

        stage.start({QStringLiteral("C1001")}, cachedData);

        QVERIFY(!wasRunningWhenCompleted);
        QVERIFY(!stage.isRunning());
    }

    void cancelledStageStaysRunningUntilActiveWorkersDrain() {
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QStringList ids = {"C1001", "C1002", "C1003"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);
        stage.cancel();

        QVERIFY(stage.isRunning());
        QVERIFY2(completedSpy.wait(1000), "Cancelled stage should finish when active workers drain");
        QVERIFY(!stage.isRunning());
        QCOMPARE(completedSpy.count(), 1);
    }

    void footprintLibraryExportPreservesExistingFilesWhenNotOverwriting() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString libName = QStringLiteral("RegressionLib");
        const QString prettyDir = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty");
        QVERIFY(QDir().mkpath(prettyDir));

        const QString oldFootprintPath = prettyDir + QDir::separator() + QStringLiteral("OldPackage.kicad_mod");
        QFile oldFootprint(oldFootprintPath);
        QVERIFY(oldFootprint.open(QIODevice::WriteOnly | QIODevice::Text));
        oldFootprint.write("(footprint easykiconverter:OldPackage)\n");
        oldFootprint.close();

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.overwriteExistingFiles = false;
        stage.setOptions(options);

        const QString componentId = QStringLiteral("C2040");
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, QStringLiteral("NewPackage"));

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        QVERIFY2(QFile::exists(oldFootprintPath), "Existing footprint should be preserved");
        QVERIFY(QFile::exists(prettyDir + QDir::separator() + QStringLiteral("NewPackage.kicad_mod")));
    }

    void footprintLibraryExportCanUseAbsolute3DModelPaths() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString libName = QStringLiteral("PathModeLib");
        const QString componentId = QStringLiteral("C3001");
        const QString footprintName = QStringLiteral("PackageWith3D");
        const QString modelName = QStringLiteral("ModelWith3D");

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.exportModel3DPathMode = ExportOptions::MODEL_3D_PATH_ABSOLUTE;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, footprintName, modelName);

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");
        QCOMPARE(completedSpy.count(), 1);

        const QString footprintPath = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty") +
                                      QDir::separator() + footprintName + QStringLiteral(".kicad_mod");
        QFile footprintFile(footprintPath);
        QVERIFY(footprintFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(footprintFile.readAll());

        const QString expectedModelPath =
            QDir::cleanPath(QDir(tempDir.path())
                                .absoluteFilePath(libName + QStringLiteral(".3dmodels") + QDir::separator() +
                                                  modelName + QStringLiteral(".wrl")));
        QVERIFY2(content.contains(QStringLiteral("(model \"%1\"").arg(expectedModelPath)),
                 "Footprint should reference the absolute 3D model path");
        QVERIFY2(!content.contains(QStringLiteral("../%1.3dmodels/%2.wrl").arg(libName, modelName)),
                 "Footprint should not contain a relative 3D model path in absolute mode");
    }

    void footprintLibraryExportUsesRelative3DModelPathsByDefault() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString libName = QStringLiteral("RelPathLib");
        const QString componentId = QStringLiteral("C4001");
        const QString footprintName = QStringLiteral("RelPackage");
        const QString modelName = QStringLiteral("RelModel");

        FootprintExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = libName;
        options.exportModel3DFormat = ExportOptions::MODEL_3D_FORMAT_WRL;
        options.exportModel3DPathMode = ExportOptions::MODEL_3D_PATH_RELATIVE;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[componentId] = makeFootprintComponent(componentId, footprintName, modelName);

        QSignalSpy completedSpy(&stage, &FootprintExportStage::completed);
        stage.start({componentId}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Footprint export should complete");

        const QString footprintPath = tempDir.path() + QDir::separator() + libName + QStringLiteral(".pretty") +
                                      QDir::separator() + footprintName + QStringLiteral(".kicad_mod");
        QFile footprintFile(footprintPath);
        QVERIFY(footprintFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(footprintFile.readAll());

        const QString expectedWrlPath = QStringLiteral("../%1.3dmodels/%2.wrl").arg(libName, modelName);
        QVERIFY2(content.contains(QStringLiteral("(model \"%1\"").arg(expectedWrlPath)),
                 "Footprint should contain relative WRL model path");

        const QString absolutePrefix = QDir::cleanPath(tempDir.path());
        QVERIFY2(!content.contains(absolutePrefix), "Footprint should not contain absolute paths in relative mode");
    }

    void symbolLibraryExportMergesMultipleComponentsIntoOneLibrary() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        SymbolExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("MergedSymbols");
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C5001")] = makeSymbolComponent(QStringLiteral("C5001"), QStringLiteral("SYM_A"));
        cachedData[QStringLiteral("C5002")] = makeSymbolComponent(QStringLiteral("C5002"), QStringLiteral("SYM_B"));

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C5001"), QStringLiteral("C5002")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 2);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);

        const QString symbolPath = tempDir.path() + QDir::separator() + QStringLiteral("MergedSymbols.kicad_sym");
        QFile symbolFile(symbolPath);
        QVERIFY(symbolFile.open(QIODevice::ReadOnly | QIODevice::Text));
        const QString content = QString::fromUtf8(symbolFile.readAll());
        QVERIFY(content.contains(QStringLiteral("(symbol \"SYM_A\"")));
        QVERIFY(content.contains(QStringLiteral("(symbol \"SYM_B\"")));
        QVERIFY(!QDir(tempDir.path() + QDir::separator() + QStringLiteral(".tmp")).exists());
    }

    void symbolLibraryExportReportsMissingSymbolData() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        SymbolExportStage stage;
        ExportOptions options;
        options.outputPath = tempDir.path();
        options.libName = QStringLiteral("MissingSymbol");
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C6001")] = QSharedPointer<ComponentData>::create();

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        QSignalSpy itemSpy(&stage, &SymbolExportStage::itemStatusChanged);
        stage.start({QStringLiteral("C6001")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
        QVERIFY(!QFile::exists(tempDir.path() + QDir::separator() + QStringLiteral("MissingSymbol.kicad_sym")));

        QVERIFY(itemSpy.count() >= 1);
        const auto args = itemSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QStringLiteral("C6001"));
        const auto status = qvariant_cast<ExportItemStatus>(args.at(1));
        QCOMPARE(status.status, ExportItemStatus::Status::Failed);
        QCOMPARE(status.errorMessage, QStringLiteral("No symbol data"));
    }

    void symbolLibraryExportRollsBackTempFileOnCommitFailure() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString blockerPath = tempDir.path() + QDir::separator() + QStringLiteral("blocked");
        QFile blocker(blockerPath);
        QVERIFY(blocker.open(QIODevice::WriteOnly | QIODevice::Text));
        blocker.write("not a directory");
        blocker.close();

        SymbolExportStage stage;
        ExportOptions options;
        options.outputPath = blockerPath;
        options.libName = QStringLiteral("RollbackSymbols");
        options.overwriteExistingFiles = true;
        stage.setOptions(options);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C7001")] = makeSymbolComponent(QStringLiteral("C7001"), QStringLiteral("SYM_FAIL"));

        QSignalSpy completedSpy(&stage, &SymbolExportStage::completed);
        stage.start({QStringLiteral("C7001")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "Symbol export should complete");
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 1);
        QVERIFY(!QFile::exists(blockerPath + QDir::separator() + QStringLiteral("RollbackSymbols.kicad_sym")));
        QVERIFY(!QDir(blockerPath + QDir::separator() + QStringLiteral(".tmp")).exists());
    }

    // === 新增：ExportTypeStage 基类行为测试 ===

    void emptyListComponentpletesImmediately() {
        ImmediateSuccessStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        stage.start(QStringList(), cachedData);

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);  // successCount
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 0);  // failedCount
        QCOMPARE(completedSpy.at(0).at(2).toInt(), 0);  // skippedCount
        QVERIFY(!stage.isRunning());
    }

    void duplicateStartWhileRunningIsIgnored() {
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        cachedData[QStringLiteral("C8001")] = QSharedPointer<ComponentData>::create();
        cachedData[QStringLiteral("C8002")] = QSharedPointer<ComponentData>::create();

        stage.start({QStringLiteral("C8001")}, cachedData);
        QVERIFY(stage.isRunning());

        // 运行中再次 start 应被忽略
        stage.start({QStringLiteral("C8002")}, cachedData);

        QVERIFY2(completedSpy.wait(3000), "First start should complete");
        QCOMPARE(completedSpy.count(), 1);  // 只有 1 次 completed

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 1);  // 只有 C8001，C8002 被忽略
    }

    void failedComponentIncrementsFailedCount() {
        FailStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);
        QSignalSpy itemSpy(&stage, &ExportTypeStage::itemStatusChanged);

        QStringList ids = {"C9001", "C9002", "C9003"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 0);  // successCount
        QCOMPARE(completedSpy.at(0).at(1).toInt(), 3);  // failedCount

        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.totalCount, 3);
        QCOMPARE(progress.completedCount, 3);
        QCOMPARE(progress.successCount, 0);
        QCOMPARE(progress.failedCount, 3);

        // 验证每个失败项都有错误信息
        int failedSignals = 0;
        for (int i = 0; i < itemSpy.count(); i++) {
            const auto status = qvariant_cast<ExportItemStatus>(itemSpy.at(i).at(1));
            if (status.status == ExportItemStatus::Status::Failed) {
                QVERIFY(!status.errorMessage.isEmpty());
                failedSignals++;
            }
        }
        QCOMPARE(failedSignals, 3);
    }

    void concurrencyLimitIsRespected() {
        ConcurrentStage stage(2);
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QStringList ids = {"CA001", "CA002", "CA003", "CA004", "CA005"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);

        QVERIFY2(completedSpy.wait(5000), "All components should complete");
        QCOMPARE(completedSpy.at(0).at(0).toInt(), 5);  // all success

        // 并发限制为 2，任何时候不应超过 2 个 worker 同时运行
        QVERIFY2(stage.maxStartedWorkers() <= 2,
                 qPrintable(QString("Max concurrent workers was %1, expected <= 2").arg(stage.maxStartedWorkers())));
    }

    void cancelStopsAfterActiveWorkersDrain() {
        DeferredStage stage;
        QSignalSpy completedSpy(&stage, &ExportTypeStage::completed);

        QStringList ids = {"CB001", "CB002", "CB003", "CB004", "CB005"};
        QMap<QString, QSharedPointer<ComponentData>> cachedData;
        for (const QString& id : ids) {
            cachedData[id] = QSharedPointer<ComponentData>::create();
        }

        stage.start(ids, cachedData);
        stage.cancel();

        QVERIFY2(completedSpy.wait(3000), "Cancelled stage should complete");
        QVERIFY(!stage.isRunning());

        // cancel() 清空队列，仅活跃 worker（1个）会完成
        const auto args = completedSpy.at(0);
        int success = args.at(0).toInt();
        int failed = args.at(1).toInt();
        QCOMPARE(success, 0);
        QCOMPARE(failed, 1);  // 仅活跃的 1 个 worker 标记为 failed

        // pending 队列被清空，completedCount 仅反映实际处理的
        const ExportTypeProgress progress = stage.getProgress();
        QCOMPARE(progress.completedCount, 1);
    }

private:
    static QSharedPointer<ComponentData> makeSymbolComponent(const QString& componentId, const QString& symbolName) {
        auto componentData = QSharedPointer<ComponentData>::create();
        componentData->setLcscId(componentId);

        auto symbolData = QSharedPointer<SymbolData>::create();
        SymbolInfo info;
        info.name = symbolName;
        info.prefix = QStringLiteral("U");
        info.package = QStringLiteral("PKG_%1").arg(componentId);
        symbolData->setInfo(info);
        symbolData->setBbox({0, 0, 0, 0});

        componentData->setSymbolData(symbolData);
        return componentData;
    }

    static QSharedPointer<ComponentData> makeFootprintComponent(const QString& componentId,
                                                                const QString& footprintName,
                                                                const QString& model3DName = QString()) {
        auto componentData = QSharedPointer<ComponentData>::create();
        componentData->setLcscId(componentId);

        auto footprintData = QSharedPointer<FootprintData>::create();
        FootprintInfo info;
        info.name = footprintName;
        info.type = QStringLiteral("smd");
        footprintData->setInfo(info);

        FootprintBBox bbox;
        bbox.x = 0;
        bbox.y = 0;
        bbox.width = 1;
        bbox.height = 1;
        footprintData->setBbox(bbox);

        if (!model3DName.isEmpty()) {
            Model3DData model3D;
            model3D.setName(model3DName);
            model3D.setUuid(QStringLiteral("uuid-%1").arg(componentId));
            footprintData->setModel3D(model3D);
        }

        componentData->setFootprintData(footprintData);
        return componentData;
    }
};

}  // namespace EasyKiConverter

QTEST_MAIN(EasyKiConverter::TestExportTypeStage)
#include "test_export_type_stage.moc"
