#include "models/ComponentData.h"
#include "models/Model3DData.h"
#include "models/SymbolData.h"
#include "services/ComponentCacheService.h"
#include "services/export/ExportWorkerHelpers.h"
#include "tests/common/TestPaths.hpp"

#include <QDir>
#include <QTemporaryDir>
#include <QTest>

using namespace EasyKiConverter;

class TestExportWorkerHelpers : public QObject {
    Q_OBJECT

private slots:

    /** @brief 初始化测试使用的临时缓存目录。 */
    void init() {
        QVERIFY(m_tempDir.isValid());
        m_cache = ComponentCacheService::instance();
        m_cache->setCacheDir(m_tempDir.path());
        m_cache->clearAllCache();
    }

    /** @brief 清理测试缓存目录中的数据。 */
    void cleanup() {
        if (m_cache != nullptr)
            m_cache->clearAllCache();
    }

    /** @brief 验证默认输出目录会追加子目录。 */
    void defaultOutputDirAppendsSubdir() {
        const QString path = ExportWorkerHelpers::defaultOutputDir(QStringLiteral("symbols"));
        QVERIFY(path.contains(QStringLiteral("/export/symbols")));
    }

    /** @brief 验证文件路径由元器件编号、目录和扩展名组成。 */
    void buildFilePathConcatenates() {
        const QString path = ExportWorkerHelpers::buildFilePath(
            QStringLiteral("C12345"), QStringLiteral("/tmp/out"), QStringLiteral(".kicad_sym"));
        QCOMPARE(path, QStringLiteral("/tmp/out/C12345.kicad_sym"));
    }

    /** @brief 验证输出目录会被创建并返回。 */
    void ensureOutputDirCreatesAndReturnsPath() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ExportOptions options;
        options.outputPath = tempDir.path();

