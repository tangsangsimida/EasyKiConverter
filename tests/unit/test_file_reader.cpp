#include "utils/cli/FileReader.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

using namespace EasyKiConverter;

class TestFileReader : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void testReadComponentListFile();
    void testReadComponentListFileEmpty();
    void testReadComponentListFileNotFound();
    void testFileExists();
    void testGetFileExtension();

private:
    QTemporaryDir* m_tempDir = nullptr;
    QString createTempFile(const QString& name, const QString& content);
};

void TestFileReader::initTestCase() {
    m_tempDir = new QTemporaryDir();
    QVERIFY(m_tempDir->isValid());
}

void TestFileReader::cleanupTestCase() {
    delete m_tempDir;
    m_tempDir = nullptr;
}

QString TestFileReader::createTempFile(const QString& name, const QString& content) {
    QString filePath = m_tempDir->path() + "/" + name;
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << content;
        file.close();
    }
    return filePath;
}

void TestFileReader::testReadComponentListFile() {
    QString content = "C23186\nC23166\nC23102\n";
    QString filePath = createTempFile("components.txt", content);

    QString error;
    QStringList ids = FileReader::readComponentListFile(filePath, error);

    QVERIFY(error.isEmpty());
    QCOMPARE(ids.size(), 3);
    QVERIFY(ids.contains("C23186"));
    QVERIFY(ids.contains("C23166"));
    QVERIFY(ids.contains("C23102"));
}

void TestFileReader::testReadComponentListFileEmpty() {
    QString content = "\n\n\n";
    QString filePath = createTempFile("empty.txt", content);

    QString error;
    QStringList ids = FileReader::readComponentListFile(filePath, error);

    QVERIFY(error.isEmpty());
    QVERIFY(ids.isEmpty());
}

void TestFileReader::testReadComponentListFileNotFound() {
    QString error;
    QStringList ids = FileReader::readComponentListFile("/nonexistent/file.txt", error);

    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains("不存在"));
    QVERIFY(ids.isEmpty());
}

void TestFileReader::testFileExists() {
    QString filePath = createTempFile("exists.txt", "test");

    QVERIFY(FileReader::fileExists(filePath));
    QVERIFY(!FileReader::fileExists("/nonexistent/file.txt"));
}

void TestFileReader::testGetFileExtension() {
    QCOMPARE(FileReader::getFileExtension("file.txt"), "txt");
    QCOMPARE(FileReader::getFileExtension("file.xlsx"), "xlsx");
    QCOMPARE(FileReader::getFileExtension("file.CSV"), "csv");
    QCOMPARE(FileReader::getFileExtension("file"), "");
    QCOMPARE(FileReader::getFileExtension("/path/to/file.txt"), "txt");
}

QTEST_MAIN(TestFileReader)
#include "test_file_reader.moc"
