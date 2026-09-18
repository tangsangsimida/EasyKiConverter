#include "ExportProgressResultsCoordinator.h"

#include "ExportProgressViewModel.h"

namespace EasyKiConverter {

/** @brief 更新成功和失败结果数量。 */
void ExportProgressResultsCoordinator::updateResultsList(ExportProgressViewModel& owner) {
    int success = 0;
    int failure = 0;
    for (const auto& item : owner.m_resultsList) {
        const QVariantMap map = item.toMap();
        if (map.value("status") == "success") {
            ++success;
        } else if (map.value("status") == "failed") {
            ++failure;
        }
    }
    if (owner.m_successCount != success) {
        owner.m_successCount = success;
        emit owner.successCountChanged();
    }
    if (owner.m_failureCount != failure) {
        owner.m_failureCount = failure;
        emit owner.failureCountChanged();
    }
}

/** @brief 转发过滤结果列表刷新信号。 */
void ExportProgressResultsCoordinator::updateFilteredResults(ExportProgressViewModel& owner) {
    emit owner.filteredResultsListChanged();
}

/** @brief 根据当前过滤模式筛选结果项。 */
QVariantList ExportProgressResultsCoordinator::filteredResultsList(const ExportProgressViewModel& owner) {
    if (owner.m_filterMode == "all") {
        return owner.m_resultsList;
    }

    QVariantList filtered;
    for (const auto& item : owner.m_resultsList) {
        const QVariantMap map = item.toMap();
        if (owner.m_filterMode == "success" && map.value("status") == "success") {
            filtered.append(item);
        } else if (owner.m_filterMode == "failed" && map.value("status") == "failed") {
            filtered.append(item);
        } else if (owner.m_filterMode == "exporting" &&
                   (map.value("status") == "pending" || map.value("status") == "in_progress")) {
            filtered.append(item);
        }
    }
    return filtered;
}

/** @brief 根据当前过滤模式返回成功数量。 */
int ExportProgressResultsCoordinator::filteredSuccessCount(const ExportProgressViewModel& owner) {
    if (owner.m_filterMode == "all" || owner.m_filterMode == "success") {
        return owner.m_successCount;
    }
    return 0;
}

/** @brief 根据当前过滤模式返回失败数量。 */
int ExportProgressResultsCoordinator::filteredFailedCount(const ExportProgressViewModel& owner) {
    if (owner.m_filterMode == "all" || owner.m_filterMode == "failed") {
        return owner.m_failureCount;
    }
    return 0;
}

/** @brief 统计待处理和进行中的结果项。 */
int ExportProgressResultsCoordinator::filteredPendingCount(const ExportProgressViewModel& owner) {
    int pending = 0;
    for (const auto& item : owner.m_resultsList) {
        const QString status = item.toMap().value("status").toString();
        if (status == "pending" || status == "in_progress") {
            ++pending;
        }
    }
    return pending;
}

/** @brief 统计指定类型状态为成功的结果项。 */
int ExportProgressResultsCoordinator::typeSuccessCount(const ExportProgressViewModel& owner, const QString& statusKey) {
    return countItemsWithTypeStatus(owner, statusKey, QStringLiteral("success"));
}

/** @brief 将导出类型名称映射到结果字段。 */
QString ExportProgressResultsCoordinator::typeStatusKey(const QString& typeName) {
    if (typeName == "Symbol") {
        return "symbolStatus";
    }
    if (typeName == "Footprint") {
        return "footprintStatus";
    }
    if (typeName == "Model3D") {
        return "model3DStatus";
    }
    if (typeName == "PreviewImages") {
        return "previewStatus";
    }
    if (typeName == "Datasheet") {
        return "datasheetStatus";
    }
    return QString();
}

/** @brief 根据启用类型和各类型状态更新结果总体状态。 */
void ExportProgressResultsCoordinator::updateOverallItemStatus(const ExportProgressViewModel& owner,
                                                               QVariantMap& result) {
    struct TypeRule {
        bool enabled;
        QString statusKey;
    };

    const QList<TypeRule> rules = {
        {owner.m_exportSymbolEnabled, "symbolStatus"},
        {owner.m_exportFootprintEnabled, "footprintStatus"},
        {owner.m_exportModel3DEnabled, "model3DStatus"},
        {owner.m_exportPreviewEnabled, "previewStatus"},
        {owner.m_exportDatasheetEnabled, "datasheetStatus"},
    };

    bool hasEnabledType = false;
    bool anyFailed = false;
    bool anyInProgress = false;
    bool allDone = true;
    for (const TypeRule& rule : rules) {
        if (!rule.enabled) {
            continue;
        }
        hasEnabledType = true;
        const QString status = result.value(rule.statusKey, "pending").toString();
        anyFailed = anyFailed || status == "failed";
        anyInProgress = anyInProgress || status == "in_progress";
        allDone = allDone && (status == "success" || status == "failed" || status == "skipped");
    }

    if (!hasEnabledType) {
        result["status"] = "success";
    } else if (allDone) {
        result["status"] = anyFailed ? "failed" : "success";
    } else if (anyInProgress) {
        result["status"] = "in_progress";
    } else {
        result["status"] = "pending";
    }
}

/** @brief 按启用的导出类型重置结果项状态。 */
void ExportProgressResultsCoordinator::resetItemForRetry(const ExportProgressViewModel& owner, QVariantMap& result) {
    result["status"] = "pending";
    result["symbolSuccess"] = !owner.m_exportSymbolEnabled;
    result["footprintSuccess"] = !owner.m_exportFootprintEnabled;
    result["model3DSuccess"] = !owner.m_exportModel3DEnabled;
    result["previewSuccess"] = !owner.m_exportPreviewEnabled;
    result["datasheetSuccess"] = !owner.m_exportDatasheetEnabled;
    result["symbolStatus"] = owner.m_exportSymbolEnabled ? "pending" : "disabled";
    result["footprintStatus"] = owner.m_exportFootprintEnabled ? "pending" : "disabled";
    result["model3DStatus"] = owner.m_exportModel3DEnabled ? "pending" : "disabled";
    result["previewStatus"] = owner.m_exportPreviewEnabled ? "pending" : "disabled";
    result["datasheetStatus"] = owner.m_exportDatasheetEnabled ? "pending" : "disabled";
    result["error"] = QString();
}

/** @brief 统计结果列表中指定字段处于指定状态的项目数。 */
int ExportProgressResultsCoordinator::countItemsWithTypeStatus(const ExportProgressViewModel& owner,
                                                               const QString& key,
                                                               const QString& expectedStatus) {
    int count = 0;
    for (const QVariant& item : owner.m_resultsList) {
        if (item.toMap().value(key).toString() == expectedStatus) {
            ++count;
        }
    }
    return count;
}

}  // namespace EasyKiConverter