        const QString result = ExportWorkerHelpers::ensureOutputDir(options, QStringLiteral("testdir"));
        QVERIFY(!result.isEmpty());
        QVERIFY(result.startsWith(tempDir.path()));
        QVERIFY(QDir(result).exists());
    }

    /** @brief 验证未设置输出目录时会回退到默认目录。 */
    void ensureOutputDirFallsBackToDefaultWhenOutputPathEmpty() {
        ExportOptions options;  // outputPath empty
        const QString result = ExportWorkerHelpers::ensureOutputDir(options, QStringLiteral("sub"));
        QVERIFY(!result.isEmpty());
        QVERIFY(result.contains(QStringLiteral("/export/sub")));
    }

    /** @brief 验证存在且禁止覆盖的文件会被跳过。 */
    void shouldSkipExistingReturnsTrueWhenFileExistsAndNotOverwriting() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString filePath = tempDir.filePath(QStringLiteral("existing.kicad_sym"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test");
        file.close();

        ExportOptions options;
        options.overwriteExistingFiles = false;
        QVERIFY(ExportWorkerHelpers::shouldSkipExisting(filePath, options));
    }

    /** @brief 验证缺失文件不会被跳过。 */
    void shouldNotSkipExistingWhenFileMissing() {
        ExportOptions options;
        options.overwriteExistingFiles = false;
        QVERIFY(!ExportWorkerHelpers::shouldSkipExisting(QStringLiteral("/nonexistent/file.kicad_sym"), options));
    }

    /** @brief 验证允许覆盖时不会跳过已有文件。 */
    void shouldNotSkipExistingWhenOverwriteEnabled() {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString filePath = tempDir.filePath(QStringLiteral("overwrite_me.kicad_sym"));
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test");
        file.close();

        ExportOptions options;
        options.overwriteExistingFiles = true;
        QVERIFY(!ExportWorkerHelpers::shouldSkipExisting(filePath, options));
    }

    /** @brief 验证回退数据会填充目标中的空字段。 */
    void mergeComponentDataFillsEmptyTargetFromFallback() {
        auto fallback = QSharedPointer<ComponentData>::create();
        fallback->setLcscId(QStringLiteral("C12345"));
        fallback->setName(QStringLiteral("Test Resistor"));
        fallback->setPackage(QStringLiteral("0603"));
        fallback->setManufacturer(QStringLiteral("Test Corp"));
        fallback->setManufacturerPart(QStringLiteral("TEST-123"));
        fallback->setDatasheet(QStringLiteral("https://example.com/ds.pdf"));
        fallback->setDatasheetFormat(QStringLiteral("pdf"));
        fallback->setDatasheetData(QByteArrayLiteral("fake-pdf-data"));
        fallback->setPrefix(QStringLiteral("R"));

        auto symbol = QSharedPointer<SymbolData>::create();
        SymbolInfo symInfo;
        symInfo.name = QStringLiteral("RES_0603");
        symbol->setInfo(symInfo);
        fallback->setSymbolData(symbol);

        ComponentData target;
        target.setLcscId(QStringLiteral("C12345"));

        ExportWorkerHelpers::mergeComponentData(target, fallback);

        QCOMPARE(target.name(), QStringLiteral("Test Resistor"));
        QCOMPARE(target.package(), QStringLiteral("0603"));
        QCOMPARE(target.manufacturer(), QStringLiteral("Test Corp"));
        QCOMPARE(target.prefix(), QStringLiteral("R"));
        QCOMPARE(target.datasheet(), QStringLiteral("https://example.com/ds.pdf"));
        QCOMPARE(target.datasheetFormat(), QStringLiteral("pdf"));
        QCOMPARE(target.datasheetData(), QByteArrayLiteral("fake-pdf-data"));
        QVERIFY(target.symbolData());
        QCOMPARE(target.symbolData()->info().name, QStringLiteral("RES_0603"));
    }

    /** @brief 验证合并不会覆盖目标已有字段。 */
    void mergeComponentDataPreservesExistingTargetValues() {
        auto fallback = QSharedPointer<ComponentData>::create();
        fallback->setLcscId(QStringLiteral("C12345"));
        fallback->setName(QStringLiteral("fallback name"));
        fallback->setPackage(QStringLiteral("fallback pkg"));

        ComponentData target;
        target.setLcscId(QStringLiteral("C12345"));
        target.setName(QStringLiteral("existing name"));

        ExportWorkerHelpers::mergeComponentData(target, fallback);

        QCOMPARE(target.name(), QStringLiteral("existing name"));  // Not overwritten
        QCOMPARE(target.package(), QStringLiteral("fallback pkg"));  // Was empty
    }

    /** @brief 验证目标编号为空时会完整复制回退数据。 */
    void mergeComponentDataCopiesFullFallbackWhenTargetLcscIdEmpty() {
        auto fallback = QSharedPointer<ComponentData>::create();
        fallback->setLcscId(QStringLiteral("C99999"));
        fallback->setName(QStringLiteral("full name"));

        ComponentData target;  // lcscId is empty

        ExportWorkerHelpers::mergeComponentData(target, fallback);

        QCOMPARE(target.lcscId(), QStringLiteral("C99999"));
        QCOMPARE(target.name(), QStringLiteral("full name"));
    }

    /** @brief 验证空回退数据不会改变目标。 */
    void mergeComponentDataIsNoopWhenFallbackIsNull() {
        ComponentData target;
        target.setLcscId(QStringLiteral("C12345"));
        target.setName(QStringLiteral("original"));

        ExportWorkerHelpers::mergeComponentData(target, nullptr);

        QCOMPARE(target.lcscId(), QStringLiteral("C12345"));
        QCOMPARE(target.name(), QStringLiteral("original"));
    }

    /** @brief 验证状态计数会根据项目状态重新计算。 */
    void recomputeTypeProgressCountsZeroesAndRecalculates() {
        ExportTypeProgress progress;
        progress.totalCount = 3;

        ExportItemStatus s1;
        s1.status = ExportItemStatus::Status::Success;
        ExportItemStatus s2;
        s2.status = ExportItemStatus::Status::Failed;
        ExportItemStatus s3;
        s3.status = ExportItemStatus::Status::InProgress;

        progress.itemStatus[QStringLiteral("C1")] = s1;
        progress.itemStatus[QStringLiteral("C2")] = s2;
        progress.itemStatus[QStringLiteral("C3")] = s3;

        progress.successCount = 99;  // garbage
        progress.failedCount = 99;
        progress.completedCount = 99;

        ExportWorkerHelpers::recomputeTypeProgressCounts(progress);

        QCOMPARE(progress.successCount, 1);
        QCOMPARE(progress.failedCount, 1);
        QCOMPARE(progress.inProgressCount, 1);
        QCOMPARE(progress.completedCount, 2);  // Success + Failed
        QCOMPARE(progress.skippedCount, 0);
    }

    /** @brief 验证磁盘缓存会从封装模型恢复 UUID 并预加载 OBJ。 */
    void loadDiskCacheRestoresFootprintModelBeforeObjLoad() {
        const QString componentId = QStringLiteral("C23186");
        const QString modelUuid = QStringLiteral("6bd5cd867e9542ebae21caaf5d2d4c4d");
        QString error;
        const QByteArray cadData =
            Test::TestPaths::readBytes(Test::TestPaths::fixturePath(QStringLiteral("easyeda/cad_basic.json")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QByteArray objData = Test::TestPaths::readBytes(
            Test::TestPaths::fixturePath(QStringLiteral("easyeda/model3d_r0603.obj")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        ComponentData metadata;
        metadata.setLcscId(componentId);
        m_cache->saveComponentMetadata(componentId, metadata);
        m_cache->saveCadDataJson(componentId, cadData);
        m_cache->saveModel3D(modelUuid, objData, QStringLiteral("obj"));

        const QSharedPointer<ComponentData> loaded = ExportWorkerHelpers::loadDiskCachedComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QVERIFY(loaded->model3DData() != nullptr);
        QCOMPARE(loaded->model3DData()->uuid(), modelUuid);
        QCOMPARE(loaded->model3DObjRaw(), objData);
    }

private:
    QTemporaryDir m_tempDir;
    ComponentCacheService* m_cache = nullptr;
};

QTEST_GUILESS_MAIN(TestExportWorkerHelpers)
#include "test_export_worker_helpers.moc"
