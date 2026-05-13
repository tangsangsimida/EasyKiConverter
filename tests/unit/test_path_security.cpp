#include "utils/PathSecurity.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestPathSecurity : public QObject {
    Q_OBJECT

private slots:

    void validatesPathComponents() {
        QVERIFY(PathSecurity::isValidPathComponent(QStringLiteral("R0603")));
        QVERIFY(!PathSecurity::isValidPathComponent(QString()));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral(".")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("..")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("../etc/passwd")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("foo/bar")));
        QVERIFY(!PathSecurity::isValidPathComponent(QStringLiteral("foo\\bar")));
    }

    void detectsPathsOutsideBaseDirectory() {
        QTemporaryDir baseDir;
        QTemporaryDir outsideDir;
        QVERIFY(baseDir.isValid());
        QVERIFY(outsideDir.isValid());

        const QString childPath = QDir(baseDir.path()).filePath(QStringLiteral("child/file.txt"));
        const QString traversalPath = QDir(baseDir.path()).filePath(QStringLiteral("../outside.txt"));

        QVERIFY(PathSecurity::isSafePath(baseDir.path(), baseDir.path()));
        QVERIFY(PathSecurity::isSafePath(childPath, baseDir.path()));
        QVERIFY(!PathSecurity::isSafePath(outsideDir.path(), baseDir.path()));
        QVERIFY(!PathSecurity::isSafePath(traversalPath, baseDir.path()));
        QVERIFY(!PathSecurity::isSafePath(QString(), baseDir.path()));
    }

    void sanitizesFilenames() {
        QCOMPARE(PathSecurity::sanitizeFilename(QStringLiteral("foo/bar:baz*?.kicad_mod")),
                 QStringLiteral("foo_bar_baz__.kicad_mod"));
        QCOMPARE(PathSecurity::sanitizeFilename(QStringLiteral("  CON  ")), QStringLiteral("CON_safe"));
        QCOMPARE(PathSecurity::sanitizeFilename(QStringLiteral("\x01\x02")), QStringLiteral("unnamed_component"));
        QCOMPARE(PathSecurity::sanitizeFilename(QStringLiteral("zero%1width").arg(QChar(0x200B))),
                 QStringLiteral("zerowidth"));
    }

    void safeRemoveRecursivelyHonorsFileLimit() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        QDir dir(tempDir.path());
        QVERIFY(dir.mkpath(QStringLiteral("target")));

        QFile first(dir.filePath(QStringLiteral("target/one.txt")));
        QVERIFY(first.open(QIODevice::WriteOnly));
        first.write("1");
        first.close();

        QFile second(dir.filePath(QStringLiteral("target/two.txt")));
        QVERIFY(second.open(QIODevice::WriteOnly));
        second.write("2");
        second.close();

        const QString targetPath = dir.filePath(QStringLiteral("target"));
        QVERIFY(!PathSecurity::safeRemoveRecursively(targetPath, 1));
        QVERIFY(QDir(targetPath).exists());
        QVERIFY(PathSecurity::safeRemoveRecursively(targetPath, 10));
        QVERIFY(!QDir(targetPath).exists());
    }
};

QTEST_GUILESS_MAIN(TestPathSecurity)
#include "test_path_security.moc"
