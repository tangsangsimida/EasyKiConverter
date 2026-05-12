#include "core/utils/AtomicFileWriter.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestAtomicFileWriter : public QObject {
    Q_OBJECT

private slots:

    void writeAtomicallyWritesDataAndCreatesTempDirectory() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString workDir = tempDir.path();
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("output.txt"));

        QVERIFY(AtomicFileWriter::writeAtomically(
            atomicTempDir, finalPath, QStringLiteral(".tmp"), QByteArrayLiteral("new content")));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("new content"));
        QVERIFY(QDir(atomicTempDir).exists());
        QVERIFY(tempFiles(atomicTempDir).isEmpty());
    }

    void writeAtomicallyKeepsExistingFileAndRemovesTempOnWriterFailure() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString workDir = tempDir.path();
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("output.txt"));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old content")));

        QVERIFY(!AtomicFileWriter::writeAtomically(
            atomicTempDir, finalPath, QStringLiteral(".tmp"), [](const QString& tempPath) {
                QFile file(tempPath);
                if (!file.open(QIODevice::WriteOnly)) {
                    return false;
                }
                file.write("partial content");
                file.close();
                return false;
            }));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("old content"));
        QVERIFY(tempFiles(atomicTempDir).isEmpty());
    }

    void writeAtomicallyReplacesExistingFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString workDir = tempDir.path();
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("output.txt"));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old content")));

        QVERIFY(AtomicFileWriter::writeAtomically(
            atomicTempDir, finalPath, QStringLiteral(".tmp"), QByteArrayLiteral("replacement")));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("replacement"));
        QVERIFY(tempFiles(atomicTempDir).isEmpty());
    }

    void copyAtomicallyCopiesAndReplacesExistingFile() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString workDir = tempDir.path();
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        const QString sourcePath = QDir(workDir).filePath(QStringLiteral("source.step"));
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("model.step"));

        QVERIFY(writeFile(sourcePath, QByteArrayLiteral("source model")));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old model")));

        QVERIFY(AtomicFileWriter::copyAtomically(sourcePath, finalPath, atomicTempDir));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("source model"));
        QCOMPARE(readFile(sourcePath), QByteArrayLiteral("source model"));
        QVERIFY(tempFiles(atomicTempDir).isEmpty());
    }

    void copyAtomicallyFailsForMissingSourceWithoutTouchingDestination() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString workDir = tempDir.path();
        const QString atomicTempDir = QDir(workDir).filePath(QStringLiteral(".atomic"));
        const QString sourcePath = QDir(workDir).filePath(QStringLiteral("missing.step"));
        const QString finalPath = QDir(workDir).filePath(QStringLiteral("model.step"));
        QVERIFY(writeFile(finalPath, QByteArrayLiteral("old model")));

        QVERIFY(!AtomicFileWriter::copyAtomically(sourcePath, finalPath, atomicTempDir));

        QCOMPARE(readFile(finalPath), QByteArrayLiteral("old model"));
        QVERIFY(!QDir(atomicTempDir).exists());
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

    static QStringList tempFiles(const QString& path) {
        QDir dir(path);
        if (!dir.exists()) {
            return {};
        }
        return dir.entryList(QDir::Files | QDir::NoDotAndDotDot);
    }
};

QTEST_GUILESS_MAIN(TestAtomicFileWriter)
#include "test_atomic_file_writer.moc"
