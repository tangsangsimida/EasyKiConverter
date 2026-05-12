#include "services/export/KiCadLibraryTableManager.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace EasyKiConverter;

class TestKiCadLibraryTableManager : public QObject {
    Q_OBJECT

private slots:

    void registerSymbolLibraryCreatesAndUpdatesProjectTable() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString projectRoot = tempDir.path();
        writeText(QDir(projectRoot).filePath(QStringLiteral("Board.kicad_pro")), QStringLiteral("{}\n"));

        const QString outputDir = QDir(projectRoot).filePath(QStringLiteral("exports"));
        QVERIFY(QDir().mkpath(outputDir));

        const QString firstLibraryPath = QDir(outputDir).filePath(QStringLiteral("FixtureLib.kicad_sym"));
        const QString secondLibraryPath = QDir(outputDir).filePath(QStringLiteral("FixtureLib_v2.kicad_sym"));

        QCOMPARE(KiCadLibraryTableManager::registerSymbolLibrary(
                     outputDir, QStringLiteral("FixtureLib"), firstLibraryPath, QStringLiteral("first description")),
                 KiCadLibraryTableManager::RegistrationResult::Registered);

        const QString tablePath = QDir(projectRoot).filePath(QStringLiteral("sym-lib-table"));
        QString content = readText(tablePath);
        QVERIFY(content.contains(QStringLiteral("(sym_lib_table")));
        QVERIFY(content.contains(QStringLiteral("(version 7)")));
        QVERIFY(content.contains(QStringLiteral("(name \"FixtureLib\")")));
        QVERIFY(content.contains(QStringLiteral("(uri \"${KIPRJMOD}/exports/FixtureLib.kicad_sym\")")));
        QVERIFY(content.contains(QStringLiteral("(descr \"first description\")")));

        QCOMPARE(KiCadLibraryTableManager::registerSymbolLibrary(
                     outputDir, QStringLiteral("FixtureLib"), secondLibraryPath, QStringLiteral("second description")),
                 KiCadLibraryTableManager::RegistrationResult::Registered);

        content = readText(tablePath);
        QCOMPARE(content.count(QStringLiteral("(name \"FixtureLib\")")), 1);
        QVERIFY(content.contains(QStringLiteral("(uri \"${KIPRJMOD}/exports/FixtureLib_v2.kicad_sym\")")));
        QVERIFY(content.contains(QStringLiteral("(descr \"second description\")")));
        QVERIFY(!content.contains(QStringLiteral("first description")));
    }

    void registerFootprintLibraryWritesEscapedProjectEntry() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString projectRoot = tempDir.path();
        writeText(QDir(projectRoot).filePath(QStringLiteral("Board.kicad_pro")), QStringLiteral("{}\n"));

        const QString outputDir = QDir(projectRoot).filePath(QStringLiteral("exports"));
        QVERIFY(QDir().mkpath(outputDir));

        const QString libName = QStringLiteral("FP \"Special\"");
        const QString libDirPath = QDir(outputDir).filePath(QStringLiteral("FP Special.pretty"));
        const QString description = QStringLiteral("contains \"quotes\" and C:\\KiCad");

        QCOMPARE(KiCadLibraryTableManager::registerFootprintLibrary(outputDir, libName, libDirPath, description),
                 KiCadLibraryTableManager::RegistrationResult::Registered);

        const QString content = readText(QDir(projectRoot).filePath(QStringLiteral("fp-lib-table")));
        QVERIFY(content.contains(QStringLiteral("(fp_lib_table")));
        QVERIFY(content.contains(QStringLiteral("(name \"FP \\\"Special\\\"\")")));
        QVERIFY(content.contains(QStringLiteral("(uri \"${KIPRJMOD}/exports/FP Special.pretty\")")));
        QVERIFY(content.contains(QStringLiteral("(descr \"contains \\\"quotes\\\" and C:\\\\KiCad\")")));
    }

    void registerLibraryWithoutProjectWritesImportGuide() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        const QString outputDir = QDir(tempDir.path()).filePath(QStringLiteral("standalone-export"));
        const QString libFilePath = QDir(outputDir).filePath(QStringLiteral("Standalone.kicad_sym"));

        QCOMPARE(KiCadLibraryTableManager::registerSymbolLibrary(
                     outputDir, QStringLiteral("Standalone"), libFilePath, QStringLiteral("standalone description")),
                 KiCadLibraryTableManager::RegistrationResult::ImportGuideWritten);

        const QString guidePath = QDir(outputDir).filePath(QStringLiteral("KiCad_IMPORT.md"));
        const QString guide = readText(guidePath);
        QVERIFY(guide.contains(QStringLiteral("did not find a KiCad project")));
        QVERIFY(guide.contains(QStringLiteral("sym-lib-table")));
        QVERIFY(guide.contains(QStringLiteral("fp-lib-table")));
    }

private:
    static void writeText(const QString& path, const QString& content) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream out(&file);
        out.setEncoding(QStringConverter::Utf8);
        out << content;
    }

    static QString readText(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTest::qFail(qPrintable(QStringLiteral("Unable to open file '%1': %2").arg(path, file.errorString())),
                         __FILE__,
                         __LINE__);
            return {};
        }

        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        return in.readAll();
    }
};

QTEST_GUILESS_MAIN(TestKiCadLibraryTableManager)
#include "test_kicad_library_table_manager.moc"
