#include "PipelineCleanup.h"

#include "utils/PathSecurity.h"

#include <QDebug>

namespace EasyKiConverter {

void PipelineCleanup::emergencyCleanup(

    QList<QSharedPointer<ComponentExportStatus>>& completedStatuses,

    QMap<QString, QSharedPointer<ComponentData>>& preloadedData,

    QList<SymbolData>& symbols,  // 改为 SymbolData

    QString& tempDir) {
    qDebug() << "PipelineCleanup: Performing emergency cleanup...";

    for (const auto& status : completedStatuses) {
        if (status) {
            status->clearIntermediateData(false);
            status->clearStepData();
        }
    }
    completedStatuses.clear();
    preloadedData.clear();
    symbols.clear();

    if (!tempDir.isEmpty()) {
        removeTempDir(tempDir);
        tempDir.clear();
    }
}

bool PipelineCleanup::removeTempDir(const QString& tempDir) {
    if (tempDir.isEmpty())
        return true;

    if (PathSecurity::safeRemoveRecursively(tempDir)) {
        qDebug() << "PipelineCleanup: Temp folder removed safely:" << tempDir;
        return true;
    } else {
        qWarning() << "PipelineCleanup: Failed to remove temp folder:" << tempDir;
        return false;
    }
}

}  // namespace EasyKiConverter
