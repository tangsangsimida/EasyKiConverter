#include "services/export/TempFileManager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestTempFileManager : public QObject {
    Q_OBJECT

private slots:

    void symbolTempFileCommitMovesFileAndUnregistersIt() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        TempFileManager manager;
        manager.setOutputPath(tempDir.path());
        QSignalSpy commitSpy(&manager, &TempFileManager::commitCompleted);

        const QString tempPath =
            manager.createSymbolTempPath(QStringLiteral("FixtureLib"), QStringLiteral(".kicad_sym"));
        QVERIFY(!tempPath.isEmpty());
        QVERIFY(writeFile(tempPath, QByteArrayLiteral("symbol library")));

        const QString finalPath = QDir(tempDir.path()).filePath(QStringLiteral("FixtureLib.kicad_sym"));
        QVERIFY(manager.commit(finalPath));

        QCOMPARE(commitSpy.count(), 1);
        QCOMPARE(commitSpy.at(0).at(0).toString(), finalPath);
        QCOMPARE(commitSpy.at(0).at(1).toBool(), true);
        QCOMPARE(readFile(finalPath), QByteArrayLiteral("symbol library"));
        QVERIFY(!QFile::exists(tempPath));
        QVERIFY(!QDir(QDir(tempDir.path()).filePath(QStringLiteral(".tmp"))).exists());
        QVERIFY(manager.registeredTempFiles().isEmpty());
    }

    void tempDirectoryCommitMovesDirectoryAndReplacesExistingTarget() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        TempFileManager manager;
        manager.setOutputPath(tempDir.path());

        const QString finalDirPath = QDir(tempDir.path()).filePath(QStringLiteral("Fixture.pretty"));
        QVERIFY(QDir().mkpath(finalDirPath));
        QVERIFY(writeFile(QDir(finalDirPath).filePath(QStringLiteral("old.kicad_mod")), QByteArrayLiteral("old")));

        const QString tempDirPath = manager.createTempDirectoryPath(QStringLiteral("Fixture.pretty"));
        QVERIFY(!tempDirPath.isEmpty());
        QVERIFY(QDir().mkpath(tempDirPath));
        QVERIFY(writeFile(QDir(tempDirPath).filePath(QStringLiteral("new.kicad_mod")), QByteArrayLiteral("new")));

        QVERIFY(manager.commitDirectory(finalDirPath));

        QVERIFY(!QDir(tempDirPath).exists());
        QVERIFY(!QDir(QDir(tempDir.path()).filePath(QStringLiteral(".tmp"))).exists());
        QVERIFY(!QFile::exists(QDir(finalDirPath).filePath(QStringLiteral("old.kicad_mod"))));
        QCOMPARE(readFile(QDir(finalDirPath).filePath(QStringLiteral("new.kicad_mod"))), QByteArrayLiteral("new"));
        QVERIFY(manager.registeredTempFiles().isEmpty());
    }

    void fileCommitBacksUpExistingTarget() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        TempFileManager manager;
        manager.setOutputPath(tempDir.path());

        const QString finalPath = QDir(tempDir.path()).filePath(QStringLiteral("FixtureLib.kicad_sym"));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old symbol library")));

        const QString tempPath =
            manager.createSymbolTempPath(QStringLiteral("FixtureLib"), QStringLiteral(".kicad_sym"));
        QVERIFY(writeFile(tempPath, QByteArrayLiteral("new symbol library")));

        QVERIFY(manager.commit(finalPath));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("new symbol library"));
        const QString backupPath = committedBackupPathFor(finalPath);
        QVERIFY2(!backupPath.isEmpty(), "Committed manifest should retain the old file backup");
        QCOMPARE(readFile(backupPath), QByteArrayLiteral("old symbol library"));
        QDir(QFileInfo(backupPath).absoluteDir().absolutePath()).removeRecursively();
    }

    void directoryCommitBacksUpExistingTarget() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        TempFileManager manager;
        manager.setOutputPath(tempDir.path());

        const QString finalDirPath = QDir(tempDir.path()).filePath(QStringLiteral("Fixture.pretty"));
        QVERIFY(QDir().mkpath(finalDirPath));
        QVERIFY(writeFile(QDir(finalDirPath).filePath(QStringLiteral("old.kicad_mod")), QByteArrayLiteral("old")));

        const QString tempDirPath = manager.createTempDirectoryPath(QStringLiteral("Fixture.pretty"));
        QVERIFY(QDir().mkpath(tempDirPath));
        QVERIFY(writeFile(QDir(tempDirPath).filePath(QStringLiteral("new.kicad_mod")), QByteArrayLiteral("new")));

        QVERIFY(manager.commitDirectory(finalDirPath));

        QCOMPARE(readFile(QDir(finalDirPath).filePath(QStringLiteral("new.kicad_mod"))), QByteArrayLiteral("new"));
        const QString backupPath = committedBackupPathFor(finalDirPath);
        QVERIFY2(!backupPath.isEmpty(), "Committed manifest should retain the old directory backup");
        QCOMPARE(readFile(QDir(backupPath).filePath(QStringLiteral("old.kicad_mod"))), QByteArrayLiteral("old"));
        QDir(QFileInfo(backupPath).absoluteDir().absolutePath()).removeRecursively();
    }

    void recoverIncompleteTransactionRestoresMissingFinalFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString backupDir = backupRootPath() + QDir::separator() + QStringLiteral("unit_recovery_tx");
        QDir(backupDir).removeRecursively();
        QVERIFY(QDir().mkpath(backupDir));

        const QString finalPath = QDir(tempDir.path()).filePath(QStringLiteral("recover.kicad_sym"));
        const QString backupPath = QDir(backupDir).filePath(QStringLiteral("recover.kicad_sym"));
        const QString tempPath = QDir(tempDir.path()).filePath(QStringLiteral("recover.tmp"));
        QVERIFY(writeFile(backupPath, QByteArrayLiteral("old")));
        QVERIFY(writeFile(tempPath, QByteArrayLiteral("new")));

        QJsonObject entry;
        entry.insert(QStringLiteral("finalPath"), finalPath);
        entry.insert(QStringLiteral("backupPath"), backupPath);
        entry.insert(QStringLiteral("tempPath"), tempPath);
        entry.insert(QStringLiteral("isDirectory"), false);
        entry.insert(QStringLiteral("existedBefore"), true);

        QJsonObject manifest;
        manifest.insert(QStringLiteral("transactionId"), QStringLiteral("unit_recovery_tx"));
        manifest.insert(QStringLiteral("outputRoot"), tempDir.path());
        manifest.insert(QStringLiteral("committed"), false);
        manifest.insert(QStringLiteral("entries"), QJsonArray{entry});
        QVERIFY(writeFile(QDir(backupDir).filePath(QStringLiteral("manifest.json")),
                          QJsonDocument(manifest).toJson(QJsonDocument::Indented)));

        QVERIFY(TempFileManager::recoverIncompleteTransactions());

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("old"));
        QVERIFY(!QFile::exists(tempPath));
        QDir(backupDir).removeRecursively();
    }

    void rollbackAllDeletesRegisteredFilesAndDirectories() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        TempFileManager manager;
        manager.setOutputPath(tempDir.path());
        QSignalSpy cleanupSpy(&manager, &TempFileManager::cleanupCompleted);

        const QString tempFilePath =
            manager.createSymbolTempPath(QStringLiteral("RollbackLib"), QStringLiteral(".kicad_sym"));
        QVERIFY(writeFile(tempFilePath, QByteArrayLiteral("temp")));

        const QString tempDirPath = manager.createTempDirectoryPath(QStringLiteral("Rollback.pretty"));
        QVERIFY(QDir().mkpath(tempDirPath));
        QVERIFY(writeFile(QDir(tempDirPath).filePath(QStringLiteral("temp.kicad_mod")), QByteArrayLiteral("temp")));

        manager.rollbackAll();

        QCOMPARE(cleanupSpy.count(), 1);
        QCOMPARE(cleanupSpy.at(0).at(0).toInt(), 2);
        QVERIFY(!QFile::exists(tempFilePath));
        QVERIFY(!QDir(tempDirPath).exists());
        QVERIFY(!QDir(QDir(tempDir.path()).filePath(QStringLiteral(".tmp"))).exists());
        QVERIFY(manager.registeredTempFiles().isEmpty());
    }

    void cleanupTempDirectoryDeletesUntrackedFilesAndRemovesEmptyDirectory() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        TempFileManager manager;
        manager.setOutputPath(tempDir.path());
        QSignalSpy cleanupSpy(&manager, &TempFileManager::cleanupCompleted);

        const QString tempDirectory = manager.tempDirectory();
        QVERIFY(QDir().mkpath(tempDirectory));
        QVERIFY(writeFile(QDir(tempDirectory).filePath(QStringLiteral("leftover.tmp")), QByteArrayLiteral("temp")));

        manager.cleanupTempDirectory();

        QCOMPARE(cleanupSpy.count(), 1);
        QCOMPARE(cleanupSpy.at(0).at(0).toInt(), 1);
        QVERIFY(!QDir(tempDirectory).exists());
    }

    void cleanupOrphanedTempFilesKeepsHiddenFiles() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        TempFileManager manager;
        manager.setOutputPath(tempDir.path());

        const QString tempDirectory = manager.tempDirectory();
        QVERIFY(QDir().mkpath(tempDirectory));
        const QString visibleFile = QDir(tempDirectory).filePath(QStringLiteral("orphan.tmp"));
        const QString hiddenFile = QDir(tempDirectory).filePath(QStringLiteral(".keep"));
        QVERIFY(writeFile(visibleFile, QByteArrayLiteral("orphan")));
        QVERIFY(writeFile(hiddenFile, QByteArrayLiteral("hidden")));

        manager.cleanupOrphanedTempFiles();

        QVERIFY(!QFile::exists(visibleFile));
        QCOMPARE(readFile(hiddenFile), QByteArrayLiteral("hidden"));
        QVERIFY(QDir(tempDirectory).exists());
    }

