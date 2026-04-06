#include "ValidationStateManager.h"

#include <QDebug>

namespace EasyKiConverter {

ValidationStateManager::ValidationStateManager(QObject* parent) : QObject(parent), m_pendingValidationCount(0) {}

ValidationStateManager::~ValidationStateManager() {}

void ValidationStateManager::startValidation(int count) {
    m_pendingValidationCount = count;
    m_validatedComponentIds.clear();
    m_failedComponentIds.clear();
    m_previewFetchEnabled = false;
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Validation started with" << count << "components pending";
}

void ValidationStateManager::addValidation(int count) {
    if (count <= 0) {
        return;
    }
    m_pendingValidationCount += count;
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Added" << count << "components to validation, pending count:" << m_pendingValidationCount;
}

void ValidationStateManager::onComponentValidated(const QString& componentId) {
    if (m_pendingValidationCount > 0) {
        m_pendingValidationCount--;
    }
    if (!m_validatedComponentIds.contains(componentId)) {
        m_validatedComponentIds.append(componentId);
    }
    emit validationStateChanged(m_pendingValidationCount);
    checkAndNotifyCompletion();
}

void ValidationStateManager::onComponentFailed(const QString& componentId) {
    if (m_pendingValidationCount > 0) {
        m_pendingValidationCount--;
    }
    if (!m_failedComponentIds.contains(componentId)) {
        m_failedComponentIds.append(componentId);
    }
    emit validationStateChanged(m_pendingValidationCount);
    checkAndNotifyCompletion();
}

void ValidationStateManager::reset() {
    m_pendingValidationCount = 0;
    m_validatedComponentIds.clear();
    m_failedComponentIds.clear();
    m_previewFetchEnabled = false;
    emit validationStateChanged(m_pendingValidationCount);
}

bool ValidationStateManager::isAllDone() const {
    return m_pendingValidationCount <= 0;
}

int ValidationStateManager::pendingCount() const {
    return m_pendingValidationCount;
}

bool ValidationStateManager::isPreviewFetchEnabled() const {
    return m_previewFetchEnabled;
}

int ValidationStateManager::validatedCount() const {
    return m_validatedComponentIds.size();
}

QStringList ValidationStateManager::validatedComponentIds() const {
    return m_validatedComponentIds;
}

void ValidationStateManager::checkAndNotifyCompletion() {
    // 改进：每当验证计数器变为0时都触发预览图获取
    // 依赖 LcscImageService 的 m_requestedComponents 来防止重复请求
    // 这样可以同时支持批量添加和混合缓存场景
    if (m_pendingValidationCount <= 0) {
        qDebug() << "Validation count reached 0," << m_validatedComponentIds.size()
                 << "components validated, triggering preview fetch";
        emit validationCompleted(m_validatedComponentIds);
    }
}

}  // namespace EasyKiConverter