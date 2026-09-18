#include "ExportProgressAggregator.h"

#include "ExportWorkerHelpers.h"

namespace EasyKiConverter {

void ExportProgressAggregator::mergeItemStatus(ExportTypeProgress& progress,
                                               const QString& componentId,
                                               const ExportItemStatus& status) {
    ExportItemStatus mergedStatus = status;
    const auto previousStatus = progress.itemStatus.constFind(componentId);
    if (previousStatus != progress.itemStatus.cend()) {
        for (const QString& diagnostic : previousStatus->diagnostics) {
            if (!mergedStatus.diagnostics.contains(diagnostic)) {
                mergedStatus.diagnostics.append(diagnostic);
            }
        }
    }
    progress.itemStatus[componentId] = mergedStatus;
    ExportWorkerHelpers::recomputeTypeProgressCounts(progress);
}

void ExportProgressAggregator::finalizeTypeProgress(ExportTypeProgress& progress,
                                                    const QString& typeName,
                                                    int totalCount) {
    progress.typeName = typeName;
    progress.totalCount = totalCount;
    progress.inProgressCount = 0;
    ExportWorkerHelpers::recomputeTypeProgressCounts(progress);
}

ExportCompletionTotals ExportProgressAggregator::countCompletedComponents(
    const QStringList& componentIds,
    const QMap<QString, ExportTypeProgress>& typeProgress) {
    ExportCompletionTotals totals;
    for (const QString& componentId : componentIds) {
        bool anyFailed = false;
        bool allDone = true;

        for (auto it = typeProgress.cbegin(); it != typeProgress.cend(); ++it) {
            const ExportItemStatus itemStatus =
                it.value().itemStatus.value(componentId, ExportItemStatus{ExportItemStatus::Status::Pending});
            if (itemStatus.status == ExportItemStatus::Status::Failed) {
                anyFailed = true;
            }
            if (itemStatus.status != ExportItemStatus::Status::Success &&
                itemStatus.status != ExportItemStatus::Status::Failed &&
                itemStatus.status != ExportItemStatus::Status::Skipped) {
                allDone = false;
            }
        }

        if (!allDone) {
            continue;
        }
        if (anyFailed) {
            totals.failedCount++;
        } else {
            totals.successCount++;
        }
    }
    return totals;
}

}  // namespace EasyKiConverter
