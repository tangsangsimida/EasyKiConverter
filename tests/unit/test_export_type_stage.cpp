#include "models/ComponentData.h"
#include "services/export/ExportTypeStage.h"
#include "services/export/FootprintExportStage.h"

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

private:
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