private:
    static bool writeFile(const QString& path, const QByteArray& data) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        return file.write(data) == data.size();
    }

    static QByteArray readFile(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            QTest::qFail(qPrintable(QStringLiteral("Unable to open file '%1': %2").arg(path, file.errorString())),
                         __FILE__,
                         __LINE__);
            return {};
        }
        return file.readAll();
    }

    static QString backupRootPath() {
        QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (appDataPath.isEmpty()) {
            appDataPath = QDir::homePath() + QDir::separator() + QStringLiteral(".easykiconverter");
        }
        return QDir(appDataPath).filePath(QStringLiteral("backups"));
    }

    static QString committedBackupPathFor(const QString& finalPath) {
        QDir backupRoot(backupRootPath());
        const QFileInfoList transactionDirs = backupRoot.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        for (const QFileInfo& transactionDir : transactionDirs) {
            QFile manifestFile(QDir(transactionDir.absoluteFilePath()).filePath(QStringLiteral("manifest.json")));
            if (!manifestFile.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QJsonObject manifest = QJsonDocument::fromJson(manifestFile.readAll()).object();
            if (!manifest.value(QStringLiteral("committed")).toBool(false)) {
                continue;
            }
            const QJsonArray entries = manifest.value(QStringLiteral("entries")).toArray();
            for (const QJsonValue& value : entries) {
                const QJsonObject entry = value.toObject();
                if (entry.value(QStringLiteral("finalPath")).toString() == finalPath) {
                    return entry.value(QStringLiteral("backupPath")).toString();
                }
            }
        }
        return QString();
    }
};

QTEST_GUILESS_MAIN(TestTempFileManager)
#include "test_temp_file_manager.moc"
