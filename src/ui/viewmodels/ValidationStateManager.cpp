#include "ValidationStateManager.h"

#include <QDebug>

namespace EasyKiConverter {

ValidationStateManager::ValidationStateManager(QObject* parent)
    : QObject(parent), m_pendingValidationCount(0), m_totalValidationCount(0), m_previewFetchEnabled(false) {}

ValidationStateManager::~ValidationStateManager() {}

void ValidationStateManager::startValidation(int count) {
    m_pendingValidationCount = count;
    m_totalValidationCount = count;
    m_validatedComponentIds.clear();
    m_failedComponentIds.clear();
    m_previewFetchEnabled = false;
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Validation started with" << count << "components pending, total:" << m_totalValidationCount;
}

void ValidationStateManager::addValidation(int count) {
    if (count <= 0) {
        return;
    }
    m_pendingValidationCount += count;
    m_totalValidationCount += count;
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Added" << count << "components to validation, pending count:" << m_pendingValidationCount
             << ", total:" << m_totalValidationCount;
}

void ValidationStateManager::cancelValidation(int count) {
    if (count <= 0) {
        return;
    }
    m_pendingValidationCount = qMax(0, m_pendingValidationCount - count);
    m_totalValidationCount = qMax(0, m_totalValidationCount - count);
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Cancelled" << count << "components from validation, pending count:" << m_pendingValidationCount
             << ", total:" << m_totalValidationCount;
    checkAndNotifyCompletion();
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
    m_totalValidationCount = 0;
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
    // 只有当所有应该验证的组件都验证完成时，才触发预览图获取
    // 需要同时满足：
    // 1. m_pendingValidationCount <= 0（没有待验证的组件）
    // 2. m_validatedComponentIds.size() == m_totalValidationCount（验证的组件数等于总数）
    if (m_pendingValidationCount <= 0 && m_validatedComponentIds.size() == m_totalValidationCount) {
        qDebug() << "All validations completed," << m_validatedComponentIds.size() << "components validated (total was"
                 << m_totalValidationCount << "), triggering preview fetch";
        emit validationCompleted(m_validatedComponentIds);
    }
}

}  // namespace EasyKiConverter
