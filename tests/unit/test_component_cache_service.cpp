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

private:
    QTemporaryDir m_tempDir;
    ComponentCacheService* m_cache = nullptr;
};

QTEST_GUILESS_MAIN(TestComponentCacheService)
#include "test_component_cache_service.moc"
