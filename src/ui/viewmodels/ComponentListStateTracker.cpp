#include "ComponentListStateTracker.h"

namespace EasyKiConverter {

// 根据元件验证阶段和可重试标记计算列表统计数据。
void ComponentListStateTracker::recompute(const QList<ComponentListItemData*>& items) {
    int validatingCount = 0;
    int validCount = 0;
    int invalidCount = 0;
    int retryableInvalidCount = 0;

    for (const ComponentListItemData* item : items) {
        if (!item) {
            continue;
        }

        const QString phase = item->validationPhase();
        if (phase == QStringLiteral("validating")) {
            ++validatingCount;
        } else if (phase == QStringLiteral("completed") || phase == QStringLiteral("fetching_preview")) {
            ++validCount;
        } else if (phase == QStringLiteral("failed")) {
            ++invalidCount;
            if (item->retryable()) {
                ++retryableInvalidCount;
            }
        }
    }

    m_validatingCount = validatingCount;
    m_validCount = validCount;
    m_invalidCount = invalidCount;
    m_retryableInvalidCount = retryableInvalidCount;
}

// 只在过滤模式真正变化时更新状态，避免重复发送界面通知。
bool ComponentListStateTracker::setFilterMode(const QString& mode) {
    if (m_filterMode == mode) {
        return false;
    }
    m_filterMode = mode;
    return true;
}

// 返回供 QML 属性读取的过滤模式。
QString ComponentListStateTracker::filterMode() const {
    return m_filterMode;
}

// 根据过滤模式选择对应计数，未知模式回退到列表总数。
int ComponentListStateTracker::filteredCount(int totalCount) const {
    if (m_filterMode == QStringLiteral("validating")) {
        return m_validatingCount;
    }
    if (m_filterMode == QStringLiteral("valid")) {
        return m_validCount;
    }
    if (m_filterMode == QStringLiteral("invalid")) {
        return m_invalidCount;
    }
    return totalCount;
}

// 返回验证中的元件数量。
int ComponentListStateTracker::validatingCount() const {
    return m_validatingCount;
}

// 返回验证成功的元件数量。
int ComponentListStateTracker::validCount() const {
    return m_validCount;
}

// 返回验证失败的元件数量。
int ComponentListStateTracker::invalidCount() const {
    return m_invalidCount;
}

// 返回是否存在可以再次发起验证的失败元件。
bool ComponentListStateTracker::hasRetryableInvalidComponents() const {
    return m_retryableInvalidCount > 0;
}

}  // namespace EasyKiConverter
