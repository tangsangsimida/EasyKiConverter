#include "services/ComponentService.h"
#include "tests/common/TestPaths.hpp"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

using namespace EasyKiConverter;

class TestComponentService : public QObject {
    Q_OBJECT

private slots:

    /** @brief 初始化独立的临时缓存目录。 */
    void init() {
        QVERIFY(m_tempDir.isValid());
        m_cache = ComponentCacheService::instance();
        m_cache->setCacheDir(m_tempDir.path());
        m_cache->clearAllCache();
    }

    /** @brief 清理测试产生的缓存数据。 */
    void cleanup() {
        if (m_cache != nullptr) {
            m_cache->clearAllCache();
        }
    }

    /** @brief 验证失效的获取状态会在新请求时重新初始化。 */
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

    /** @brief 验证活动中的重复请求不会被重复调度。 */
    void testActiveDuplicateRequestIsStillSkipped() {
        const QString componentId = QStringLiteral("C23456");

        ComponentService service;
        service.testSetFetchingState(componentId, QStringLiteral("original-active-entry"), false, true);

        service.fetchComponentData(componentId, false);

        QVERIFY(service.testHasFetchingState(componentId));
        QCOMPARE(service.testFetchingComponentId(componentId), QStringLiteral("original-active-entry"));
        QVERIFY(service.testFetchingRequestActive(componentId));
    }

    /** @brief 验证取消缓存加载后不会重新创建获取状态。 */
    void testCancelledCacheLoadDoesNotRecreateFetchingEntry() {
        const QString componentId = QStringLiteral("C34567");
        saveCorruptSymbolFootprintCache(componentId);

        ComponentService service;
        service.fetchComponentData(componentId, false);
        service.cancelRequestForComponent(componentId);

        QTest::qWait(200);

        QVERIFY(!service.testHasFetchingState(componentId));
    }

    /** @brief 验证缓存 CAD 封装中的模型 UUID 会在 OBJ 预加载前恢复。 */
    void testCachedFootprintModelRestoresBeforeObjPreload() {
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
        metadata.setName(QStringLiteral("Cached R0603"));
        m_cache->saveComponentMetadata(componentId, metadata);
        m_cache->saveCadDataJson(componentId, cadData);
        m_cache->saveModel3D(modelUuid, objData, QStringLiteral("obj"));

        ComponentData receivedData;
        bool received = false;
        ComponentService service;
        connect(&service, &ComponentService::cadDataReady, this, [&](const QString& id, const ComponentData& data) {
            if (id == componentId) {
                receivedData = data;
                received = true;
            }
        });

        service.fetchComponentData(componentId, true);

        QTRY_VERIFY_WITH_TIMEOUT(received, 3000);
        QVERIFY(receivedData.model3DData() != nullptr);
        QCOMPARE(receivedData.model3DData()->uuid(), modelUuid);
        QCOMPARE(receivedData.model3DObjRaw(), objData);
    }

    /** @brief 验证空批量请求会立即发出完成信号。 */
    void testEmptyBatchFetchCompletesImmediately() {
        ComponentService service;
        QSignalSpy completedSpy(&service, &ComponentService::allComponentsDataCollectedWithErrors);

        service.fetchMultipleComponentsData({}, false);

        QCOMPARE(completedSpy.count(), 1);
        const QList<QVariant> arguments = completedSpy.at(0);
        QCOMPARE(arguments.size(), 2);
        const QList<ComponentData> collectedData = qvariant_cast<QList<ComponentData>>(arguments.at(0));
        const QMap<QString, QString> failedComponents = qvariant_cast<QMap<QString, QString>>(arguments.at(1));
        QCOMPARE(collectedData.size(), 0);
        QVERIFY(failedComponents.isEmpty());
    }

    /** @brief 验证清空缓存后新请求可以恢复缓存写入。 */
    void testNewRequestReleasesGlobalCacheTombstone() {
        const QString componentId = QStringLiteral("C54331");
        m_cache->clearAllCache();

        ComponentService service;
        service.fetchComponentData(componentId, false);

        ComponentData data;
        data.setLcscId(componentId);
        data.setName(QStringLiteral("New request component"));
        m_cache->saveComponentMetadata(componentId, data, m_cache->currentGeneration());

        QVERIFY(m_cache->hasCache(componentId));
        service.cancelAllPendingRequests();
    }

private:
    /** @brief 写入用于触发缓存加载失败回退的最小缓存。 */
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
