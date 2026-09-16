#include "services/CacheRepository.h"
#include "services/ComponentCacheService.h"
#include "services/LcscImageService.h"

#include <QAtomicInt>
#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestComponentCacheService : public QObject {
    Q_OBJECT

private slots:

    // 为每个测试准备独立的缓存目录并清理旧状态。
    void init() {
        QVERIFY(m_tempDir.isValid());
        m_cache = ComponentCacheService::instance();
        m_cache->setCacheDir(m_tempDir.path());
        m_cache->clearAllCache();
    }

    // 清理测试期间产生的一级和二级缓存。
    void cleanup() {
        if (m_cache != nullptr) {
            m_cache->clearAllCache();
        }
    }

    // 验证预览图 URL 经过保存和读取后保持规范化结果。
    void testPreviewImageUrlsRoundTrip() {
        const QString componentId = QStringLiteral("C12345");
        const QStringList previewUrls = {QStringLiteral("https://image.lceda.cn/a.jpg"),
                                         QStringLiteral("/image.lceda.cn/components/b.jpg")};
        const QStringList normalizedUrls = {QStringLiteral("https://image.lceda.cn/a.jpg"),
                                            QStringLiteral("https://image.lceda.cn/components/b.jpg")};

        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Test Component"));
        data.setPreviewImages(previewUrls);

        m_cache->saveComponentMetadata(componentId, data);

        const QJsonObject memoryMetadata = m_cache->loadMetadataFromMemory(componentId);
        QVERIFY(!memoryMetadata.isEmpty());
        const QJsonArray memoryPreviewUrls = memoryMetadata.value(QStringLiteral("previewImages")).toArray();
        QCOMPARE(memoryPreviewUrls.size(), previewUrls.size());
        QCOMPARE(memoryPreviewUrls.at(0).toString(), previewUrls.at(0));
        QCOMPARE(memoryPreviewUrls.at(1).toString(), normalizedUrls.at(1));

        QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QCOMPARE(loadedData->name(), QStringLiteral("Test Component"));
        QCOMPARE(loadedData->previewImages(), normalizedUrls);
    }

    // 验证异常预览图 URL 会在读取时被规范化。
    void testMalformedPreviewImageUrlNormalization() {
        const QString componentId = QStringLiteral("C54321");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Malformed URL Component"));
        data.setPreviewImages({QStringLiteral("https://image.lceda.cn//image.lceda.cn/components/c.jpg")});

        m_cache->saveComponentMetadata(componentId, data);

        QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QCOMPARE(loadedData->previewImages().value(0), QStringLiteral("https://image.lceda.cn/components/c.jpg"));
    }

    // 验证错误页和非图片字节不会进入预览图缓存。
    void testInvalidPreviewImageDataIsRejected() {
        const QString componentId = QStringLiteral("C54322");
        m_cache->savePreviewImage(componentId, QByteArrayLiteral("<html>403</html>"), 0);
        QVERIFY(m_cache->loadPreviewImage(componentId, 0).isEmpty());
        QVERIFY(!QFileInfo::exists(m_cache->previewImagePath(componentId, 0)));
    }

    // 验证图片服务不会把损坏的缓存文件报告为可用预览图。
    void testImageServiceRejectsCorruptCachedPreview() {
        const QString componentId = QStringLiteral("C54326");
        const QString imagePath = m_cache->previewImagePath(componentId, 0);
        QVERIFY(QDir().mkpath(QFileInfo(imagePath).absolutePath()));
        QFile imageFile(imagePath);
        QVERIFY(imageFile.open(QIODevice::WriteOnly));
        QVERIFY(imageFile.write(QByteArrayLiteral("<html>403</html>")) > 0);
        imageFile.close();

        LcscImageService imageService;
        QSignalSpy errorSpy(&imageService, &LcscImageService::error);
        imageService.fetchPreviewImages(componentId);

        QVERIFY2(errorSpy.wait(3000), "Corrupt cached preview should report an error");
        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.at(0).at(0).toString(), componentId);
        QCOMPARE(errorSpy.at(0).at(1).toString(), QStringLiteral("No images downloaded"));
    }

    // 验证既不是 PDF 也不是 HTML 的响应不会进入数据手册缓存。
    void testInvalidPdfDatasheetDataIsRejected() {
        const QString componentId = QStringLiteral("C54323");
        m_cache->saveDatasheet(componentId, QByteArrayLiteral("not-a-datasheet"), QStringLiteral("pdf"));
        QVERIFY(m_cache->loadDatasheet(componentId).isEmpty());
    }

    // 验证未知数据手册格式不会写入错误的 PDF 缓存路径。
    void testUnsupportedDatasheetFormatIsRejected() {
        const QString componentId = QStringLiteral("C54324");
        const QByteArray htmlData = QByteArrayLiteral("<html><body>unsupported</body></html>");

        m_cache->saveDatasheet(componentId, htmlData, QStringLiteral("doc"));

        QVERIFY(m_cache->loadDatasheet(componentId).isEmpty());
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(componentId + QStringLiteral("/datasheet.pdf"))));
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(componentId + QStringLiteral("/datasheet.html"))));
    }

    // 验证大小写不同的元器件编号使用同一缓存目录和内存键，并可被完整删除。
    void testComponentCacheNormalizesIdCaseForRemoval() {
        const QString componentId = QStringLiteral("c54328");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Case normalized component"));

        m_cache->saveComponentMetadata(componentId, data);

        QVERIFY(m_cache->hasCache(componentId));
        QVERIFY(QFileInfo::exists(m_tempDir.filePath(QStringLiteral("C54328/component.json"))));
        QSignalSpy cacheSizeSpy(m_cache, &ComponentCacheService::cacheSizeChanged);
        m_cache->removeCache(componentId);

        QVERIFY(!m_cache->hasCache(componentId));
        QVERIFY(m_cache->loadComponentData(componentId) == nullptr);
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(QStringLiteral("C54328"))));
        QVERIFY(!cacheSizeSpy.isEmpty());
        QCOMPARE(cacheSizeSpy.last().at(0).toLongLong(), m_cache->getCacheSize());
    }

    // 验证升级前的小写缓存目录仍可读取并通过规范化编号删除。
    void testLegacyLowercaseComponentCacheRemainsAccessible() {
        const QString componentId = QStringLiteral("C54329");
        const QString legacyDir = m_tempDir.filePath(QStringLiteral("c54329"));
        QVERIFY(QDir().mkpath(legacyDir));

        QJsonObject metadata;
        metadata.insert(QStringLiteral("lcscId"), componentId);
        metadata.insert(QStringLiteral("name"), QStringLiteral("Legacy component"));
        QFile metadataFile(QDir(legacyDir).filePath(QStringLiteral("component.json")));
        QVERIFY(metadataFile.open(QIODevice::WriteOnly));
        QVERIFY(metadataFile.write(QJsonDocument(metadata).toJson(QJsonDocument::Compact)) > 0);
        metadataFile.close();

        const QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Legacy component"));

        m_cache->removeCache(componentId);
        QVERIFY(!QFileInfo::exists(legacyDir));
    }

    // 验证数据手册下载不会直接返回格式无效的磁盘缓存。
    void testDownloadDatasheetRemovesInvalidCachedData() {
        const QString componentId = QStringLiteral("C54325");
        QVERIFY(QDir().mkpath(m_tempDir.filePath(componentId)));
        const QString cachedPath = m_tempDir.filePath(componentId + QStringLiteral("/datasheet.pdf"));
        QFile cachedFile(cachedPath);
        QVERIFY(cachedFile.open(QIODevice::WriteOnly));
        QVERIFY(cachedFile.write(QByteArrayLiteral("not-a-pdf")) > 0);
        cachedFile.close();

        QAtomicInt cancelled(1);
        QString format;
        const QByteArray data = m_cache->downloadDatasheet(
            componentId, QStringLiteral("https://example.com/manual.pdf"), &format, nullptr, &cancelled);

        QVERIFY(data.isEmpty());
        QVERIFY(!QFileInfo::exists(cachedPath));
    }

    // 验证 PDF URL 对应的 HTML 回退缓存可以被后续下载请求复用。
    void testDownloadDatasheetUsesHtmlFallbackCache() {
        const QString componentId = QStringLiteral("C54327");
        const QByteArray htmlData = QByteArrayLiteral("<html><body>cached</body></html>");
        m_cache->saveDatasheet(componentId, htmlData, QStringLiteral("pdf"));

        QAtomicInt cancelled(1);
        QString format;
        const QByteArray data = m_cache->downloadDatasheet(
            componentId, QStringLiteral("https://example.com/manual.pdf"), &format, nullptr, &cancelled);

        QCOMPARE(data, htmlData);
        QCOMPARE(format, QStringLiteral("html"));
    }

    // 验证超出预览图范围的索引会在网络请求前被拒绝。
    void testDownloadPreviewRejectsOutOfRangeIndex() {
        QAtomicInt cancelled(0);

        const QByteArray data = m_cache->downloadPreviewImage(
            QStringLiteral("C54326"), QStringLiteral("https://example.com/preview.jpg"), 3, nullptr, &cancelled);

        QVERIFY(data.isEmpty());
    }

    // 验证异步预览图入口会在网络请求前拒绝超出范围的索引。
    void testAsyncPreviewRejectsOutOfRangeIndex() {
        QAtomicInt cancelled(0);
        bool callbackCalled = false;
        QByteArray callbackData;
        ComponentExportStatus::NetworkDiagnostics callbackDiag;

        CacheRepository::instance()->fetchPreviewImageAsync(
            QStringLiteral("C54328"),
            QStringLiteral("https://example.com/preview.jpg"),
            3,
            &cancelled,
            false,
            [&](const QByteArray& data, const ComponentExportStatus::NetworkDiagnostics& diag) {
                callbackCalled = true;
                callbackData = data;
                callbackDiag = diag;
            });

        QVERIFY(callbackCalled);
        QVERIFY(callbackData.isEmpty());
        QCOMPARE(callbackDiag.errorString, QStringLiteral("Invalid preview image index"));
    }

    // 验证缓存自愈会清理非空但格式无效的媒体和三维文件。
    void testCacheHealthRemovesInvalidNonEmptyFiles() {
        const QString componentId = QStringLiteral("C54324");
        const QString modelUuid = QStringLiteral("health-invalid-model");
        ComponentData metadata;
        metadata.setLcscId(componentId);
        metadata.setName(QStringLiteral("Health Check Component"));
        m_cache->saveComponentMetadata(componentId, metadata);

        const QString componentDir = m_tempDir.filePath(componentId);
        QFile previewFile(QDir(componentDir).filePath(QStringLiteral("preview_0.jpg")));
        QVERIFY(previewFile.open(QIODevice::WriteOnly));
        QVERIFY(previewFile.write(QByteArrayLiteral("<html>403</html>")) > 0);
        previewFile.close();

        QFile datasheetFile(QDir(componentDir).filePath(QStringLiteral("datasheet.pdf")));
        QVERIFY(datasheetFile.open(QIODevice::WriteOnly));
        QVERIFY(datasheetFile.write(QByteArrayLiteral("not-a-pdf")) > 0);
        datasheetFile.close();

        const QString invalidModelPath = m_tempDir.filePath(QStringLiteral("model3d/health-invalid-model.obj"));
        QFile invalidModelFile(invalidModelPath);
        QVERIFY(QDir().mkpath(QFileInfo(invalidModelPath).absolutePath()));
        QVERIFY(invalidModelFile.open(QIODevice::WriteOnly));
        QVERIFY(invalidModelFile.write(QByteArrayLiteral("not-an-obj")) > 0);
        invalidModelFile.close();
        QVERIFY(QFileInfo::exists(invalidModelPath));

        m_cache->setCacheDir(m_tempDir.path(), false);

        QVERIFY(!QFileInfo::exists(previewFile.fileName()));
        QVERIFY(!QFileInfo::exists(datasheetFile.fileName()));
        QVERIFY(m_cache->loadModel3D(modelUuid, QStringLiteral("obj")).isEmpty());
    }

    // 验证读取三维缓存时会拒绝并删除结构无效的模型文件。
    void testLoadModel3DRejectsInvalidContent() {
        const QString modelUuid = QStringLiteral("load-invalid-model");
        const QString invalidModelPath = m_tempDir.filePath(QStringLiteral("model3d/load-invalid-model.obj"));
        QFile invalidModelFile(invalidModelPath);
        QVERIFY(QDir().mkpath(QFileInfo(invalidModelPath).absolutePath()));
        QVERIFY(invalidModelFile.open(QIODevice::WriteOnly));
        QVERIFY(invalidModelFile.write(QByteArrayLiteral("not-an-obj")) > 0);
        invalidModelFile.close();

        QVERIFY(m_cache->loadModel3D(modelUuid, QStringLiteral("obj")).isEmpty());
        QVERIFY(!QFileInfo::exists(m_tempDir.filePath(QStringLiteral("model3d/load-invalid-model.obj"))));
    }

    // 验证仅由封装数据携带的 3D 模型也能完整写入并恢复缓存元数据。
    void testFootprintModel3DMetadataRoundTrip() {
        const QString componentId = QStringLiteral("C54321");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Footprint Model Component"));

        auto footprint = QSharedPointer<FootprintData>::create();
        Model3DData model;
        model.setUuid(QStringLiteral("footprint-only-model"));
        model.setName(QStringLiteral("FOOTPRINT_ONLY"));
        model.setTranslation(Model3DBase(1.0, 2.0, 3.0));
        model.setRotation(Model3DBase(4.0, 5.0, 6.0));
        footprint->setModel3D(model);
        data.setFootprintData(footprint);

        m_cache->saveComponentMetadata(componentId, data, 0, true);

        const QJsonObject metadata = m_cache->loadMetadataFromMemory(componentId);
        QCOMPARE(metadata.value(QStringLiteral("model3duuid")).toString(), QStringLiteral("footprint-only-model"));
        const QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QVERIFY(loadedData->model3DData() != nullptr);
        QCOMPARE(loadedData->model3DData()->uuid(), QStringLiteral("footprint-only-model"));
        QCOMPARE(loadedData->model3DData()->translation().z, 3.0);
        QCOMPARE(loadedData->model3DData()->rotation().x, 4.0);
    }

    // 回归测试：removeCache tombstone 阻止旧写入，但 clearTombstone 后允许新写入
    void testRemoveCacheTombstoneBlocksStaleWrites() {
        const QString componentId = QStringLiteral("C99999");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Tombstoned Component"));

        // 先正常写入
        m_cache->saveComponentMetadata(componentId, data);
        QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Tombstoned Component"));

        // removeCache 会设置 tombstone
        m_cache->removeCache(componentId);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);

        // tombstone 状态下，带 generation 的写入应被阻止
        const uint64_t staleGen = m_cache->currentGeneration();
        data.setName(QStringLiteral("Should Not Write"));
        m_cache->saveComponentMetadata(componentId, data, staleGen);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);  // tombstone 阻止了写入

        // clearTombstone 后，新写入应成功
        m_cache->clearTombstone(componentId);
        data.setName(QStringLiteral("Should Write Now"));
        m_cache->saveComponentMetadata(componentId, data);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Should Write Now"));
    }

    // 验证无效元器件编号不会污染 tombstone 状态。
    void testRemoveCacheIgnoresInvalidComponentId() {
        const QString invalidComponentId = QStringLiteral("not-a-component");

        m_cache->clearGlobalTombstone();
        m_cache->removeCache(invalidComponentId);

        QVERIFY(!m_cache->isTombstoned(invalidComponentId));
    }

    // 回归测试：clearAllCache 全局 tombstone 阻止所有写入，clearGlobalTombstone 解除
    void testGlobalTombstoneBlocksAllWrites() {
        const QString componentId = QStringLiteral("C88888");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Global Tombstone Test"));

        // clearAllCache 设置全局 tombstone
        m_cache->clearAllCache();

        // 带 generation 的写入应被阻止
        const uint64_t gen = m_cache->currentGeneration();
        m_cache->saveComponentMetadata(componentId, data, gen);
        QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);  // 全局 tombstone 阻止了写入

        // clearGlobalTombstone 后，新写入应成功
        m_cache->clearGlobalTombstone();
        m_cache->saveComponentMetadata(componentId, data);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Global Tombstone Test"));
    }

    // 验证清空全部缓存时会删除根目录下的遗留文件。
    void testClearAllCacheRemovesRootFiles() {
        const QString rootFilePath = QDir(m_cache->cacheDir()).filePath(QStringLiteral("legacy-cache.json"));
        QFile rootFile(rootFilePath);
        QVERIFY(rootFile.open(QIODevice::WriteOnly));
        QVERIFY(rootFile.write(QByteArrayLiteral("legacy")) > 0);
        rootFile.close();
        QVERIFY(QFileInfo::exists(rootFilePath));

        m_cache->clearAllCache();

        QVERIFY(!QFileInfo::exists(rootFilePath));
    }

    // 回归测试：generation 不匹配时写入被丢弃
    void testStaleGenerationBlocksWrite() {
        const QString componentId = QStringLiteral("C77777");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Stale Gen Test"));

        // 捕获当前 generation
        const uint64_t oldGen = m_cache->currentGeneration();

        // clearAllCache 递增 generation
        m_cache->clearAllCache();
        m_cache->clearGlobalTombstone();  // 解除全局 tombstone

        // 用旧 generation 写入应被丢弃
        m_cache->saveComponentMetadata(componentId, data, oldGen);
        QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded == nullptr);  // generation 不匹配，写入被丢弃

        // 用新 generation 写入应成功
        const uint64_t newGen = m_cache->currentGeneration();
        m_cache->saveComponentMetadata(componentId, data, newGen);
        loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QCOMPARE(loaded->name(), QStringLiteral("Stale Gen Test"));
    }

    // 验证缓存目录迁移后元数据和预览图仍可读取。
    void testCacheDirMigrationPreservesExistingCache() {
        const QString componentId = QStringLiteral("C24680");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Migrated Component"));

        m_cache->saveComponentMetadata(componentId, data);
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::white);
        QBuffer imageBuffer;
        QVERIFY(imageBuffer.open(QIODevice::WriteOnly));
        QVERIFY(image.save(&imageBuffer, "PNG"));
        const QByteArray previewData = imageBuffer.data();
        m_cache->savePreviewImage(componentId, previewData, 0);

        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());

        m_cache->setCacheDir(newCacheDir.path(), /*migrateExistingCache=*/true);

        QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QCOMPARE(loadedData->name(), QStringLiteral("Migrated Component"));
        QCOMPARE(m_cache->loadPreviewImage(componentId, 0), previewData);
    }

    // 验证缓存枚举和磁盘大小统计在目录迁移前后保持一致。
    void testCacheEnumerationAndSizeAfterMigration() {
        const QString firstComponentId = QStringLiteral("C24681");
        const QString secondComponentId = QStringLiteral("C24682");

        for (const QString& componentId : {firstComponentId, secondComponentId}) {
            ComponentData data;
            data.setLcscId(componentId);
            data.setName(QStringLiteral("Enumerated Component %1").arg(componentId));
            m_cache->saveComponentMetadata(componentId, data);
            m_cache->saveCadDataJson(componentId, QByteArrayLiteral("{\"shape\":[]}"));
        }

        const QStringList cachedIdsBeforeMigration = m_cache->getCachedComponentIds();
        QVERIFY(cachedIdsBeforeMigration.contains(firstComponentId));
        QVERIFY(cachedIdsBeforeMigration.contains(secondComponentId));
        const qint64 sizeBeforeMigration = m_cache->getCacheSize();
        QVERIFY(sizeBeforeMigration > 0);

        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());
        m_cache->setCacheDir(newCacheDir.path(), /*migrateExistingCache=*/true);

        const QStringList cachedIdsAfterMigration = m_cache->getCachedComponentIds();
        QVERIFY(cachedIdsAfterMigration.contains(firstComponentId));
        QVERIFY(cachedIdsAfterMigration.contains(secondComponentId));
        QVERIFY(m_cache->getCacheSize() >= sizeBeforeMigration);
    }

    // 验证缓存目录迁移后三维模型文件仍可读取。
    void testCacheDirMigrationPreservesObjModelCache() {
        const QString uuid = QStringLiteral("migrated-model-13579");
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nf 1 2 3");
        m_cache->saveModel3D(uuid, objData, QStringLiteral("obj"));

        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());
        m_cache->setCacheDir(newCacheDir.path(), /*migrateExistingCache=*/true);

        QVERIFY(m_cache->hasModel3DCached(uuid, QStringLiteral("obj")));
        QCOMPARE(m_cache->loadModel3D(uuid, QStringLiteral("obj")), objData);
    }

    // 验证切换缓存目录会使原一级缓存失效。
    void testCacheDirChangeInvalidatesL1Cache() {
        const QString componentId = QStringLiteral("C11223");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Old Directory Component"));
        m_cache->saveComponentMetadata(componentId, data);
        m_cache->saveSymbolData(componentId, QByteArray("old-symbol"));
        QVERIFY(m_cache->hasInMemoryCache(componentId));

        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());
        m_cache->setCacheDir(newCacheDir.path());

        QVERIFY(!m_cache->hasInMemoryCache(componentId));
        QCOMPARE(m_cache->getMemoryCacheSize(), qint64(0));
        QVERIFY(m_cache->loadSymbolData(componentId).isEmpty());
        QVERIFY(m_cache->loadComponentData(componentId) == nullptr);
    }

    // 验证异常三维元数据会使对应缓存失效并触发自愈。
    void testMalformedModel3DMetadataInvalidatesCache() {
        const QString componentId = QStringLiteral("C13579");
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Malformed 3D Component"));
        m_cache->saveComponentMetadata(componentId, data);

        const QString metadataPath = QDir(m_cache->componentCacheDir(componentId)).filePath("component.json");
        QFile metadataFile(metadataPath);
        QVERIFY(metadataFile.open(QIODevice::ReadOnly));
        QJsonParseError parseError;
        QJsonDocument document = QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
        metadataFile.close();
        QCOMPARE(parseError.error, QJsonParseError::NoError);

        QJsonObject metadata = document.object();
        metadata.insert(QStringLiteral("model3duuid"), QStringLiteral("uuid-13579"));
        QJsonObject malformedTranslation;
        malformedTranslation.insert(QStringLiteral("x"), QStringLiteral("invalid"));
        malformedTranslation.insert(QStringLiteral("y"), 0.0);
        malformedTranslation.insert(QStringLiteral("z"), 0.0);
        metadata.insert(QStringLiteral("model3dTranslation"), malformedTranslation);

        QVERIFY(metadataFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QVERIFY(metadataFile.write(QJsonDocument(metadata).toJson()) > 0);
        metadataFile.close();

        // 自愈流程应删除无法被正常加载的三维元数据缓存。
        m_cache->setCacheDir(m_tempDir.path());
        QVERIFY(!QFileInfo::exists(metadataPath));
        QVERIFY(!m_cache->hasCache(componentId));
        QVERIFY(m_cache->loadComponentData(componentId) == nullptr);
    }

    // 验证 STEP、OBJ 和 WRL 三种模型文件均可往返磁盘缓存。
    void testModel3DArtifactsRoundTripThroughDiskCache() {
        const QString uuid = QStringLiteral("model-cache-13579");
        const QList<QPair<QString, QByteArray>> artifacts = {
            {QStringLiteral("step"), QByteArray("ISO-10303-21; cached step END-ISO-10303-21;")},
            {QStringLiteral("obj"), QByteArray("v 0 0 0\nf 1 2 3")},
            {QStringLiteral("wrl"),
             QByteArray("#VRML V2.0 utf8\nShape { geometry IndexedFaceSet { coord Coordinate { point [0 0 0, 1 0 0, "
                        "0 1 0] } coordIndex [0, 1, 2, -1] } }")},
        };

        for (const auto& artifact : artifacts) {
            m_cache->saveModel3D(uuid, artifact.second, artifact.first);
            QVERIFY(m_cache->hasModel3DCached(uuid, artifact.first));
            QCOMPARE(m_cache->loadModel3D(uuid, artifact.first), artifact.second);
        }
    }

    // 验证复制三维模型时拒绝空缓存文件。
    void testCopyModel3DRejectsEmptyCacheFile() {
        const QString uuid = QStringLiteral("empty-model-13579");
        const QString modelPath = QDir(m_cache->cacheDir()).filePath(QStringLiteral("model3d/%1.step").arg(uuid));
        QVERIFY(QDir().mkpath(QFileInfo(modelPath).absolutePath()));
        QFile modelFile(modelPath);
        QVERIFY(modelFile.open(QIODevice::WriteOnly));
        modelFile.close();

        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());
        const QString destinationPath = QDir(outputDir.path()).filePath(QStringLiteral("model.step"));
        QVERIFY(!m_cache->copyModel3DToFile(uuid, QStringLiteral("step"), destinationPath));
        QVERIFY(!QFileInfo::exists(destinationPath));
    }

    // 验证三维缓存命中和复制接口都会拒绝结构无效的模型文件。
    void testModel3DCacheInterfacesRejectInvalidContent() {
        const QString uuid = QStringLiteral("invalid-interface-model-13579");
        const QString modelPath = m_tempDir.filePath(QStringLiteral("model3d/invalid-interface-model-13579.obj"));
        QFile modelFile(modelPath);
        QVERIFY(QDir().mkpath(QFileInfo(modelPath).absolutePath()));
        QVERIFY(modelFile.open(QIODevice::WriteOnly));
        QVERIFY(modelFile.write(QByteArrayLiteral("not-an-obj")) > 0);
        modelFile.close();
        QVERIFY(!m_cache->hasModel3DCached(uuid, QStringLiteral("obj")));

        QVERIFY(modelFile.open(QIODevice::WriteOnly));
        QVERIFY(modelFile.write(QByteArrayLiteral("not-an-obj")) > 0);
        modelFile.close();
        QTemporaryDir outputDir;
        QVERIFY(outputDir.isValid());
        const QString destinationPath = QDir(outputDir.path()).filePath(QStringLiteral("model.obj"));
        QVERIFY(!m_cache->copyModel3DToFile(uuid, QStringLiteral("obj"), destinationPath));
        QVERIFY(!QFileInfo::exists(destinationPath));
    }

    // 验证三维缓存不会接受项目未支持的文件扩展名。
    void testModel3DRejectsUnsupportedExtension() {
        const QString uuid = QStringLiteral("unsupported-extension-model-13579");
        const QByteArray objData = QByteArrayLiteral("v 0 0 0\nf 1 2 3");

        m_cache->saveModel3D(uuid, objData, QStringLiteral("txt"));

        QVERIFY(!m_cache->hasModel3DCached(uuid, QStringLiteral("txt")));
    }

    // 验证三维缓存扩展名大小写不同仍能命中同一个缓存文件。
    void testModel3DNormalizesExtensionCase() {
        const QString uuid = QStringLiteral("case-normalized-model-13579");
        const QByteArray stepData = QByteArrayLiteral("ISO-10303-21;\nDATA;\nENDSEC;\nEND-ISO-10303-21;\n");

        m_cache->saveModel3D(uuid, stepData, QStringLiteral("STEP"));

        QVERIFY(m_cache->hasModel3DCached(uuid, QStringLiteral("step")));
        QCOMPARE(m_cache->loadModel3D(uuid, QStringLiteral("step")), stepData);
    }

    // 验证权威 CAD 元数据移除模型后会清理旧三维关联。
    void testAuthoritativeCadMetadataClearsRemovedModel3D() {
        const QString componentId = QStringLiteral("C24681");
        ComponentData withModel;
        withModel.setLcscId(componentId);
        withModel.setName(QStringLiteral("With model"));
        auto model = QSharedPointer<Model3DData>::create();
        model->setUuid(QStringLiteral("model-to-remove"));
        withModel.setModel3DData(model);
        m_cache->saveComponentMetadata(componentId, withModel);

        ComponentData withoutModel;
        withoutModel.setLcscId(componentId);
        withoutModel.setName(QStringLiteral("Without model"));
        m_cache->saveComponentMetadata(componentId, withoutModel, 0, /*replaceModel3DMetadata=*/true);

        const QSharedPointer<ComponentData> loaded = m_cache->loadComponentData(componentId);
        QVERIFY(loaded != nullptr);
        QVERIFY(!loaded->model3DData());
        QCOMPARE(loaded->name(), QStringLiteral("Without model"));
    }

private:
    QTemporaryDir m_tempDir;
    ComponentCacheService* m_cache = nullptr;
};

QTEST_GUILESS_MAIN(TestComponentCacheService)
#include "test_component_cache_service.moc"
