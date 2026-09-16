#include "services/ComponentCacheService.h"

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
        m_cache->savePreviewImage(componentId, QByteArray("preview-data"), 0);

        QTemporaryDir newCacheDir;
        QVERIFY(newCacheDir.isValid());

        m_cache->setCacheDir(newCacheDir.path(), /*migrateExistingCache=*/true);

        QSharedPointer<ComponentData> loadedData = m_cache->loadComponentData(componentId);
        QVERIFY(loadedData != nullptr);
        QCOMPARE(loadedData->name(), QStringLiteral("Migrated Component"));
        QCOMPARE(m_cache->loadPreviewImage(componentId, 0), QByteArray("preview-data"));
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
            {QStringLiteral("step"), QByteArray("ISO-10303-21; cached step")},
            {QStringLiteral("obj"), QByteArray("v 0 0 0\nf 1 2 3")},
            {QStringLiteral("wrl"), QByteArray("#VRML V2.0 cached wrl")},
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
