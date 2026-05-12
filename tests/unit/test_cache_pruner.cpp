#include "services/CachePruner.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestCachePruner : public QObject {
    Q_OBJECT

private slots:
    void testModel3DExcludedFromSize();
    void testPruneDoesNotDeleteModel3D();

private:
    void writeFile(const QString& path, qsizetype size);
};

void TestCachePruner::writeFile(const QString& path, qsizetype size) {
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    QVERIFY(file.write(QByteArray(size, 'x')) == size);
}

void TestCachePruner::testModel3DExcludedFromSize() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir root(tempDir.path());
    QVERIFY(root.mkpath(QStringLiteral("C123")));
    QVERIFY(root.mkpath(QStringLiteral("model3d")));

    writeFile(root.filePath(QStringLiteral("C123/component.json")), 10);
    writeFile(root.filePath(QStringLiteral("model3d/model.step")), 1000);

    CachePruner pruner(tempDir.path());
    QCOMPARE(pruner.currentCacheSize(), qint64(10));
}

void TestCachePruner::testPruneDoesNotDeleteModel3D() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    QDir root(tempDir.path());
    QVERIFY(root.mkpath(QStringLiteral("C123")));
    QVERIFY(root.mkpath(QStringLiteral("model3d")));

    const QString componentPath = root.filePath(QStringLiteral("C123/component.json"));
    const QString modelPath = root.filePath(QStringLiteral("model3d/model.step"));
    writeFile(componentPath, 10);
    writeFile(modelPath, 1000);

    CachePruner pruner(tempDir.path());
    QCOMPARE(pruner.pruneTo(0), qint64(0));

    QVERIFY(!QFileInfo::exists(componentPath));
    QVERIFY(QFileInfo::exists(modelPath));
}

QTEST_GUILESS_MAIN(TestCachePruner)
#include "test_cache_pruner.moc"
