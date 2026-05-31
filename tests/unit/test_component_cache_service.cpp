#include "services/ComponentCacheService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestComponentCacheService : public QObject {
    Q_OBJECT

private slots:

    void init() {
        QVERIFY(m_tempDir.isValid());
        m_cache = ComponentCacheService::instance();
        m_cache->setCacheDir(m_tempDir.path());
        m_cache->clearAllCache();
    }

    void cleanup() {
        if (m_cache != nullptr) {
            m_cache->clearAllCache();
        }
    }

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

private:
    QTemporaryDir m_tempDir;
    ComponentCacheService* m_cache = nullptr;
};

QTEST_GUILESS_MAIN(TestComponentCacheService)
#include "test_component_cache_service.moc"
