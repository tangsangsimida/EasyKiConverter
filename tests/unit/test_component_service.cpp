#include "services/ComponentService.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestComponentService : public QObject {
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

    void testInactiveStaleFetchingEntryIsReinitialized() {
        const QString componentId = QStringLiteral("C12345");
        saveCorruptSymbolFootprintCache(componentId);

        ComponentService service;
        service.testSetFetchingState(componentId, QString(), false, false);

        service.fetchComponentData(componentId, false);

        QVERIFY(service.testHasFetchingState(componentId));
        QCOMPARE(service.testFetchingComponentId(componentId), componentId);
        QVERIFY(service.testFetchingRequestActive(componentId));
        QVERIFY(!service.testFetchingHasCadData(componentId));

        service.cancelAllPendingRequests();
    }

    void testActiveDuplicateRequestIsStillSkipped() {
        const QString componentId = QStringLiteral("C23456");

        ComponentService service;
        service.testSetFetchingState(componentId, QStringLiteral("original-active-entry"), false, true);

        service.fetchComponentData(componentId, false);

        QVERIFY(service.testHasFetchingState(componentId));
        QCOMPARE(service.testFetchingComponentId(componentId), QStringLiteral("original-active-entry"));
        QVERIFY(service.testFetchingRequestActive(componentId));
    }

    void testCancelledCacheLoadDoesNotRecreateFetchingEntry() {
        const QString componentId = QStringLiteral("C34567");
        saveCorruptSymbolFootprintCache(componentId);

        ComponentService service;
        service.fetchComponentData(componentId, false);
        service.cancelRequestForComponent(componentId);

        QTest::qWait(200);

        QVERIFY(!service.testHasFetchingState(componentId));
    }

private:
    void saveCorruptSymbolFootprintCache(const QString& componentId) {
        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("Cached Component"));
        m_cache->saveComponentMetadata(componentId, data);
        m_cache->saveCadDataJson(componentId, QByteArrayLiteral("{}"));
        QVERIFY(m_cache->hasSymbolFootprintCache(componentId));
    }

    QTemporaryDir m_tempDir;
    ComponentCacheService* m_cache = nullptr;
};

QTEST_GUILESS_MAIN(TestComponentService)
#include "test_component_service.moc"
