#include "ValidationStateManager.h"

#include <QDebug>

namespace EasyKiConverter {

ValidationStateManager::ValidationStateManager(QObject* parent)
    : QObject(parent), m_pendingValidationCount(0), m_previewFetchEnabled(false) {}

ValidationStateManager::~ValidationStateManager() {}

void ValidationStateManager::startValidation(int count) {
    m_pendingValidationCount = count;
    m_validatedComponentIds.clear();
    m_previewFetchEnabled = false;
    emit validationStateChanged(m_pendingValidationCount);
    qDebug() << "Validation started with" << count << "components pending";
}

void ValidationStateManager::onComponentValidated(const QString& componentId) {
    m_pendingValidationCount--;
    if (!m_validatedComponentIds.contains(componentId)) {
        m_validatedComponentIds.append(componentId);
    }
    emit validationStateChanged(m_pendingValidationCount);
    checkAndNotifyCompletion();
}

void ValidationStateManager::onComponentFailed(const QString& componentId) {
    Q_UNUSED(componentId);
    m_pendingValidationCount--;
    emit validationStateChanged(m_pendingValidationCount);
    checkAndNotifyCompletion();
}

void ValidationStateManager::reset() {
    m_pendingValidationCount = 0;
    m_validatedComponentIds.clear();
    m_previewFetchEnabled = false;
    emit validationStateChanged(m_pendingValidationCount);
}

bool ValidationStateManager::isAllDone() const {
    return m_pendingValidationCount <= 0;
}

bool ValidationStateManager::isPreviewFetchEnabled() const {
    return m_previewFetchEnabled;
}

int ValidationStateManager::pendingCount() const {
    return m_pendingValidationCount;
}

int ValidationStateManager::validatedCount() const {
    return m_validatedComponentIds.size();
}

QStringList ValidationStateManager::validatedComponentIds() const {
    return m_validatedComponentIds;
}

void ValidationStateManager::checkAndNotifyCompletion() {
    if (m_pendingValidationCount <= 0 && !m_previewFetchEnabled) {
        m_previewFetchEnabled = true;
        qDebug() << "All validations completed," << m_validatedComponentIds.size()
                 << "components validated, enabling preview fetch";
        emit validationCompleted(m_validatedComponentIds);
    }
}

}  // namespace EasyKiConverter