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

private:
    static QSharedPointer<ComponentData> makeFootprintComponent(const QString& componentId,
                                                                const QString& footprintName) {
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

        componentData->setFootprintData(footprintData);
        return componentData;
    }
};

}  // namespace EasyKiConverter

QTEST_MAIN(EasyKiConverter::TestExportTypeStage)
#include "test_export_type_stage.moc"
