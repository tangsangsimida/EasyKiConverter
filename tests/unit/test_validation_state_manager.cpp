#include "ui/viewmodels/ValidationStateManager.h"

#include <QSignalSpy>
#include <QTest>

using namespace EasyKiConverter;

class TestValidationStateManager : public QObject {
    Q_OBJECT

private slots:

    // === 初始状态 ===

    void initialState() {
        ValidationStateManager manager;
        QCOMPARE(manager.pendingCount(), 0);
        QCOMPARE(manager.validatedCount(), 0);
        QVERIFY(manager.isAllDone());
        QVERIFY(!manager.isPreviewFetchEnabled());
        QVERIFY(manager.validatedComponentIds().isEmpty());
    }

    // === startValidation ===

    void startValidation_SetsPendingCount() {
        ValidationStateManager manager;
        manager.startValidation(5);
        QCOMPARE(manager.pendingCount(), 5);
        QVERIFY(!manager.isAllDone());
    }

    void startValidation_ClearsPreviousState() {
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));

        // 重新开始应清除之前的状态
        manager.startValidation(2);
        QCOMPARE(manager.pendingCount(), 2);
        QCOMPARE(manager.validatedCount(), 0);
        QVERIFY(manager.validatedComponentIds().isEmpty());
    }

    void startValidation_DisablesPreview() {
        ValidationStateManager manager;
        manager.startValidation(1);
        QVERIFY(!manager.isPreviewFetchEnabled());
    }

    void startValidation_EmitsSignal() {
        ValidationStateManager manager;
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.startValidation(3);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
    }

    // === addValidation ===

    void addValidation_IncrementsCounts() {
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.addValidation(2);
        QCOMPARE(manager.pendingCount(), 5);
    }

    void addValidation_EmitsSignal() {
        ValidationStateManager manager;
        manager.startValidation(1);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.addValidation(3);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 4);
    }

    void addValidation_IgnoresNonPositive() {
        ValidationStateManager manager;
        manager.startValidation(3);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.addValidation(0);
        manager.addValidation(-1);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(manager.pendingCount(), 3);
    }

    // === cancelValidation ===

    void cancelValidation_DecrementsCounts() {
        ValidationStateManager manager;
        manager.startValidation(5);
        manager.cancelValidation(2);
        QCOMPARE(manager.pendingCount(), 3);
    }

    void cancelValidation_FloorsAtZero() {
        ValidationStateManager manager;
        manager.startValidation(2);
        manager.cancelValidation(5);
        QCOMPARE(manager.pendingCount(), 0);
    }

    void cancelValidation_IgnoresNonPositive() {
        ValidationStateManager manager;
        manager.startValidation(3);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.cancelValidation(0);
        QCOMPARE(spy.count(), 0);
    }

    void cancelValidation_EmitsSignal() {
        ValidationStateManager manager;
        manager.startValidation(5);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.cancelValidation(2);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
    }

    void cancelValidation_DoesNotTriggerCompletion() {
        ValidationStateManager manager;
        manager.startValidation(2);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);
        manager.cancelValidation(2);

        // cancelValidation 将 total 也减为 0，条件 total > 0 不满足，不触发完成信号
        QCOMPARE(completionSpy.count(), 0);
        QVERIFY(manager.isAllDone());
    }

    // === onComponentValidated ===

    void onComponentValidated_DecrementsPending() {
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));
        QCOMPARE(manager.pendingCount(), 2);
        QCOMPARE(manager.validatedCount(), 1);
    }

    void onComponentValidated_DeduplicatesId() {
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentValidated(QStringLiteral("C1"));
        QCOMPARE(manager.validatedCount(), 1);
        QCOMPARE(manager.pendingCount(), 1);
    }

    void onComponentValidated_EmitsStateChanged() {
        ValidationStateManager manager;
        manager.startValidation(2);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.onComponentValidated(QStringLiteral("C1"));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
    }

    void onComponentValidated_ReturnsCorrectIds() {
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentValidated(QStringLiteral("C2"));
        QStringList ids = manager.validatedComponentIds();
        QCOMPARE(ids.size(), 2);
        QVERIFY(ids.contains(QStringLiteral("C1")));
        QVERIFY(ids.contains(QStringLiteral("C2")));
    }

    // === onComponentFailed ===

    void onComponentFailed_DecrementsPending() {
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentFailed(QStringLiteral("C1"));
        QCOMPARE(manager.pendingCount(), 2);
        QCOMPARE(manager.validatedCount(), 0);  // 失败的不计入 validated
    }

    void onComponentFailed_DeduplicatesId() {
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentFailed(QStringLiteral("C1"));
        manager.onComponentFailed(QStringLiteral("C1"));
        QCOMPARE(manager.pendingCount(), 1);
    }

    // === 完成流程 ===

    void completion_AllValidated() {
        ValidationStateManager manager;
        manager.startValidation(3);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentValidated(QStringLiteral("C2"));
        QCOMPARE(completionSpy.count(), 0);  // 还没完成

        manager.onComponentValidated(QStringLiteral("C3"));
        QCOMPARE(completionSpy.count(), 1);  // 全部完成

        QStringList completedIds = completionSpy.at(0).at(0).toStringList();
        QCOMPARE(completedIds.size(), 3);
    }

    void completion_MixedValidatedAndFailed() {
        ValidationStateManager manager;
        manager.startValidation(3);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentFailed(QStringLiteral("C2"));
        QCOMPARE(completionSpy.count(), 0);  // 还没完成

        manager.onComponentValidated(QStringLiteral("C3"));
        QCOMPARE(completionSpy.count(), 1);  // 全部完成（2 成功 + 1 失败 = 3）

        QStringList completedIds = completionSpy.at(0).at(0).toStringList();
        QCOMPARE(completedIds.size(), 2);  // 只包含成功的
        QVERIFY(completedIds.contains(QStringLiteral("C1")));
        QVERIFY(completedIds.contains(QStringLiteral("C3")));
    }

    void completion_AllFailed() {
        ValidationStateManager manager;
        manager.startValidation(2);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);
        manager.onComponentFailed(QStringLiteral("C1"));
        manager.onComponentFailed(QStringLiteral("C2"));
        QCOMPARE(completionSpy.count(), 1);

        QStringList completedIds = completionSpy.at(0).at(0).toStringList();
        QVERIFY(completedIds.isEmpty());  // 全部失败，没有成功的 ID
    }

    void completion_WithAddValidation() {
        ValidationStateManager manager;
        manager.startValidation(1);

        QSignalSpy completionSpy(&manager, &ValidationStateManager::validationCompleted);

        // startValidation(1) + onComponentValidated → 第一次完成
        manager.onComponentValidated(QStringLiteral("C1"));
        QCOMPARE(completionSpy.count(), 1);

        // addValidation 增加 pending 和 total
        manager.addValidation(1);
        QCOMPARE(manager.pendingCount(), 1);

        // 第二次验证 → 第二次完成
        manager.onComponentValidated(QStringLiteral("C2"));
        QCOMPARE(completionSpy.count(), 2);
    }

    // === reset ===

    void reset_ClearsAllState() {
        ValidationStateManager manager;
        manager.startValidation(3);
        manager.onComponentValidated(QStringLiteral("C1"));
        manager.onComponentFailed(QStringLiteral("C2"));

        manager.reset();
        QCOMPARE(manager.pendingCount(), 0);
        QCOMPARE(manager.validatedCount(), 0);
        QVERIFY(manager.isAllDone());
        QVERIFY(manager.validatedComponentIds().isEmpty());
        QVERIFY(!manager.isPreviewFetchEnabled());
    }

    void reset_EmitsSignal() {
        ValidationStateManager manager;
        manager.startValidation(3);
        QSignalSpy spy(&manager, &ValidationStateManager::validationStateChanged);
        manager.reset();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
    }

    // === isAllDone ===

    void isAllDone_TrueWhenPendingZero() {
        ValidationStateManager manager;
        manager.startValidation(1);
        QVERIFY(!manager.isAllDone());
        manager.onComponentValidated(QStringLiteral("C1"));
        QVERIFY(manager.isAllDone());
    }

    void isAllDone_TrueWhenCancelledBelowZero() {
        ValidationStateManager manager;
        manager.startValidation(2);
        manager.cancelValidation(5);
        QVERIFY(manager.isAllDone());
    }
};

QTEST_GUILESS_MAIN(TestValidationStateManager)
#include "test_validation_state_manager.moc"
